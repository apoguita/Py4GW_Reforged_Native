#include "base/py_bindings.h"

#include "GW/player/player.h"
#include "GW/context/context.h"
#include "GW/context/character.h"
#include "GW/context/chat.h"
#include "GW/context/game.h"
#include "GW/context/world.h"
#include "GW/common/constants/friend_list.h"
#include "GW/map/map.h"
#include "GW/agent/agent.h"
#include "GW/chat/chat.h"
#include "GW/friend_list/friend_list.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/party/party.h"
#include "GW/ui/ui.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace py = pybind11;

namespace {

// Parity with legacy GW::PartyMgr::GetIsPlayerLoaded(-1):
// returns true if the current player is connected in the party.
static bool GetIsPlayerLoaded() {
    auto* party_ctx = GW::Context::GetPartyContext();
    if (!(party_ctx && party_ctx->player_party && party_ctx->player_party->players.valid()))
        return false;
    uint32_t player_id = GW::player::GetPlayerNumber();
    for (const auto& player : party_ctx->player_party->players) {
        if (player.login_number == player_id)
            return player.connected();
    }
    return false;
}

std::wstring StrToWide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

std::string WideToStr(const wchar_t* wstr) {
    if (!wstr) return {};
    std::string result;
    while (*wstr) {
        wchar_t ch = *wstr++;
        result.push_back(static_cast<char>(ch < 128 ? ch : '?'));
    }
    return result;
}

static uint32_t PickHighest(uint32_t a, uint32_t b) {
    constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();
    bool a_ok = (a != 0 && a != INVALID);
    bool b_ok = (b != 0 && b != INVALID);
    if (a_ok && b_ok) return (a > b) ? a : b;
    if (a_ok) return a;
    if (b_ok) return b;
    return 0;
}

// ── PyPlayer class (parity with legacy py_player.h PyPlayer) ──
struct PyPlayer {
    int id = 0;
    int agent = 0;
    int target_id = 0;
    int mouse_over_id = 0;
    int observing_id = 0;
    std::string account_name;
    std::string account_email;
    uint32_t player_uuid[4] = {};
    int wins = 0;
    int losses = 0;
    int rating = 0;
    int qualifier_points = 0;
    int rank = 0;
    int tournament_reward_points = 0;
    int morale = 0;
    int experience = 0;
    int level = 0;
    int current_kurzick = 0, total_earned_kurzick = 0, max_kurzick = 0;
    int current_luxon = 0, total_earned_luxon = 0, max_luxon = 0;
    int current_imperial = 0, total_earned_imperial = 0, max_imperial = 0;
    int current_balth = 0, total_earned_balth = 0, max_balth = 0;
    int current_skill_points = 0, total_earned_skill_points = 0;
    std::vector<std::pair<int, int>> party_morale;
    std::vector<uint32_t> missions_completed, missions_bonus, missions_completed_hm, missions_bonus_hm;
    std::vector<std::pair<int, int>> controlled_minions;
    std::vector<uint32_t> unlocked_map, learnable_character_skills, unlocked_character_skills;

    PyPlayer() { GetContext(); }
    void ResetContext();
    void GetContext();

    void SendDialog(uint32_t dialog_id);
    bool ChangeTarget(uint32_t target_id);
    bool InteractAgent(int agent_id, bool call_target);
    bool CallTarget(int agent_id);
    bool IsAgentIDValid(int agent_id);

    void RequestChatHistory();
    bool IsChatHistoryReady();
    std::vector<std::string> GetChatHistory();
    bool Istyping();
    void SendChatCommand(const std::string& msg);
    void SendChat(char channel, const std::string& msg);
    void SendWhisper(const std::string& name, const std::string& msg);
    void SendFakeChat(int channel, const std::string& message);
    void SendFakeChatColored(int channel, const std::string& message, int r, int g, int b);

    uint32_t GetPlayerStatus();
    bool SetPlayerStatus(uint32_t status);
};

// ═══════════════════════════════════════════════════════════════════
//  PyPlayer implementation
// ═══════════════════════════════════════════════════════════════════

void PyPlayer::ResetContext() {
    id = 0; agent = 0; target_id = 0; mouse_over_id = 0; observing_id = 0;
    account_name.clear(); account_email.clear();
    for (auto& u : player_uuid) u = 0;
    wins = 0; losses = 0; rating = 0; qualifier_points = 0; rank = 0; tournament_reward_points = 0;
    morale = 0; experience = 0; level = 0;
    current_kurzick = total_earned_kurzick = max_kurzick = 0;
    current_luxon = total_earned_luxon = max_luxon = 0;
    current_imperial = total_earned_imperial = max_imperial = 0;
    current_balth = total_earned_balth = max_balth = 0;
    current_skill_points = total_earned_skill_points = 0;
    party_morale.clear();
    missions_completed.clear(); missions_bonus.clear();
    missions_completed_hm.clear(); missions_bonus_hm.clear();
    controlled_minions.clear();
    unlocked_map.clear(); learnable_character_skills.clear(); unlocked_character_skills.clear();
}

void PyPlayer::GetContext() {
    auto instance_type = GW::map::GetInstanceType();
    bool is_map_ready = GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving()
        && instance_type != GW::Constants::InstanceType::Loading;

    if (!is_map_ready) {
        ResetContext();
        return;
    }

    auto player_loaded = GetIsPlayerLoaded();
    if (!player_loaded) {
        ResetContext();
        return;
    }

    id = static_cast<int>(GW::agent::GetControlledCharacterId());
    agent = id;
    target_id = static_cast<int>(GW::agent::GetTargetId());
    observing_id = static_cast<int>(GW::agent::GetObservingId());

    if (GW::map::GetIsMapLoaded()) {
        auto* world = GW::Context::GetWorldContext();
        if (world) {
            auto* char_ctx = GW::Context::GetCharContext();
            if (char_ctx) {
                account_email = WideToStr(char_ctx->player_email);
                for (int i = 0; i < 4; ++i) player_uuid[i] = char_ctx->player_uuid[i];
            }

            auto* account = world->accountInfo;
            if (account) {
                account_name = WideToStr(account->account_name);
                wins = account->wins;
                losses = account->losses;
                rating = account->rating;
                qualifier_points = account->qualifier_points;
                rank = account->rank;
                tournament_reward_points = account->tournament_reward_points;
            }

            morale = static_cast<int>(PickHighest(world->morale, world->morale_dupe));

            party_morale.clear();
            auto* party_morale_info = world->player_morale_info;
            constexpr size_t MAX_PARTY_SIZE = 16;
            const uint32_t UINT_MAX_VALUE = std::numeric_limits<uint32_t>::max();
            for (size_t i = 0; i < MAX_PARTY_SIZE && party_morale_info; ++i) {
                uint32_t pm_agent_id = party_morale_info->agent_id;
                uint32_t pm_morale = party_morale_info->morale;
                if (pm_agent_id == 0 || pm_agent_id == UINT_MAX_VALUE)
                    break;
                party_morale.emplace_back(static_cast<int>(pm_agent_id), static_cast<int>(pm_morale));
                ++party_morale_info;
            }

            experience = static_cast<int>(PickHighest(world->experience, world->experience_dupe));
            level = static_cast<int>(PickHighest(world->level, world->level_dupe));
            current_kurzick = static_cast<int>(PickHighest(world->current_kurzick, world->current_kurzick_dupe));
            total_earned_kurzick = static_cast<int>(PickHighest(world->total_earned_kurzick, world->total_earned_kurzick_dupe));
            max_kurzick = world->max_kurzick;
            current_luxon = static_cast<int>(PickHighest(world->current_luxon, world->current_luxon_dupe));
            total_earned_luxon = static_cast<int>(PickHighest(world->total_earned_luxon, world->total_earned_luxon_dupe));
            max_luxon = world->max_luxon;
            current_imperial = static_cast<int>(PickHighest(world->current_imperial, world->current_imperial_dupe));
            total_earned_imperial = static_cast<int>(PickHighest(world->total_earned_imperial, world->total_earned_imperial_dupe));
            max_imperial = world->max_imperial;
            current_balth = static_cast<int>(PickHighest(world->current_balth, world->current_balth_dupe));
            total_earned_balth = static_cast<int>(PickHighest(world->total_earned_balth, world->total_earned_balth_dupe));
            max_balth = world->max_balth;
            current_skill_points = static_cast<int>(PickHighest(world->current_skill_points, world->current_skill_points_dupe));
            total_earned_skill_points = static_cast<int>(PickHighest(world->total_earned_skill_points, world->total_earned_skill_points_dupe));

            auto copy_uint_array = [](const auto& arr, std::vector<uint32_t>& out) {
                out.assign(arr.begin(), arr.end());
            };
            copy_uint_array(world->missions_completed, missions_completed);
            copy_uint_array(world->missions_bonus, missions_bonus);
            copy_uint_array(world->missions_completed_hm, missions_completed_hm);
            copy_uint_array(world->missions_bonus_hm, missions_bonus_hm);

            for (size_t i = 0; i < world->controlled_minion_count.size(); ++i) {
                const auto& cm = world->controlled_minion_count[i];
                controlled_minions.emplace_back(static_cast<int>(cm.agent_id), static_cast<int>(cm.minion_count));
            }

            copy_uint_array(world->unlocked_map, unlocked_map);
            copy_uint_array(world->learnable_character_skills, learnable_character_skills);
            copy_uint_array(world->unlocked_character_skills, unlocked_character_skills);
        }
    }
}

void PyPlayer::SendDialog(uint32_t dialog_id) {
    GW::agent::SendDialog(dialog_id);
}
bool PyPlayer::ChangeTarget(uint32_t new_target_id) {
    if (new_target_id == 0) return false;
    if (!GW::agent::GetAgentByID(new_target_id)) return false;
    GW::game_thread::Enqueue([new_target_id] {
        if (GW::agent::Agent* a = GW::agent::GetAgentByID(new_target_id)) {
            GW::agent::ChangeTarget(a);
        }
    });
    return true;
}
bool PyPlayer::InteractAgent(int agent_id, bool call_target) {
    if (agent_id == 0) return false;
    GW::game_thread::Enqueue([agent_id, call_target] {
        if (GW::agent::Agent* a = GW::agent::GetAgentByID(agent_id)) {
            GW::agent::InteractAgent(a, call_target);
        }
    });
    return true;
}
bool PyPlayer::CallTarget(int agent_id) {
    if (agent_id == 0) return false;
    GW::game_thread::Enqueue([agent_id] {
        const auto id = static_cast<uint32_t>(agent_id);
        GW::agent::Agent* agent = GW::agent::GetAgentByID(id);
        if (!agent || !agent->GetAsAgentLiving()) {
            return;
        }
        GW::agent::CallTarget(id);
    });
    return true;
}
bool PyPlayer::IsAgentIDValid(int agent_id) {
    return GW::agent::GetAgentByID(static_cast<uint32_t>(agent_id)) != nullptr;
}

// Chat history — reads from GW::chat::GetChatLog(), async-decode via game thread.
static std::vector<std::string> g_chat_history;
static bool g_chat_ready = false;

void PyPlayer::RequestChatHistory() {
    g_chat_ready = false;
    g_chat_history.clear();

    std::thread([]() {
        auto* log = GW::chat::GetChatLog();
        if (!log) { g_chat_ready = true; return; }

        std::vector<std::wstring> temp_chat_log;
        for (size_t i = 0; i < GW::Context::CHAT_LOG_LENGTH; ++i) {
            if (log->messages[i]) {
                temp_chat_log.push_back(log->messages[i]->message);
            }
        }

        std::vector<std::wstring> decoded_chat(temp_chat_log.size());
        auto start_time = std::chrono::steady_clock::now();

        GW::game_thread::Enqueue([temp_chat_log, &decoded_chat]() {
            for (size_t i = 0; i < temp_chat_log.size(); ++i) {
                GW::ui::AsyncDecodeStr(temp_chat_log[i].c_str(), &decoded_chat[i]);
            }
        });

        // Wait for all messages to be decoded (max 500ms timeout per entry)
        for (size_t i = 0; i < temp_chat_log.size(); ++i) {
            while (decoded_chat[i].empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= 500) {
                    decoded_chat[i] = L"[ERROR: Timeout]";
                    break;
                }
            }
        }

        std::vector<std::string> converted_chat;
        for (const auto& decoded_msg : decoded_chat) {
            std::string msg;
            for (const wchar_t* p = decoded_msg.c_str(); *p; ++p)
                msg.push_back(static_cast<char>(*p < 128 ? *p : '?'));
            converted_chat.push_back(msg);
        }

        g_chat_history = std::move(converted_chat);
        g_chat_ready = true;
    }).detach();
}
bool PyPlayer::IsChatHistoryReady()    { return g_chat_ready; }
std::vector<std::string> PyPlayer::GetChatHistory() { return g_chat_history; }
bool PyPlayer::Istyping()              { return GW::chat::GetIsTyping(); }
void PyPlayer::SendChatCommand(const std::string& msg) { GW::chat::SendChat('/', msg.c_str()); }
void PyPlayer::SendChat(char channel, const std::string& msg) { GW::chat::SendChat(channel, msg.c_str()); }
void PyPlayer::SendWhisper(const std::string& name, const std::string& msg) { GW::chat::SendChat(name.c_str(), msg.c_str()); }
void PyPlayer::SendFakeChat(int channel, const std::string& message) { GW::chat::SendFakeChat(channel, message); }
void PyPlayer::SendFakeChatColored(int channel, const std::string& message, int r, int g, int b) {
    GW::chat::SendFakeChatColored(channel, message, r, g, b);
}

uint32_t PyPlayer::GetPlayerStatus() {
    return static_cast<uint32_t>(GW::friend_list::GetMyStatus());
}
bool PyPlayer::SetPlayerStatus(uint32_t status) {
    constexpr auto max_status = static_cast<uint32_t>(GW::Constants::FriendStatus::Away);
    if (status > max_status) return false;
    const auto friend_status = static_cast<GW::Constants::FriendStatus>(status);
    GW::game_thread::Enqueue([friend_status] {
        GW::friend_list::SetFriendListStatus(friend_status);
    });
    return true;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyPlayer, m) {
    m.doc() = "Py4GW Player bindings";

    // ── PyPlayer class (legacy surface) ──
    py::class_<PyPlayer>(m, "PyPlayer")
        .def(py::init<>())
        .def("GetContext", &PyPlayer::GetContext)
        .def_readonly("id", &PyPlayer::id)
        .def_readonly("agent", &PyPlayer::agent)
        .def_readonly("target_id", &PyPlayer::target_id)
        .def_readonly("mouse_over_id", &PyPlayer::mouse_over_id)
        .def_readonly("observing_id", &PyPlayer::observing_id)
        .def_readonly("account_name", &PyPlayer::account_name)
        .def_readonly("account_email", &PyPlayer::account_email)
        .def_readonly("player_uuid", &PyPlayer::player_uuid)
        .def_readonly("wins", &PyPlayer::wins)
        .def_readonly("losses", &PyPlayer::losses)
        .def_readonly("rating", &PyPlayer::rating)
        .def_readonly("qualifier_points", &PyPlayer::qualifier_points)
        .def_readonly("rank", &PyPlayer::rank)
        .def_readonly("tournament_reward_points", &PyPlayer::tournament_reward_points)
        .def_readonly("morale", &PyPlayer::morale)
        .def_readonly("party_morale", &PyPlayer::party_morale)
        .def_readonly("experience", &PyPlayer::experience)
        .def_readonly("level", &PyPlayer::level)
        .def_readonly("current_kurzick", &PyPlayer::current_kurzick)
        .def_readonly("total_earned_kurzick", &PyPlayer::total_earned_kurzick)
        .def_readonly("max_kurzick", &PyPlayer::max_kurzick)
        .def_readonly("current_luxon", &PyPlayer::current_luxon)
        .def_readonly("total_earned_luxon", &PyPlayer::total_earned_luxon)
        .def_readonly("max_luxon", &PyPlayer::max_luxon)
        .def_readonly("current_imperial", &PyPlayer::current_imperial)
        .def_readonly("total_earned_imperial", &PyPlayer::total_earned_imperial)
        .def_readonly("max_imperial", &PyPlayer::max_imperial)
        .def_readonly("current_balth", &PyPlayer::current_balth)
        .def_readonly("total_earned_balth", &PyPlayer::total_earned_balth)
        .def_readonly("max_balth", &PyPlayer::max_balth)
        .def_readonly("current_skill_points", &PyPlayer::current_skill_points)
        .def_readonly("total_earned_skill_points", &PyPlayer::total_earned_skill_points)
        .def_readonly("missions_completed", &PyPlayer::missions_completed)
        .def_readonly("missions_bonus", &PyPlayer::missions_bonus)
        .def_readonly("missions_completed_hm", &PyPlayer::missions_completed_hm)
        .def_readonly("missions_bonus_hm", &PyPlayer::missions_bonus_hm)
        .def_readonly("controlled_minions", &PyPlayer::controlled_minions)
        .def_readonly("unlocked_maps", &PyPlayer::unlocked_map)
        .def_readonly("learnable_character_skills", &PyPlayer::learnable_character_skills)
        .def_readonly("unlocked_character_skills", &PyPlayer::unlocked_character_skills)
        .def("SendDialog", &PyPlayer::SendDialog, py::arg("dialog_id"))
        .def("ChangeTarget", &PyPlayer::ChangeTarget, py::arg("target_id"))
        .def("InteractAgent", &PyPlayer::InteractAgent, py::arg("agent_id"), py::arg("call_target"))
        .def("CallTarget", &PyPlayer::CallTarget, py::arg("agent_id"))
        .def("IsAgentIDValid", &PyPlayer::IsAgentIDValid, py::arg("agent_id"))
        .def("GetChatHistory", &PyPlayer::GetChatHistory)
        .def("RequestChatHistory", &PyPlayer::RequestChatHistory)
        .def("IsChatHistoryReady", &PyPlayer::IsChatHistoryReady)
        .def("Istyping", &PyPlayer::Istyping)
        .def("SendChatCommand", &PyPlayer::SendChatCommand, py::arg("msg"))
        .def("SendChat", &PyPlayer::SendChat, py::arg("channel"), py::arg("msg"))
        .def("SendWhisper", &PyPlayer::SendWhisper, py::arg("name"), py::arg("msg"))
        .def("SendFakeChat", &PyPlayer::SendFakeChat, py::arg("channel"), py::arg("message"))
        .def("SendFakeChatColored", &PyPlayer::SendFakeChatColored, py::arg("channel"), py::arg("message"), py::arg("r"), py::arg("g"), py::arg("b"))
        .def("GetPlayerStatus", &PyPlayer::GetPlayerStatus)
        .def("SetPlayerStatus", &PyPlayer::SetPlayerStatus, py::arg("status"));

    // ── PlayerStatus enum (exposes GW::Constants::FriendStatus) ──
    py::enum_<GW::Constants::FriendStatus>(m, "PlayerStatus")
        .value("Offline", GW::Constants::FriendStatus::Offline)
        .value("Online", GW::Constants::FriendStatus::Online)
        .value("DND", GW::Constants::FriendStatus::DND)
        .value("Away", GW::Constants::FriendStatus::Away);

    // ── Modern free-function surface ──
    m.def("set_active_title", [](uint32_t title_id) -> bool {
        return GW::player::SetActiveTitle(static_cast<GW::Constants::TitleID>(title_id));
    }, py::arg("title_id"));
    m.def("remove_active_title", []() -> bool { return GW::player::RemoveActiveTitle(); });
    m.def("get_active_title_id", []() -> uint32_t { return static_cast<uint32_t>(GW::player::GetActiveTitleId()); });
    m.def("deposit_faction", [](uint32_t allegiance) -> bool { return GW::player::DepositFaction(allegiance); }, py::arg("allegiance"));
    m.def("get_player_agent_id", [](uint32_t pid) -> uint32_t { return GW::player::GetPlayerAgentId(pid); }, py::arg("player_id"));
    m.def("get_amount_of_players_in_instance", []() -> uint32_t { return GW::player::GetAmountOfPlayersInInstance(); });
    m.def("get_player_number", []() -> uint32_t { return GW::player::GetPlayerNumber(); });
    m.def("get_player_name", [](uint32_t pid) -> std::string { return WideToStr(GW::player::GetPlayerName(pid)); }, py::arg("player_id") = 0);
    m.def("change_second_profession", [](uint32_t prof, uint32_t hero_idx) -> bool {
        return GW::player::ChangeSecondProfession(static_cast<GW::Constants::Profession>(prof), hero_idx);
    }, py::arg("profession"), py::arg("hero_index") = 0);
    m.def("get_title_ids", []() -> py::list {
        auto ids = GW::player::GetTitleIDs();
        py::list result;
        for (auto id : ids) result.append(id);
        return result;
    });
}

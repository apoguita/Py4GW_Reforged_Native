#include "base/py_bindings.h"

#include "GW/common/game_pos.h"
#include "GW/common/constants/constants.h"
#include "GW/party/party.h"
#include "GW/context/context.h"
#include "GW/context/game.h"
#include "GW/context/world.h"
#include "GW/context/party.h"
#include "GW/agent/agent.h"
#include "GW/skillbar/skillbar.h"
#include "GW/game_thread/game_thread.h"
#include "GW/ui/ui.h"

#include <string>
#include <vector>

namespace py = pybind11;

namespace {

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

// ── Hero class (parity with legacy py_party.h Hero) ──
struct Hero {
    GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
    std::string hero_name;
    int hero_profession = 0;

    Hero() = default;
    explicit Hero(int id);
    explicit Hero(const std::string& name);
    int GetID() const;
    std::string GetName() const;
    int GetProfession() const;
    bool FlagHero(uint32_t idx);
    bool operator==(const Hero& o) const { return hero_id == o.hero_id; }
    bool operator!=(const Hero& o) const { return hero_id != o.hero_id; }
};

// ── PartyMember structs ──
struct PlayerPartyMember {
    int login_number = 0;
    int called_target_id = 0;
    bool is_connected = false;
    bool is_ticked = false;
    PlayerPartyMember() = default;
    PlayerPartyMember(int login, int target, bool connected, bool ticked)
        : login_number(login), called_target_id(target), is_connected(connected), is_ticked(ticked) {}
};

struct HeroPartyMember {
    int agent_id = 0;
    int owner_player_id = 0;
    int hero_id = 0;
    int level = 0;
    int primary = 0;
    int secondary = 0;
    HeroPartyMember() = default;
    HeroPartyMember(int agent, int owner, int hid, int lvl)
        : agent_id(agent), owner_player_id(owner), hero_id(hid), level(lvl) {}
};

struct HenchmanPartyMember {
    int agent_id = 0;
    int profession = 0;
    int level = 0;
    HenchmanPartyMember() = default;
    HenchmanPartyMember(int agent, int prof, int lvl)
        : agent_id(agent), profession(prof), level(lvl) {}
};

// ── PartyTick (parity with legacy) ──
struct PartyTick {
    bool ticked = false;
    PartyTick() = default;
    PartyTick(bool t) : ticked(t) {}  // non-explicit: parity with legacy (allows `tick = false`)
    bool IsTicked() const { return ticked; }
    void SetTicked(bool t) { ticked = t; }
    void ToggleTicked() { ticked = !ticked; }
    void SetTickToggle(bool enable) { GW::party::set_tick_toggle(enable); }
};

// ── PetInfo (parity with legacy) ──
struct PetInfo {
    uint32_t agent_id = 0;
    uint32_t owner_agent_id = 0;
    std::string pet_name;
    uint32_t model_file_id1 = 0;
    uint32_t model_file_id2 = 0;
    uint32_t behavior = 0;
    uint32_t locked_target_id = 0;
    PetInfo() = default;
    explicit PetInfo(uint32_t owner);
};

// ── PyParty class (parity with legacy py_party.h PyParty) ──
struct PyParty {
    // Public data members (read/write)
    int party_id = 0;
    std::vector<PlayerPartyMember> players;
    std::vector<HeroPartyMember> heroes;
    std::vector<HenchmanPartyMember> henchmen;
    std::vector<uint32_t> others;
    bool is_in_hard_mode = false;
    bool is_hard_mode_unlocked = false;
    int party_size = 0;
    int party_player_count = 0;
    int party_hero_count = 0;
    int party_henchman_count = 0;
    bool is_party_defeated = false;
    bool is_party_loaded = false;
    bool is_party_leader = false;
    PartyTick tick = false;  // parity with legacy py_party.h: PartyTick object, not bool

    PyParty() { GetContext(); }
    void ResetContext();
    void GetContext();

    // Operations
    bool ReturnToOutpost();
    bool SetHardMode(bool flag);
    bool RespondToPartyRequest(int party_id, bool accept);
    bool AddHero(int hero_id);
    bool KickHero(int hero_id);
    bool KickAllHeroes();
    bool AddHenchman(int henchman_id);
    bool KickHenchman(int henchman_id);
    bool KickPlayer(int player_id);
    bool InvitePlayer(int player_id);
    bool LeaveParty();
    bool FlagHero(int agent_id, float x, float y);
    bool FlagAllHeroes(float x, float y);
    bool UnflagHero(int agent_id);
    bool UnflagAllHeroes();
    bool IsHeroFlagged(int hero);
    bool IsAllFlagged();
    float GetAllFlagX();
    float GetAllFlagY();
    int GetHeroAgentID(int hero_index);
    int GetAgentHeroID(int agent_id);
    int GetAgentIDByLoginNumber(int login_number);
    std::string GetPlayerNameByLoginNumber(int login_number);
    bool SearchParty(uint32_t search_type, const std::string& advertisement);
    bool SearchPartyCancel();
    bool SearchPartyReply(bool accept);
    void SetHeroBehavior(int agent_id, int behaviour);
    void SetPetBehaviour(int behaviour, int lock_target_id);
    PetInfo GetPetInfo(uint32_t owner_agent_id);
    bool GetIsPlayerTicked(int player_id);
    void UseHeroSkill(uint32_t hero_id, uint32_t skill_slot, uint32_t target_id);
    bool SetHeroSkillAIEnabled(uint32_t hero_agent_id, uint32_t skill_slot, bool enabled);
    uintptr_t GetPartyContextPtr();
};

// ═══════════════════════════════════════════════════════════════════
//  Hero implementation (maps legacy hero_data lookup table)
// ═══════════════════════════════════════════════════════════════════

// Legacy hero name→ID lookup (matches py_party.h hero_data table)
static const std::pair<const wchar_t*, GW::Constants::HeroID> kHeroNameMap[] = {
    {L"", GW::Constants::HeroID::NoHero},
    {L"Norgu", GW::Constants::HeroID::Norgu},
    {L"Goren", GW::Constants::HeroID::Goren},
    {L"Tahlkora", GW::Constants::HeroID::Tahlkora},
    {L"Master Of Whispers", GW::Constants::HeroID::MasterOfWhispers},
    {L"Acolyte Jin", GW::Constants::HeroID::AcolyteJin},
    {L"Koss", GW::Constants::HeroID::Koss},
    {L"Dunkoro", GW::Constants::HeroID::Dunkoro},
    {L"Acolyte Sousuke", GW::Constants::HeroID::AcolyteSousuke},
    {L"Melonni", GW::Constants::HeroID::Melonni},
    {L"Zhed Shadowhoof", GW::Constants::HeroID::ZhedShadowhoof},
    {L"General Morgahn", GW::Constants::HeroID::GeneralMorgahn},
    {L"Magrid The Sly", GW::Constants::HeroID::MargridTheSly},
    {L"Zenmai", GW::Constants::HeroID::Zenmai},
    {L"Olias", GW::Constants::HeroID::Olias},
    {L"Razah", GW::Constants::HeroID::Razah},
    {L"M.O.X.", GW::Constants::HeroID::MOX},
    {L"Keiran Thackeray", GW::Constants::HeroID::KeiranThackeray},
    {L"Jora", GW::Constants::HeroID::Jora},
    {L"Pyre Fierceshot", GW::Constants::HeroID::PyreFierceshot},
    {L"Anton", GW::Constants::HeroID::Anton},
    {L"Livia", GW::Constants::HeroID::Livia},
    {L"Hayda", GW::Constants::HeroID::Hayda},
    {L"Kahmu", GW::Constants::HeroID::Kahmu},
    {L"Gwen", GW::Constants::HeroID::Gwen},
    {L"Xandra", GW::Constants::HeroID::Xandra},
    {L"Vekk", GW::Constants::HeroID::Vekk},
    {L"Ogden Stonehealer", GW::Constants::HeroID::Ogden},
    {L"Mercenary Hero 1", GW::Constants::HeroID::Merc1},
    {L"Mercenary Hero 2", GW::Constants::HeroID::Merc2},
    {L"Mercenary Hero 3", GW::Constants::HeroID::Merc3},
    {L"Mercenary Hero 4", GW::Constants::HeroID::Merc4},
    {L"Mercenary Hero 5", GW::Constants::HeroID::Merc5},
    {L"Mercenary Hero 6", GW::Constants::HeroID::Merc6},
    {L"Mercenary Hero 7", GW::Constants::HeroID::Merc7},
    {L"Mercenary Hero 8", GW::Constants::HeroID::Merc8},
    {L"Miku", GW::Constants::HeroID::Miku},
    {L"Zei Ri", GW::Constants::HeroID::ZeiRi},
};

Hero::Hero(int id) {
    if (id >= 0 && id <= static_cast<int>(GW::Constants::HeroID::ZeiRi)) {
        hero_id = static_cast<GW::Constants::HeroID>(id);
    }
}

Hero::Hero(const std::string& name) {
    std::wstring wname(name.begin(), name.end());
    for (const auto& entry : kHeroNameMap) {
        if (wname == entry.first) {
            hero_id = entry.second;
            return;
        }
    }
    hero_id = GW::Constants::HeroID::NoHero;
}

int Hero::GetID() const { return static_cast<int>(hero_id); }
std::string Hero::GetName() const { return hero_name; }
int Hero::GetProfession() const { return hero_profession; }

bool Hero::FlagHero(uint32_t idx) {
    // Use the GW::party module for flagging
    GW::ui::ControlAction key;
    switch (idx) {
    case 1: key = GW::ui::ControlAction_CommandHero1; break;
    case 2: key = GW::ui::ControlAction_CommandHero2; break;
    case 3: key = GW::ui::ControlAction_CommandHero3; break;
    case 4: key = GW::ui::ControlAction_CommandHero4; break;
    case 5: key = GW::ui::ControlAction_CommandHero5; break;
    case 6: key = GW::ui::ControlAction_CommandHero6; break;
    case 7: key = GW::ui::ControlAction_CommandHero7; break;
    default: return false;
    }
    GW::ui::Keypress(key);
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  PetInfo implementation
// ═══════════════════════════════════════════════════════════════════

PetInfo::PetInfo(uint32_t owner) : owner_agent_id(owner) {
    // Minimal: just store owner. Legacy filled from GW context.
}

// ═══════════════════════════════════════════════════════════════════
//  PyParty implementation (delegates to GW::party:: free functions)
// ═══════════════════════════════════════════════════════════════════

void PyParty::ResetContext() {
    party_id = 0;
    players.clear();
    heroes.clear();
    henchmen.clear();
    others.clear();
    is_in_hard_mode = false;
    is_hard_mode_unlocked = false;
    party_size = 0;
    party_player_count = 0;
    party_hero_count = 0;
    party_henchman_count = 0;
    is_party_defeated = false;
    is_party_loaded = false;
    is_party_leader = false;
    tick = false;
}

void PyParty::GetContext() {
    ResetContext();
    is_party_loaded = GW::party::get_is_party_loaded();
    if (!is_party_loaded) return;

    auto* info = GW::party::get_party_info();
    if (!info) return;

    tick = GW::party::get_is_party_ticked();
    is_in_hard_mode = GW::party::get_is_party_in_hard_mode();
    is_hard_mode_unlocked = GW::party::get_is_hard_mode_unlocked();
    party_size = static_cast<int>(GW::party::get_party_size());
    party_player_count = static_cast<int>(GW::party::get_party_player_count());
    party_hero_count = static_cast<int>(GW::party::get_party_hero_count());
    party_henchman_count = static_cast<int>(GW::party::get_party_henchman_count());
    is_party_defeated = GW::party::get_is_party_defeated();
    is_party_leader = GW::party::get_is_leader();
    party_id = static_cast<int>(info->party_id);

    // Populate players from the game's party context
    if (info->players.valid()) {
        for (const auto& p : info->players) {
            players.push_back(PlayerPartyMember(
                static_cast<int>(p.login_number),
                static_cast<int>(p.calledTargetId),
                p.connected(),
                p.ticked()
            ));
        }
    }

    // Populate heroes
    if (info->heroes.valid()) {
        for (const auto& h : info->heroes) {
            heroes.push_back(HeroPartyMember(
                static_cast<int>(h.agent_id),
                static_cast<int>(h.owner_player_id),
                static_cast<int>(h.hero_id),
                static_cast<int>(h.level)
            ));
        }
    }

    // Populate henchmen
    if (info->henchmen.valid()) {
        for (const auto& h : info->henchmen) {
            henchmen.push_back(HenchmanPartyMember(
                static_cast<int>(h.agent_id),
                static_cast<int>(h.profession),
                static_cast<int>(h.level)
            ));
        }
    }
}

bool PyParty::ReturnToOutpost()           { return GW::party::return_to_outpost(); }
bool PyParty::SetHardMode(bool flag)      { return GW::party::set_hard_mode(flag); }
bool PyParty::RespondToPartyRequest(int party_id, bool accept) { return GW::party::respond_to_party_request(party_id, accept); }
bool PyParty::AddHero(int hero_id)        { return GW::party::add_hero(hero_id); }
bool PyParty::KickHero(int hero_id)       { return GW::party::kick_hero(hero_id); }
bool PyParty::KickAllHeroes()             { return GW::party::kick_all_heroes(); }
bool PyParty::AddHenchman(int id)         { return GW::party::add_henchman(id); }
bool PyParty::KickHenchman(int id)        { return GW::party::kick_henchman(id); }
bool PyParty::KickPlayer(int id)          { return GW::party::kick_player(id); }
bool PyParty::InvitePlayer(int id)        { return GW::party::invite_player(id); }
bool PyParty::LeaveParty()                { return GW::party::leave_party(); }
bool PyParty::FlagHero(int agent_id, float x, float y) {
    return GW::party::flag_hero_agent(static_cast<GW::party::AgentID>(agent_id), GW::GamePos(x, y));
}
bool PyParty::FlagAllHeroes(float x, float y) { return GW::party::flag_all(GW::GamePos(x, y)); }
bool PyParty::UnflagHero(int agent_id)    { return GW::party::unflag_hero(agent_id); }
bool PyParty::UnflagAllHeroes()           { return GW::party::unflag_all(); }

bool PyParty::IsHeroFlagged(int hero) {
    // Legacy used world->hero_flags. Reforged context provides this.
    auto* game = GW::Context::GetGameContext();
    if (!game || !game->world) return false;
    if (hero == 0) {
        const float x = game->world->all_flag.x;
        const float y = game->world->all_flag.y;
        return (x != 0.0f || y != 0.0f);
    }
    return false; // hero_flags access via context not directly available
}

bool PyParty::IsAllFlagged() {
    auto* game = GW::Context::GetGameContext();
    if (!game || !game->world) return false;
    const float x = game->world->all_flag.x;
    const float y = game->world->all_flag.y;
    return (x != 0.0f || y != 0.0f);
}

float PyParty::GetAllFlagX() {
    auto* game = GW::Context::GetGameContext();
    return (game && game->world) ? game->world->all_flag.x : 0.0f;
}

float PyParty::GetAllFlagY() {
    auto* game = GW::Context::GetGameContext();
    return (game && game->world) ? game->world->all_flag.y : 0.0f;
}

int PyParty::GetHeroAgentID(int hero_index) { return static_cast<int>(GW::party::get_hero_agent_id(hero_index)); }
int PyParty::GetAgentHeroID(int agent_id) {
    return static_cast<int>(GW::party::get_agent_hero_id(static_cast<GW::party::AgentID>(agent_id)));
}
int PyParty::GetAgentIDByLoginNumber(int login_number) {
    return static_cast<int>(GW::agent::GetAgentIdByLoginNumber(static_cast<uint32_t>(login_number)));
}
std::string PyParty::GetPlayerNameByLoginNumber(int login_number) {
    wchar_t* name = GW::agent::GetPlayerNameByLoginNumber(static_cast<uint32_t>(login_number));
    if (!name) return {};
    std::string result;
    for (; *name; ++name) result.push_back(static_cast<char>(*name < 128 ? *name : '?'));
    return result;
}
bool PyParty::SearchParty(uint32_t type, const std::string& ad) {
    auto wad = StrToWide(ad);
    return GW::party::search_party(type, ad.empty() ? nullptr : wad.c_str());
}
bool PyParty::SearchPartyCancel() { return GW::party::search_party_cancel(); }
bool PyParty::SearchPartyReply(bool accept) { return GW::party::search_party_reply(accept); }
void PyParty::SetHeroBehavior(int agent_id, int behaviour) {
    GW::party::set_hero_behavior(agent_id, static_cast<GW::Constants::HeroBehavior>(behaviour));
}
void PyParty::SetPetBehaviour(int behaviour, int lock_target_id) {
    GW::party::set_pet_behavior(static_cast<GW::Constants::HeroBehavior>(behaviour), lock_target_id);
}
PetInfo PyParty::GetPetInfo(uint32_t owner) { return PetInfo(owner); }
bool PyParty::GetIsPlayerTicked(int player_id) { return GW::party::get_is_player_ticked(player_id); }
void PyParty::UseHeroSkill(uint32_t hero_id, uint32_t skill_slot, uint32_t target_id) {
    const uint32_t skill_idx = skill_slot - 1;
    const uint32_t hero_zero = hero_id - 1;
    GW::ui::ControlAction hero_action;
    switch (hero_zero) {
    case 0: hero_action = GW::ui::ControlAction_Hero1Skill1; break;
    case 1: hero_action = GW::ui::ControlAction_Hero2Skill1; break;
    case 2: hero_action = GW::ui::ControlAction_Hero3Skill1; break;
    case 3: hero_action = GW::ui::ControlAction_Hero4Skill1; break;
    case 4: hero_action = GW::ui::ControlAction_Hero5Skill1; break;
    case 5: hero_action = GW::ui::ControlAction_Hero6Skill1; break;
    case 6: hero_action = GW::ui::ControlAction_Hero7Skill1; break;
    default: return;
    }
    const uint32_t curr_target = GW::agent::GetTargetId();
    GW::game_thread::Enqueue([target_id, skill_idx, hero_action, curr_target]() {
        if (target_id && target_id != GW::agent::GetTargetId())
            GW::agent::ChangeTarget(static_cast<GW::agent::AgentID>(target_id));
        GW::ui::Keypress(static_cast<GW::ui::ControlAction>(
            static_cast<uint32_t>(hero_action) + skill_idx));
        if (curr_target && target_id != curr_target)
            GW::agent::ChangeTarget(static_cast<GW::agent::AgentID>(curr_target));
    });
}
bool PyParty::SetHeroSkillAIEnabled(uint32_t agent_id, uint32_t slot, bool enabled) {
    return GW::party::set_hero_skill_ai_enabled(agent_id, slot, enabled);
}
uintptr_t PyParty::GetPartyContextPtr() { return reinterpret_cast<uintptr_t>(GW::Context::GetPartyContext()); }

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyParty, m) {
    m.doc() = "Py4GW Party bindings";

    // ── HeroID enum (from GW::Constants) ──
    py::enum_<GW::Constants::HeroID>(m, "HeroType")
        .value("None", GW::Constants::HeroID::NoHero)
        .value("Norgu", GW::Constants::HeroID::Norgu).value("Goren", GW::Constants::HeroID::Goren)
        .value("Tahlkora", GW::Constants::HeroID::Tahlkora).value("MasterOfWhispers", GW::Constants::HeroID::MasterOfWhispers)
        .value("AcolyteJin", GW::Constants::HeroID::AcolyteJin).value("Koss", GW::Constants::HeroID::Koss)
        .value("Dunkoro", GW::Constants::HeroID::Dunkoro).value("AcolyteSousuke", GW::Constants::HeroID::AcolyteSousuke)
        .value("Melonni", GW::Constants::HeroID::Melonni).value("ZhedShadowhoof", GW::Constants::HeroID::ZhedShadowhoof)
        .value("GeneralMorgahn", GW::Constants::HeroID::GeneralMorgahn).value("MargridTheSly", GW::Constants::HeroID::MargridTheSly)
        .value("Zenmai", GW::Constants::HeroID::Zenmai).value("Olias", GW::Constants::HeroID::Olias)
        .value("Razah", GW::Constants::HeroID::Razah).value("MOX", GW::Constants::HeroID::MOX)
        .value("KeiranThackeray", GW::Constants::HeroID::KeiranThackeray).value("Jora", GW::Constants::HeroID::Jora)
        .value("PyreFierceshot", GW::Constants::HeroID::PyreFierceshot).value("Anton", GW::Constants::HeroID::Anton)
        .value("Livia", GW::Constants::HeroID::Livia).value("Hayda", GW::Constants::HeroID::Hayda)
        .value("Kahmu", GW::Constants::HeroID::Kahmu).value("Gwen", GW::Constants::HeroID::Gwen)
        .value("Xandra", GW::Constants::HeroID::Xandra).value("Vekk", GW::Constants::HeroID::Vekk)
        .value("Ogden", GW::Constants::HeroID::Ogden)
        .value("MercenaryHero1", GW::Constants::HeroID::Merc1).value("MercenaryHero2", GW::Constants::HeroID::Merc2)
        .value("MercenaryHero3", GW::Constants::HeroID::Merc3).value("MercenaryHero4", GW::Constants::HeroID::Merc4)
        .value("MercenaryHero5", GW::Constants::HeroID::Merc5).value("MercenaryHero6", GW::Constants::HeroID::Merc6)
        .value("MercenaryHero7", GW::Constants::HeroID::Merc7).value("MercenaryHero8", GW::Constants::HeroID::Merc8)
        .value("Miku", GW::Constants::HeroID::Miku).value("ZeiRi", GW::Constants::HeroID::ZeiRi);

    // ── Hero class ──
    py::class_<Hero>(m, "Hero")
        .def(py::init<int>())
        .def(py::init<const std::string&>())
        .def("GetID", &Hero::GetID)
        .def("GetName", &Hero::GetName)
        .def("GetProfession", &Hero::GetProfession)
        .def("FlagHero", &Hero::FlagHero, py::arg("idx"))
        .def("__eq__", &Hero::operator==)
        .def("__ne__", &Hero::operator!=)
        .def("__repr__", [](const Hero& h) {
            return "<Hero name='" + h.GetName() + "' id=" + std::to_string(h.GetID()) + ">";
        });

    // ── PartyTick class ──
    py::class_<PartyTick>(m, "PartyTick")
        .def(py::init<bool>(), py::arg("ticked") = false)
        .def("IsTicked", &PartyTick::IsTicked)
        .def("SetTicked", &PartyTick::SetTicked, py::arg("ticked"))
        .def("ToggleTicked", &PartyTick::ToggleTicked)
        .def("SetTickToggle", &PartyTick::SetTickToggle, py::arg("enable"));

    // ── PartyMember structs ──
    py::class_<PlayerPartyMember>(m, "PlayerPartyMember")
        .def(py::init<int, int, bool, bool>(),
             py::arg("login_number") = 0, py::arg("called_target_id") = 0,
             py::arg("is_connected") = false, py::arg("is_ticked") = false)
        .def_readwrite("login_number", &PlayerPartyMember::login_number)
        .def_readwrite("called_target_id", &PlayerPartyMember::called_target_id)
        .def_readwrite("is_connected", &PlayerPartyMember::is_connected)
        .def_readwrite("is_ticked", &PlayerPartyMember::is_ticked);

    py::class_<HeroPartyMember>(m, "HeroPartyMember")
        .def(py::init<int, int, int, int>(),
             py::arg("agent_id") = 0, py::arg("owner_player_id") = 0,
             py::arg("hero_id") = 0, py::arg("level") = 0)
        .def_readwrite("agent_id", &HeroPartyMember::agent_id)
        .def_readwrite("owner_player_id", &HeroPartyMember::owner_player_id)
        .def_readwrite("hero_id", &HeroPartyMember::hero_id)
        .def_readwrite("level", &HeroPartyMember::level)
        .def_readwrite("primary", &HeroPartyMember::primary)
        .def_readwrite("secondary", &HeroPartyMember::secondary);

    py::class_<HenchmanPartyMember>(m, "HenchmanPartyMember")
        .def(py::init<int, int, int>(),
             py::arg("agent_id") = 0, py::arg("profession") = 0, py::arg("level") = 0)
        .def_readwrite("agent_id", &HenchmanPartyMember::agent_id)
        .def_readwrite("profession", &HenchmanPartyMember::profession)
        .def_readwrite("level", &HenchmanPartyMember::level);

    // ── PetInfo class ──
    py::class_<PetInfo>(m, "PetInfo")
        .def(py::init<uint32_t>(), py::arg("owner_agent_id"))
        .def_readonly("agent_id", &PetInfo::agent_id)
        .def_readonly("owner_agent_id", &PetInfo::owner_agent_id)
        .def_readonly("pet_name", &PetInfo::pet_name)
        .def_readonly("model_file_id1", &PetInfo::model_file_id1)
        .def_readonly("model_file_id2", &PetInfo::model_file_id2)
        .def_readonly("behavior", &PetInfo::behavior)
        .def_readonly("locked_target_id", &PetInfo::locked_target_id);

    // ── PyParty class (legacy surface) ──
    py::class_<PyParty>(m, "PyParty")
        .def(py::init<>())
        .def("GetContext", &PyParty::GetContext)
        .def("ReturnToOutpost", &PyParty::ReturnToOutpost)
        .def("SetHardMode", &PyParty::SetHardMode, py::arg("flag"))
        .def("RespondToPartyRequest", &PyParty::RespondToPartyRequest, py::arg("party_id"), py::arg("accept"))
        .def("AddHero", &PyParty::AddHero, py::arg("hero_id"))
        .def("KickHero", &PyParty::KickHero, py::arg("hero_id"))
        .def("KickAllHeroes", &PyParty::KickAllHeroes)
        .def("AddHenchman", &PyParty::AddHenchman, py::arg("henchman_id"))
        .def("KickHenchman", &PyParty::KickHenchman, py::arg("henchman_id"))
        .def("KickPlayer", &PyParty::KickPlayer, py::arg("player_id"))
        .def("InvitePlayer", &PyParty::InvitePlayer, py::arg("player_id"))
        .def("LeaveParty", &PyParty::LeaveParty)
        .def("FlagHero", &PyParty::FlagHero, py::arg("agent_id"), py::arg("x"), py::arg("y"))
        .def("FlagAllHeroes", &PyParty::FlagAllHeroes, py::arg("x"), py::arg("y"))
        .def("UnflagHero", &PyParty::UnflagHero, py::arg("agent_id"))
        .def("UnflagAllHeroes", &PyParty::UnflagAllHeroes)
        .def("IsHeroFlagged", &PyParty::IsHeroFlagged, py::arg("hero"))
        .def("IsAllFlagged", &PyParty::IsAllFlagged)
        .def("GetAllFlagX", &PyParty::GetAllFlagX)
        .def("GetAllFlagY", &PyParty::GetAllFlagY)
        .def("GetHeroAgentID", &PyParty::GetHeroAgentID, py::arg("hero_index"))
        .def("GetAgentHeroID", &PyParty::GetAgentHeroID, py::arg("agent_id"))
        .def("GetAgentIDByLoginNumber", &PyParty::GetAgentIDByLoginNumber, py::arg("login_number"))
        .def("GetPlayerNameByLoginNumber", &PyParty::GetPlayerNameByLoginNumber, py::arg("login_number"))
        .def("SearchParty", &PyParty::SearchParty, py::arg("search_type"), py::arg("advertisement"))
        .def("SearchPartyCancel", &PyParty::SearchPartyCancel)
        .def("SearchPartyReply", &PyParty::SearchPartyReply, py::arg("accept"))
        .def("SetHeroBehavior", &PyParty::SetHeroBehavior, py::arg("agent_id"), py::arg("behavior"))
        .def("SetPetBehavior", &PyParty::SetPetBehaviour, py::arg("behaviour"), py::arg("lock_target_id"))
        .def("GetPetInfo", &PyParty::GetPetInfo, py::arg("owner_agent_id"))
        .def("GetIsPlayerTicked", &PyParty::GetIsPlayerTicked, py::arg("player_id"))
        .def("UseHeroSkill", &PyParty::UseHeroSkill, py::arg("hero_id"), py::arg("skill_slot"), py::arg("target_id"))
        .def("SetHeroSkillAIEnabled", &PyParty::SetHeroSkillAIEnabled, py::arg("hero_agent_id"), py::arg("skill_slot"), py::arg("enabled"))
        .def("GetPartyContextPtr", &PyParty::GetPartyContextPtr)
        .def_readwrite("party_id", &PyParty::party_id)
        .def_readwrite("players", &PyParty::players)
        .def_readwrite("heroes", &PyParty::heroes)
        .def_readwrite("henchmen", &PyParty::henchmen)
        .def_readwrite("others", &PyParty::others)
        .def_readwrite("is_in_hard_mode", &PyParty::is_in_hard_mode)
        .def_readwrite("is_hard_mode_unlocked", &PyParty::is_hard_mode_unlocked)
        .def_readwrite("party_size", &PyParty::party_size)
        .def_readwrite("party_player_count", &PyParty::party_player_count)
        .def_readwrite("party_hero_count", &PyParty::party_hero_count)
        .def_readwrite("party_henchman_count", &PyParty::party_henchman_count)
        .def_readwrite("is_party_defeated", &PyParty::is_party_defeated)
        .def_readwrite("is_party_loaded", &PyParty::is_party_loaded)
        .def_readwrite("is_party_leader", &PyParty::is_party_leader)
        .def_readwrite("tick", &PyParty::tick);

    // ── Modern free-function surface (convenience, kept alongside legacy class) ──
    m.def("set_tick_toggle", [](bool enable) { GW::party::set_tick_toggle(enable); }, py::arg("enable"));
    m.def("tick", [](bool flag) -> bool { return GW::party::tick(flag); }, py::arg("flag") = true);
    m.def("get_party_size", []() -> uint32_t { return GW::party::get_party_size(); });
    m.def("get_party_player_count", []() -> uint32_t { return GW::party::get_party_player_count(); });
    m.def("get_party_hero_count", []() -> uint32_t { return GW::party::get_party_hero_count(); });
    m.def("get_party_henchman_count", []() -> uint32_t { return GW::party::get_party_henchman_count(); });
    m.def("get_is_party_defeated", []() -> bool { return GW::party::get_is_party_defeated(); });
    m.def("get_is_party_in_hard_mode", []() -> bool { return GW::party::get_is_party_in_hard_mode(); });
    m.def("get_is_hard_mode_unlocked", []() -> bool { return GW::party::get_is_hard_mode_unlocked(); });
    m.def("get_is_party_ticked", []() -> bool { return GW::party::get_is_party_ticked(); });
    m.def("get_is_player_ticked", [](uint32_t idx) -> bool { return GW::party::get_is_player_ticked(idx); }, py::arg("player_index") = 0xFFFFFFFF);
    m.def("get_is_player_loaded", [](uint32_t idx) -> bool { return GW::party::get_is_player_loaded(idx); }, py::arg("player_index") = 0xFFFFFFFF);
    m.def("get_is_party_loaded", []() -> bool { return GW::party::get_is_party_loaded(); });
    m.def("get_is_leader", []() -> bool { return GW::party::get_is_leader(); });
    m.def("set_hard_mode", [](bool flag) -> bool { return GW::party::set_hard_mode(flag); }, py::arg("flag"));
    m.def("return_to_outpost", []() -> bool { return GW::party::return_to_outpost(); });
    m.def("respond_to_party_request", [](uint32_t pid, bool accept) -> bool { return GW::party::respond_to_party_request(pid, accept); }, py::arg("party_id"), py::arg("accept"));
    m.def("leave_party", []() -> bool { return GW::party::leave_party(); });
    m.def("add_hero", [](uint32_t hid) -> bool { return GW::party::add_hero(hid); }, py::arg("hero_id"));
    m.def("kick_hero", [](uint32_t hid) -> bool { return GW::party::kick_hero(hid); }, py::arg("hero_id"));
    m.def("kick_all_heroes", []() -> bool { return GW::party::kick_all_heroes(); });
    m.def("add_henchman", [](uint32_t aid) -> bool { return GW::party::add_henchman(aid); }, py::arg("agent_id"));
    m.def("kick_henchman", [](uint32_t aid) -> bool { return GW::party::kick_henchman(aid); }, py::arg("agent_id"));
    m.def("invite_player_by_id", [](uint32_t pid) -> bool { return GW::party::invite_player(pid); }, py::arg("player_id"));
    m.def("invite_player_by_name", [](const std::string& name) -> bool {
        auto wname = StrToWide(name);
        return GW::party::invite_player(wname.c_str());
    }, py::arg("player_name"));
    m.def("kick_player", [](uint32_t pid) -> bool { return GW::party::kick_player(pid); }, py::arg("player_id"));
    m.def("flag_hero", [](uint32_t idx, float x, float y) -> bool { return GW::party::flag_hero(idx, GW::GamePos(x, y)); }, py::arg("hero_index"), py::arg("x"), py::arg("y"));
    m.def("flag_hero_agent", [](uint32_t aid, float x, float y) -> bool { return GW::party::flag_hero_agent(static_cast<GW::party::AgentID>(aid), GW::GamePos(x, y)); }, py::arg("agent_id"), py::arg("x"), py::arg("y"));
    m.def("unflag_hero", [](uint32_t idx) -> bool { return GW::party::unflag_hero(idx); }, py::arg("hero_index"));
    m.def("flag_all", [](float x, float y) -> bool { return GW::party::flag_all(GW::GamePos(x, y)); }, py::arg("x"), py::arg("y"));
    m.def("unflag_all", []() -> bool { return GW::party::unflag_all(); });
    m.def("set_hero_behavior", [](uint32_t aid, uint32_t bhv) -> bool { return GW::party::set_hero_behavior(aid, static_cast<GW::Constants::HeroBehavior>(bhv)); }, py::arg("agent_id"), py::arg("behavior"));
    m.def("set_hero_skill_ai_enabled", [](uint32_t aid, uint32_t slot, bool en) -> bool { return GW::party::set_hero_skill_ai_enabled(aid, slot, en); }, py::arg("hero_agent_id"), py::arg("skill_slot"), py::arg("enabled"));
    m.def("set_pet_behavior", [](uint32_t bhv, uint32_t lock_id) -> bool { return GW::party::set_pet_behavior(static_cast<GW::Constants::HeroBehavior>(bhv), lock_id); }, py::arg("behavior"), py::arg("lock_target_id") = 0);
    m.def("get_hero_agent_id", [](uint32_t idx) -> uint32_t { return GW::party::get_hero_agent_id(idx); }, py::arg("hero_index"));
    m.def("get_agent_hero_id", [](uint32_t aid) -> uint32_t { return GW::party::get_agent_hero_id(static_cast<GW::party::AgentID>(aid)); }, py::arg("agent_id"));
    m.def("search_party", [](uint32_t type, const std::string& ad) -> bool {
        auto wad = StrToWide(ad);
        return GW::party::search_party(type, ad.empty() ? nullptr : wad.c_str());
    }, py::arg("search_type"), py::arg("advertisement") = "");
    m.def("search_party_cancel", []() -> bool { return GW::party::search_party_cancel(); });
    m.def("search_party_reply", [](bool accept) -> bool { return GW::party::search_party_reply(accept); }, py::arg("accept"));
}

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/agent/agent.h"
#include "GW/common/constants/constants.h"
#include "GW/game_thread/game_thread.h"

#include <cstring>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

std::string WideToStr(const wchar_t* wstr) {
    if (!wstr) return {};
    std::string result;
    while (*wstr) {
        wchar_t ch = *wstr++;
        result.push_back(static_cast<char>(ch < 128 ? ch : '?'));
    }
    return result;
}

// ── Profession class (parity with legacy py_agent.h Profession) ──
struct Profession {
    GW::Constants::Profession profession = GW::Constants::Profession::None;

    Profession() = default;
    explicit Profession(int prof) : profession(static_cast<GW::Constants::Profession>(prof)) {}
    explicit Profession(const std::string& prof) {
        if (prof == "Warrior") profession = GW::Constants::Profession::Warrior;
        else if (prof == "Ranger") profession = GW::Constants::Profession::Ranger;
        else if (prof == "Monk") profession = GW::Constants::Profession::Monk;
        else if (prof == "Necromancer") profession = GW::Constants::Profession::Necromancer;
        else if (prof == "Mesmer") profession = GW::Constants::Profession::Mesmer;
        else if (prof == "Elementalist") profession = GW::Constants::Profession::Elementalist;
        else if (prof == "Assassin") profession = GW::Constants::Profession::Assassin;
        else if (prof == "Ritualist") profession = GW::Constants::Profession::Ritualist;
        else if (prof == "Paragon") profession = GW::Constants::Profession::Paragon;
        else if (prof == "Dervish") profession = GW::Constants::Profession::Dervish;
        else profession = GW::Constants::Profession::None;
    }

    void Set(int prof) { profession = static_cast<GW::Constants::Profession>(prof); }
    GW::Constants::Profession Get() const { return profession; }
    int ToInt() const { return static_cast<int>(profession); }
    std::string GetName() const {
        switch (profession) {
        case GW::Constants::Profession::None: return "None";
        case GW::Constants::Profession::Warrior: return "Warrior";
        case GW::Constants::Profession::Ranger: return "Ranger";
        case GW::Constants::Profession::Monk: return "Monk";
        case GW::Constants::Profession::Necromancer: return "Necromancer";
        case GW::Constants::Profession::Mesmer: return "Mesmer";
        case GW::Constants::Profession::Elementalist: return "Elementalist";
        case GW::Constants::Profession::Assassin: return "Assassin";
        case GW::Constants::Profession::Ritualist: return "Ritualist";
        case GW::Constants::Profession::Paragon: return "Paragon";
        case GW::Constants::Profession::Dervish: return "Dervish";
        }
        return "Unknown";
    }
    bool operator==(const Profession& o) const { return profession == o.profession; }
    bool operator!=(const Profession& o) const { return profession != o.profession; }
};

// ── PyAgent class (parity with legacy py_agent.h PyAgent) ──
struct PyAgent {
    uint32_t agent_id = 0;

    PyAgent() = default;
    explicit PyAgent(uint32_t id) : agent_id(id) {}

    void GetContext() { /* data refreshed per-field */ }

    uint32_t GetAgentID() const { return agent_id; }
    uint32_t GetPlayerNumber() const { return GW::agent::GetAgentIdByLoginNumber(agent_id); }
    std::string GetName() const { return WideToStr(GW::agent::GetAgentEncName(agent_id)); }
    uint32_t GetPrimary() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        auto* l = a ? a->GetAsAgentLiving() : nullptr;
        return l ? static_cast<uint32_t>(l->primary) : 0;
    }
    uint32_t GetSecondary() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        auto* l = a ? a->GetAsAgentLiving() : nullptr;
        return l ? static_cast<uint32_t>(l->secondary) : 0;
    }
    uint32_t GetLevel() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        auto* l = a ? a->GetAsAgentLiving() : nullptr;
        return l ? static_cast<uint32_t>(l->level) : 0;
    }
    float GetHP() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        auto* l = a ? a->GetAsAgentLiving() : nullptr;
        return l ? l->hp : 0.0f;
    }
    float GetRotation() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        return a ? a->rotation_angle : 0.0f;
    }
    py::tuple GetPos() const {
        auto* a = GW::agent::GetAgentByID(agent_id);
        if (!a) return py::make_tuple(0.0f, 0.0f, 0.0f);
        return py::make_tuple(a->pos.x, a->pos.y, a->z);
    }
    bool GetIsLiving() const { auto* a = GW::agent::GetAgentByID(agent_id); return a && a->GetAsAgentLiving(); }
    bool GetIsDead() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l && l->GetIsDead(); }
    bool GetIsMoving() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l && l->GetIsMoving(); }
    bool GetIsAttacking() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l && l->GetIsAttacking(); }
    bool GetIsKnockedDown() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l && l->GetIsKnockedDown(); }
    bool GetIsCasting() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l && l->GetIsCasting(); }
    uint32_t GetAllegiance() const { auto* a = GW::agent::GetAgentByID(agent_id); auto* l = a ? a->GetAsAgentLiving() : nullptr; return l ? static_cast<uint32_t>(l->allegiance) : 0; }
    bool GetIsGadget() const { auto* a = GW::agent::GetAgentByID(agent_id); return a && a->GetIsGadgetType(); }
    bool GetIsItem() const { auto* a = GW::agent::GetAgentByID(agent_id); return a && a->GetIsItemType(); }

    static uint32_t GetTargetId() { return GW::agent::GetTargetId(); }
    static uint32_t GetControlledCharacterId() { return GW::agent::GetControlledCharacterId(); }
    static uint32_t GetObservingId() { return GW::agent::GetObservingId(); }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyAgent, m) {
    m.doc() = "Py4GW Agent bindings";

    // ── ProfessionType enum (exposes GW::Constants::Profession) ──
    py::enum_<GW::Constants::Profession>(m, "ProfessionType")
        .value("None", GW::Constants::Profession::None).value("Warrior", GW::Constants::Profession::Warrior)
        .value("Ranger", GW::Constants::Profession::Ranger).value("Monk", GW::Constants::Profession::Monk)
        .value("Necromancer", GW::Constants::Profession::Necromancer).value("Mesmer", GW::Constants::Profession::Mesmer)
        .value("Elementalist", GW::Constants::Profession::Elementalist).value("Assassin", GW::Constants::Profession::Assassin)
        .value("Ritualist", GW::Constants::Profession::Ritualist).value("Paragon", GW::Constants::Profession::Paragon)
        .value("Dervish", GW::Constants::Profession::Dervish);

    // ── Profession class ──
    py::class_<Profession>(m, "Profession")
        .def(py::init<>())
        .def(py::init<int>())
        .def(py::init<const std::string&>())
        .def("GetName", &Profession::GetName)
        .def("ToInt", &Profession::ToInt)
        .def("Set", &Profession::Set, py::arg("prof"))
        .def("Get", &Profession::Get)
        .def("__eq__", &Profession::operator==)
        .def("__ne__", &Profession::operator!=);

    // ── PyAgent class (legacy surface) ──
    py::class_<PyAgent>(m, "PyAgent")
        .def(py::init<>())
        .def(py::init<uint32_t>(), py::arg("agent_id"))
        .def("GetContext", &PyAgent::GetContext)
        .def("GetAgentID", &PyAgent::GetAgentID)
        .def("GetPlayerNumber", &PyAgent::GetPlayerNumber)
        .def("GetName", &PyAgent::GetName)
        .def("GetPrimary", &PyAgent::GetPrimary)
        .def("GetSecondary", &PyAgent::GetSecondary)
        .def("GetLevel", &PyAgent::GetLevel)
        .def("GetHP", &PyAgent::GetHP)
        .def("GetRotation", &PyAgent::GetRotation)
        .def("GetPos", &PyAgent::GetPos)
        .def("GetIsLiving", &PyAgent::GetIsLiving)
        .def("GetIsDead", &PyAgent::GetIsDead)
        .def("GetIsMoving", &PyAgent::GetIsMoving)
        .def("GetIsAttacking", &PyAgent::GetIsAttacking)
        .def("GetIsKnockedDown", &PyAgent::GetIsKnockedDown)
        .def("GetIsCasting", &PyAgent::GetIsCasting)
        .def("GetAllegiance", &PyAgent::GetAllegiance)
        .def("GetIsGadget", &PyAgent::GetIsGadget)
        .def("GetIsItem", &PyAgent::GetIsItem)
        .def_static("GetTargetId", &PyAgent::GetTargetId)
        .def_static("GetControlledCharacterId", &PyAgent::GetControlledCharacterId)
        .def_static("GetObservingId", &PyAgent::GetObservingId);

    // ── Free-function surface (modern) ──
    m.def("send_dialog", [](uint32_t id) { return GW::agent::SendDialog(id); }, py::arg("dialog_id"));
    m.def("get_observing_id", []() -> uint32_t { return GW::agent::GetObservingId(); });
    m.def("get_controlled_character_id", []() -> uint32_t { return GW::agent::GetControlledCharacterId(); });
    m.def("get_target_id", []() -> uint32_t { return GW::agent::GetTargetId(); });
    m.def("get_amount_of_players_in_instance", []() -> uint32_t { return GW::agent::GetAmountOfPlayersInInstance(); });
    m.def("is_observing", []() -> bool { return GW::agent::IsObserving(); });
    m.def("change_target", [](uint32_t id) { return GW::agent::ChangeTarget(static_cast<GW::agent::AgentID>(id)); }, py::arg("agent_id"));
    m.def("move", [](float x, float y, uint32_t z) { return GW::agent::Move(x, y, z); }, py::arg("x"), py::arg("y"), py::arg("zplane") = 0);
    m.def("interact_agent", [](uint32_t id, bool call) { auto* a = GW::agent::GetAgentByID(id); return a ? GW::agent::InteractAgent(a, call) : false; }, py::arg("agent_id"), py::arg("call_target") = false);
    m.def("call_target", [](uint32_t id) { return GW::agent::CallTarget(id); }, py::arg("agent_id"));
    m.def("get_player_name_by_login_number", [](uint32_t n) -> std::string { return WideToStr(GW::agent::GetPlayerNameByLoginNumber(n)); }, py::arg("login_number"));
    m.def("get_agent_id_by_login_number", [](uint32_t n) -> uint32_t { return GW::agent::GetAgentIdByLoginNumber(n); }, py::arg("login_number"));
    m.def("get_hero_agent_id", [](uint32_t h) -> uint32_t { return GW::agent::GetHeroAgentID(h); }, py::arg("hero_index"));
    m.def("get_agent_enc_name", [](uint32_t id) -> std::vector<uint8_t> {
        const wchar_t* enc = GW::agent::GetAgentEncName(id);
        if (!enc) return {};
        size_t n = 0; while (enc[n] != 0) ++n;
        const size_t bc = (n + 1) * sizeof(wchar_t);
        std::vector<uint8_t> out(bc);
        std::memcpy(out.data(), enc, bc);
        return out;
    }, py::arg("agent_id"));
    m.def("get_agent_is_targettable", [](uint32_t id) -> bool {
        auto* a = GW::agent::GetAgentByID(id); return a ? GW::agent::GetIsAgentTargettable(a) : false;
    }, py::arg("agent_id"));
}

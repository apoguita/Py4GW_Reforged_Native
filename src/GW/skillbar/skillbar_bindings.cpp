#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/skillbar/skillbar.h"
#include "GW/context/skill.h"

#include "GW/agent/agent.h"
#include "GW/game_thread/game_thread.h"
#include "GW/ui/ui.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

// Ported from the legacy PySkillbar.Skillbar class (py_skillbar.h/.cpp). Holds a
// snapshot of the player's GW::Context::Skillbar taken at construction / GetContext,
// mirroring the legacy object API that existing widgets drive. Data members read the
// snapshot; action methods delegate to GW::skillbar (bulk data is also reachable via
// the ctypes skillbar context, but this class preserves the legacy object surface).
struct PySkillbarObject {
    GW::Context::Skillbar data{};

    PySkillbarObject() { GetContext(); }

    void GetContext() {
        data = GW::Context::Skillbar{};
        if (const GW::Context::Skillbar* sb = GW::skillbar::GetPlayerSkillbar()) {
            data = *sb;
        }
    }

    static void ValidateSlot(uint32_t slot) {
        if (slot < 1 || slot > 8) {
            throw std::out_of_range(
                "Skill slot out of range (must be between 1 and 8) Given: " + std::to_string(slot));
        }
    }

    GW::Context::SkillbarSkill GetSkill(uint32_t slot) const {
        ValidateSlot(slot);
        return data.skills[slot - 1];
    }

    py::list GetSkills() const {
        py::list out;
        for (const auto& s : data.skills) out.append(s);
        return out;
    }

    bool LoadSkillTemplate(const std::string& tpl) const {
        return GW::skillbar::LoadSkillTemplate(tpl.c_str());
    }

    bool LoadHeroSkillTemplate(uint32_t hero_index, const std::string& tpl) const {
        return GW::skillbar::LoadSkillTemplate(tpl.c_str(), hero_index);
    }

    bool UseSkill(uint32_t slot, uint32_t target) const {
        ValidateSlot(slot);
        return GW::skillbar::UseSkill(slot - 1, target);
    }

    bool UseSkillTargetless(uint32_t slot) const {
        ValidateSlot(slot);
        return GW::skillbar::PointBlankUseSkill(slot - 1);
    }

    bool HeroUseSkill(uint32_t target_agent_id, uint32_t skill_number, uint32_t hero_index) const {
        const uint32_t skill_idx = skill_number - 1;
        const uint32_t hero_zero = hero_index - 1;
        GW::ui::ControlAction hero_action;
        switch (hero_zero) {
        case 0: hero_action = GW::ui::ControlAction_Hero1Skill1; break;
        case 1: hero_action = GW::ui::ControlAction_Hero2Skill1; break;
        case 2: hero_action = GW::ui::ControlAction_Hero3Skill1; break;
        case 3: hero_action = GW::ui::ControlAction_Hero4Skill1; break;
        case 4: hero_action = GW::ui::ControlAction_Hero5Skill1; break;
        case 5: hero_action = GW::ui::ControlAction_Hero6Skill1; break;
        case 6: hero_action = GW::ui::ControlAction_Hero7Skill1; break;
        default: return false;
        }
        const uint32_t curr_target_id = GW::agent::GetTargetId();
        GW::game_thread::Enqueue([target_agent_id, skill_idx, hero_action, curr_target_id]() {
            if (target_agent_id && target_agent_id != GW::agent::GetTargetId()) {
                GW::agent::ChangeTarget(target_agent_id);
            }
            const auto keypress_id =
                static_cast<GW::ui::ControlAction>(static_cast<uint32_t>(hero_action) + skill_idx);
            GW::ui::Keypress(keypress_id);
            if (curr_target_id && target_agent_id != curr_target_id) {
                GW::agent::ChangeTarget(curr_target_id);
            }
        });
        return true;
    }

    bool ChangeHeroSecondary(uint32_t hero_index, uint32_t profession) const {
        return GW::skillbar::ChangeSecondProfession(
            static_cast<GW::Constants::Profession>(profession), hero_index);
    }

    py::list GetHeroSkillbar(uint32_t hero_index) const {
        py::list out;
        if (const GW::Context::Skillbar* sb = GW::skillbar::GetHeroSkillbar(hero_index)) {
            for (const auto& s : sb->skills) out.append(s);
        }
        return out;
    }

    int GetHoveredSkill() const {
        const GW::Context::Skill* skill = GW::skillbar::GetHoveredSkill();
        return skill ? static_cast<int>(skill->skill_id) : 0;
    }

    bool IsSkillUnlocked(int skill_id) const {
        return GW::skillbar::GetIsSkillUnlocked(static_cast<GW::Constants::SkillID>(skill_id));
    }

    bool IsSkillLearnt(int skill_id) const {
        return GW::skillbar::GetIsSkillLearnt(static_cast<GW::Constants::SkillID>(skill_id));
    }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PySkillbar, m) {
    m.doc() = "Py4GW Skillbar bindings";

    // Ported from the legacy PySkillbar.SkillbarSkill class (py_skillbar.h/.cpp): a single
    // skillbar slot. Same member surface as legacy (id is a SkillID, adrenaline_a/b,
    // recharge, event, get_recharge) over the reforged GW::Context::SkillbarSkill. `id`
    // returns a PySkill.SkillID so legacy `.id.id` / `.id.GetName()` keep working.
    py::class_<GW::Context::SkillbarSkill>(m, "SkillbarSkill")
        .def_property_readonly("id", [](const GW::Context::SkillbarSkill& s) {
            return py::module_::import("PySkill").attr("SkillID")(static_cast<int>(s.skill_id));
        })
        .def_readonly("adrenaline_a", &GW::Context::SkillbarSkill::adrenaline_a)
        .def_readonly("adrenaline_b", &GW::Context::SkillbarSkill::adrenaline_b)
        .def_readonly("recharge", &GW::Context::SkillbarSkill::recharge)
        .def_readonly("event", &GW::Context::SkillbarSkill::event)
        .def_property_readonly("get_recharge", [](const GW::Context::SkillbarSkill& s) {
            return s.GetRecharge();
        });

    // Ported from the legacy PySkillbar.Skillbar class. Snapshot of the player's
    // skillbar on construction; GetContext() refreshes it. Data members read the
    // snapshot; actions delegate to GW::skillbar.
    py::class_<PySkillbarObject>(m, "Skillbar")
        .def(py::init<>())
        .def_property_readonly("agent_id", [](const PySkillbarObject& s) { return s.data.agent_id; })
        .def_property_readonly("disabled", [](const PySkillbarObject& s) { return s.data.disabled; })
        .def_property_readonly("casting", [](const PySkillbarObject& s) { return s.data.casting; })
        .def_property_readonly("skills", &PySkillbarObject::GetSkills)
        .def("GetContext", &PySkillbarObject::GetContext)
        .def("GetSkill", &PySkillbarObject::GetSkill, py::arg("slot"),
             "Get the skill from the skillbar by slot (1-8)")
        .def("LoadSkillTemplate", &PySkillbarObject::LoadSkillTemplate, py::arg("skill_template"),
             "Load a skill template into the player's skillbar")
        .def("LoadHeroSkillTemplate", &PySkillbarObject::LoadHeroSkillTemplate,
             py::arg("hero_index"), py::arg("skill_template"),
             "Load a skill template into a hero's skillbar")
        .def("UseSkill", &PySkillbarObject::UseSkill, py::arg("slot"), py::arg("target") = 0)
        .def("UseSkillTargetless", &PySkillbarObject::UseSkillTargetless, py::arg("slot"),
             "Use a skill point-blank / targetless")
        .def("HeroUseSkill", &PySkillbarObject::HeroUseSkill,
             py::arg("target_agent_id"), py::arg("skill_number"), py::arg("hero_idx"))
        .def("ChangeHeroSecondary", &PySkillbarObject::ChangeHeroSecondary,
             py::arg("hero_index"), py::arg("profession"))
        .def("GetHeroSkillbar", &PySkillbarObject::GetHeroSkillbar, py::arg("hero_index"))
        .def("GetHoveredSkill", &PySkillbarObject::GetHoveredSkill)
        .def("IsSkillUnlocked", &PySkillbarObject::IsSkillUnlocked, py::arg("skill_id"))
        .def("IsSkillLearnt", &PySkillbarObject::IsSkillLearnt, py::arg("skill_id"));

    m.def("get_skill_slot", [](uint32_t skill_id) -> int {
        return GW::skillbar::GetSkillSlot(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("use_skill", [](uint32_t slot, uint32_t target) -> bool {
        return GW::skillbar::UseSkill(slot, target);
    }, py::arg("slot"), py::arg("target") = 0);

    m.def("point_blank_use_skill", [](uint32_t slot) -> bool {
        return GW::skillbar::PointBlankUseSkill(slot);
    }, py::arg("slot"));

    m.def("use_skill_by_id", [](uint32_t skill_id, uint32_t target) -> bool {
        return GW::skillbar::UseSkillByID(skill_id, target);
    }, py::arg("skill_id"), py::arg("target") = 0);

    m.def("get_is_skill_unlocked", [](uint32_t skill_id) -> bool {
        return GW::skillbar::GetIsSkillUnlocked(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("get_is_skill_learnt", [](uint32_t skill_id) -> bool {
        return GW::skillbar::GetIsSkillLearnt(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("get_skill_profession", [](uint32_t skill_id) -> uint32_t {
        return static_cast<uint32_t>(GW::skillbar::GetSkillProfession(
            static_cast<GW::Constants::SkillID>(skill_id)));
    }, py::arg("skill_id"));

    // Skill icon file ids (GW.dat texture ids). icon_file_id is the primary
    // icon; the hi-res id is the larger variant. Feed either to
    // PyTexture.get_texture_by_file_id or a "gwdat://<id>" key.
    m.def("get_skill_icon_file_id", [](uint32_t skill_id) -> uint32_t {
        const GW::Context::Skill* s = GW::skillbar::GetSkillConstantData(
            static_cast<GW::Constants::SkillID>(skill_id));
        return s ? s->icon_file_id : 0u;
    }, py::arg("skill_id"));

    m.def("get_skill_icon_file_id_hi_res", [](uint32_t skill_id) -> uint32_t {
        const GW::Context::Skill* s = GW::skillbar::GetSkillConstantData(
            static_cast<GW::Constants::SkillID>(skill_id));
        return s ? s->icon_file_id_hi_res : 0u;
    }, py::arg("skill_id"));

    m.def("get_attribute_profession", [](uint32_t attribute_id) -> uint32_t {
        return static_cast<uint32_t>(GW::skillbar::GetAttributeProfession(
            static_cast<GW::Constants::Attribute>(attribute_id)));
    }, py::arg("attribute_id"));

    m.def("change_second_profession", [](uint32_t profession, uint32_t hero_index) -> bool {
        return GW::skillbar::ChangeSecondProfession(
            static_cast<GW::Constants::Profession>(profession), hero_index);
    }, py::arg("profession"), py::arg("hero_index") = 0);

    m.def("load_skill_template", [](const std::string& template_str, uint32_t hero_index) -> bool {
        return GW::skillbar::LoadSkillTemplate(template_str.c_str(), hero_index);
    }, py::arg("template"), py::arg("hero_index") = 0);

    m.def("load_skillbar", [](const py::list& skill_ids, uint32_t hero_index) -> bool {
        size_t count = static_cast<size_t>(std::min(static_cast<size_t>(skill_ids.size()), size_t(8)));
        GW::Constants::SkillID skills[8] = {};
        for (size_t i = 0; i < count; ++i)
            skills[i] = static_cast<GW::Constants::SkillID>(skill_ids[i].cast<uint32_t>());
        return GW::skillbar::LoadSkillbar(skills, count, hero_index);
    }, py::arg("skill_ids"), py::arg("hero_index") = 0);

    m.def("set_attributes", [](const py::list& attr_ids, const py::list& attr_values, uint32_t hero_index) -> bool {
        size_t count = std::min(attr_ids.size(), attr_values.size());
        std::vector<uint32_t> ids(count), vals(count);
        for (size_t i = 0; i < count; ++i) {
            ids[i] = attr_ids[i].cast<uint32_t>();
            vals[i] = attr_values[i].cast<uint32_t>();
        }
        return GW::skillbar::SetAttributes(static_cast<uint32_t>(count), ids.data(), vals.data(), hero_index);
    }, py::arg("attribute_ids"), py::arg("attribute_values"), py::arg("hero_index") = 0);

    m.def("encode_skill_template", [](uint32_t hero_index) -> std::string {
        auto tpl = GW::skillbar::GetSkillTemplate(hero_index);
        char buffer[256] = {};
        if (GW::skillbar::EncodeSkillTemplate(tpl, buffer, sizeof(buffer)))
            return std::string(buffer);
        return {};
    }, py::arg("hero_index") = 0);

    m.def("decode_skill_template", [](const std::string& template_str) -> py::dict {
        py::dict result;
        GW::Context::SkillTemplate tpl;
        if (GW::skillbar::DecodeSkillTemplate(&tpl, template_str.c_str())) {
            result["profession"] = static_cast<uint32_t>(tpl.primary);
            result["secondary_profession"] = static_cast<uint32_t>(tpl.secondary);
            py::list skills;
            for (uint32_t i = 0; i < 8; ++i)
                skills.append(static_cast<uint32_t>(tpl.skills[i]));
            result["skills"] = skills;
            py::list attrs;
            for (uint32_t i = 0; i < 16; ++i) {
                if (tpl.attributes[i].attribute != GW::Constants::Attribute::None) {
                    py::dict attr;
                    attr["id"] = static_cast<uint32_t>(tpl.attributes[i].attribute);
                    attr["level"] = tpl.attributes[i].points;
                    attrs.append(attr);
                }
            }
            result["attributes"] = attrs;
        }
        return result;
    }, py::arg("template"));

    // --- ported from legacy PySkillbar.Skillbar (operations not covered by ctypes) ---

    m.def("get_hovered_skill_id", []() -> uint32_t {
        const GW::Context::Skill* skill = GW::skillbar::GetHoveredSkill();
        return skill ? static_cast<uint32_t>(skill->skill_id) : 0u;
    });

    m.def("hero_use_skill", [](uint32_t target_agent_id, uint32_t skill_number, uint32_t hero_index) -> bool {
        const uint32_t skill_idx = skill_number - 1;
        const uint32_t hero_zero = hero_index - 1;
        GW::ui::ControlAction hero_action;
        switch (hero_zero) {
        case 0: hero_action = GW::ui::ControlAction_Hero1Skill1; break;
        case 1: hero_action = GW::ui::ControlAction_Hero2Skill1; break;
        case 2: hero_action = GW::ui::ControlAction_Hero3Skill1; break;
        case 3: hero_action = GW::ui::ControlAction_Hero4Skill1; break;
        case 4: hero_action = GW::ui::ControlAction_Hero5Skill1; break;
        case 5: hero_action = GW::ui::ControlAction_Hero6Skill1; break;
        case 6: hero_action = GW::ui::ControlAction_Hero7Skill1; break;
        default: return false;
        }
        const uint32_t curr_target_id = GW::agent::GetTargetId();
        GW::game_thread::Enqueue([target_agent_id, skill_idx, hero_action, curr_target_id]() {
            if (target_agent_id && target_agent_id != GW::agent::GetTargetId()) {
                GW::agent::ChangeTarget(target_agent_id);
            }
            const auto keypress_id =
                static_cast<GW::ui::ControlAction>(static_cast<uint32_t>(hero_action) + skill_idx);
            GW::ui::Keypress(keypress_id);
            if (curr_target_id && target_agent_id != curr_target_id) {
                GW::agent::ChangeTarget(curr_target_id);
            }
        });
        return true;
    }, py::arg("target_agent_id"), py::arg("skill_number"), py::arg("hero_index"));
}

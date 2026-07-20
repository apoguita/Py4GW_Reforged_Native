// PySkill - skill constant-data bindings.
//
// Reproduces the legacy `PySkill` module surface (SkillID / SkillType / Skill) so
// the existing Python consumers (Py4GWCoreLib SkillCache.py, Skill.py,
// enums_src/Model_enums.py) work unchanged, but over the reforged data layer:
// GW::Context::Skill (via GW::skillbar::GetSkillConstantData) plus the generated
// name lookups in skill_names.cpp. No shim, no legacy table re-port.

#include "base/py_bindings.h"

#include "GW/skillbar/skillbar.h"
#include "GW/skillbar/skill_names.h"
#include "GW/context/skill.h"
#include "GW/common/constants/skills.h"
#include "GW/common/constants/constants.h"

#include <cstdint>
#include <string>

namespace py = pybind11;

namespace {

struct PySkillID {
    int id = static_cast<int>(GW::Constants::SkillID::No_Skill);

    PySkillID() = default;
    explicit PySkillID(int skill_id) : id(skill_id) {}
    explicit PySkillID(const std::string& name)
        : id(static_cast<int>(GW::skillbar::GetSkillIDByName(name))) {}

    bool operator==(int other) const { return id == other; }
    bool operator!=(int other) const { return id != other; }

    std::string GetName() const {
        return GW::skillbar::GetSkillNameByID(static_cast<uint32_t>(id));
    }
};

struct PySkillType {
    int id = static_cast<int>(GW::Constants::SkillType::Skill);

    PySkillType() = default;
    explicit PySkillType(int type_id) : id(type_id) {}

    bool operator==(int other) const { return id == other; }
    bool operator!=(int other) const { return id != other; }

    std::string GetName() const {
        return GW::skillbar::GetSkillTypeNameByID(static_cast<uint32_t>(id));
    }
};

// Legacy exposed a full Profession class (from the un-migrated PyAgent module).
// Skill consumers only use `.ToInt()` and `.GetName()`, so this is the minimal
// faithful surface for the profession field.
struct PySkillProfession {
    int id = static_cast<int>(GW::Constants::Profession::None);

    PySkillProfession() = default;
    explicit PySkillProfession(int profession_id) : id(profession_id) {}

    int ToInt() const { return id; }
    std::string GetName() const {
        return GW::skillbar::GetProfessionNameById(static_cast<uint32_t>(id));
    }
};

struct PySkill {
    PySkillID id;
    int campaign = 0;
    PySkillType type;
    int special = 0;
    int combo_req = 0;
    int effect1 = 0;
    int condition = 0;
    int effect2 = 0;
    int weapon_req = 0;
    PySkillProfession profession;
    // Legacy exposed an AttributeClass object; no in-repo consumer calls a method
    // on `.attribute`, so it is exposed as the raw attribute id (documented
    // first-pass simplification - add a wrapper if a .GetName() caller appears).
    int attribute = 0;
    int title = 0;
    int id_pvp = 0;
    int combo = 0;
    int target = 0;
    int skill_equip_type = 0;
    int overcast = 0;
    int energy_cost = 0;
    int health_cost = 0;
    int adrenaline = 0;
    float activation = 0.f;
    float aftercast = 0.f;
    int duration_0pts = 0;
    int duration_15pts = 0;
    int recharge = 0;
    int skill_arguments = 0;
    int scale_0pts = 0;
    int scale_15pts = 0;
    int bonus_scale_0pts = 0;
    int bonus_scale_15pts = 0;
    float aoe_range = 0.f;
    float const_effect = 0.f;
    int caster_overhead_animation_id = 0;
    int caster_body_animation_id = 0;
    int target_body_animation_id = 0;
    int target_overhead_animation_id = 0;
    int projectile_animation1_id = 0;
    int projectile_animation2_id = 0;
    int icon_file_id = 0;
    int icon_file2_id = 0;
    int icon_file_hi_res_id = 0;
    int name_id = 0;
    int concise = 0;
    int description_id = 0;
    bool is_touch_range = false;
    bool is_elite = false;
    bool is_half_range = false;
    bool is_pvp = false;
    bool is_pve = false;
    bool is_playable = false;
    bool is_stacking = false;
    bool is_non_stacking = false;
    bool is_unused = false;
    // Vestigial in legacy (never populated); kept for surface parity.
    int adrenaline_a = 0;
    int adrenaline_b = 0;
    int recharge2 = 0;
    uint32_t h0004 = 0;
    int h0032 = 0;
    int h0037 = 0;

    void GetContext() {
        auto* s = GW::skillbar::GetSkillConstantData(static_cast<GW::Constants::SkillID>(id.id));
        if (!s) return;

        campaign = static_cast<int>(s->campaign);
        type = PySkillType(static_cast<int>(s->type));
        special = static_cast<int>(s->special);
        combo_req = static_cast<int>(s->combo_req);
        effect1 = static_cast<int>(s->effect1);
        condition = static_cast<int>(s->condition);
        effect2 = static_cast<int>(s->effect2);
        weapon_req = static_cast<int>(s->weapon_req);
        profession = PySkillProfession(static_cast<int>(s->profession));
        attribute = static_cast<int>(s->attribute);
        title = static_cast<int>(s->title);
        id_pvp = static_cast<int>(s->skill_id_pvp);
        combo = static_cast<int>(s->combo);
        target = static_cast<int>(s->target);
        skill_equip_type = static_cast<int>(s->skill_equip_type);
        overcast = static_cast<int>(s->overcast);
        energy_cost = static_cast<int>(s->GetEnergyCost());
        health_cost = static_cast<int>(s->health_cost);
        adrenaline = static_cast<int>(s->adrenaline);
        activation = s->activation;
        aftercast = s->aftercast;
        duration_0pts = static_cast<int>(s->duration0);
        duration_15pts = static_cast<int>(s->duration15);
        recharge = static_cast<int>(s->recharge);
        skill_arguments = static_cast<int>(s->skill_arguments);
        scale_0pts = static_cast<int>(s->scale0);
        scale_15pts = static_cast<int>(s->scale15);
        bonus_scale_0pts = static_cast<int>(s->bonusScale0);
        bonus_scale_15pts = static_cast<int>(s->bonusScale15);
        aoe_range = s->aoe_range;
        const_effect = s->const_effect;
        caster_overhead_animation_id = static_cast<int>(s->caster_overhead_animation_id);
        caster_body_animation_id = static_cast<int>(s->caster_body_animation_id);
        target_body_animation_id = static_cast<int>(s->target_body_animation_id);
        target_overhead_animation_id = static_cast<int>(s->target_overhead_animation_id);
        projectile_animation1_id = static_cast<int>(s->projectile_animation_1_id);
        projectile_animation2_id = static_cast<int>(s->projectile_animation_2_id);
        icon_file_id = static_cast<int>(s->icon_file_id);
        icon_file2_id = static_cast<int>(s->icon_file_id_2);
        icon_file_hi_res_id = static_cast<int>(s->icon_file_id_hi_res);
        name_id = static_cast<int>(s->name);
        concise = static_cast<int>(s->concise);
        description_id = static_cast<int>(s->description);

        is_touch_range = s->IsTouchRange();
        is_elite = s->IsElite();
        is_half_range = s->IsHalfRange();
        is_pvp = s->IsPvP();
        is_pve = s->IsPvE();
        is_playable = s->IsPlayable();
        is_stacking = s->IsStacking();
        is_non_stacking = s->IsNonStacking();
        is_unused = s->IsUnused();

        h0004 = s->h0004;
        h0032 = static_cast<int>(s->h0032);
        h0037 = static_cast<int>(s->h0037);
    }

    PySkill() { GetContext(); }
    explicit PySkill(int skill_id) : id(skill_id) { GetContext(); }
    explicit PySkill(const std::string& name) : id(name) { GetContext(); }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PySkill, m) {
    m.doc() = "Py4GW Skill constant-data bindings";

    py::class_<PySkillID>(m, "SkillID")
        .def(py::init<>())
        .def(py::init<int>(), py::arg("id"))
        .def(py::init<std::string>(), py::arg("skillname"))
        .def("__eq__", &PySkillID::operator==)
        .def("__ne__", &PySkillID::operator!=)
        .def("GetName", &PySkillID::GetName)
        .def_readonly("id", &PySkillID::id);

    py::class_<PySkillType>(m, "SkillType")
        .def(py::init<>())
        .def(py::init<int>())
        .def("__eq__", &PySkillType::operator==)
        .def("__ne__", &PySkillType::operator!=)
        .def("GetName", &PySkillType::GetName)
        .def_readonly("id", &PySkillType::id);

    py::class_<PySkillProfession>(m, "SkillProfession")
        .def(py::init<>())
        .def(py::init<int>(), py::arg("id"))
        .def("ToInt", &PySkillProfession::ToInt)
        .def("GetName", &PySkillProfession::GetName)
        .def_readonly("id", &PySkillProfession::id);

    py::class_<PySkill>(m, "Skill")
        .def(py::init<>())
        .def(py::init<int>(), py::arg("id"))
        .def(py::init<std::string>(), py::arg("skillname"))
        .def("GetContext", &PySkill::GetContext)
        .def_readonly("id", &PySkill::id)
        .def_readonly("campaign", &PySkill::campaign)
        .def_readonly("type", &PySkill::type)
        .def_readonly("special", &PySkill::special)
        .def_readonly("combo_req", &PySkill::combo_req)
        .def_readonly("effect1", &PySkill::effect1)
        .def_readonly("condition", &PySkill::condition)
        .def_readonly("effect2", &PySkill::effect2)
        .def_readonly("weapon_req", &PySkill::weapon_req)
        .def_readonly("profession", &PySkill::profession)
        .def_readonly("attribute", &PySkill::attribute)
        .def_readonly("title", &PySkill::title)
        .def_readonly("id_pvp", &PySkill::id_pvp)
        .def_readonly("combo", &PySkill::combo)
        .def_readonly("target", &PySkill::target)
        .def_readonly("skill_equip_type", &PySkill::skill_equip_type)
        .def_readonly("overcast", &PySkill::overcast)
        .def_readonly("energy_cost", &PySkill::energy_cost)
        .def_readonly("health_cost", &PySkill::health_cost)
        .def_readonly("adrenaline", &PySkill::adrenaline)
        .def_readonly("activation", &PySkill::activation)
        .def_readonly("aftercast", &PySkill::aftercast)
        .def_readonly("duration_0pts", &PySkill::duration_0pts)
        .def_readonly("duration_15pts", &PySkill::duration_15pts)
        .def_readonly("recharge", &PySkill::recharge)
        .def_readonly("skill_arguments", &PySkill::skill_arguments)
        .def_readonly("scale_0pts", &PySkill::scale_0pts)
        .def_readonly("scale_15pts", &PySkill::scale_15pts)
        .def_readonly("bonus_scale_0pts", &PySkill::bonus_scale_0pts)
        .def_readonly("bonus_scale_15pts", &PySkill::bonus_scale_15pts)
        .def_readonly("aoe_range", &PySkill::aoe_range)
        .def_readonly("const_effect", &PySkill::const_effect)
        .def_readonly("caster_overhead_animation_id", &PySkill::caster_overhead_animation_id)
        .def_readonly("caster_body_animation_id", &PySkill::caster_body_animation_id)
        .def_readonly("target_body_animation_id", &PySkill::target_body_animation_id)
        .def_readonly("target_overhead_animation_id", &PySkill::target_overhead_animation_id)
        .def_readonly("projectile_animation1_id", &PySkill::projectile_animation1_id)
        .def_readonly("projectile_animation2_id", &PySkill::projectile_animation2_id)
        .def_readonly("icon_file_id", &PySkill::icon_file_id)
        .def_readonly("icon_file2_id", &PySkill::icon_file2_id)
        .def_readonly("icon_file_hi_res_id", &PySkill::icon_file_hi_res_id)
        .def_readonly("name_id", &PySkill::name_id)
        .def_readonly("concise", &PySkill::concise)
        .def_readonly("description_id", &PySkill::description_id)
        .def_readonly("is_touch_range", &PySkill::is_touch_range)
        .def_readonly("is_elite", &PySkill::is_elite)
        .def_readonly("is_half_range", &PySkill::is_half_range)
        .def_readonly("is_pvp", &PySkill::is_pvp)
        .def_readonly("is_pve", &PySkill::is_pve)
        .def_readonly("is_playable", &PySkill::is_playable)
        .def_readonly("is_stacking", &PySkill::is_stacking)
        .def_readonly("is_non_stacking", &PySkill::is_non_stacking)
        .def_readonly("is_unused", &PySkill::is_unused)
        .def_readonly("adrenaline_a", &PySkill::adrenaline_a)
        .def_readonly("adrenaline_b", &PySkill::adrenaline_b)
        .def_readonly("recharge2", &PySkill::recharge2)
        .def_readonly("h0004", &PySkill::h0004)
        .def_readonly("h0032", &PySkill::h0032)
        .def_readonly("h0037", &PySkill::h0037);
}

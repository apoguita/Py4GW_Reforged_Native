#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/effects/effects.h"

#include <cstdint>
#include <vector>

namespace py = pybind11;

namespace {

// Value snapshots of Context::Effect / Context::Buff. Names kept identical to the
// legacy PyEffects EffectType / BuffType so `from PyEffects import BuffType,
// EffectType` and the isinstance() checks in BuffStruct.py keep working.
struct PyEffectType {
    int skill_id = 0;
    uint32_t attribute_level = 0;
    uint32_t effect_id = 0;
    uint32_t agent_id = 0;
    float duration = 0.f;
    uint32_t timestamp = 0;
    uint32_t time_elapsed = 0;
    uint32_t time_remaining = 0;
};

struct PyBuffType {
    int skill_id = 0;
    uint32_t buff_id = 0;
    uint32_t target_agent_id = 0;
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyEffects, m) {
    m.doc() = "Py4GW Effects bindings";

    m.def("get_alcohol_level", []() -> uint32_t {
        return GW::effects::GetAlcoholLevel();
    });

    m.def("get_drunk_af", [](uint32_t intensity, uint32_t tint) {
        GW::effects::GetDrunkAf(intensity, tint);
    }, py::arg("intensity"), py::arg("tint"));

    m.def("drop_buff", [](uint32_t buff_id) {
        return GW::effects::DropBuff(buff_id);
    }, py::arg("buff_id"));

    m.def("effect_count", [](uint32_t agent_id) -> uint32_t {
        auto* effects = GW::effects::GetAgentEffects(agent_id);
        return (effects && effects->valid()) ? effects->size() : 0;
    }, py::arg("agent_id"));

    m.def("buff_count", [](uint32_t agent_id) -> uint32_t {
        auto* buffs = GW::effects::GetAgentBuffs(agent_id);
        return (buffs && buffs->valid()) ? buffs->size() : 0;
    }, py::arg("agent_id"));

    m.def("effect_exists", [](uint32_t agent_id, uint32_t skill_id) -> bool {
        auto* effects = GW::effects::GetAgentEffects(agent_id);
        const auto requested_skill_id = static_cast<GW::Constants::SkillID>(skill_id);
        if (!effects || !effects->valid()) return false;
        for (size_t i = 0; i < effects->size(); ++i) {
            if ((*effects)[i].skill_id == requested_skill_id) return true;
        }
        return false;
    }, py::arg("agent_id"), py::arg("skill_id"));

    m.def("buff_exists", [](uint32_t agent_id, uint32_t skill_id) -> bool {
        auto* buffs = GW::effects::GetAgentBuffs(agent_id);
        const auto requested_skill_id = static_cast<GW::Constants::SkillID>(skill_id);
        if (!buffs || !buffs->valid()) return false;
        for (size_t i = 0; i < buffs->size(); ++i) {
            if ((*buffs)[i].skill_id == requested_skill_id) return true;
        }
        return false;
    }, py::arg("agent_id"), py::arg("skill_id"));

    py::class_<PyEffectType>(m, "EffectType")
        .def_readonly("skill_id", &PyEffectType::skill_id)
        .def_readonly("attribute_level", &PyEffectType::attribute_level)
        .def_readonly("effect_id", &PyEffectType::effect_id)
        .def_readonly("agent_id", &PyEffectType::agent_id)
        .def_readonly("duration", &PyEffectType::duration)
        .def_readonly("timestamp", &PyEffectType::timestamp)
        .def_readonly("time_elapsed", &PyEffectType::time_elapsed)
        .def_readonly("time_remaining", &PyEffectType::time_remaining);

    py::class_<PyBuffType>(m, "BuffType")
        .def_readonly("skill_id", &PyBuffType::skill_id)
        .def_readonly("buff_id", &PyBuffType::buff_id)
        .def_readonly("target_agent_id", &PyBuffType::target_agent_id);

    m.def("get_effects", [](uint32_t agent_id) -> std::vector<PyEffectType> {
        std::vector<PyEffectType> out;
        auto* effects = GW::effects::GetAgentEffects(agent_id);
        if (!effects || !effects->valid()) return out;
        for (size_t i = 0; i < effects->size(); ++i) {
            const auto& e = (*effects)[i];
            PyEffectType pe;
            pe.skill_id = static_cast<int>(e.skill_id);
            pe.attribute_level = e.attribute_level;
            pe.effect_id = e.effect_id;
            pe.agent_id = e.agent_id;
            pe.duration = e.duration;
            pe.timestamp = e.timestamp;
            pe.time_elapsed = e.GetTimeElapsed();
            pe.time_remaining = e.GetTimeRemaining();
            out.push_back(pe);
        }
        return out;
    }, py::arg("agent_id"));

    m.def("get_buffs", [](uint32_t agent_id) -> std::vector<PyBuffType> {
        std::vector<PyBuffType> out;
        auto* buffs = GW::effects::GetAgentBuffs(agent_id);
        if (!buffs || !buffs->valid()) return out;
        for (size_t i = 0; i < buffs->size(); ++i) {
            const auto& b = (*buffs)[i];
            PyBuffType pb;
            pb.skill_id = static_cast<int>(b.skill_id);
            pb.buff_id = b.buff_id;
            pb.target_agent_id = b.target_agent_id;
            out.push_back(pb);
        }
        return out;
    }, py::arg("agent_id"));

    // ── PyEffects class (legacy surface: PyEffects(agent_id).GetEffects() etc.) ──
    struct PyEffectsWrapper { uint32_t agent_id = 0; };
    py::class_<PyEffectsWrapper>(m, "PyEffects")
        .def(py::init<uint32_t>(), py::arg("agent_id"))
        .def("GetEffects", [](const PyEffectsWrapper& w) {
            std::vector<PyEffectType> out;
            auto* effects = GW::effects::GetAgentEffects(w.agent_id);
            if (!effects || !effects->valid()) return out;
            for (size_t i = 0; i < effects->size(); ++i) {
                const auto& e = (*effects)[i];
                PyEffectType pe; pe.skill_id = static_cast<int>(e.skill_id);
                pe.attribute_level = e.attribute_level; pe.effect_id = e.effect_id;
                pe.agent_id = e.agent_id; pe.duration = e.duration;
                pe.timestamp = e.timestamp; pe.time_elapsed = e.GetTimeElapsed();
                pe.time_remaining = e.GetTimeRemaining(); out.push_back(pe);
            }
            return out;
        })
        .def("GetBuffs", [](const PyEffectsWrapper& w) {
            std::vector<PyBuffType> out;
            auto* buffs = GW::effects::GetAgentBuffs(w.agent_id);
            if (!buffs || !buffs->valid()) return out;
            for (size_t i = 0; i < buffs->size(); ++i) {
                const auto& b = (*buffs)[i]; PyBuffType pb;
                pb.skill_id = static_cast<int>(b.skill_id); pb.buff_id = b.buff_id;
                pb.target_agent_id = b.target_agent_id; out.push_back(pb);
            }
            return out;
        })
        .def("GetEffectCount", [](const PyEffectsWrapper& w) -> uint32_t {
            auto* e = GW::effects::GetAgentEffects(w.agent_id);
            return (e && e->valid()) ? e->size() : 0;
        })
        .def("GetBuffCount", [](const PyEffectsWrapper& w) -> uint32_t {
            auto* b = GW::effects::GetAgentBuffs(w.agent_id);
            return (b && b->valid()) ? b->size() : 0;
        })
        .def("EffectExists", [](const PyEffectsWrapper& w, uint32_t skill_id) -> bool {
            auto* effects = GW::effects::GetAgentEffects(w.agent_id);
            if (!effects || !effects->valid()) return false;
            for (size_t i = 0; i < effects->size(); ++i)
                if ((*effects)[i].skill_id == static_cast<GW::Constants::SkillID>(skill_id)) return true;
            return false;
        }, py::arg("skill_id"))
        .def("BuffExists", [](const PyEffectsWrapper& w, uint32_t skill_id) -> bool {
            auto* buffs = GW::effects::GetAgentBuffs(w.agent_id);
            if (!buffs || !buffs->valid()) return false;
            for (size_t i = 0; i < buffs->size(); ++i)
                if ((*buffs)[i].skill_id == static_cast<GW::Constants::SkillID>(skill_id)) return true;
            return false;
        }, py::arg("skill_id"))
        .def("DropBuff", [](const PyEffectsWrapper&, uint32_t buff_id) { GW::effects::DropBuff(buff_id); }, py::arg("skill_id"))
        .def_static("GetAlcoholLevel", []() -> uint32_t { return GW::effects::GetAlcoholLevel(); })
        .def_static("ApplyDrunkEffect", [](uint32_t intensity, uint32_t tint) { GW::effects::GetDrunkAf(intensity, tint); },
             py::arg("intensity") = 0, py::arg("tint") = 0);
}

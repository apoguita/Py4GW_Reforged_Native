#include "base/py_bindings.h"

#include "listeners/listeners.h"

#include "listeners/auto_open_chest_listener.h"
#include "listeners/faction_warning_listener.h"
#include "listeners/skill_filter_listener.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyListeners, m) {
    m.doc() = "Runtime toggles for native game-event listeners";

    m.def("list", &PY4GW::listeners::GetListenerNames, "List the names of all toggleable listeners");
    m.def("enable", &PY4GW::listeners::Enable, "Enable a listener by name", py::arg("name"));
    m.def("disable", &PY4GW::listeners::Disable, "Disable a listener by name", py::arg("name"));
    m.def("toggle", &PY4GW::listeners::Toggle, "Toggle a listener by name", py::arg("name"));
    m.def("set_enabled", &PY4GW::listeners::SetEnabled, "Set a listener's enabled state", py::arg("name"), py::arg("enabled"));
    m.def("is_enabled", &PY4GW::listeners::IsEnabled, "Check whether a listener is enabled", py::arg("name"));

    // --- skill_list_filter config ---
    // Toggle the listener on/off via enable("skill_list_filter"); these tune
    // which skills it hides once enabled.
    m.def("set_hide_known_skills",
        [](bool value) { PY4GW::listeners::SkillListFilter().SetHideKnownSkills(value); },
        "Hide skills the character already owns from tome / trainer / capture windows",
        py::arg("value"));
    m.def("get_hide_known_skills",
        [] { return PY4GW::listeners::SkillListFilter().HideKnownSkills(); },
        "Whether known skills are hidden from the skill-selection windows");
    m.def("set_hide_nonelites_on_capture",
        [](bool value) { PY4GW::listeners::SkillListFilter().SetHideNonElitesOnCapture(value); },
        "In the skill-capture window, hide all non-elite skills",
        py::arg("value"));
    m.def("get_hide_nonelites_on_capture",
        [] { return PY4GW::listeners::SkillListFilter().HideNonElitesOnCapture(); },
        "Whether non-elite skills are hidden in the skill-capture window");

    // --- faction_warning config ---
    // Toggle the listener on/off via enable("faction_warning"); this sets the
    // percentage of the faction cap at which the warning fires.
    m.def("set_faction_warn_percent",
        [](int percent) { PY4GW::listeners::FactionWarning().SetWarnPercent(percent); },
        "Percentage (0-100) of the faction cap at which to warn",
        py::arg("percent"));
    m.def("get_faction_warn_percent",
        [] { return PY4GW::listeners::FactionWarning().WarnPercent(); },
        "The configured faction-warning percentage");

    // --- auto_open_locked_chest config ---
    // Toggle the listener on/off via enable("auto_open_locked_chest"); these
    // pick which auto-open response(s) it sends.
    m.def("set_auto_open_use_key",
        [](bool value) { PY4GW::listeners::AutoOpenLockedChest().SetUseKey(value); },
        "Auto-send the 'use key' response at a locked chest",
        py::arg("value"));
    m.def("get_auto_open_use_key",
        [] { return PY4GW::listeners::AutoOpenLockedChest().UseKey(); },
        "Whether the 'use key' response is auto-sent");
    m.def("set_auto_open_use_lockpick",
        [](bool value) { PY4GW::listeners::AutoOpenLockedChest().SetUseLockpick(value); },
        "Auto-send the 'use lockpick' response at a locked chest",
        py::arg("value"));
    m.def("get_auto_open_use_lockpick",
        [] { return PY4GW::listeners::AutoOpenLockedChest().UseLockpick(); },
        "Whether the 'use lockpick' response is auto-sent");
}

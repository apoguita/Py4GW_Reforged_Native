#include "base/py_bindings.h"

#include "listeners/agent_events_listener.h"

#include <string>

namespace py = pybind11;

using PY4GW::listeners::AgentEvents;
using PY4GW::listeners::RawAgentEvent;

PYBIND11_EMBEDDED_MODULE(PyAgentEvents, m) {
    m.doc() = "Per-agent event capture (agent_events listener). The capture is a "
              "named listener: enabled by default at startup, toggle it via "
              "PyListeners or the enable()/disable() helpers here, then drain the "
              "raw event buffer each frame. All interpretation stays in Python.";

    // Event type constants (single source: GW_AGENT_EVENT_TYPES X-macro).
    py::module_ types = m.def_submodule("PyEventType", "Agent event type constants");
#define GW_AGENT_EVENT_BIND(name, value) types.attr(#name) = static_cast<uint32_t>(value);
    GW_AGENT_EVENT_TYPES(GW_AGENT_EVENT_BIND)
#undef GW_AGENT_EVENT_BIND

    py::class_<RawAgentEvent>(m, "PyRawAgentEvent")
        .def(py::init<>())
        .def_readonly("timestamp", &RawAgentEvent::timestamp)
        .def_readonly("event_type", &RawAgentEvent::event_type)
        .def_readonly("agent_id", &RawAgentEvent::agent_id)
        .def_readonly("value", &RawAgentEvent::value)
        .def_readonly("target_id", &RawAgentEvent::target_id)
        .def_readonly("float_value", &RawAgentEvent::float_value)
        .def_readonly("agent_max_hp", &RawAgentEvent::agent_max_hp)
        .def_readonly("agent_max_energy", &RawAgentEvent::agent_max_energy)
        .def_readonly("target_max_hp", &RawAgentEvent::target_max_hp)
        .def_readonly("target_max_energy", &RawAgentEvent::target_max_energy)
        .def("__repr__", [](const RawAgentEvent& e) {
            return "<RawAgentEvent type=" + std::to_string(e.event_type) +
                   " agent=" + std::to_string(e.agent_id) +
                   " value=" + std::to_string(e.value) +
                   " target=" + std::to_string(e.target_id) +
                   " float=" + std::to_string(e.float_value) + ">";
        })
        .def("as_tuple", [](const RawAgentEvent& e) {
            return py::make_tuple(e.timestamp, e.event_type, e.agent_id,
                                  e.value, e.target_id, e.float_value);
        });

    // Toggle convenience (also available via PyListeners by name "agent_events").
    m.def("enable", []() { AgentEvents().Enable(); },
        "Install the capture hooks and start recording agent events.");
    m.def("disable", []() { AgentEvents().Disable(); },
        "Remove the capture hooks and clear the buffer.");
    m.def("is_enabled", []() { return AgentEvents().IsEnabled(); });

    // Buffer access.
    m.def("get_and_clear_events", []() { return AgentEvents().GetAndClearEvents(); },
        "Return all captured events and clear the buffer (call each frame).");
    m.def("peek_events", []() { return AgentEvents().PeekEvents(); },
        "Return the captured events without clearing (for debugging).");
    m.def("get_event_count", []() { return static_cast<uint32_t>(AgentEvents().GetEventCount()); });
    m.def("get_capacity", []() { return static_cast<uint32_t>(AgentEvents().GetCapacity()); });
}

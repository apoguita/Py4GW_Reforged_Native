#include "base/py_bindings.h"

#include "listeners/listeners.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyListeners, m) {
    m.doc() = "Runtime toggles for native game-event listeners";

    m.def("list", &PY4GW::listeners::GetListenerNames, "List the names of all toggleable listeners");
    m.def("enable", &PY4GW::listeners::Enable, "Enable a listener by name", py::arg("name"));
    m.def("disable", &PY4GW::listeners::Disable, "Disable a listener by name", py::arg("name"));
    m.def("toggle", &PY4GW::listeners::Toggle, "Toggle a listener by name", py::arg("name"));
    m.def("set_enabled", &PY4GW::listeners::SetEnabled, "Set a listener's enabled state", py::arg("name"), py::arg("enabled"));
    m.def("is_enabled", &PY4GW::listeners::IsEnabled, "Check whether a listener is enabled", py::arg("name"));
}

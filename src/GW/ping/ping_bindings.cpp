#include "base/py_bindings.h"

#include "GW/ping/ping.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyPing, m) {
    m.doc() = "Py4GW ping tracker bindings";

    py::class_<GW::ping::PingTracker>(m, "PingHandler")
        .def(py::init<>())
        .def("Terminate", &GW::ping::PingTracker::Terminate)
        .def("GetCurrentPing", &GW::ping::PingTracker::GetCurrentPing)
        .def("GetAveragePing", &GW::ping::PingTracker::GetAveragePing)
        .def("GetMinPing", &GW::ping::PingTracker::GetMinPing)
        .def("GetMaxPing", &GW::ping::PingTracker::GetMaxPing);
}

#include "base/py_bindings.h"

#include "profiler/profiler.h"
#include "system/system.h"

#include <string>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyProfiler, m) {
    m.doc() = "Py4GW performance counters (see legacy bind_Profiler)";

    // Reporting surface (legacy Console.get_profiler_* parity).
    m.def("get_metric_names", &PY4GW::Profiler::GetMetricNames, "Get a list of all profiler metric names");
    m.def("get_reports", &PY4GW::Profiler::CalculateReportAll, "Get per-metric reports: (name, min, avg, p50, p95, p99, max)");
    m.def("get_history", &PY4GW::Profiler::GetMetricHistory, "Get the sample history for a metric", py::arg("metric_name"));
    m.def("reset", &PY4GW::Profiler::Reset, "Clear all profiler history");

    // Helpers so scripts can time their own sections independently of the
    // callback scheduler. End uses the current frame stamp, matching the
    // scheduler's internal timing.
    m.def("start", &PY4GW::Profiler::Start, "Begin timing a named metric", py::arg("name"));
    m.def("end", [](const std::string& name) {
        PY4GW::Profiler::End(PY4GW::System::GetTickCount64(), name);
    }, "End timing a named metric", py::arg("name"));
}

#include "base/py_bindings.h"

#include "callback/callback.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyCallback, m) {
    m.doc() = "Frame callback scheduler with phased execution and priorities";

    py::enum_<PY4GW::PyCallback::Phase>(m, "Phase")
        .value("PreUpdate", PY4GW::PyCallback::Phase::PreUpdate)
        .value("Data", PY4GW::PyCallback::Phase::Data)
        .value("Update", PY4GW::PyCallback::Phase::Update)
        .export_values();

    py::enum_<PY4GW::PyCallback::Context>(m, "Context")
        .value("Update", PY4GW::PyCallback::Context::Update)
        .value("Draw", PY4GW::PyCallback::Context::Draw)
        .value("Main", PY4GW::PyCallback::Context::Main)
        .export_values();

    py::class_<PY4GW::PyCallback>(m, "PyCallback")
        .def_static("Register", &PY4GW::PyCallback::Register,
            py::arg("name"),
            py::arg("fn"),
            py::arg("phase"),
            py::arg("priority") = 99,
            py::arg("context") = PY4GW::PyCallback::Context::Draw)
        .def_static("RemoveById", &PY4GW::PyCallback::RemoveById, py::arg("id"))
        .def_static("RemoveByName", &PY4GW::PyCallback::RemoveByName, py::arg("name"))
        .def_static("PauseById", &PY4GW::PyCallback::PauseById, py::arg("id"))
        .def_static("ResumeById", &PY4GW::PyCallback::ResumeById, py::arg("id"))
        .def_static("IsPaused", &PY4GW::PyCallback::IsPaused, py::arg("id"))
        .def_static("IsRegistered", &PY4GW::PyCallback::IsRegistered, py::arg("id"))
        .def_static("Clear", &PY4GW::PyCallback::Clear)
        .def_static("GetCallbackInfo", &PY4GW::PyCallback::GetCallbackInfo, R"doc(Returns a list of tuples)doc");
}

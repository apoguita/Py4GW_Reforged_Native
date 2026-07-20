#include "base/py_bindings.h"

#include "virtual_input/virtual_input.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyKeystroke, m) {
    // Binding for the scan code-based key handler
    py::class_<PY4GW::KeyHandler>(m, "PyKeyHandler")
        .def(py::init<>()) // Constructor

        // Single key functions
        .def("PressKey", &PY4GW::KeyHandler::press_key, "Press a single key using scan code", py::arg("virtualKeyCode"))
        .def("ReleaseKey", &PY4GW::KeyHandler::release_key, "Release a single key using scan code", py::arg("virtualKeyCode"))
        .def("PushKey", &PY4GW::KeyHandler::push_key, "Press and release a single key using scan code", py::arg("virtualKeyCode"))

        // Key combination functions
        .def("PressKeyCombo", &PY4GW::KeyHandler::press_key_combo, "Press a combination of keys using scan codes", py::arg("keys"))
        .def("ReleaseKeyCombo", &PY4GW::KeyHandler::release_key_combo, "Release a combination of keys using scan codes", py::arg("keys"))
        .def("PushKeyCombo", &PY4GW::KeyHandler::push_key_combo, "Press and release a combination of keys using scan codes", py::arg("keys"));
}

PYBIND11_EMBEDDED_MODULE(PyMouse, m) {
    // Binding for the mouse handler
    py::class_<PY4GW::MouseHandler>(m, "PyMouse")
        .def(py::init<>()) // Constructor
        // Mouse movement
        .def("MoveMouse", &PY4GW::MouseHandler::MoveMouse, "Move mouse to (x, y) relative to the client window", py::arg("x"), py::arg("y"))
        // Mouse click functions
        .def("Click", &PY4GW::MouseHandler::Click, "Click the mouse button at (x, y)", py::arg("button") = 0, py::arg("x") = 0, py::arg("y") = 0)
        .def("DoubleClick", &PY4GW::MouseHandler::DoubleClick, "Double click the mouse button at (x, y)", py::arg("button") = 0, py::arg("x") = 0, py::arg("y") = 0)
        // Mouse scroll
        .def("Scroll", &PY4GW::MouseHandler::Scroll, "Scroll the mouse wheel", py::arg("delta"), py::arg("x") = 0, py::arg("y") = 0)
        // Mouse button press/release
        .def("PressButton", &PY4GW::MouseHandler::PressButton, "Press a mouse button at (x, y)", py::arg("button") = 0, py::arg("x") = 0, py::arg("y") = 0)
        .def("ReleaseButton", &PY4GW::MouseHandler::ReleaseButton, "Release a mouse button at (x, y)", py::arg("button") = 0, py::arg("x") = 0, py::arg("y") = 0);
}

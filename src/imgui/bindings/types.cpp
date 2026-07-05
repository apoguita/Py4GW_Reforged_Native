#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>

#include <string>

namespace PY4GW::imgui_bindings {

void register_types(py::module_& m) {
    py::class_<ImVec2>(m, "Vec2")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f)
        .def(py::init([](const py::sequence& s) { return ImVec2(s[0].cast<float>(), s[1].cast<float>()); }))
        .def_readwrite("x", &ImVec2::x)
        .def_readwrite("y", &ImVec2::y)
        // Tuple-like ergonomics so the library can use a Vec2 as either a point
        // (.x/.y) or a sequence (v[0], len(v), `x, y = v`), matching the Python Vec2f.
        .def("__len__", [](const ImVec2&) { return 2; })
        .def("__getitem__", [](const ImVec2& v, int i) -> float {
            if (i < 0) i += 2;
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            throw py::index_error();
        })
        .def("__iter__", [](const ImVec2& v) { return py::iter(py::make_tuple(v.x, v.y)); })
        .def("__repr__", [](const ImVec2& v) { return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; })
        .def("__eq__", [](const ImVec2& a, const ImVec2& b) { return a.x == b.x && a.y == b.y; });
    py::implicitly_convertible<py::tuple, ImVec2>();

    py::class_<ImVec4>(m, "Vec4")
        .def(py::init<>())
        .def(py::init<float, float, float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("z") = 0.0f, py::arg("w") = 0.0f)
        .def(py::init([](const py::sequence& s) { return ImVec4(s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>(), s[3].cast<float>()); }))
        .def_readwrite("x", &ImVec4::x)
        .def_readwrite("y", &ImVec4::y)
        .def_readwrite("z", &ImVec4::z)
        .def_readwrite("w", &ImVec4::w)
        .def("__len__", [](const ImVec4&) { return 4; })
        .def("__getitem__", [](const ImVec4& v, int i) -> float {
            if (i < 0) i += 4;
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            if (i == 2) return v.z;
            if (i == 3) return v.w;
            throw py::index_error();
        })
        .def("__iter__", [](const ImVec4& v) { return py::iter(py::make_tuple(v.x, v.y, v.z, v.w)); })
        .def("__repr__", [](const ImVec4& v) { return "(" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w) + ")"; });
    py::implicitly_convertible<py::tuple, ImVec4>();

    // Color packing helpers -> ImU32 (the form every draw-list call expects).
    m.def("color", [](float r, float g, float b, float a) {
              return static_cast<ImU32>(IM_COL32(static_cast<int>(r * 255.0f), static_cast<int>(g * 255.0f),
                                                 static_cast<int>(b * 255.0f), static_cast<int>(a * 255.0f)));
          }, py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 1.0f,
          "Pack normalized 0..1 floats into a 0xAABBGGRR ImU32 color.");
    m.def("color_u32", [](int r, int g, int b, int a) {
              return static_cast<ImU32>(IM_COL32(r, g, b, a));
          }, py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 255,
          "Pack 0..255 ints into a 0xAABBGGRR ImU32 color.");
}

}  // namespace PY4GW::imgui_bindings

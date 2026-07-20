#pragma once
#include "base/py_bindings.h"
#include <imgui.h>
#include <array>
#include <string>
#include <vector>

namespace py = pybind11;

/*
 * bind_helpers.h — Template-based pybind11 helpers for Dear ImGui.
 *
 * Collapses repetitive widget/enum binding patterns into one-liners.
 *
 *   IOBIND("checkbox",     bool,   ImGui::Checkbox);
 *   IOBIND("input_float",  float,  ImGui::InputFloat);
 *
 *   IOVECBIND("drag_float2", float, 2, ImGui::DragFloat2, py::arg("speed"), py::arg("v_min"), py::arg("v_max"));
 */

namespace bind_helpers {

// ── Single-value I/O widget ────────────────────────────────────
// fn(label, T* v, extra...)  →  Python fn(label, v, extra...) → T
template<typename T, typename... ExtraArgs>
void IOWidget(py::module_& m, const char* name,
              bool(*fn)(const char*, T*, ExtraArgs...),
              const std::initializer_list<py::arg>& args_list)
{
    auto wrapper = [fn](const std::string& label, T v, ExtraArgs... extra) -> T {
        T t = v;
        fn(label.c_str(), &t, extra...);
        return t;
    };
    // Unpack initializer_list explicitly since C++ can't expand parameter packs from it
    auto it = args_list.begin();
    auto end = args_list.end();
    size_t n = std::distance(it, end);

    if (n == 0)
        m.def(name, wrapper, py::arg("label"), py::arg("value"));
    else if (n == 1)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *it);
    else if (n == 2)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1));
    else if (n == 3)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2));
    else if (n == 4)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2), *(it+3));
    else if (n == 5)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2), *(it+3), *(it+4));
    else
        m.def(name, wrapper, py::arg("label"), py::arg("value"));
}

// ── Multi-component I/O widget ─────────────────────────────────
// fn(label, T v[N], extra...)  →  Python fn(label, [T,N], extra...) → [T,N]
template<typename T, size_t N, typename... ExtraArgs>
void IOVecWidget(py::module_& m, const char* name,
                 bool(*fn)(const char*, T*, ExtraArgs...),
                 const std::initializer_list<py::arg>& args_list)
{
    auto wrapper = [fn](const std::string& label, const std::array<T, N>& v, ExtraArgs... extra) -> std::array<T, N> {
        std::array<T, N> t = v;
        fn(label.c_str(), t.data(), extra...);
        return t;
    };
    auto it = args_list.begin();
    auto end = args_list.end();
    size_t n = std::distance(it, end);

    if (n == 0)
        m.def(name, wrapper, py::arg("label"), py::arg("value"));
    else if (n == 1)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *it);
    else if (n == 2)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1));
    else if (n == 3)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2));
    else if (n == 4)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2), *(it+3));
    else if (n == 5)
        m.def(name, wrapper, py::arg("label"), py::arg("value"), *(it), *(it+1), *(it+2), *(it+3), *(it+4));
    else
        m.def(name, wrapper, py::arg("label"), py::arg("value"));
}

// ── A2/V2/A4/V4 helpers ────────────────────────────────────────
inline std::array<float, 2> A2(const ImVec2& v) { return {v.x, v.y}; }
inline ImVec2 V2(const std::array<float, 2>& a) { return {a[0], a[1]}; }
inline std::array<float, 4> A4(const ImVec4& v) { return {v.x, v.y, v.z, v.w}; }
inline ImVec4 V4(const std::array<float, 4>& a) { return {a[0], a[1], a[2], a[3]}; }

} // namespace bind_helpers

// ── Convenience macros ─────────────────────────────────────────

#define IOBIND(name, T, fn, ...) \
    bind_helpers::IOWidget<T>(m, name, fn, {__VA_ARGS__})

#define IOVECBIND(name, T, N, fn, ...) \
    bind_helpers::IOVecWidget<T, N>(m, name, fn, {__VA_ARGS__})

// Flags enum: chain .value().value()...export_values().__or__.__and__
#define BIND_FLAGS_ENUM(name, type) \
    py::enum_<type>(m, name)

// Attach bitwise operators AFTER .export_values() — must chain
#define BIND_FLAGS_END(type) \
    .export_values() \
    .def("__or__", [](type a, type b) -> type { return static_cast<type>(static_cast<int>(a) | static_cast<int>(b)); }) \
    .def("__and__", [](type a, type b) -> type { return static_cast<type>(static_cast<int>(a) & static_cast<int>(b)); })

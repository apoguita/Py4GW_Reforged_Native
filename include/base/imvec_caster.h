#pragma once
#include "base/py_bindings.h"
#include <imgui.h>
#include <array>

namespace py = pybind11;

/*
 * imvec_caster.h — Automatic pybind11 type conversions for ImVec2, ImVec4.
 * After including this, ImGui::GetWindowPos() returns a Python tuple directly.
 */

namespace pybind11::detail {

// ── ImVec2 ↔ std::array<float,2> ───────────────────────────────
template <>
struct type_caster<ImVec2> {
    PYBIND11_TYPE_CASTER(ImVec2, const_name("ImVec2"));

    bool load(handle src, bool) {
        if (py::isinstance<py::tuple>(src) || py::isinstance<py::list>(src)) {
            auto seq = py::reinterpret_borrow<py::sequence>(src);
            if (py::len(seq) >= 2) {
                value.x = seq[0].cast<float>();
                value.y = seq[1].cast<float>();
                return true;
            }
        }
        return false;
    }

    static handle cast(ImVec2 src, return_value_policy, handle) {
        return py::make_tuple(src.x, src.y).release();
    }
};

// ── ImVec4 ↔ std::array<float,4> ───────────────────────────────
template <>
struct type_caster<ImVec4> {
    PYBIND11_TYPE_CASTER(ImVec4, const_name("ImVec4"));

    bool load(handle src, bool) {
        if (py::isinstance<py::tuple>(src) || py::isinstance<py::list>(src)) {
            auto seq = py::reinterpret_borrow<py::sequence>(src);
            if (py::len(seq) >= 4) {
                value.x = seq[0].cast<float>();
                value.y = seq[1].cast<float>();
                value.z = seq[2].cast<float>();
                value.w = seq[3].cast<float>();
                return true;
            }
        }
        return false;
    }

    static handle cast(ImVec4 src, return_value_policy, handle) {
        return py::make_tuple(src.x, src.y, src.z, src.w).release();
    }
};

} // namespace pybind11::detail

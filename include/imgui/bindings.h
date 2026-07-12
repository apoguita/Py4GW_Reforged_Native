#pragma once

#include "base/error_handling.h"

#include <pybind11/pybind11.h>

// Shared registration surface for the PyImGui module.
//
// The module is declared once (PYBIND11_EMBEDDED_MODULE in imgui_bindings.cpp)
// and assembled from per-domain registration functions. Each register_* call
// receives the same module object and contributes one domain of the binding.
//
// Ordering contract: value types and enums must already be registered on the
// module before any register_* that references them runs.

namespace PY4GW::imgui_bindings {

namespace py = pybind11;

// Fabricated opt-in docking flag (NOT a real ImGuiWindowFlags bit; picked above
// the internal ImGui flag range). Docking is opt-in: windows are non-dockable by
// default, and DefaultWindowFlags injects ImGuiWindowFlags_NoDocking unless this
// bit is present. The bit is always stripped before reaching ImGui. Exposed to
// Python as PyImGui.WindowFlags.Docking.
constexpr int kWindowFlags_Docking = 1 << 30;

// Value types: Vec2, Vec4, and color packing helpers. Tuples/lists convert
// implicitly to Vec2/Vec4 so callers never have to construct them explicitly.
// Must be registered before anything that uses ImVec2/ImVec4.
void register_types(py::module_& m);

// All flag/enum surfaces, each with uniform bitwise operators. Must be
// registered before functions/classes that reference the enums.
void register_enums(py::module_& m);

// Live ImGuiStyle handle (returned by-reference from get_style) plus the
// StyleConfig snapshot helper.
void register_style(py::module_& m);

// Docking entry points plus DockBuilder layout helpers (imgui_internal), so
// dock layouts can be authored entirely from Python.
void register_docking(py::module_& m);

// Draw list as a real bound class (ImDrawList) plus window/foreground/background
// accessors. Foreground/background lists are what game overlays (ESP, world
// drawing) need, and were previously unreachable from Python.
void register_drawlist(py::module_& m);

// Live, guarded ImGuiIO handle: read everything useful, write only the safe
// subset, never expose the pointer/array internals that would corrupt the host.
void register_io(py::module_& m);

// Vendored imgui addons exposed as PyImGui submodules: filebrowser, hotkey,
// markdown, memory_editor, anim, text_editor.
void register_addons(py::module_& m);

}  // namespace PY4GW::imgui_bindings

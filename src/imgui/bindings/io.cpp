#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>

#include <stdexcept>

namespace PY4GW::imgui_bindings {

namespace {

// Empty Python-side handle. All access goes through the live ImGuiIO so the
// values are always current and writes take effect immediately. The host owns
// the IO; we expose a read-everything / write-safe-subset view only.
struct IOHandle {};

ImGuiIO& LiveIO() {
    if (ImGui::GetCurrentContext() == nullptr) {
        throw std::runtime_error("ImGui context is not active");
    }
    return ImGui::GetIO();
}

}  // namespace

void register_io(py::module_& m) {
    py::class_<IOHandle>(m, "ImGuiIO")
        // ── read-only state (per-frame) ──
        .def_property_readonly("display_size", [](IOHandle&) { return LiveIO().DisplaySize; })
        // Flattened _x/_y accessors for legacy parity (the library reads display_size_x /
        // mouse_pos_x, not the Vec2 form).
        .def_property_readonly("display_size_x", [](IOHandle&) { return LiveIO().DisplaySize.x; })
        .def_property_readonly("display_size_y", [](IOHandle&) { return LiveIO().DisplaySize.y; })
        .def_property_readonly("delta_time", [](IOHandle&) { return LiveIO().DeltaTime; })
        .def_property_readonly("framerate", [](IOHandle&) { return LiveIO().Framerate; })
        .def_property_readonly("mouse_pos", [](IOHandle&) { return LiveIO().MousePos; })
        .def_property_readonly("mouse_pos_x", [](IOHandle&) { return LiveIO().MousePos.x; })
        .def_property_readonly("mouse_pos_y", [](IOHandle&) { return LiveIO().MousePos.y; })
        .def_property_readonly("mouse_pos_prev_x", [](IOHandle&) { return LiveIO().MousePosPrev.x; })
        .def_property_readonly("mouse_pos_prev_y", [](IOHandle&) { return LiveIO().MousePosPrev.y; })
        .def_property_readonly("mouse_wheel", [](IOHandle&) { return LiveIO().MouseWheel; })
        .def_property_readonly("mouse_wheel_h", [](IOHandle&) { return LiveIO().MouseWheelH; })
        .def_property_readonly("key_ctrl", [](IOHandle&) { return LiveIO().KeyCtrl; })
        .def_property_readonly("key_shift", [](IOHandle&) { return LiveIO().KeyShift; })
        .def_property_readonly("key_alt", [](IOHandle&) { return LiveIO().KeyAlt; })
        .def_property_readonly("key_super", [](IOHandle&) { return LiveIO().KeySuper; })
        .def_property_readonly("want_capture_mouse", [](IOHandle&) { return LiveIO().WantCaptureMouse; })
        .def_property_readonly("want_capture_keyboard", [](IOHandle&) { return LiveIO().WantCaptureKeyboard; })
        .def_property_readonly("want_text_input", [](IOHandle&) { return LiveIO().WantTextInput; })
        .def_property_readonly("want_set_mouse_pos", [](IOHandle&) { return LiveIO().WantSetMousePos; })
        .def_property_readonly("want_save_ini_settings", [](IOHandle&) { return LiveIO().WantSaveIniSettings; })
        .def_property_readonly("backend_flags", [](IOHandle&) { return static_cast<int>(LiveIO().BackendFlags); })
        .def_property_readonly("metrics_render_vertices", [](IOHandle&) { return LiveIO().MetricsRenderVertices; })
        .def_property_readonly("metrics_render_indices", [](IOHandle&) { return LiveIO().MetricsRenderIndices; })
        .def_property_readonly("metrics_active_windows", [](IOHandle&) { return LiveIO().MetricsActiveWindows; })
        .def("mouse_down", [](IOHandle&, int button) {
                 if (button < 0 || button >= 5) {
                     throw std::out_of_range("mouse button must be 0..4");
                 }
                 return LiveIO().MouseDown[button];
             }, py::arg("button"))

        // ── read/write safe configuration subset ──
        .def_property("config_flags",
             [](IOHandle&) { return static_cast<int>(LiveIO().ConfigFlags); },
             [](IOHandle&, int v) { LiveIO().ConfigFlags = static_cast<ImGuiConfigFlags>(v); })
        .def_property("mouse_draw_cursor",
             [](IOHandle&) { return LiveIO().MouseDrawCursor; },
             [](IOHandle&, bool v) { LiveIO().MouseDrawCursor = v; })
        .def_property("ini_saving_rate",
             [](IOHandle&) { return LiveIO().IniSavingRate; },
             [](IOHandle&, float v) { LiveIO().IniSavingRate = v; })
        .def_property("mouse_double_click_time",
             [](IOHandle&) { return LiveIO().MouseDoubleClickTime; },
             [](IOHandle&, float v) { LiveIO().MouseDoubleClickTime = v; })
        .def_property("mouse_drag_threshold",
             [](IOHandle&) { return LiveIO().MouseDragThreshold; },
             [](IOHandle&, float v) { LiveIO().MouseDragThreshold = v; })
        .def_property("config_docking_no_split",
             [](IOHandle&) { return LiveIO().ConfigDockingNoSplit; },
             [](IOHandle&, bool v) { LiveIO().ConfigDockingNoSplit = v; })
        .def_property("config_docking_with_shift",
             [](IOHandle&) { return LiveIO().ConfigDockingWithShift; },
             [](IOHandle&, bool v) { LiveIO().ConfigDockingWithShift = v; })
        .def_property("config_windows_move_from_title_bar_only",
             [](IOHandle&) { return LiveIO().ConfigWindowsMoveFromTitleBarOnly; },
             [](IOHandle&, bool v) { LiveIO().ConfigWindowsMoveFromTitleBarOnly = v; })
        .def_property("config_input_text_cursor_blink",
             [](IOHandle&) { return LiveIO().ConfigInputTextCursorBlink; },
             [](IOHandle&, bool v) { LiveIO().ConfigInputTextCursorBlink = v; })
        // ImGui 1.92 DPI font scaling: when enabled, the docking backend keeps
        // style.FontScaleDpi in sync with the monitor DPI so fonts stay crisp
        // across monitors. Viewports variant additionally scales window sizes.
        .def_property("config_dpi_scale_fonts",
             [](IOHandle&) { return LiveIO().ConfigDpiScaleFonts; },
             [](IOHandle&, bool v) { LiveIO().ConfigDpiScaleFonts = v; })
        .def_property("config_dpi_scale_viewports",
             [](IOHandle&) { return LiveIO().ConfigDpiScaleViewports; },
             [](IOHandle&, bool v) { LiveIO().ConfigDpiScaleViewports = v; })

        // ── config flag helpers ──
        .def("add_config_flag", [](IOHandle&, int f) {
                 LiveIO().ConfigFlags |= static_cast<ImGuiConfigFlags>(f);
             }, py::arg("flag"))
        .def("remove_config_flag", [](IOHandle&, int f) {
                 LiveIO().ConfigFlags &= ~static_cast<ImGuiConfigFlags>(f);
             }, py::arg("flag"))
        .def("has_config_flag", [](IOHandle&, int f) {
                 return (LiveIO().ConfigFlags & static_cast<ImGuiConfigFlags>(f)) != 0;
             }, py::arg("flag"))
        .def("has_backend_flag", [](IOHandle&, int f) {
                 return (LiveIO().BackendFlags & static_cast<ImGuiBackendFlags>(f)) != 0;
             }, py::arg("flag"));

    m.def("get_io", []() { return IOHandle{}; },
          "Live ImGuiIO handle (read state, write the safe config subset).");
}

}  // namespace PY4GW::imgui_bindings

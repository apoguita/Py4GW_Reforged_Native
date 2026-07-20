#include "base/py_bindings.h"

#include <imgui.h>

#include "imgui/bindings.h"
#include "imgui/ext/ext.h"
#include "imgui/font_manager.h"
#include "imgui/imgui_manager.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;


// ── Table sort helpers ─────────────────────────────────────────
static int TableColSortCol(const ImGuiTableColumnSortSpecs& s) { return s.ColumnIndex; }
static int TableColSortDir(const ImGuiTableColumnSortSpecs& s) { return s.SortDirection; }
static void ClearSortSpecsDirty(ImGuiTableSortSpecs* s) { s->SpecsDirty = false; }
static void SetNextWindowDetached(bool detached, bool no_taskbar_icon, bool no_decoration, bool top_level) {
    if (!detached) {
        return;
    }
    ImGuiWindowClass window_class;
    window_class.ParentViewportId = top_level ? 0 : static_cast<ImGuiID>(-1);
    window_class.ViewportFlagsOverrideSet |= ImGuiViewportFlags_NoAutoMerge;
    if (no_taskbar_icon) {
        window_class.ViewportFlagsOverrideSet |= ImGuiViewportFlags_NoTaskBarIcon;
    }
    if (no_decoration) {
        window_class.ViewportFlagsOverrideSet |= ImGuiViewportFlags_NoDecoration;
    }
    ImGui::SetNextWindowClass(&window_class);
}
static void SetNextWindowMainViewport() {
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    if (main_viewport != nullptr) {
        ImGui::SetNextWindowViewport(main_viewport->ID);
    }
}


// ── String/wstring helpers ─────────────────────────────────────
static std::wstring ToWide(const std::string& s) { return {s.begin(), s.end()}; }

// ── Text format helpers (ImGui takes printf-style format) ─────
static void TextV(const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::TextUnformatted(s.c_str());
}
static void TextColoredV(const ImVec4& col, const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::TextColored(col, "%s", s.c_str());
}
static void TextDisabledV(const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::TextDisabled("%s", s.c_str());
}
static void TextWrappedV(const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::TextWrapped("%s", s.c_str());
}
static void LabelTextV(const std::string& label, const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::LabelText(label.c_str(), "%s", s.c_str());
}
static void BulletTextV(const std::string& fmt, const std::vector<std::string>& args) {
    std::string s = fmt;
    for(const auto& a:args){auto p=s.find("%s");if(p==std::string::npos)break;s.replace(p,2,a);}
    ImGui::BulletText("%s", s.c_str());
}
static ImGuiWindowFlags DefaultWindowFlags(int flags, bool allow_docking = false) {
    // Docking is opt-in: a window is dockable only if it carries the fabricated
    // WindowFlags.Docking bit (or the legacy allow_docking arg). Otherwise inject
    // NoDocking. The fabricated bit is always stripped before it reaches ImGui.
    const bool docking = allow_docking || (flags & PY4GW::imgui_bindings::kWindowFlags_Docking);
    ImGuiWindowFlags window_flags = static_cast<ImGuiWindowFlags>(flags & ~PY4GW::imgui_bindings::kWindowFlags_Docking);
    if (!docking) {
        window_flags = static_cast<ImGuiWindowFlags>(window_flags | ImGuiWindowFlags_NoDocking);
    }
    return window_flags;
}

// ═══════════════════════════════════════════════════════════════════
//  MODULE
// ═══════════════════════════════════════════════════════════════════

PYBIND11_EMBEDDED_MODULE(PyImGui, m) {
    m.doc() = "Py4GW ImGui bindings — full API parity";

    PY4GW::imgui_bindings::register_types(m);
    PY4GW::imgui_bindings::register_enums(m);
    // ═══════════════ TYPES / OBJECTS ══════════════════════════════

    py::class_<ImGuiTableColumnSortSpecs>(m, "TableColumnSortSpecs")
        .def_property_readonly("ColumnIndex", &TableColSortCol)
        .def_property_readonly("SortDirection", &TableColSortDir);

    py::class_<ImGuiTableSortSpecs>(m, "TableSortSpecs")
        .def_readonly("SpecsCount", &ImGuiTableSortSpecs::SpecsCount)
        .def_readonly("SpecsDirty", &ImGuiTableSortSpecs::SpecsDirty)
        .def_property_readonly("Specs", [](const ImGuiTableSortSpecs& self) {
            return py::cast(self.Specs, py::return_value_policy::reference);
        });

    PY4GW::imgui_bindings::register_style(m);
    // ═══════════════ WINDOW ═════════════════════════════════════
    // Three begin overloads:
    //   begin(name)                -> bool               (no close button)
    //   begin(name, flags)         -> bool               (no close button, with flags)
    //   begin(name, p_open, flags) -> (bool, bool)       (close button: visible, still_open)
    m.def("begin", [](const char* name, bool dockable) -> bool {
        return ImGui::Begin(name, nullptr, DefaultWindowFlags(0, dockable));
    }, py::arg("name"), py::arg("dockable") = false);
    m.def("begin", [](const char* name, int flags, bool dockable) -> bool {
        return ImGui::Begin(name, nullptr, DefaultWindowFlags(flags, dockable));
    }, py::arg("name"), py::arg("flags"), py::arg("dockable") = false);
    m.def("begin", [](const char* name, py::object p_open, int flags, bool dockable) -> py::tuple {
        if (p_open.is_none()) {
            bool vis = ImGui::Begin(name, nullptr, DefaultWindowFlags(flags, dockable));
            return py::make_tuple(vis, true);
        }
        bool open = p_open.cast<bool>();
        bool vis = ImGui::Begin(name, &open, DefaultWindowFlags(flags, dockable));
        return py::make_tuple(vis, open);
    }, py::arg("name"), py::arg("p_open") = py::none(), py::arg("flags") = 0, py::arg("dockable") = false);
    // Legacy explicit close-button window (restored). Returns (visible, still_open).
    m.def("begin_with_close", [](const char* name, bool p_open, int flags) -> py::tuple {
        bool open = p_open; bool vis = ImGui::Begin(name, &open, DefaultWindowFlags(flags)); return py::make_tuple(vis, open);
    }, py::arg("name"), py::arg("p_open"), py::arg("flags") = 0);
    m.def("end", &ImGui::End);

    // Current ImGui requires a matching EndChild() for EVERY BeginChild() call,
    // regardless of its return value (older ImGui only needed it when the child
    // was visible). Legacy Python uses `if begin_child(): ...; end_child()`, which
    // skips end_child() when BeginChild returns false (collapsed/clipped parent) —
    // corrupting the child stack. Always return true so the paired end_child()
    // always runs; ImGui still clips content that isn't visible.
    m.def("begin_child", [](const char* id, const ImVec2& size, ImGuiChildFlags border, ImGuiWindowFlags flags) -> bool {
        ImGui::BeginChild(id, size, border, flags);
        return true;
    }, py::arg("id"), py::arg("size") = ImVec2(0,0), py::arg("border") = 0, py::arg("flags") = 0);  // legacy kwarg names id/size/border/flags
    m.def("end_child", &ImGui::EndChild);
    m.def("begin_group", &ImGui::BeginGroup); m.def("end_group", &ImGui::EndGroup);
    m.def("begin_disabled", &ImGui::BeginDisabled, py::arg("disabled") = true);
    m.def("end_disabled", &ImGui::EndDisabled);

    // ═══════════════ WINDOW SETUP ═══════════════════════════════
    m.def("set_next_window_pos", [](float x, float y, ImGuiCond cond){ ImGui::SetNextWindowPos(ImVec2(x, y), cond); },
          py::arg("x"), py::arg("y"), py::arg("cond") = 0);  // legacy two-float overload
    m.def("set_next_window_pos", &ImGui::SetNextWindowPos, py::arg("pos"), py::arg("cond") = 0, py::arg("pivot") = ImVec2(0,0));
    m.def("set_next_window_size", [](float w, float h, ImGuiCond cond){ ImGui::SetNextWindowSize(ImVec2(w, h), cond); },
          py::arg("width"), py::arg("height"), py::arg("cond") = 0);  // legacy two-float overload
    m.def("set_next_window_size", &ImGui::SetNextWindowSize, py::arg("size"), py::arg("cond") = 0);
    m.def("set_next_window_size_constraints", [](const ImVec2& min, const ImVec2& max) { ImGui::SetNextWindowSizeConstraints(min, max); },
          py::arg("size_min"), py::arg("size_max"));
    m.def("set_next_window_content_size", &ImGui::SetNextWindowContentSize, py::arg("size"));
    m.def("set_next_window_collapsed", &ImGui::SetNextWindowCollapsed, py::arg("collapsed"), py::arg("cond") = 0);
    m.def("set_next_window_focus", &ImGui::SetNextWindowFocus);
    m.def("set_next_window_bg_alpha", &ImGui::SetNextWindowBgAlpha, py::arg("alpha"));
    m.def("set_next_window_scroll", &ImGui::SetNextWindowScroll, py::arg("scroll"));
    m.def("set_next_window_viewport", &ImGui::SetNextWindowViewport, py::arg("viewport_id"));
    m.def(
        "set_next_window_detached",
        &SetNextWindowDetached,
        py::arg("detached") = true,
        py::arg("no_taskbar_icon") = false,
        py::arg("no_decoration") = false,
        py::arg("top_level") = true
    );
    m.def("set_next_window_main_viewport", &SetNextWindowMainViewport);

    m.def("set_window_pos", [](float x, float y, ImGuiCond cond){ ImGui::SetWindowPos(ImVec2(x, y), cond); },
          py::arg("x"), py::arg("y"), py::arg("cond") = 0);  // legacy three-scalar form
    m.def("set_window_pos", py::overload_cast<const ImVec2&, ImGuiCond>(&ImGui::SetWindowPos), py::arg("pos"), py::arg("cond") = 0);
    m.def("set_window_size", [](float w, float h, ImGuiCond cond){ ImGui::SetWindowSize(ImVec2(w, h), cond); },
          py::arg("width"), py::arg("height"), py::arg("cond") = 0);  // legacy three-scalar form
    m.def("set_window_size", py::overload_cast<const ImVec2&, ImGuiCond>(&ImGui::SetWindowSize), py::arg("size"), py::arg("cond") = 0);
    m.def("set_window_collapsed", py::overload_cast<bool, ImGuiCond>(&ImGui::SetWindowCollapsed), py::arg("collapsed"), py::arg("cond") = 0);
    m.def("set_window_focus", py::overload_cast<const char*>(&ImGui::SetWindowFocus), py::arg("name"));

    // ═══════════════ WINDOW QUERY ═══════════════════════════════
    m.def("get_window_pos", &ImGui::GetWindowPos);
    m.def("get_window_size", &ImGui::GetWindowSize);
    m.def("get_window_width", &ImGui::GetWindowWidth);
    m.def("get_window_height", &ImGui::GetWindowHeight);
    m.def("get_content_region_avail", &ImGui::GetContentRegionAvail);
    // ImGui 1.92 removed GetWindowContentRegionMin/Max from the public API. Restored using public API
    // only (avoiding imgui_internal.h, which adds internal overloads that make &ImGui::Func ambiguous
    // for the other bindings in this TU). Equivalent when queried at the content start — which is how
    // these were used (e.g. the pathing-map renderer calls them right after begin_child): the content
    // region min is the initial cursor position (window-local), and max is that plus the avail size.
    m.def("get_window_content_region_min", []() -> std::pair<float, float> {
        ImVec2 start = ImGui::GetCursorStartPos();
        return { start.x, start.y };
    });
    m.def("get_window_content_region_max", []() -> std::pair<float, float> {
        ImVec2 start = ImGui::GetCursorStartPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        return { start.x + avail.x, start.y + avail.y };
    });
    m.def("is_window_appearing", &ImGui::IsWindowAppearing);
    m.def("is_window_collapsed", &ImGui::IsWindowCollapsed);
    m.def("is_window_focused", &ImGui::IsWindowFocused, py::arg("flags") = 0);
    m.def("is_window_hovered", &ImGui::IsWindowHovered, py::arg("flags") = 0);
    m.def("is_rect_visible", py::overload_cast<const ImVec2&>(&ImGui::IsRectVisible), py::arg("size"));

    // ═══════════════ LAYOUT ═════════════════════════════════════
    m.def("separator", &ImGui::Separator);
    m.def("separator_text", [](const char* label) { ImGui::SeparatorText(label); }, py::arg("label"));
    m.def("same_line", &ImGui::SameLine, py::arg("offset_from_start_x") = 0.0f, py::arg("spacing") = -1.0f);
    m.def("spacing", &ImGui::Spacing);
    m.def("new_line", &ImGui::NewLine);
    m.def("dummy", &ImGui::Dummy, py::arg("size"));
    m.def("indent", &ImGui::Indent, py::arg("indent_w") = 0.0f);
    m.def("unindent", &ImGui::Unindent, py::arg("indent_w") = 0.0f);
    m.def("align_text_to_frame_padding", &ImGui::AlignTextToFramePadding);
    m.def("get_frame_height", &ImGui::GetFrameHeight);
    m.def("get_frame_height_with_spacing", &ImGui::GetFrameHeightWithSpacing);
    m.def("get_font_size", &ImGui::GetFontSize);
    // ImGui 1.92: fonts are dynamic. A FontId resolves to one of 4 style fonts
    // (each with Font Awesome merged) rendered at its designed size via
    // PushFont(font, size). See FontManager.
    m.def("push_font", [](int idx) {
        using PY4GW::imgui::FontId;
        auto& mgr = PY4GW::imgui::FontManager::Instance();
        if (idx >= 0 && idx < static_cast<int>(FontId::Count)) {
            const auto id = static_cast<FontId>(idx);
            ImGui::PushFont(mgr.Get(id), mgr.GetSize(id));
        } else {
            ImGui::PushFont(nullptr, 0.0f);
        }
    }, py::arg("font_index") = 0);
    m.def("pop_font", &ImGui::PopFont);
    // Font-scaling entry points (used by ImGui.py push_font/pop_font). Renders the
    // FontId's style font at (designed size * scale). Each pushes/pops exactly one
    // font-stack entry (balanced).
    m.def("push_font_scaled", [](int idx, float scale) {
        using PY4GW::imgui::FontId;
        auto& mgr = PY4GW::imgui::FontManager::Instance();
        if (idx >= 0 && idx < static_cast<int>(FontId::Count)) {
            const auto id = static_cast<FontId>(idx);
            ImGui::PushFont(mgr.Get(id), mgr.GetSize(id) * scale);
        } else {
            // Scale the CURRENT font by its UNSCALED base size. GetFontSize()
            // returns the already-globally-scaled value, so feeding it back into
            // PushFont() would re-apply FontScaleMain/FontScaleDpi (double scale).
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * scale);
        }
    }, py::arg("font_index"), py::arg("scale") = 1.0f);
    m.def("pop_font_scaled", &ImGui::PopFont);

    // ImGui 1.92 dynamic-font entry points. Fonts now scale continuously, so
    // these render any style at any pixel size instead of being limited to the
    // fixed 24-id (4 style x 6 size) table above. All balance with pop_font.
    //
    // Push a style font (0=Regular,1=Bold,2=Italic,3=BoldItalic) at an absolute
    // pixel size. size <= 0 keeps the current shared size.
    m.def("push_style_font", [](int style, float size) {
        auto& mgr = PY4GW::imgui::FontManager::Instance();
        ImGui::PushFont(mgr.GetStyleFont(style), size > 0.0f ? size : 0.0f);
    }, py::arg("style"), py::arg("size") = 0.0f);
    // Re-scale the CURRENT font to an absolute base size (before global factors).
    m.def("push_font_size", [](float size) {
        ImGui::PushFont(nullptr, size);
    }, py::arg("size"));
    // Global font scale = style.FontScaleMain, the 1.92 replacement for the
    // obsolete io.FontGlobalScale / SetWindowFontScale. Scales every window.
    m.def("get_global_font_scale", []() { return ImGui::GetStyle().FontScaleMain; });
    m.def("set_global_font_scale", [](float scale) { ImGui::GetStyle().FontScaleMain = scale; },
          py::arg("scale"));

    // ═══════════════ TEXT ═══════════════════════════════════════
    m.def("text_unformatted", &ImGui::TextUnformatted, py::arg("text"), py::arg("text_end") = nullptr);
    m.def("text_link", [](const char* label) -> bool { return ImGui::TextLink(label); }, py::arg("label"));
    m.def("text_link_open_url", [](const char* label, const char* url) -> bool {
        return ImGui::TextLinkOpenURL(label, url);
    }, py::arg("label"), py::arg("url") = nullptr);

    m.def("TextV", &TextV, py::arg("fmt"), py::arg("args"));
    m.def("TextColoredV", &TextColoredV, py::arg("color"), py::arg("fmt"), py::arg("args"));
    m.def("TextDisabledV", &TextDisabledV, py::arg("fmt"), py::arg("args"));
    m.def("TextWrappedV", &TextWrappedV, py::arg("fmt"), py::arg("args"));
    m.def("LabelTextV", &LabelTextV, py::arg("label"), py::arg("fmt"), py::arg("args"));
    m.def("BulletTextV", &BulletTextV, py::arg("fmt"), py::arg("args"));

    m.def("text", [](const char* t) { ImGui::TextUnformatted(t); }, py::arg("text"));
    m.def("text_colored", [](const ImVec4& col, const char* t) { ImGui::TextColored(col, "%s", t); },
          py::arg("color"), py::arg("text"));
    m.def("text_colored", [](float r, float g, float b, float a, const char* t) { ImGui::TextColored(ImVec4(r,g,b,a), "%s", t); },
          py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"), py::arg("text"));
    // Legacy (text, color) overload — routes to the (color, text) form.
    m.def("text_colored", [](const char* t, const ImVec4& col) { ImGui::TextColored(col, "%s", t); },
          py::arg("text"), py::arg("color"));
    m.def("text_disabled", [](const char* t) { ImGui::TextDisabled("%s", t); }, py::arg("text"));
    m.def("text_wrapped", [](const char* t) { ImGui::TextWrapped("%s", t); }, py::arg("text"));
    m.def("bullet_text", [](const char* t) { ImGui::BulletText("%s", t); }, py::arg("text"));
    m.def("label_text", [](const char* label, const char* t) { ImGui::LabelText(label, "%s", t); }, py::arg("label"), py::arg("text"));

    // ═══════════════ WIDGETS ════════════════════════════════════
    m.def("button", &ImGui::Button, py::arg("label"), py::arg("size") = ImVec2(0,0));
    m.def("button", [](const char* label, float w, float h) -> bool { return ImGui::Button(label, ImVec2(w,h)); },
          py::arg("label"), py::arg("width") = 0.0f, py::arg("height") = 0.0f);
    m.def("small_button", &ImGui::SmallButton, py::arg("label"));
    m.def("invisible_button", [](const char* str_id, const ImVec2& size, ImGuiButtonFlags flags) -> bool {
        return ImGui::InvisibleButton(str_id, size, flags);
    }, py::arg("str_id"), py::arg("size"), py::arg("flags") = 0);
    m.def("arrow_button", &ImGui::ArrowButton, py::arg("str_id"), py::arg("dir"));
    m.def("checkbox", [](const char* label, bool v) -> bool { ImGui::Checkbox(label, &v); return v; }, py::arg("label"), py::arg("value"));
    m.def("radio_button", [](const char* label, int v, int btn) -> int {
        if (ImGui::RadioButton(label, v == btn)) v = btn; return v;
    }, py::arg("label"), py::arg("value"), py::arg("v_button"));
    // Legacy took loose floats (fraction, size_x, size_y, overlay); the library calls it
    // with 4 positional args. Reforged had a single ImVec2-size binding that rejected them.
    m.def("progress_bar", [](float fraction, float size_x, float size_y, const char* overlay){
        ImGui::ProgressBar(fraction, ImVec2(size_x, size_y), (overlay && overlay[0]) ? overlay : nullptr);
    }, py::arg("fraction"), py::arg("size_arg_x") = -1.0f, py::arg("size_arg_y") = 0.0f, py::arg("overlay") = nullptr);
    m.def("bullet", &ImGui::Bullet);
    m.def("checkbox_flags", [](const char* label, int flags, int flags_value) -> int {
        ImGui::CheckboxFlags(label, &flags, flags_value);
        return flags;
    }, py::arg("label"), py::arg("flags"), py::arg("flags_value"));

    // ── Sliders ──
    m.def("slider_float", [](const char* label, float v, float mn, float mx, const char* fmt, int flags) -> float {
        ImGui::SliderFloat(label, &v, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("slider_int", [](const char* label, int v, int mn, int mx, const char* fmt, int flags) -> int {
        ImGui::SliderInt(label, &v, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("slider_angle", [](const char* label, float v_rad, float v_deg_min, float v_deg_max, const char* fmt, int flags) -> float {
        ImGui::SliderAngle(label, &v_rad, v_deg_min, v_deg_max, fmt, flags); return v_rad;
    }, py::arg("label"), py::arg("v_rad"), py::arg("v_degrees_min") = -360.0f, py::arg("v_degrees_max") = 360.0f, py::arg("format") = "%.0f deg", py::arg("flags") = 0);
    m.def("v_slider_float", [](const char* label, const ImVec2& size, float v, float mn, float mx, const char* fmt, int flags) -> float {
        ImGui::VSliderFloat(label, size, &v, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("size"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("v_slider_int", [](const char* label, const ImVec2& size, int v, int mn, int mx, const char* fmt, int flags) -> int {
        ImGui::VSliderInt(label, size, &v, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("size"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%d", py::arg("flags") = 0);

    m.def("slider_float2", [](const char* label, const std::array<float,2>& vin, float mn, float mx, const char* fmt, int flags) -> std::array<float,2> { auto v=vin; ImGui::SliderFloat2(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("slider_float3", [](const char* label, const std::array<float,3>& vin, float mn, float mx, const char* fmt, int flags) -> std::array<float,3> { auto v=vin; ImGui::SliderFloat3(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("slider_float4", [](const char* label, const std::array<float,4>& vin, float mn, float mx, const char* fmt, int flags) -> std::array<float,4> { auto v=vin; ImGui::SliderFloat4(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("slider_int2",   [](const char* label, const std::array<int,2>& vin, int mn, int mx, const char* fmt, int flags) -> std::array<int,2> { auto v=vin; ImGui::SliderInt2(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("slider_int3",   [](const char* label, const std::array<int,3>& vin, int mn, int mx, const char* fmt, int flags) -> std::array<int,3> { auto v=vin; ImGui::SliderInt3(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("slider_int4",   [](const char* label, const std::array<int,4>& vin, int mn, int mx, const char* fmt, int flags) -> std::array<int,4> { auto v=vin; ImGui::SliderInt4(label, v.data(), mn, mx, fmt, flags); return v; }, py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"), py::arg("format") = "%d", py::arg("flags") = 0);

    m.def("drag_float", [](const char* label, float v, float speed, float mn, float mx, const char* fmt, int flags) -> float {
        ImGui::DragFloat(label, &v, speed, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0.0f, py::arg("v_max") = 0.0f, py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("drag_float_range2", [](const char* label, float v_min_cur, float v_max_cur, float speed, float mn, float mx, const char* fmt, const char* fmt_max, int flags) -> py::tuple {
        ImGui::DragFloatRange2(label, &v_min_cur, &v_max_cur, speed, mn, mx, fmt, fmt_max, flags);
        return py::make_tuple(v_min_cur, v_max_cur);
    }, py::arg("label"), py::arg("v_current_min"), py::arg("v_current_max"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0.0f, py::arg("v_max") = 0.0f, py::arg("format") = "%.3f", py::arg("format_max") = nullptr, py::arg("flags") = 0);
    m.def("drag_int", [](const char* label, int v, float speed, int mn, int mx, const char* fmt, int flags) -> int {
        ImGui::DragInt(label, &v, speed, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0, py::arg("v_max") = 0, py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("drag_int_range2", [](const char* label, int v_min_cur, int v_max_cur, float speed, int mn, int mx, const char* fmt, const char* fmt_max, int flags) -> py::tuple {
        ImGui::DragIntRange2(label, &v_min_cur, &v_max_cur, speed, mn, mx, fmt, fmt_max, flags);
        return py::make_tuple(v_min_cur, v_max_cur);
    }, py::arg("label"), py::arg("v_current_min"), py::arg("v_current_max"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0, py::arg("v_max") = 0, py::arg("format") = "%d", py::arg("format_max") = nullptr, py::arg("flags") = 0);
    m.def("drag_float2", [](const char* label, const std::array<float,2>& vin, float s, float mn, float mx, const char* fmt, int flags) -> std::array<float,2> { auto v=vin; ImGui::DragFloat2(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0.0f, py::arg("v_max") = 0.0f, py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("drag_float3", [](const char* label, const std::array<float,3>& vin, float s, float mn, float mx, const char* fmt, int flags) -> std::array<float,3> { auto v=vin; ImGui::DragFloat3(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0.0f, py::arg("v_max") = 0.0f, py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("drag_float4", [](const char* label, const std::array<float,4>& vin, float s, float mn, float mx, const char* fmt, int flags) -> std::array<float,4> { auto v=vin; ImGui::DragFloat4(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0.0f, py::arg("v_max") = 0.0f, py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("drag_int2", [](const char* label, const std::array<int,2>& vin, float s, int mn, int mx, const char* fmt, int flags) -> std::array<int,2> { auto v=vin; ImGui::DragInt2(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0, py::arg("v_max") = 0, py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("drag_int3", [](const char* label, const std::array<int,3>& vin, float s, int mn, int mx, const char* fmt, int flags) -> std::array<int,3> { auto v=vin; ImGui::DragInt3(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0, py::arg("v_max") = 0, py::arg("format") = "%d", py::arg("flags") = 0);
    m.def("drag_int4", [](const char* label, const std::array<int,4>& vin, float s, int mn, int mx, const char* fmt, int flags) -> std::array<int,4> { auto v=vin; ImGui::DragInt4(label, v.data(), s, mn, mx, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("v_speed") = 1.0f, py::arg("v_min") = 0, py::arg("v_max") = 0, py::arg("format") = "%d", py::arg("flags") = 0);

    // ═══════════════ INPUT ══════════════════════════════════════
    m.def("input_float", [](const char* label, float v, float step, float step_fast, const char* fmt, int flags) -> float {
        ImGui::InputFloat(label, &v, step, step_fast, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("step") = 0.0f, py::arg("step_fast") = 0.0f, py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("input_float2", [](const char* label, const std::array<float,2>& vin, const char* fmt, int flags) -> std::array<float,2> { auto v=vin; ImGui::InputFloat2(label, v.data(), fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("input_float3", [](const char* label, const std::array<float,3>& vin, const char* fmt, int flags) -> std::array<float,3> { auto v=vin; ImGui::InputFloat3(label, v.data(), fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("input_float4", [](const char* label, const std::array<float,4>& vin, const char* fmt, int flags) -> std::array<float,4> { auto v=vin; ImGui::InputFloat4(label, v.data(), fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("format") = "%.3f", py::arg("flags") = 0);
    m.def("input_int", [](const char* label, int v, int step, int step_fast, int flags) -> int {
        ImGui::InputInt(label, &v, step, step_fast, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("step") = 1, py::arg("step_fast") = 100, py::arg("flags") = 0);
    m.def("input_int2", [](const char* label, const std::array<int,2>& vin, int flags) -> std::array<int,2> { auto v=vin; ImGui::InputInt2(label, v.data(), flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("flags") = 0);
    m.def("input_int3", [](const char* label, const std::array<int,3>& vin, int flags) -> std::array<int,3> { auto v=vin; ImGui::InputInt3(label, v.data(), flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("flags") = 0);
    m.def("input_int4", [](const char* label, const std::array<int,4>& vin, int flags) -> std::array<int,4> { auto v=vin; ImGui::InputInt4(label, v.data(), flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("flags") = 0);
    m.def("input_double", [](const char* label, double v, double step, double step_fast, const char* fmt, int flags) -> double {
        ImGui::InputDouble(label, &v, step, step_fast, fmt, flags); return v;
    }, py::arg("label"), py::arg("v"), py::arg("step") = 0.0, py::arg("step_fast") = 0.0, py::arg("format") = "%.6f", py::arg("flags") = 0);
    m.def("input_text", [](const char* label, const std::string& text, int flags) -> std::string {
        std::vector<char> buf(256, 0);
        // Legacy truncated to 256; copy at most 255 bytes (+ NUL). An unbounded copy here
        // overflowed the fixed 256-byte buffer for long persisted strings (heap corruption).
        std::copy_n(text.begin(), std::min<size_t>(text.size(), 255), buf.begin());
        ImGui::InputText(label, buf.data(), buf.size(), flags);
        return std::string(buf.data());
    }, py::arg("label"), py::arg("text") = "", py::arg("flags") = 0);
    m.def("input_text_with_hint", [](const char* label, const char* hint, const std::string& text, int flags) -> std::string {
        std::vector<char> buf(256);
        if(!text.empty()) std::copy(text.begin(), text.end(), buf.begin());
        ImGui::InputTextWithHint(label, hint, buf.data(), buf.size(), flags);
        return std::string(buf.data());
    }, py::arg("label"), py::arg("hint"), py::arg("text") = "", py::arg("flags") = 0);
    m.def("input_text_multiline", [](const char* label, const std::string& text, const ImVec2& size, int flags) -> std::string {
        std::vector<char> buf(2048);
        if(!text.empty()) std::copy(text.begin(), text.end(), buf.begin());
        ImGui::InputTextMultiline(label, buf.data(), buf.size(), size, flags);
        return std::string(buf.data());
    }, py::arg("label"), py::arg("text") = "", py::arg("size") = ImVec2(0,0), py::arg("flags") = 0);

    // ═══════════════ COMBO / LIST BOX ═══════════════════════════
    m.def("begin_combo", &ImGui::BeginCombo, py::arg("label"), py::arg("preview_value"), py::arg("flags") = 0);
    m.def("end_combo", &ImGui::EndCombo);
    m.def("combo", [](const char* label, int cur, const std::vector<std::string>& items) -> int {
        std::vector<const char*> ptrs; for(auto& s:items) ptrs.push_back(s.c_str());
        ImGui::Combo(label, &cur, ptrs.data(), (int)ptrs.size());
        return cur;
    }, py::arg("label"), py::arg("current_item"), py::arg("items"));
    m.def("begin_list_box", &ImGui::BeginListBox, py::arg("label"), py::arg("size") = ImVec2(0,0));
    m.def("end_list_box", &ImGui::EndListBox);
    m.def("list_box", [](const char* label, int cur, const std::vector<std::string>& items, int height) -> int {
        std::vector<const char*> ptrs; for(auto& s:items) ptrs.push_back(s.c_str());
        ImGui::ListBox(label, &cur, ptrs.data(), (int)ptrs.size(), height);
        return cur;
    }, py::arg("label"), py::arg("current_item"), py::arg("items"), py::arg("height_in_items") = -1);

    // ═══════════════ SELECTABLE ═════════════════════════════════
    m.def("selectable", py::overload_cast<const char*, bool, ImGuiSelectableFlags, const ImVec2&>(&ImGui::Selectable),
          py::arg("label"), py::arg("selected") = false, py::arg("flags") = 0, py::arg("size") = ImVec2(0,0));

    // ═══════════════ COLOR ══════════════════════════════════════
    m.def("color_edit3", [](const char* label, const std::array<float,3>& cin, int flags) -> std::array<float,3> { auto c=cin; ImGui::ColorEdit3(label, c.data(), flags); return c; },
          py::arg("label"), py::arg("col"), py::arg("flags") = 0);
    m.def("color_edit4", [](const char* label, const std::array<float,4>& cin, int flags) -> std::array<float,4> { auto c=cin; ImGui::ColorEdit4(label, c.data(), flags); return c; },
          py::arg("label"), py::arg("col"), py::arg("flags") = 0);
    m.def("color_picker3", [](const char* label, const std::array<float,3>& cin, int flags) -> std::array<float,3> { auto c=cin; ImGui::ColorPicker3(label, c.data(), flags); return c; },
          py::arg("label"), py::arg("col"), py::arg("flags") = 0);
    m.def("color_picker4", [](const char* label, const std::array<float,4>& cin, int flags) -> std::array<float,4> { auto c=cin; ImGui::ColorPicker4(label, c.data(), flags); return c; },
          py::arg("label"), py::arg("col"), py::arg("flags") = 0);
    m.def("color_button", &ImGui::ColorButton,
          py::arg("desc_id"), py::arg("col"), py::arg("flags") = 0, py::arg("size") = ImVec2(0,0));
    m.def("set_color_edit_options", &ImGui::SetColorEditOptions, py::arg("flags"));
    m.def("color_convert_u32_to_float4", &ImGui::ColorConvertU32ToFloat4, py::arg("in"));
    m.def("color_convert_float4_to_u32", &ImGui::ColorConvertFloat4ToU32, py::arg("in"));
    m.def("color_convert_rgb_to_hsv", [](float r, float g, float b) -> py::tuple {
        float h = 0.0f, s = 0.0f, v = 0.0f;
        ImGui::ColorConvertRGBtoHSV(r, g, b, h, s, v);
        return py::make_tuple(h, s, v);
    }, py::arg("r"), py::arg("g"), py::arg("b"));
    m.def("color_convert_hsv_to_rgb", [](float h, float s, float v) -> py::tuple {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
        return py::make_tuple(r, g, b);
    }, py::arg("h"), py::arg("s"), py::arg("v"));
    m.def("get_color_u32", py::overload_cast<ImGuiCol, float>(&ImGui::GetColorU32), py::arg("idx"), py::arg("alpha_mul") = 1.0f);
    m.def("get_color_u32_vec4", py::overload_cast<const ImVec4&>(&ImGui::GetColorU32), py::arg("col"));
    m.def("get_style_color_vec4", &ImGui::GetStyleColorVec4, py::arg("idx"));

    // ═══════════════ IMAGE ══════════════════════════════════════
    m.def("image", [](ImTextureID tex, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1) {
        ImGui::Image(tex, size, uv0, uv1);
    }, py::arg("tex_id"), py::arg("size"), py::arg("uv0") = ImVec2(0,0), py::arg("uv1") = ImVec2(1,1));
    m.def("image_with_bg", [](ImTextureID tex, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col) {
        ImGui::ImageWithBg(tex, size, uv0, uv1, bg_col, tint_col);
    }, py::arg("tex_id"), py::arg("size"), py::arg("uv0") = ImVec2(0,0), py::arg("uv1") = ImVec2(1,1), py::arg("bg_col") = ImVec4(0,0,0,0), py::arg("tint_col") = ImVec4(1,1,1,1));
    m.def("image_button", [](const char* str_id, ImTextureID tex, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col) -> bool {
        return ImGui::ImageButton(str_id, tex, size, uv0, uv1, bg_col, tint_col);
    }, py::arg("str_id"), py::arg("tex_id"), py::arg("size"), py::arg("uv0") = ImVec2(0,0), py::arg("uv1") = ImVec2(1,1), py::arg("bg_col") = ImVec4(0,0,0,0), py::arg("tint_col") = ImVec4(1,1,1,1));

    // ═══════════════ TREE / COLLAPSING ══════════════════════════
    m.def("tree_node", py::overload_cast<const char*>(&ImGui::TreeNode), py::arg("label"));
    m.def("tree_node_ex", py::overload_cast<const char*, ImGuiTreeNodeFlags>(&ImGui::TreeNodeEx),
          py::arg("label"), py::arg("flags") = 0);
    m.def("tree_pop", &ImGui::TreePop);
    m.def("tree_push", py::overload_cast<const char*>(&ImGui::TreePush), py::arg("str_id"));
    m.def("tree_push_ptr", py::overload_cast<const void*>(&ImGui::TreePush), py::arg("ptr_id"));
    m.def("get_tree_node_to_label_spacing", &ImGui::GetTreeNodeToLabelSpacing);
    m.def("set_next_item_open", &ImGui::SetNextItemOpen, py::arg("is_open"), py::arg("cond") = 0);
    m.def("set_next_item_storage_id", &ImGui::SetNextItemStorageID, py::arg("storage_id"));
    m.def("tree_node_get_open", &ImGui::TreeNodeGetOpen, py::arg("storage_id"));
    m.def("collapsing_header", py::overload_cast<const char*, ImGuiTreeNodeFlags>(&ImGui::CollapsingHeader),
          py::arg("label"), py::arg("flags") = 0);

    // ═══════════════ TABS ═══════════════════════════════════════
    m.def("begin_tab_bar", &ImGui::BeginTabBar, py::arg("str_id"), py::arg("flags") = 0);
    m.def("end_tab_bar", &ImGui::EndTabBar);
    m.def("begin_tab_item", &ImGui::BeginTabItem, py::arg("label"), py::arg("p_open") = nullptr, py::arg("flags") = 0);
    m.def("begin_tab_item_closable", [](const char* label, bool p_open, int flags) -> py::tuple {
        bool open = p_open; bool vis = ImGui::BeginTabItem(label, &open, flags); return py::make_tuple(vis, open);
    }, py::arg("label"), py::arg("p_open") = true, py::arg("flags") = 0);
    m.def("end_tab_item", &ImGui::EndTabItem);
    m.def("tab_item_button", &ImGui::TabItemButton, py::arg("label"), py::arg("flags") = 0);
    m.def("set_tab_item_closed", &ImGui::SetTabItemClosed, py::arg("tab_or_docked_window_label"));

    // ═══════════════ TABLES ═════════════════════════════════════
    m.def("begin_table", &ImGui::BeginTable,
          py::arg("str_id"), py::arg("column"), py::arg("flags") = 0,
          py::arg("outer_size") = ImVec2(0,0), py::arg("inner_width") = 0.0f);
    // Legacy (id, cols, flags, width, height) overload — packs scalars into outer_size.
    m.def("begin_table", [](const char* id, int cols, ImGuiTableFlags flags, float width, float height) -> bool {
        return ImGui::BeginTable(id, cols, flags, ImVec2(width, height));
    }, py::arg("str_id"), py::arg("column"), py::arg("flags") = 0,
       py::arg("width") = 0.0f, py::arg("height") = 0.0f);
    m.def("end_table", &ImGui::EndTable);
    m.def("table_next_row", &ImGui::TableNextRow, py::arg("row_flags") = 0, py::arg("min_row_height") = 0.0f);
    m.def("table_next_column", &ImGui::TableNextColumn);
    m.def("table_set_column_index", &ImGui::TableSetColumnIndex, py::arg("column_n"));
    m.def("table_setup_column", &ImGui::TableSetupColumn,
          py::arg("label"), py::arg("flags") = 0, py::arg("init_width_or_weight") = 0.0f, py::arg("user_id") = 0);
    m.def("table_setup_scroll_freeze", &ImGui::TableSetupScrollFreeze, py::arg("cols"), py::arg("rows"));
    m.def("table_headers_row", &ImGui::TableHeadersRow);
    m.def("table_header", &ImGui::TableHeader, py::arg("label"));
    m.def("table_angled_headers_row", &ImGui::TableAngledHeadersRow);
    m.def("table_get_column_count", &ImGui::TableGetColumnCount);
    m.def("table_get_column_index", &ImGui::TableGetColumnIndex);
    m.def("table_get_row_index", &ImGui::TableGetRowIndex);
    m.def("table_get_column_name", &ImGui::TableGetColumnName, py::arg("column_n") = -1);
    m.def("table_get_column_flags", &ImGui::TableGetColumnFlags, py::arg("column_n") = -1);
    m.def("table_get_hovered_column", &ImGui::TableGetHoveredColumn);
    m.def("table_set_column_enabled", &ImGui::TableSetColumnEnabled, py::arg("column_n"), py::arg("v"));
    m.def("table_set_bg_color", &ImGui::TableSetBgColor, py::arg("target"), py::arg("color"), py::arg("column_n") = -1);
    m.def("table_get_sort_specs", &ImGui::TableGetSortSpecs, py::return_value_policy::reference);
    m.def("clear_sort_specs_dirty", &ClearSortSpecsDirty);

    // Legacy columns
    m.def("columns", &ImGui::Columns, py::arg("count") = 1, py::arg("id") = nullptr, py::arg("borders") = true);
    m.def("next_column", &ImGui::NextColumn);
    m.def("end_columns", []{ ImGui::Columns(1); });  // legacy ImGui_EndColumns: reset to a single column
    m.def("set_column_width", &ImGui::SetColumnWidth, py::arg("column_index"), py::arg("width"));
    m.def("set_column_offset", &ImGui::SetColumnOffset, py::arg("column_index"), py::arg("offset_x"));
    m.def("get_column_index", &ImGui::GetColumnIndex);
    m.def("get_column_width", &ImGui::GetColumnWidth, py::arg("column_index") = -1);
    m.def("get_column_offset", &ImGui::GetColumnOffset, py::arg("column_index") = -1);
    m.def("get_columns_count", &ImGui::GetColumnsCount);

    // ═══════════════ MENUS ══════════════════════════════════════
    m.def("begin_menu_bar", &ImGui::BeginMenuBar);
    m.def("end_menu_bar", &ImGui::EndMenuBar);
    m.def("begin_main_menu_bar", &ImGui::BeginMainMenuBar);
    m.def("end_main_menu_bar", &ImGui::EndMainMenuBar);
    m.def("begin_menu", &ImGui::BeginMenu, py::arg("label"), py::arg("enabled") = true);
    m.def("end_menu", &ImGui::EndMenu);
    m.def("menu_item", py::overload_cast<const char*, const char*, bool, bool>(&ImGui::MenuItem),
          py::arg("label"), py::arg("shortcut") = nullptr, py::arg("selected") = false, py::arg("enabled") = true);

    // ═══════════════ POPUPS / TOOLTIPS ══════════════════════════
    m.def("open_popup", py::overload_cast<const char*, ImGuiPopupFlags>(&ImGui::OpenPopup), py::arg("str_id"), py::arg("popup_flags") = 0);
    m.def("open_popup_on_item_click", &ImGui::OpenPopupOnItemClick, py::arg("str_id") = nullptr, py::arg("popup_flags") = 0);
    m.def("begin_popup", &ImGui::BeginPopup, py::arg("str_id"), py::arg("flags") = 0);
    m.def("end_popup", &ImGui::EndPopup);
    m.def("end_popup_modal", []() { ImGui::EndPopup(); });
    m.def("begin_popup_modal", &ImGui::BeginPopupModal, py::arg("name"), py::arg("p_open") = nullptr, py::arg("flags") = 0);
    m.def("close_current_popup", &ImGui::CloseCurrentPopup);
    m.def("begin_popup_context_item", &ImGui::BeginPopupContextItem, py::arg("str_id") = nullptr, py::arg("popup_flags") = 0);
    m.def("begin_popup_context_window", &ImGui::BeginPopupContextWindow, py::arg("str_id") = nullptr, py::arg("popup_flags") = 0);
    m.def("begin_popup_context_void", &ImGui::BeginPopupContextVoid, py::arg("str_id") = nullptr, py::arg("popup_flags") = 0);
    m.def("is_popup_open", &ImGui::IsPopupOpen, py::arg("str_id"), py::arg("flags") = 0);
    m.def("begin_tooltip", &ImGui::BeginTooltip);
    m.def("end_tooltip", &ImGui::EndTooltip);
    m.def("set_tooltip", [](const char* fmt) { ImGui::SetTooltip("%s", fmt); }, py::arg("fmt"));
    m.def("show_tooltip", [](const char* text) {
        if(ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::TextUnformatted(text); ImGui::EndTooltip(); }
    }, py::arg("text"));
    m.def("begin_item_tooltip", &ImGui::BeginItemTooltip);
    m.def("set_item_tooltip", [](const char* fmt) { ImGui::SetItemTooltip("%s", fmt); }, py::arg("fmt"));

    // ═══════════════ CURSOR ═════════════════════════════════════
    m.def("get_cursor_pos", &ImGui::GetCursorPos);
    m.def("set_cursor_pos", &ImGui::SetCursorPos, py::arg("local_pos"));
    m.def("get_cursor_pos_x", &ImGui::GetCursorPosX);
    m.def("set_cursor_pos_x", &ImGui::SetCursorPosX, py::arg("local_x"));
    m.def("get_cursor_pos_y", &ImGui::GetCursorPosY);
    m.def("set_cursor_pos_y", &ImGui::SetCursorPosY, py::arg("local_y"));
    m.def("get_cursor_screen_pos", &ImGui::GetCursorScreenPos);
    m.def("set_cursor_screen_pos", &ImGui::SetCursorScreenPos, py::arg("pos"));
    m.def("get_cursor_start_pos", &ImGui::GetCursorStartPos);

    // ═══════════════ SCROLLING ══════════════════════════════════
    m.def("get_scroll_x", &ImGui::GetScrollX); m.def("get_scroll_y", &ImGui::GetScrollY);
    m.def("get_scroll_max_x", &ImGui::GetScrollMaxX); m.def("get_scroll_max_y", &ImGui::GetScrollMaxY);
    m.def("set_scroll_x", &ImGui::SetScrollX, py::arg("scroll_x"));
    m.def("set_scroll_y", &ImGui::SetScrollY, py::arg("scroll_y"));
    m.def("set_scroll_here_x", &ImGui::SetScrollHereX, py::arg("center_x_ratio") = 0.5f);
    m.def("set_scroll_here_y", &ImGui::SetScrollHereY, py::arg("center_y_ratio") = 0.5f);
    m.def("set_scroll_from_pos_x", &ImGui::SetScrollFromPosX, py::arg("local_x"), py::arg("center_x_ratio") = 0.5f);
    m.def("set_scroll_from_pos_y", &ImGui::SetScrollFromPosY, py::arg("local_y"), py::arg("center_y_ratio") = 0.5f);

    // ═══════════════ ITEM QUERY ═════════════════════════════════
    m.def("is_item_hovered", &ImGui::IsItemHovered, py::arg("flags") = 0);
    m.def("is_item_active", &ImGui::IsItemActive); m.def("is_item_focused", &ImGui::IsItemFocused);
    m.def("is_item_clicked", &ImGui::IsItemClicked, py::arg("mouse_button") = 0);
    m.def("is_item_visible", &ImGui::IsItemVisible); m.def("is_item_edited", &ImGui::IsItemEdited);
    m.def("is_item_activated", &ImGui::IsItemActivated); m.def("is_item_deactivated", &ImGui::IsItemDeactivated);
    m.def("is_item_deactivated_after_edit", &ImGui::IsItemDeactivatedAfterEdit);
    m.def("is_item_toggled_open", &ImGui::IsItemToggledOpen);
    m.def("is_any_item_hovered", &ImGui::IsAnyItemHovered); m.def("is_any_item_active", &ImGui::IsAnyItemActive);
    m.def("is_any_item_focused", &ImGui::IsAnyItemFocused);
    m.def("get_item_id", &ImGui::GetItemID);
    m.def("get_item_rect_min", &ImGui::GetItemRectMin); m.def("get_item_rect_max", &ImGui::GetItemRectMax);
    m.def("get_item_rect_size", &ImGui::GetItemRectSize);
    m.def("get_item_flags", &ImGui::GetItemFlags);
    m.def("set_item_default_focus", &ImGui::SetItemDefaultFocus);
    m.def("set_nav_cursor_visible", &ImGui::SetNavCursorVisible, py::arg("visible"));
    m.def("set_next_item_width", &ImGui::SetNextItemWidth, py::arg("item_width"));
    m.def("set_next_item_allow_overlap", &ImGui::SetNextItemAllowOverlap);

    // ═══════════════ ID / FOCUS ═════════════════════════════════
    m.def("push_id", py::overload_cast<const char*>(&ImGui::PushID), py::arg("str_id"));
    m.def("push_id_int", py::overload_cast<int>(&ImGui::PushID), py::arg("int_id"));
    m.def("pop_id", &ImGui::PopID);
    m.def("get_id", py::overload_cast<const char*>(&ImGui::GetID), py::arg("str_id"));
    m.def("get_id_int", py::overload_cast<int>(&ImGui::GetID), py::arg("int_id"));
    m.def("set_keyboard_focus_here", &ImGui::SetKeyboardFocusHere, py::arg("offset") = 0);

    // ═══════════════ KEYBOARD ═══════════════════════════════════
    m.def("is_key_down", [](int key) { return ImGui::IsKeyDown(static_cast<ImGuiKey>(key)); }, py::arg("key"));
    m.def("is_key_pressed", [](int key, bool repeat = true) { return ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), repeat); }, py::arg("key"), py::arg("repeat") = true);
    m.def("is_key_released", [](int key) { return ImGui::IsKeyReleased(static_cast<ImGuiKey>(key)); }, py::arg("key"));
    m.def("is_key_chord_pressed", &ImGui::IsKeyChordPressed, py::arg("key_chord"));
    m.def("get_key_name", [](int key) -> const char* { return ImGui::GetKeyName(static_cast<ImGuiKey>(key)); }, py::arg("key"));
    m.def("get_key_pressed_amount", [](int key, float repeat_delay, float rate) { return ImGui::GetKeyPressedAmount(static_cast<ImGuiKey>(key), repeat_delay, rate); }, py::arg("key"), py::arg("repeat_delay"), py::arg("rate"));
    m.def("set_next_frame_want_capture_keyboard", &ImGui::SetNextFrameWantCaptureKeyboard, py::arg("want_capture_keyboard"));
    m.def("shortcut", &ImGui::Shortcut, py::arg("key_chord"), py::arg("flags") = 0);
    m.def("set_next_item_shortcut", &ImGui::SetNextItemShortcut, py::arg("key_chord"), py::arg("flags") = 0);
    m.def("set_item_key_owner", [](int key) { ImGui::SetItemKeyOwner(static_cast<ImGuiKey>(key)); }, py::arg("key"));

    // ═══════════════ MOUSE ══════════════════════════════════════
    m.def("set_mouse_cursor", &ImGui::SetMouseCursor, py::arg("cursor_type"));
    m.def("get_mouse_cursor", &ImGui::GetMouseCursor);
    m.def("get_mouse_pos", &ImGui::GetMousePos);
    m.def("get_mouse_pos_on_opening_current_popup", &ImGui::GetMousePosOnOpeningCurrentPopup);
    m.def("is_mouse_down", &ImGui::IsMouseDown, py::arg("button"));
    m.def("is_mouse_clicked", &ImGui::IsMouseClicked, py::arg("button"), py::arg("repeat") = false);
    m.def("is_mouse_released", &ImGui::IsMouseReleased, py::arg("button"));
    m.def("is_mouse_double_clicked", &ImGui::IsMouseDoubleClicked, py::arg("button"));
    m.def("is_mouse_released_with_delay", &ImGui::IsMouseReleasedWithDelay, py::arg("button"), py::arg("delay"));
    m.def("is_mouse_dragging", &ImGui::IsMouseDragging, py::arg("button"), py::arg("lock_threshold") = -1.0f);
    m.def("is_mouse_hovering_rect", &ImGui::IsMouseHoveringRect, py::arg("r_min"), py::arg("r_max"), py::arg("clip") = true);
    m.def("is_any_mouse_down", &ImGui::IsAnyMouseDown);
    m.def("is_mouse_pos_valid", [](const ImVec2& mouse_pos) { return ImGui::IsMousePosValid(&mouse_pos); }, py::arg("mouse_pos") = ImVec2(-FLT_MAX, -FLT_MAX));
    m.def("get_mouse_clicked_count", &ImGui::GetMouseClickedCount, py::arg("button"));
    m.def("get_mouse_drag_delta", &ImGui::GetMouseDragDelta, py::arg("button") = 0, py::arg("lock_threshold") = -1.0f);
    m.def("reset_mouse_drag_delta", &ImGui::ResetMouseDragDelta, py::arg("button") = 0);
    m.def("set_next_frame_want_capture_mouse", &ImGui::SetNextFrameWantCaptureMouse, py::arg("want_capture_mouse"));

    // ═══════════════ STYLE ══════════════════════════════════════
    m.def("push_style_color", py::overload_cast<ImGuiCol, const ImVec4&>(&ImGui::PushStyleColor), py::arg("idx"), py::arg("col"));
    m.def("push_style_color_u32", py::overload_cast<ImGuiCol, ImU32>(&ImGui::PushStyleColor), py::arg("idx"), py::arg("col_u32"));
    m.def("pop_style_color", &ImGui::PopStyleColor, py::arg("count") = 1);
    m.def("push_style_var", py::overload_cast<ImGuiStyleVar, float>(&ImGui::PushStyleVar), py::arg("idx"), py::arg("val"));
    m.def("push_style_var_vec2", py::overload_cast<ImGuiStyleVar, const ImVec2&>(&ImGui::PushStyleVar), py::arg("idx"), py::arg("val"));
    m.def("push_style_var_x", &ImGui::PushStyleVarX, py::arg("idx"), py::arg("val_x"));
    m.def("push_style_var_y", &ImGui::PushStyleVarY, py::arg("idx"), py::arg("val_y"));
    m.def("pop_style_var", &ImGui::PopStyleVar, py::arg("count") = 1);
    m.def("push_item_flag", &ImGui::PushItemFlag, py::arg("option"), py::arg("enabled"));
    m.def("pop_item_flag", &ImGui::PopItemFlag);
    m.def("push_item_width", &ImGui::PushItemWidth, py::arg("item_width"));
    m.def("pop_item_width", &ImGui::PopItemWidth);
    m.def("calc_item_width", &ImGui::CalcItemWidth);
    m.def("push_text_wrap_pos", &ImGui::PushTextWrapPos, py::arg("wrap_local_pos_x") = 0.0f);
    m.def("pop_text_wrap_pos", &ImGui::PopTextWrapPos);
    m.def("push_button_repeat", &ImGui::PushButtonRepeat, py::arg("repeat"));
    m.def("pop_button_repeat", &ImGui::PopButtonRepeat);
    m.def("style_colors_dark", &ImGui::StyleColorsDark, py::arg("dst") = nullptr);
    m.def("style_colors_light", &ImGui::StyleColorsLight, py::arg("dst") = nullptr);
    m.def("style_colors_classic", &ImGui::StyleColorsClassic, py::arg("dst") = nullptr);
    m.def("get_style_color_name", [](ImGuiCol idx) -> const char* { return ImGui::GetStyleColorName(idx); }, py::arg("idx"));

    // ═══════════════ CLIP RECT ══════════════════════════════════
    m.def("push_clip_rect", &ImGui::PushClipRect, py::arg("clip_rect_min"), py::arg("clip_rect_max"), py::arg("intersect_with_current_clip_rect"));
    // Legacy (x, y, width, height, intersect) overload — computes max from min + size.
    m.def("push_clip_rect", [](float x, float y, float w, float h, bool intersect) {
        ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), intersect);
    }, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"), py::arg("intersect_with_current_clip_rect"));
    m.def("pop_clip_rect", &ImGui::PopClipRect);

    // ═══════════════ FONT ═══════════════════════════════════════
    m.def("get_text_line_height", &ImGui::GetTextLineHeight);
    m.def("get_text_line_height_with_spacing", &ImGui::GetTextLineHeightWithSpacing);
    m.def("calc_text_size", &ImGui::CalcTextSize, py::arg("text"), py::arg("text_end") = nullptr, py::arg("hide_text_after_double_hash") = false, py::arg("wrap_width") = -1.0f);
    m.def("get_font", &ImGui::GetFont, py::return_value_policy::reference);
    m.def("get_font_tex_uv_white_pixel", &ImGui::GetFontTexUvWhitePixel);
    m.def("set_window_font_scale", [](float s) { /* OBSOLETE - use style.FontScaleMain */ });

    // ═══════════════ CLIPBOARD / LOG ════════════════════════════
    m.def("get_clipboard_text", &ImGui::GetClipboardText);
    m.def("set_clipboard_text", &ImGui::SetClipboardText, py::arg("text"));
    m.def("log_to_tty", &ImGui::LogToTTY, py::arg("auto_open_depth") = -1);
    m.def("log_to_file", &ImGui::LogToFile, py::arg("auto_open_depth") = -1, py::arg("filename") = nullptr);
    m.def("log_to_clipboard", &ImGui::LogToClipboard, py::arg("auto_open_depth") = -1);
    m.def("log_buttons", &ImGui::LogButtons);
    m.def("log_finish", &ImGui::LogFinish);
    m.def("get_time", &ImGui::GetTime);
    m.def("get_frame_count", &ImGui::GetFrameCount);

    // ═══════════════ INI ════════════════════════════════════════
    m.def("load_ini_settings_from_disk", &ImGui::LoadIniSettingsFromDisk, py::arg("ini_filename"));
    m.def("load_ini_settings_from_memory", &ImGui::LoadIniSettingsFromMemory, py::arg("ini_data"), py::arg("ini_size") = 0);
    m.def("save_ini_settings_to_disk", &ImGui::SaveIniSettingsToDisk, py::arg("ini_filename"));
    m.def("save_ini_settings_to_memory", []() -> std::string {
        size_t out_size = 0;
        const char* data = ImGui::SaveIniSettingsToMemory(&out_size);
        return data ? std::string(data, out_size) : std::string();
    });

    // ═══════════════ DRAG & DROP ════════════════════════════════
    m.def("begin_drag_drop_source", &ImGui::BeginDragDropSource, py::arg("flags") = 0);
    m.def("set_drag_drop_payload", &ImGui::SetDragDropPayload, py::arg("type"), py::arg("data"), py::arg("sz"), py::arg("cond") = 0);
    m.def("end_drag_drop_source", &ImGui::EndDragDropSource);
    m.def("begin_drag_drop_target", &ImGui::BeginDragDropTarget);
    m.def("accept_drag_drop_payload", &ImGui::AcceptDragDropPayload, py::arg("type"), py::arg("flags") = 0, py::return_value_policy::reference);
    m.def("end_drag_drop_target", &ImGui::EndDragDropTarget);
    m.def("get_drag_drop_payload", &ImGui::GetDragDropPayload, py::return_value_policy::reference);

    PY4GW::imgui_bindings::register_docking(m);
    m.def("is_docking_enabled", &PY4GW::imgui::IsDockingEnabled);
    m.def("set_docking_enabled", &PY4GW::imgui::SetDockingEnabled, py::arg("enabled"));
    m.def("is_multi_viewport_enabled", &PY4GW::imgui::IsMultiViewportEnabled);
    m.def("set_multi_viewport_enabled", &PY4GW::imgui::SetMultiViewportEnabled, py::arg("enabled"));
    m.def("has_multi_viewport_support", &PY4GW::imgui::HasMultiViewportSupport);
    // ═══════════════ VIEWPORT ═══════════════════════════════════
    m.def("get_main_viewport", &ImGui::GetMainViewport, py::return_value_policy::reference);
    m.def("get_window_viewport", &ImGui::GetWindowViewport, py::return_value_policy::reference);
    m.def("get_window_dpi_scale", &ImGui::GetWindowDpiScale);

    // ═══════════════ PLOTTING ═══════════════════════════════════
    // Take `values` BY VALUE (not const&): ImGui's Plot_ArrayGetter reads values.data()
    // synchronously, and reading it off a marshalled temporary was faulting (access
    // violation in the getter). A by-value parameter is a buffer the lambda owns for the
    // whole call. Also guard empty input so PlotLines never sees a null/degenerate array.
    m.def("plot_lines", [](const char* label, std::vector<float> values, int offset, const char* overlay, float scale_min, float scale_max, const ImVec2& graph_size) {
        if (values.empty()) return;
        ImGui::PlotLines(label, values.data(), (int)values.size(), offset, overlay, scale_min, scale_max, graph_size);
    }, py::arg("label"), py::arg("values"), py::arg("values_offset") = 0, py::arg("overlay_text") = nullptr,
       py::arg("scale_min") = FLT_MAX, py::arg("scale_max") = FLT_MAX, py::arg("graph_size") = ImVec2(0,0));
    m.def("plot_histogram", [](const char* label, std::vector<float> values, int offset, const char* overlay, float scale_min, float scale_max, const ImVec2& graph_size) {
        if (values.empty()) return;
        ImGui::PlotHistogram(label, values.data(), (int)values.size(), offset, overlay, scale_min, scale_max, graph_size);
    }, py::arg("label"), py::arg("values"), py::arg("values_offset") = 0, py::arg("overlay_text") = nullptr,
       py::arg("scale_min") = FLT_MAX, py::arg("scale_max") = FLT_MAX, py::arg("graph_size") = ImVec2(0,0));
    m.def("value_bool", [](const char* prefix, bool v) { ImGui::Value(prefix, v); }, py::arg("prefix"), py::arg("v"));
    m.def("value_int", [](const char* prefix, int v) { ImGui::Value(prefix, v); }, py::arg("prefix"), py::arg("v"));
    m.def("value_uint", [](const char* prefix, unsigned int v) { ImGui::Value(prefix, v); }, py::arg("prefix"), py::arg("v"));
    m.def("value_float", [](const char* prefix, float v, const char* float_format) { ImGui::Value(prefix, v, float_format); }, py::arg("prefix"), py::arg("v"), py::arg("float_format") = nullptr);

    // ═══════════════ DEBUG ══════════════════════════════════════
    m.def("show_demo_window", []() { ImGui::ShowDemoWindow(); });
    m.def("show_metrics_window", []() { ImGui::ShowMetricsWindow(); });
    m.def("show_debug_log_window", []() { ImGui::ShowDebugLogWindow(); });
    m.def("show_id_stack_tool_window", []() { ImGui::ShowIDStackToolWindow(); });
    m.def("show_about_window", []() { ImGui::ShowAboutWindow(); });
    m.def("show_style_editor", []() { ImGui::ShowStyleEditor(); });
    m.def("show_style_selector", &ImGui::ShowStyleSelector, py::arg("label"));
    m.def("show_font_selector", &ImGui::ShowFontSelector, py::arg("label"));
    m.def("show_user_guide", &ImGui::ShowUserGuide);
    m.def("get_version", &ImGui::GetVersion);
    m.def("debug_flash_style_color", &ImGui::DebugFlashStyleColor, py::arg("idx"));
    m.def("debug_start_item_picker", &ImGui::DebugStartItemPicker);
    m.def("debug_text_encoding", [](const char* text) { ImGui::DebugTextEncoding(text); }, py::arg("text"));


    // ═══════════════ MULTI-SELECT ════════════════════════════════
    m.def("begin_multi_select", &ImGui::BeginMultiSelect, py::arg("flags") = 0, py::arg("selection_size") = -1, py::arg("items_count") = -1);
    m.def("end_multi_select", &ImGui::EndMultiSelect);


    // ── domain modules assembled into the PyImGui module ──
    PY4GW::imgui_bindings::register_drawlist(m);
    PY4GW::imgui_bindings::register_io(m);
    PY4GW::imgui_bindings::register_addons(m);
    PY4GW::imgui_bindings::register_implot(m);
    PY4GW::ext::register_ext(m);   // own module/namespace (imgui/ext/ext.h), not imgui_bindings
}

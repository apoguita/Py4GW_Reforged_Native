#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>
#include <pybind11/stl.h>  // std::array<->Python caster for StyleConfig members

#include <array>
#include <stdexcept>

namespace PY4GW::imgui_bindings {

namespace {

// Snapshot/editor copy of ImGuiStyle. Pull() reads the live style, Push()
// writes it back, Reset() restores the captured default. The live style itself
// is also exposed directly via get_style().
struct StyleConfig {
    ImGuiStyle& style;
    ImGuiStyle  default_style;
    float Alpha, DisabledAlpha, WindowRounding, WindowBorderSize, ChildRounding, ChildBorderSize,
          PopupRounding, PopupBorderSize, FrameRounding, FrameBorderSize, IndentSpacing,
          ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding,
          LogSliderDeadzone, TabRounding, TabBorderSize, TabCloseButtonMinWidthUnselected,
          SeparatorTextBorderSize, MouseCursorScale, CurveTessellationTol, CircleTessellationMaxError;
    std::array<float, 2> WindowPadding, WindowMinSize, WindowTitleAlign, FramePadding, ItemSpacing,
          ItemInnerSpacing, CellPadding, TouchExtraPadding, ButtonTextAlign, SelectableTextAlign,
          SeparatorTextAlign, SeparatorTextPadding, DisplayWindowPadding, DisplaySafeAreaPadding;
    bool AntiAliasedLines, AntiAliasedLinesUseTex, AntiAliasedFill;
    int WindowMenuButtonPosition, ColorButtonPosition;
    std::array<float, 4> Colors[ImGuiCol_COUNT];

    StyleConfig() : style(ImGui::GetStyle()), default_style(ImGui::GetStyle()) { Pull(); }
    void Pull() {
        const ImGuiStyle& s = style;
        Alpha=s.Alpha; DisabledAlpha=s.DisabledAlpha;
        WindowPadding={s.WindowPadding.x,s.WindowPadding.y}; WindowRounding=s.WindowRounding; WindowBorderSize=s.WindowBorderSize;
        WindowMinSize={s.WindowMinSize.x,s.WindowMinSize.y}; WindowTitleAlign={s.WindowTitleAlign.x,s.WindowTitleAlign.y};
        WindowMenuButtonPosition=(int)s.WindowMenuButtonPosition;
        ChildRounding=s.ChildRounding; ChildBorderSize=s.ChildBorderSize;
        PopupRounding=s.PopupRounding; PopupBorderSize=s.PopupBorderSize;
        FramePadding={s.FramePadding.x,s.FramePadding.y}; FrameRounding=s.FrameRounding; FrameBorderSize=s.FrameBorderSize;
        ItemSpacing={s.ItemSpacing.x,s.ItemSpacing.y}; ItemInnerSpacing={s.ItemInnerSpacing.x,s.ItemInnerSpacing.y};
        CellPadding={s.CellPadding.x,s.CellPadding.y}; TouchExtraPadding={s.TouchExtraPadding.x,s.TouchExtraPadding.y};
        IndentSpacing=s.IndentSpacing; ColumnsMinSpacing=s.ColumnsMinSpacing;
        ScrollbarSize=s.ScrollbarSize; ScrollbarRounding=s.ScrollbarRounding;
        GrabMinSize=s.GrabMinSize; GrabRounding=s.GrabRounding;
        LogSliderDeadzone=s.LogSliderDeadzone; TabRounding=s.TabRounding; TabBorderSize=s.TabBorderSize;
        TabCloseButtonMinWidthUnselected=s.TabCloseButtonMinWidthUnselected; ColorButtonPosition=(int)s.ColorButtonPosition;
        ButtonTextAlign={s.ButtonTextAlign.x,s.ButtonTextAlign.y}; SelectableTextAlign={s.SelectableTextAlign.x,s.SelectableTextAlign.y};
        SeparatorTextBorderSize=s.SeparatorTextBorderSize;
        SeparatorTextAlign={s.SeparatorTextAlign.x,s.SeparatorTextAlign.y}; SeparatorTextPadding={s.SeparatorTextPadding.x,s.SeparatorTextPadding.y};
        DisplayWindowPadding={s.DisplayWindowPadding.x,s.DisplayWindowPadding.y}; DisplaySafeAreaPadding={s.DisplaySafeAreaPadding.x,s.DisplaySafeAreaPadding.y};
        MouseCursorScale=s.MouseCursorScale; AntiAliasedLines=s.AntiAliasedLines;
        AntiAliasedLinesUseTex=s.AntiAliasedLinesUseTex; AntiAliasedFill=s.AntiAliasedFill;
        CurveTessellationTol=s.CurveTessellationTol; CircleTessellationMaxError=s.CircleTessellationMaxError;
        for (int i=0;i<ImGuiCol_COUNT;++i) Colors[i]={s.Colors[i].x,s.Colors[i].y,s.Colors[i].z,s.Colors[i].w};
    }
    void Push() const {
        ImGuiStyle& s=style; s.Alpha=Alpha; s.DisabledAlpha=DisabledAlpha;
        s.WindowPadding=ImVec2(WindowPadding[0],WindowPadding[1]); s.WindowRounding=WindowRounding; s.WindowBorderSize=WindowBorderSize;
        s.WindowMinSize=ImVec2(WindowMinSize[0],WindowMinSize[1]); s.WindowTitleAlign=ImVec2(WindowTitleAlign[0],WindowTitleAlign[1]);
        s.WindowMenuButtonPosition=(ImGuiDir)WindowMenuButtonPosition;
        s.ChildRounding=ChildRounding; s.ChildBorderSize=ChildBorderSize;
        s.PopupRounding=PopupRounding; s.PopupBorderSize=PopupBorderSize;
        s.FramePadding=ImVec2(FramePadding[0],FramePadding[1]); s.FrameRounding=FrameRounding; s.FrameBorderSize=FrameBorderSize;
        s.ItemSpacing=ImVec2(ItemSpacing[0],ItemSpacing[1]); s.ItemInnerSpacing=ImVec2(ItemInnerSpacing[0],ItemInnerSpacing[1]);
        s.CellPadding=ImVec2(CellPadding[0],CellPadding[1]); s.TouchExtraPadding=ImVec2(TouchExtraPadding[0],TouchExtraPadding[1]);
        s.IndentSpacing=IndentSpacing; s.ColumnsMinSpacing=ColumnsMinSpacing;
        s.ScrollbarSize=ScrollbarSize; s.ScrollbarRounding=ScrollbarRounding;
        s.GrabMinSize=GrabMinSize; s.GrabRounding=GrabRounding;
        s.LogSliderDeadzone=LogSliderDeadzone; s.TabRounding=TabRounding; s.TabBorderSize=TabBorderSize;
        s.TabCloseButtonMinWidthUnselected=TabCloseButtonMinWidthUnselected; s.ColorButtonPosition=(ImGuiDir)ColorButtonPosition;
        s.ButtonTextAlign=ImVec2(ButtonTextAlign[0],ButtonTextAlign[1]); s.SelectableTextAlign=ImVec2(SelectableTextAlign[0],SelectableTextAlign[1]);
        s.SeparatorTextBorderSize=SeparatorTextBorderSize;
        s.SeparatorTextAlign=ImVec2(SeparatorTextAlign[0],SeparatorTextAlign[1]); s.SeparatorTextPadding=ImVec2(SeparatorTextPadding[0],SeparatorTextPadding[1]);
        s.DisplayWindowPadding=ImVec2(DisplayWindowPadding[0],DisplayWindowPadding[1]); s.DisplaySafeAreaPadding=ImVec2(DisplaySafeAreaPadding[0],DisplaySafeAreaPadding[1]);
        s.MouseCursorScale=MouseCursorScale; s.AntiAliasedLines=AntiAliasedLines;
        s.AntiAliasedLinesUseTex=AntiAliasedLinesUseTex; s.AntiAliasedFill=AntiAliasedFill;
        s.CurveTessellationTol=CurveTessellationTol; s.CircleTessellationMaxError=CircleTessellationMaxError;
        for (int i=0;i<ImGuiCol_COUNT;++i) s.Colors[i]=ImVec4(Colors[i][0],Colors[i][1],Colors[i][2],Colors[i][3]);
    }
    void Reset() { style=default_style; Pull(); }
};

}  // namespace

void register_style(py::module_& m) {
    // Live style handle: get_style() returns ImGui::GetStyle() by reference, so
    // field writes take effect immediately.
    py::class_<ImGuiStyle>(m, "ImGuiStyle")
        .def_readwrite("Alpha", &ImGuiStyle::Alpha).def_readwrite("DisabledAlpha", &ImGuiStyle::DisabledAlpha)
        // ImGui 1.92 font scaling: FontSizeBase is the unscaled base size (feed to
        // PushFont(NULL, size)); FontScaleMain is the app/user global scale (the
        // replacement for io.FontGlobalScale); FontScaleDpi is the monitor-DPI
        // factor. Final size == FontSizeBase * FontScaleMain * FontScaleDpi.
        .def_readwrite("FontSizeBase", &ImGuiStyle::FontSizeBase)
        .def_readwrite("FontScaleMain", &ImGuiStyle::FontScaleMain)
        .def_readwrite("FontScaleDpi", &ImGuiStyle::FontScaleDpi)
        .def_readwrite("WindowPadding", &ImGuiStyle::WindowPadding).def_readwrite("WindowRounding", &ImGuiStyle::WindowRounding)
        .def_readwrite("WindowBorderSize", &ImGuiStyle::WindowBorderSize).def_readwrite("WindowMinSize", &ImGuiStyle::WindowMinSize)
        .def_readwrite("WindowTitleAlign", &ImGuiStyle::WindowTitleAlign).def_readwrite("WindowMenuButtonPosition", &ImGuiStyle::WindowMenuButtonPosition)
        .def_readwrite("ChildRounding", &ImGuiStyle::ChildRounding).def_readwrite("ChildBorderSize", &ImGuiStyle::ChildBorderSize)
        .def_readwrite("PopupRounding", &ImGuiStyle::PopupRounding).def_readwrite("PopupBorderSize", &ImGuiStyle::PopupBorderSize)
        .def_readwrite("FramePadding", &ImGuiStyle::FramePadding).def_readwrite("FrameRounding", &ImGuiStyle::FrameRounding)
        .def_readwrite("FrameBorderSize", &ImGuiStyle::FrameBorderSize).def_readwrite("ItemSpacing", &ImGuiStyle::ItemSpacing)
        .def_readwrite("ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing).def_readwrite("CellPadding", &ImGuiStyle::CellPadding)
        .def_readwrite("TouchExtraPadding", &ImGuiStyle::TouchExtraPadding).def_readwrite("IndentSpacing", &ImGuiStyle::IndentSpacing)
        .def_readwrite("ColumnsMinSpacing", &ImGuiStyle::ColumnsMinSpacing).def_readwrite("ScrollbarSize", &ImGuiStyle::ScrollbarSize)
        .def_readwrite("ScrollbarRounding", &ImGuiStyle::ScrollbarRounding).def_readwrite("GrabMinSize", &ImGuiStyle::GrabMinSize)
        .def_readwrite("GrabRounding", &ImGuiStyle::GrabRounding).def_readwrite("LogSliderDeadzone", &ImGuiStyle::LogSliderDeadzone)
        .def_readwrite("TabRounding", &ImGuiStyle::TabRounding).def_readwrite("TabBorderSize", &ImGuiStyle::TabBorderSize)
        .def_readwrite("TabCloseButtonMinWidthUnselected", &ImGuiStyle::TabCloseButtonMinWidthUnselected)
        .def_readwrite("ColorButtonPosition", &ImGuiStyle::ColorButtonPosition).def_readwrite("ButtonTextAlign", &ImGuiStyle::ButtonTextAlign)
        .def_readwrite("SelectableTextAlign", &ImGuiStyle::SelectableTextAlign).def_readwrite("SeparatorTextBorderSize", &ImGuiStyle::SeparatorTextBorderSize)
        .def_readwrite("SeparatorTextAlign", &ImGuiStyle::SeparatorTextAlign).def_readwrite("SeparatorTextPadding", &ImGuiStyle::SeparatorTextPadding)
        .def_readwrite("DisplayWindowPadding", &ImGuiStyle::DisplayWindowPadding).def_readwrite("DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding)
        .def_readwrite("MouseCursorScale", &ImGuiStyle::MouseCursorScale).def_readwrite("AntiAliasedLines", &ImGuiStyle::AntiAliasedLines)
        .def_readwrite("AntiAliasedLinesUseTex", &ImGuiStyle::AntiAliasedLinesUseTex).def_readwrite("AntiAliasedFill", &ImGuiStyle::AntiAliasedFill)
        .def_readwrite("CurveTessellationTol", &ImGuiStyle::CurveTessellationTol).def_readwrite("CircleTessellationMaxError", &ImGuiStyle::CircleTessellationMaxError)
        .def_readwrite("HoverStationaryDelay", &ImGuiStyle::HoverStationaryDelay).def_readwrite("HoverDelayShort", &ImGuiStyle::HoverDelayShort)
        .def_readwrite("HoverDelayNormal", &ImGuiStyle::HoverDelayNormal).def_readwrite("HoverFlagsForTooltipMouse", &ImGuiStyle::HoverFlagsForTooltipMouse)
        .def_readwrite("HoverFlagsForTooltipNav", &ImGuiStyle::HoverFlagsForTooltipNav)
        .def("get_color", [](ImGuiStyle& self, int idx) {
                 if (idx < 0 || idx >= ImGuiCol_COUNT) throw std::out_of_range("ImGuiCol index");
                 return self.Colors[idx];
             }, py::arg("idx"))
        .def("set_color", [](ImGuiStyle& self, int idx, const ImVec4& col) {
                 if (idx < 0 || idx >= ImGuiCol_COUNT) throw std::out_of_range("ImGuiCol index");
                 self.Colors[idx] = col;
             }, py::arg("idx"), py::arg("col"))
        .def(py::init<>()).def("ScaleAllSizes", &ImGuiStyle::ScaleAllSizes, py::arg("scale_factor"));

    m.def("get_style", []() -> ImGuiStyle& { return ImGui::GetStyle(); }, py::return_value_policy::reference);

    py::class_<StyleConfig>(m, "StyleConfig")
        .def(py::init([]() { if (!ImGui::GetCurrentContext()) throw std::runtime_error("no ImGui context"); return StyleConfig(); }))
        .def("Pull", &StyleConfig::Pull).def("Push", &StyleConfig::Push).def("Reset", &StyleConfig::Reset)
        .def_readwrite("Alpha", &StyleConfig::Alpha).def_readwrite("DisabledAlpha", &StyleConfig::DisabledAlpha)
        .def_readwrite("WindowPadding", &StyleConfig::WindowPadding).def_readwrite("WindowRounding", &StyleConfig::WindowRounding)
        .def_readwrite("WindowBorderSize", &StyleConfig::WindowBorderSize).def_readwrite("WindowMinSize", &StyleConfig::WindowMinSize)
        .def_readwrite("WindowTitleAlign", &StyleConfig::WindowTitleAlign).def_readwrite("WindowMenuButtonPosition", &StyleConfig::WindowMenuButtonPosition)
        .def_readwrite("ChildRounding", &StyleConfig::ChildRounding).def_readwrite("ChildBorderSize", &StyleConfig::ChildBorderSize)
        .def_readwrite("PopupRounding", &StyleConfig::PopupRounding).def_readwrite("PopupBorderSize", &StyleConfig::PopupBorderSize)
        .def_readwrite("FramePadding", &StyleConfig::FramePadding).def_readwrite("FrameRounding", &StyleConfig::FrameRounding)
        .def_readwrite("FrameBorderSize", &StyleConfig::FrameBorderSize).def_readwrite("ItemSpacing", &StyleConfig::ItemSpacing)
        .def_readwrite("ItemInnerSpacing", &StyleConfig::ItemInnerSpacing).def_readwrite("CellPadding", &StyleConfig::CellPadding)
        .def_readwrite("TouchExtraPadding", &StyleConfig::TouchExtraPadding).def_readwrite("IndentSpacing", &StyleConfig::IndentSpacing)
        .def_readwrite("ColumnsMinSpacing", &StyleConfig::ColumnsMinSpacing).def_readwrite("ScrollbarSize", &StyleConfig::ScrollbarSize)
        .def_readwrite("ScrollbarRounding", &StyleConfig::ScrollbarRounding).def_readwrite("GrabMinSize", &StyleConfig::GrabMinSize)
        .def_readwrite("GrabRounding", &StyleConfig::GrabRounding).def_readwrite("LogSliderDeadzone", &StyleConfig::LogSliderDeadzone)
        .def_readwrite("TabRounding", &StyleConfig::TabRounding).def_readwrite("TabBorderSize", &StyleConfig::TabBorderSize)
        .def_readwrite("TabCloseButtonMinWidthUnselected", &StyleConfig::TabCloseButtonMinWidthUnselected)
        .def_readwrite("SeparatorTextBorderSize", &StyleConfig::SeparatorTextBorderSize).def_readwrite("ColorButtonPosition", &StyleConfig::ColorButtonPosition)
        .def_readwrite("ButtonTextAlign", &StyleConfig::ButtonTextAlign).def_readwrite("SelectableTextAlign", &StyleConfig::SelectableTextAlign)
        .def_readwrite("SeparatorTextAlign", &StyleConfig::SeparatorTextAlign).def_readwrite("SeparatorTextPadding", &StyleConfig::SeparatorTextPadding)
        .def_readwrite("DisplayWindowPadding", &StyleConfig::DisplayWindowPadding).def_readwrite("DisplaySafeAreaPadding", &StyleConfig::DisplaySafeAreaPadding)
        .def_readwrite("MouseCursorScale", &StyleConfig::MouseCursorScale).def_readwrite("AntiAliasedLines", &StyleConfig::AntiAliasedLines)
        .def_readwrite("AntiAliasedLinesUseTex", &StyleConfig::AntiAliasedLinesUseTex).def_readwrite("AntiAliasedFill", &StyleConfig::AntiAliasedFill)
        .def_readwrite("CurveTessellationTol", &StyleConfig::CurveTessellationTol).def_readwrite("CircleTessellationMaxError", &StyleConfig::CircleTessellationMaxError)
        .def("get_color", [](StyleConfig& self, int idx) {
            if (idx<0||idx>=ImGuiCol_COUNT) throw std::out_of_range("ImGuiCol index");
            return py::make_tuple(self.Colors[idx][0],self.Colors[idx][1],self.Colors[idx][2],self.Colors[idx][3]);
        })
        .def("set_color", [](StyleConfig& self, int idx, float r, float g, float b, float a) {
            if (idx<0||idx>=ImGuiCol_COUNT) throw std::out_of_range("ImGuiCol index");
            self.Colors[idx]={r,g,b,a};
        });
}

}  // namespace PY4GW::imgui_bindings

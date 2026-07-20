#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>
#include <implot.h>

#include "base/py_bindings.h"

#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

// ImPlot exposed as the PyImGui.implot submodule.
//
// This binding aims to expose the full ImPlot v1.0 surface (the ImPlotSpec
// rework): the plot/subplot lifecycle, axis/legend setup, every PlotX item
// type, plot querying, drag tools, annotations/tags, styling, and colormaps.
//
// Conventions kept consistent across the module:
//   * Colors are optional 4-float (r, g, b, a) tuples/lists. None (or an
//     omitted optional) means IMPLOT_AUTO_COL: let ImPlot pick from the
//     current colormap / item color. This matches the PyImGui push_style_color
//     float-tuple convention.
//   * Per-item styling is carried by ImPlotSpec, exposed here as a real class
//     you can construct, mutate, and pass as `spec=`. A `make_spec(...)` helper
//     builds one from common keyword arguments. The four legacy convenience
//     functions (plot_line/plot_line_xy/plot_bars/plot_shaded) keep their old
//     scalar signatures and gained an optional trailing `spec` that, when
//     provided, fully overrides the scalar style args (old callers unaffected).
//   * Array-taking calls copy their std::vector by value and guard empty input
//     (an empty pointer handed to ImPlot faults on the render thread).
//   * ImVec2 tuples are (x, y); sizes are (width, height).
//
// The ImPlot context is owned by imgui_manager (CreateContext right after
// ImGui's, DestroyContext right before it); nothing here creates or destroys
// it. Host-owned context/IO calls are deliberately not bound.

namespace PY4GW::imgui_bindings {

namespace {

using Color = std::array<float, 4>;
using Vec2f = std::array<float, 2>;

// None -> IMPLOT_AUTO_COL; a 4-float (r, g, b, a) -> explicit color.
ImVec4 ToColor(const std::optional<Color>& c) {
    if (!c) {
        return IMPLOT_AUTO_COL;
    }
    const Color& v = *c;
    return ImVec4(v[0], v[1], v[2], v[3]);
}

// Explicit color (no auto fallback).
ImVec4 ToVec4(const Color& v) { return ImVec4(v[0], v[1], v[2], v[3]); }

Color FromVec4(const ImVec4& v) { return Color{v.x, v.y, v.z, v.w}; }
Vec2f FromVec2(const ImVec2& v) { return Vec2f{v.x, v.y}; }

// Convert a Python list of raw c-strings for the "array of label_ids" calls.
std::vector<const char*> CStrs(const std::vector<std::string>& labels) {
    std::vector<const char*> out;
    out.reserve(labels.size());
    for (const std::string& s : labels) {
        out.push_back(s.c_str());
    }
    return out;
}

}  // namespace

void register_implot(py::module_& parent) {
    py::module_ m = parent.def_submodule("implot", "ImPlot plotting library (full surface).");

    // ─────────────────────────────────────────────────────────────────────
    // Enums / constants (exposed as module attributes, matching PyImGui style)
    // ─────────────────────────────────────────────────────────────────────
    m.attr("AUTO") = IMPLOT_AUTO;             // -1: auto sizing / auto colormap index
    // ImAxis
    m.attr("X1") = static_cast<int>(ImAxis_X1);
    m.attr("X2") = static_cast<int>(ImAxis_X2);
    m.attr("X3") = static_cast<int>(ImAxis_X3);
    m.attr("Y1") = static_cast<int>(ImAxis_Y1);
    m.attr("Y2") = static_cast<int>(ImAxis_Y2);
    m.attr("Y3") = static_cast<int>(ImAxis_Y3);

    // ImPlotFlags
    m.attr("Flags_None") = static_cast<int>(ImPlotFlags_None);
    m.attr("Flags_NoTitle") = static_cast<int>(ImPlotFlags_NoTitle);
    m.attr("Flags_NoLegend") = static_cast<int>(ImPlotFlags_NoLegend);
    m.attr("Flags_NoMouseText") = static_cast<int>(ImPlotFlags_NoMouseText);
    m.attr("Flags_NoInputs") = static_cast<int>(ImPlotFlags_NoInputs);
    m.attr("Flags_NoMenus") = static_cast<int>(ImPlotFlags_NoMenus);
    m.attr("Flags_NoBoxSelect") = static_cast<int>(ImPlotFlags_NoBoxSelect);
    m.attr("Flags_NoFrame") = static_cast<int>(ImPlotFlags_NoFrame);
    m.attr("Flags_Equal") = static_cast<int>(ImPlotFlags_Equal);
    m.attr("Flags_Crosshairs") = static_cast<int>(ImPlotFlags_Crosshairs);
    m.attr("Flags_CanvasOnly") = static_cast<int>(ImPlotFlags_CanvasOnly);

    // ImPlotAxisFlags
    m.attr("AxisFlags_None") = static_cast<int>(ImPlotAxisFlags_None);
    m.attr("AxisFlags_NoLabel") = static_cast<int>(ImPlotAxisFlags_NoLabel);
    m.attr("AxisFlags_NoGridLines") = static_cast<int>(ImPlotAxisFlags_NoGridLines);
    m.attr("AxisFlags_NoTickMarks") = static_cast<int>(ImPlotAxisFlags_NoTickMarks);
    m.attr("AxisFlags_NoTickLabels") = static_cast<int>(ImPlotAxisFlags_NoTickLabels);
    m.attr("AxisFlags_NoInitialFit") = static_cast<int>(ImPlotAxisFlags_NoInitialFit);
    m.attr("AxisFlags_NoMenus") = static_cast<int>(ImPlotAxisFlags_NoMenus);
    m.attr("AxisFlags_NoSideSwitch") = static_cast<int>(ImPlotAxisFlags_NoSideSwitch);
    m.attr("AxisFlags_NoHighlight") = static_cast<int>(ImPlotAxisFlags_NoHighlight);
    m.attr("AxisFlags_Opposite") = static_cast<int>(ImPlotAxisFlags_Opposite);
    m.attr("AxisFlags_Foreground") = static_cast<int>(ImPlotAxisFlags_Foreground);
    m.attr("AxisFlags_Invert") = static_cast<int>(ImPlotAxisFlags_Invert);
    m.attr("AxisFlags_AutoFit") = static_cast<int>(ImPlotAxisFlags_AutoFit);
    m.attr("AxisFlags_RangeFit") = static_cast<int>(ImPlotAxisFlags_RangeFit);
    m.attr("AxisFlags_PanStretch") = static_cast<int>(ImPlotAxisFlags_PanStretch);
    m.attr("AxisFlags_LockMin") = static_cast<int>(ImPlotAxisFlags_LockMin);
    m.attr("AxisFlags_LockMax") = static_cast<int>(ImPlotAxisFlags_LockMax);
    m.attr("AxisFlags_Lock") = static_cast<int>(ImPlotAxisFlags_Lock);
    m.attr("AxisFlags_NoDecorations") = static_cast<int>(ImPlotAxisFlags_NoDecorations);
    m.attr("AxisFlags_AuxDefault") = static_cast<int>(ImPlotAxisFlags_AuxDefault);

    // ImPlotSubplotFlags
    m.attr("SubplotFlags_None") = static_cast<int>(ImPlotSubplotFlags_None);
    m.attr("SubplotFlags_NoTitle") = static_cast<int>(ImPlotSubplotFlags_NoTitle);
    m.attr("SubplotFlags_NoLegend") = static_cast<int>(ImPlotSubplotFlags_NoLegend);
    m.attr("SubplotFlags_NoMenus") = static_cast<int>(ImPlotSubplotFlags_NoMenus);
    m.attr("SubplotFlags_NoResize") = static_cast<int>(ImPlotSubplotFlags_NoResize);
    m.attr("SubplotFlags_NoAlign") = static_cast<int>(ImPlotSubplotFlags_NoAlign);
    m.attr("SubplotFlags_ShareItems") = static_cast<int>(ImPlotSubplotFlags_ShareItems);
    m.attr("SubplotFlags_LinkRows") = static_cast<int>(ImPlotSubplotFlags_LinkRows);
    m.attr("SubplotFlags_LinkCols") = static_cast<int>(ImPlotSubplotFlags_LinkCols);
    m.attr("SubplotFlags_LinkAllX") = static_cast<int>(ImPlotSubplotFlags_LinkAllX);
    m.attr("SubplotFlags_LinkAllY") = static_cast<int>(ImPlotSubplotFlags_LinkAllY);
    m.attr("SubplotFlags_ColMajor") = static_cast<int>(ImPlotSubplotFlags_ColMajor);

    // ImPlotLegendFlags
    m.attr("LegendFlags_None") = static_cast<int>(ImPlotLegendFlags_None);
    m.attr("LegendFlags_NoButtons") = static_cast<int>(ImPlotLegendFlags_NoButtons);
    m.attr("LegendFlags_NoHighlightItem") = static_cast<int>(ImPlotLegendFlags_NoHighlightItem);
    m.attr("LegendFlags_NoHighlightAxis") = static_cast<int>(ImPlotLegendFlags_NoHighlightAxis);
    m.attr("LegendFlags_NoMenus") = static_cast<int>(ImPlotLegendFlags_NoMenus);
    m.attr("LegendFlags_Outside") = static_cast<int>(ImPlotLegendFlags_Outside);
    m.attr("LegendFlags_Horizontal") = static_cast<int>(ImPlotLegendFlags_Horizontal);
    m.attr("LegendFlags_Sort") = static_cast<int>(ImPlotLegendFlags_Sort);
    m.attr("LegendFlags_Reverse") = static_cast<int>(ImPlotLegendFlags_Reverse);

    // ImPlotMouseTextFlags
    m.attr("MouseTextFlags_None") = static_cast<int>(ImPlotMouseTextFlags_None);
    m.attr("MouseTextFlags_NoAuxAxes") = static_cast<int>(ImPlotMouseTextFlags_NoAuxAxes);
    m.attr("MouseTextFlags_NoFormat") = static_cast<int>(ImPlotMouseTextFlags_NoFormat);
    m.attr("MouseTextFlags_ShowAlways") = static_cast<int>(ImPlotMouseTextFlags_ShowAlways);

    // ImPlotDragToolFlags
    m.attr("DragToolFlags_None") = static_cast<int>(ImPlotDragToolFlags_None);
    m.attr("DragToolFlags_NoCursors") = static_cast<int>(ImPlotDragToolFlags_NoCursors);
    m.attr("DragToolFlags_NoFit") = static_cast<int>(ImPlotDragToolFlags_NoFit);
    m.attr("DragToolFlags_NoInputs") = static_cast<int>(ImPlotDragToolFlags_NoInputs);
    m.attr("DragToolFlags_Delayed") = static_cast<int>(ImPlotDragToolFlags_Delayed);

    // ImPlotColormapScaleFlags
    m.attr("ColormapScaleFlags_None") = static_cast<int>(ImPlotColormapScaleFlags_None);
    m.attr("ColormapScaleFlags_NoLabel") = static_cast<int>(ImPlotColormapScaleFlags_NoLabel);
    m.attr("ColormapScaleFlags_Opposite") = static_cast<int>(ImPlotColormapScaleFlags_Opposite);
    m.attr("ColormapScaleFlags_Invert") = static_cast<int>(ImPlotColormapScaleFlags_Invert);

    // Item flags (common) + per-plot-type flags. All go into ImPlotSpec.Flags.
    m.attr("ItemFlags_None") = static_cast<int>(ImPlotItemFlags_None);
    m.attr("ItemFlags_NoLegend") = static_cast<int>(ImPlotItemFlags_NoLegend);
    m.attr("ItemFlags_NoFit") = static_cast<int>(ImPlotItemFlags_NoFit);

    m.attr("LineFlags_None") = static_cast<int>(ImPlotLineFlags_None);
    m.attr("LineFlags_Segments") = static_cast<int>(ImPlotLineFlags_Segments);
    m.attr("LineFlags_Loop") = static_cast<int>(ImPlotLineFlags_Loop);
    m.attr("LineFlags_SkipNaN") = static_cast<int>(ImPlotLineFlags_SkipNaN);
    m.attr("LineFlags_NoClip") = static_cast<int>(ImPlotLineFlags_NoClip);
    m.attr("LineFlags_Shaded") = static_cast<int>(ImPlotLineFlags_Shaded);

    m.attr("ScatterFlags_None") = static_cast<int>(ImPlotScatterFlags_None);
    m.attr("ScatterFlags_NoClip") = static_cast<int>(ImPlotScatterFlags_NoClip);

    m.attr("StairsFlags_None") = static_cast<int>(ImPlotStairsFlags_None);
    m.attr("StairsFlags_PreStep") = static_cast<int>(ImPlotStairsFlags_PreStep);
    m.attr("StairsFlags_Shaded") = static_cast<int>(ImPlotStairsFlags_Shaded);

    m.attr("ShadedFlags_None") = static_cast<int>(ImPlotShadedFlags_None);

    m.attr("BarsFlags_None") = static_cast<int>(ImPlotBarsFlags_None);
    m.attr("BarsFlags_Horizontal") = static_cast<int>(ImPlotBarsFlags_Horizontal);

    m.attr("BarGroupsFlags_None") = static_cast<int>(ImPlotBarGroupsFlags_None);
    m.attr("BarGroupsFlags_Horizontal") = static_cast<int>(ImPlotBarGroupsFlags_Horizontal);
    m.attr("BarGroupsFlags_Stacked") = static_cast<int>(ImPlotBarGroupsFlags_Stacked);

    m.attr("ErrorBarsFlags_None") = static_cast<int>(ImPlotErrorBarsFlags_None);
    m.attr("ErrorBarsFlags_Horizontal") = static_cast<int>(ImPlotErrorBarsFlags_Horizontal);

    m.attr("StemsFlags_None") = static_cast<int>(ImPlotStemsFlags_None);
    m.attr("StemsFlags_Horizontal") = static_cast<int>(ImPlotStemsFlags_Horizontal);

    m.attr("InfLinesFlags_None") = static_cast<int>(ImPlotInfLinesFlags_None);
    m.attr("InfLinesFlags_Horizontal") = static_cast<int>(ImPlotInfLinesFlags_Horizontal);

    m.attr("PieChartFlags_None") = static_cast<int>(ImPlotPieChartFlags_None);
    m.attr("PieChartFlags_Normalize") = static_cast<int>(ImPlotPieChartFlags_Normalize);
    m.attr("PieChartFlags_IgnoreHidden") = static_cast<int>(ImPlotPieChartFlags_IgnoreHidden);
    m.attr("PieChartFlags_Exploding") = static_cast<int>(ImPlotPieChartFlags_Exploding);
    m.attr("PieChartFlags_NoSliceBorder") = static_cast<int>(ImPlotPieChartFlags_NoSliceBorder);

    m.attr("HeatmapFlags_None") = static_cast<int>(ImPlotHeatmapFlags_None);
    m.attr("HeatmapFlags_ColMajor") = static_cast<int>(ImPlotHeatmapFlags_ColMajor);

    m.attr("HistogramFlags_None") = static_cast<int>(ImPlotHistogramFlags_None);
    m.attr("HistogramFlags_Horizontal") = static_cast<int>(ImPlotHistogramFlags_Horizontal);
    m.attr("HistogramFlags_Cumulative") = static_cast<int>(ImPlotHistogramFlags_Cumulative);
    m.attr("HistogramFlags_Density") = static_cast<int>(ImPlotHistogramFlags_Density);
    m.attr("HistogramFlags_NoOutliers") = static_cast<int>(ImPlotHistogramFlags_NoOutliers);
    m.attr("HistogramFlags_ColMajor") = static_cast<int>(ImPlotHistogramFlags_ColMajor);

    m.attr("DigitalFlags_None") = static_cast<int>(ImPlotDigitalFlags_None);
    m.attr("ImageFlags_None") = static_cast<int>(ImPlotImageFlags_None);
    m.attr("TextFlags_None") = static_cast<int>(ImPlotTextFlags_None);
    m.attr("TextFlags_Vertical") = static_cast<int>(ImPlotTextFlags_Vertical);
    m.attr("DummyFlags_None") = static_cast<int>(ImPlotDummyFlags_None);

    // ImPlotCond
    m.attr("Cond_None") = static_cast<int>(ImPlotCond_None);
    m.attr("Cond_Always") = static_cast<int>(ImPlotCond_Always);
    m.attr("Cond_Once") = static_cast<int>(ImPlotCond_Once);

    // ImPlotCol
    m.attr("Col_FrameBg") = static_cast<int>(ImPlotCol_FrameBg);
    m.attr("Col_PlotBg") = static_cast<int>(ImPlotCol_PlotBg);
    m.attr("Col_PlotBorder") = static_cast<int>(ImPlotCol_PlotBorder);
    m.attr("Col_LegendBg") = static_cast<int>(ImPlotCol_LegendBg);
    m.attr("Col_LegendBorder") = static_cast<int>(ImPlotCol_LegendBorder);
    m.attr("Col_LegendText") = static_cast<int>(ImPlotCol_LegendText);
    m.attr("Col_TitleText") = static_cast<int>(ImPlotCol_TitleText);
    m.attr("Col_InlayText") = static_cast<int>(ImPlotCol_InlayText);
    m.attr("Col_AxisText") = static_cast<int>(ImPlotCol_AxisText);
    m.attr("Col_AxisGrid") = static_cast<int>(ImPlotCol_AxisGrid);
    m.attr("Col_AxisTick") = static_cast<int>(ImPlotCol_AxisTick);
    m.attr("Col_AxisBg") = static_cast<int>(ImPlotCol_AxisBg);
    m.attr("Col_AxisBgHovered") = static_cast<int>(ImPlotCol_AxisBgHovered);
    m.attr("Col_AxisBgActive") = static_cast<int>(ImPlotCol_AxisBgActive);
    m.attr("Col_Selection") = static_cast<int>(ImPlotCol_Selection);
    m.attr("Col_Crosshairs") = static_cast<int>(ImPlotCol_Crosshairs);
    m.attr("Col_COUNT") = static_cast<int>(ImPlotCol_COUNT);

    // ImPlotStyleVar
    m.attr("StyleVar_PlotDefaultSize") = static_cast<int>(ImPlotStyleVar_PlotDefaultSize);
    m.attr("StyleVar_PlotMinSize") = static_cast<int>(ImPlotStyleVar_PlotMinSize);
    m.attr("StyleVar_PlotBorderSize") = static_cast<int>(ImPlotStyleVar_PlotBorderSize);
    m.attr("StyleVar_MinorAlpha") = static_cast<int>(ImPlotStyleVar_MinorAlpha);
    m.attr("StyleVar_MajorTickLen") = static_cast<int>(ImPlotStyleVar_MajorTickLen);
    m.attr("StyleVar_MinorTickLen") = static_cast<int>(ImPlotStyleVar_MinorTickLen);
    m.attr("StyleVar_MajorTickSize") = static_cast<int>(ImPlotStyleVar_MajorTickSize);
    m.attr("StyleVar_MinorTickSize") = static_cast<int>(ImPlotStyleVar_MinorTickSize);
    m.attr("StyleVar_MajorGridSize") = static_cast<int>(ImPlotStyleVar_MajorGridSize);
    m.attr("StyleVar_MinorGridSize") = static_cast<int>(ImPlotStyleVar_MinorGridSize);
    m.attr("StyleVar_PlotPadding") = static_cast<int>(ImPlotStyleVar_PlotPadding);
    m.attr("StyleVar_LabelPadding") = static_cast<int>(ImPlotStyleVar_LabelPadding);
    m.attr("StyleVar_LegendPadding") = static_cast<int>(ImPlotStyleVar_LegendPadding);
    m.attr("StyleVar_LegendInnerPadding") = static_cast<int>(ImPlotStyleVar_LegendInnerPadding);
    m.attr("StyleVar_LegendSpacing") = static_cast<int>(ImPlotStyleVar_LegendSpacing);
    m.attr("StyleVar_MousePosPadding") = static_cast<int>(ImPlotStyleVar_MousePosPadding);
    m.attr("StyleVar_AnnotationPadding") = static_cast<int>(ImPlotStyleVar_AnnotationPadding);
    m.attr("StyleVar_FitPadding") = static_cast<int>(ImPlotStyleVar_FitPadding);
    m.attr("StyleVar_DigitalPadding") = static_cast<int>(ImPlotStyleVar_DigitalPadding);
    m.attr("StyleVar_DigitalSpacing") = static_cast<int>(ImPlotStyleVar_DigitalSpacing);

    // ImPlotScale
    m.attr("Scale_Linear") = static_cast<int>(ImPlotScale_Linear);
    m.attr("Scale_Time") = static_cast<int>(ImPlotScale_Time);
    m.attr("Scale_Log10") = static_cast<int>(ImPlotScale_Log10);
    m.attr("Scale_SymLog") = static_cast<int>(ImPlotScale_SymLog);

    // ImPlotMarker
    m.attr("Marker_None") = static_cast<int>(ImPlotMarker_None);
    m.attr("Marker_Auto") = static_cast<int>(ImPlotMarker_Auto);
    m.attr("Marker_Circle") = static_cast<int>(ImPlotMarker_Circle);
    m.attr("Marker_Square") = static_cast<int>(ImPlotMarker_Square);
    m.attr("Marker_Diamond") = static_cast<int>(ImPlotMarker_Diamond);
    m.attr("Marker_Up") = static_cast<int>(ImPlotMarker_Up);
    m.attr("Marker_Down") = static_cast<int>(ImPlotMarker_Down);
    m.attr("Marker_Left") = static_cast<int>(ImPlotMarker_Left);
    m.attr("Marker_Right") = static_cast<int>(ImPlotMarker_Right);
    m.attr("Marker_Cross") = static_cast<int>(ImPlotMarker_Cross);
    m.attr("Marker_Plus") = static_cast<int>(ImPlotMarker_Plus);
    m.attr("Marker_Asterisk") = static_cast<int>(ImPlotMarker_Asterisk);
    m.attr("Marker_Vertical") = static_cast<int>(ImPlotMarker_Vertical);
    m.attr("Marker_Horizontal") = static_cast<int>(ImPlotMarker_Horizontal);

    // ImPlotColormap (built-in)
    m.attr("Colormap_Deep") = static_cast<int>(ImPlotColormap_Deep);
    m.attr("Colormap_Dark") = static_cast<int>(ImPlotColormap_Dark);
    m.attr("Colormap_Pastel") = static_cast<int>(ImPlotColormap_Pastel);
    m.attr("Colormap_Paired") = static_cast<int>(ImPlotColormap_Paired);
    m.attr("Colormap_Viridis") = static_cast<int>(ImPlotColormap_Viridis);
    m.attr("Colormap_Plasma") = static_cast<int>(ImPlotColormap_Plasma);
    m.attr("Colormap_Hot") = static_cast<int>(ImPlotColormap_Hot);
    m.attr("Colormap_Cool") = static_cast<int>(ImPlotColormap_Cool);
    m.attr("Colormap_Pink") = static_cast<int>(ImPlotColormap_Pink);
    m.attr("Colormap_Jet") = static_cast<int>(ImPlotColormap_Jet);
    m.attr("Colormap_Twilight") = static_cast<int>(ImPlotColormap_Twilight);
    m.attr("Colormap_RdBu") = static_cast<int>(ImPlotColormap_RdBu);
    m.attr("Colormap_BrBG") = static_cast<int>(ImPlotColormap_BrBG);
    m.attr("Colormap_PiYG") = static_cast<int>(ImPlotColormap_PiYG);
    m.attr("Colormap_Spectral") = static_cast<int>(ImPlotColormap_Spectral);
    m.attr("Colormap_Greys") = static_cast<int>(ImPlotColormap_Greys);

    // ImPlotLocation
    m.attr("Location_Center") = static_cast<int>(ImPlotLocation_Center);
    m.attr("Location_North") = static_cast<int>(ImPlotLocation_North);
    m.attr("Location_South") = static_cast<int>(ImPlotLocation_South);
    m.attr("Location_West") = static_cast<int>(ImPlotLocation_West);
    m.attr("Location_East") = static_cast<int>(ImPlotLocation_East);
    m.attr("Location_NorthWest") = static_cast<int>(ImPlotLocation_NorthWest);
    m.attr("Location_NorthEast") = static_cast<int>(ImPlotLocation_NorthEast);
    m.attr("Location_SouthWest") = static_cast<int>(ImPlotLocation_SouthWest);
    m.attr("Location_SouthEast") = static_cast<int>(ImPlotLocation_SouthEast);

    // ImPlotBin (auto binning methods)
    m.attr("Bin_Sqrt") = static_cast<int>(ImPlotBin_Sqrt);
    m.attr("Bin_Sturges") = static_cast<int>(ImPlotBin_Sturges);
    m.attr("Bin_Rice") = static_cast<int>(ImPlotBin_Rice);
    m.attr("Bin_Scott") = static_cast<int>(ImPlotBin_Scott);

    // ─────────────────────────────────────────────────────────────────────
    // Value structs
    // ─────────────────────────────────────────────────────────────────────
    py::class_<ImPlotPoint>(m, "ImPlotPoint")
        .def(py::init<>())
        .def(py::init<double, double>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &ImPlotPoint::x)
        .def_readwrite("y", &ImPlotPoint::y)
        .def("__repr__", [](const ImPlotPoint& p) {
            return "ImPlotPoint(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
        });

    py::class_<ImPlotRange>(m, "ImPlotRange")
        .def(py::init<>())
        .def(py::init<double, double>(), py::arg("min"), py::arg("max"))
        .def_readwrite("min", &ImPlotRange::Min)
        .def_readwrite("max", &ImPlotRange::Max)
        .def("size", &ImPlotRange::Size)
        .def("contains", &ImPlotRange::Contains, py::arg("value"))
        .def("clamp", &ImPlotRange::Clamp, py::arg("value"));

    py::class_<ImPlotRect>(m, "ImPlotRect")
        .def(py::init<>())
        .def(py::init<double, double, double, double>(), py::arg("x_min"), py::arg("x_max"),
             py::arg("y_min"), py::arg("y_max"))
        .def_readwrite("x", &ImPlotRect::X)
        .def_readwrite("y", &ImPlotRect::Y)
        .def_property_readonly("x_min", [](const ImPlotRect& r) { return r.X.Min; })
        .def_property_readonly("x_max", [](const ImPlotRect& r) { return r.X.Max; })
        .def_property_readonly("y_min", [](const ImPlotRect& r) { return r.Y.Min; })
        .def_property_readonly("y_max", [](const ImPlotRect& r) { return r.Y.Max; });

    // ImPlotSpec: per-item style. Array-pointer props (LineColors, FillColors,
    // MarkerSizes, ...) are intentionally not exposed — they require caller-owned
    // buffers with lifetimes ImPlot outlives. Single colors cover the UI needs.
    py::class_<ImPlotSpec>(m, "ImPlotSpec")
        .def(py::init<>())
        .def_property(
            "line_color", [](const ImPlotSpec& s) { return FromVec4(s.LineColor); },
            [](ImPlotSpec& s, Color c) { s.LineColor = ToVec4(c); })
        .def_readwrite("line_weight", &ImPlotSpec::LineWeight)
        .def_property(
            "fill_color", [](const ImPlotSpec& s) { return FromVec4(s.FillColor); },
            [](ImPlotSpec& s, Color c) { s.FillColor = ToVec4(c); })
        .def_readwrite("fill_alpha", &ImPlotSpec::FillAlpha)
        .def_readwrite("marker", &ImPlotSpec::Marker)
        .def_readwrite("marker_size", &ImPlotSpec::MarkerSize)
        .def_property(
            "marker_line_color", [](const ImPlotSpec& s) { return FromVec4(s.MarkerLineColor); },
            [](ImPlotSpec& s, Color c) { s.MarkerLineColor = ToVec4(c); })
        .def_property(
            "marker_fill_color", [](const ImPlotSpec& s) { return FromVec4(s.MarkerFillColor); },
            [](ImPlotSpec& s, Color c) { s.MarkerFillColor = ToVec4(c); })
        .def_readwrite("size", &ImPlotSpec::Size)
        .def_readwrite("offset", &ImPlotSpec::Offset)
        .def_readwrite("stride", &ImPlotSpec::Stride)
        .def_readwrite("flags", &ImPlotSpec::Flags);

    // Ergonomic builder: implot.make_spec(line_col=..., marker=..., flags=...).
    m.def(
        "make_spec",
        [](std::optional<Color> line_col, float line_weight, std::optional<Color> fill_col,
           float fill_alpha, int marker, float marker_size, std::optional<Color> marker_line_col,
           std::optional<Color> marker_fill_col, float size, int flags) {
            ImPlotSpec s;
            s.LineColor = ToColor(line_col);
            s.LineWeight = line_weight;
            s.FillColor = ToColor(fill_col);
            s.FillAlpha = fill_alpha;
            s.Marker = static_cast<ImPlotMarker>(marker);
            s.MarkerSize = marker_size;
            s.MarkerLineColor = ToColor(marker_line_col);
            s.MarkerFillColor = ToColor(marker_fill_col);
            s.Size = size;
            s.Flags = flags;
            return s;
        },
        py::arg("line_col") = py::none(), py::arg("line_weight") = 1.0f,
        py::arg("fill_col") = py::none(), py::arg("fill_alpha") = 1.0f,
        py::arg("marker") = static_cast<int>(ImPlotMarker_None), py::arg("marker_size") = 4.0f,
        py::arg("marker_line_col") = py::none(), py::arg("marker_fill_col") = py::none(),
        py::arg("size") = 4.0f, py::arg("flags") = 0);

    // ImPlotStyle (live reference via get_style()).
    py::class_<ImPlotStyle>(m, "ImPlotStyle")
        .def_readwrite("plot_border_size", &ImPlotStyle::PlotBorderSize)
        .def_readwrite("minor_alpha", &ImPlotStyle::MinorAlpha)
        .def_readwrite("digital_padding", &ImPlotStyle::DigitalPadding)
        .def_readwrite("digital_spacing", &ImPlotStyle::DigitalSpacing)
        .def_readwrite("colormap", &ImPlotStyle::Colormap)
        .def_readwrite("use_local_time", &ImPlotStyle::UseLocalTime)
        .def_readwrite("use_iso_8601", &ImPlotStyle::UseISO8601)
        .def_readwrite("use_24_hour_clock", &ImPlotStyle::Use24HourClock)
#define IMPLOT_STYLE_VEC2(py_name, field)                                              \
    .def_property(                                                                      \
        py_name, [](const ImPlotStyle& s) { return FromVec2(s.field); },               \
        [](ImPlotStyle& s, Vec2f v) { s.field = ImVec2(v[0], v[1]); })
        IMPLOT_STYLE_VEC2("plot_default_size", PlotDefaultSize)
        IMPLOT_STYLE_VEC2("plot_min_size", PlotMinSize)
        IMPLOT_STYLE_VEC2("major_tick_len", MajorTickLen)
        IMPLOT_STYLE_VEC2("minor_tick_len", MinorTickLen)
        IMPLOT_STYLE_VEC2("major_tick_size", MajorTickSize)
        IMPLOT_STYLE_VEC2("minor_tick_size", MinorTickSize)
        IMPLOT_STYLE_VEC2("major_grid_size", MajorGridSize)
        IMPLOT_STYLE_VEC2("minor_grid_size", MinorGridSize)
        IMPLOT_STYLE_VEC2("plot_padding", PlotPadding)
        IMPLOT_STYLE_VEC2("label_padding", LabelPadding)
        IMPLOT_STYLE_VEC2("legend_padding", LegendPadding)
        IMPLOT_STYLE_VEC2("legend_inner_padding", LegendInnerPadding)
        IMPLOT_STYLE_VEC2("legend_spacing", LegendSpacing)
        IMPLOT_STYLE_VEC2("mouse_pos_padding", MousePosPadding)
        IMPLOT_STYLE_VEC2("annotation_padding", AnnotationPadding)
        IMPLOT_STYLE_VEC2("fit_padding", FitPadding)
#undef IMPLOT_STYLE_VEC2
        .def(
            "get_color",
            [](const ImPlotStyle& s, int idx) {
                if (idx < 0 || idx >= ImPlotCol_COUNT) {
                    return Color{0, 0, 0, 0};
                }
                return FromVec4(s.Colors[idx]);
            },
            py::arg("idx"))
        .def(
            "set_color",
            [](ImPlotStyle& s, int idx, Color c) {
                if (idx >= 0 && idx < ImPlotCol_COUNT) {
                    s.Colors[idx] = ToVec4(c);
                }
            },
            py::arg("idx"), py::arg("color"));

    // ─────────────────────────────────────────────────────────────────────
    // Plot & subplot lifecycle
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "begin_plot",
        [](const char* title_id, float width, float height, int flags) {
            return ImPlot::BeginPlot(title_id, ImVec2(width, height), static_cast<ImPlotFlags>(flags));
        },
        py::arg("title_id"), py::arg("width") = -1.0f, py::arg("height") = 0.0f, py::arg("flags") = 0);
    m.def("end_plot", &ImPlot::EndPlot);

    m.def(
        "begin_subplots",
        [](const char* title_id, int rows, int cols, float width, float height, int flags,
           std::optional<std::vector<float>> row_ratios, std::optional<std::vector<float>> col_ratios) {
            float* rr = (row_ratios && !row_ratios->empty()) ? row_ratios->data() : nullptr;
            float* cr = (col_ratios && !col_ratios->empty()) ? col_ratios->data() : nullptr;
            return ImPlot::BeginSubplots(title_id, rows, cols, ImVec2(width, height),
                                         static_cast<ImPlotSubplotFlags>(flags), rr, cr);
        },
        py::arg("title_id"), py::arg("rows"), py::arg("cols"), py::arg("width"), py::arg("height"),
        py::arg("flags") = 0, py::arg("row_ratios") = py::none(), py::arg("col_ratios") = py::none());
    m.def("end_subplots", &ImPlot::EndSubplots);

    m.def(
        "begin_aligned_plots",
        [](const char* group_id, bool vertical) { return ImPlot::BeginAlignedPlots(group_id, vertical); },
        py::arg("group_id"), py::arg("vertical") = true);
    m.def("end_aligned_plots", &ImPlot::EndAlignedPlots);

    // ─────────────────────────────────────────────────────────────────────
    // Setup (call after begin_plot, before item calls)
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "setup_axes",
        [](const char* x_label, const char* y_label, int x_flags, int y_flags) {
            ImPlot::SetupAxes(x_label, y_label, static_cast<ImPlotAxisFlags>(x_flags),
                              static_cast<ImPlotAxisFlags>(y_flags));
        },
        py::arg("x_label") = nullptr, py::arg("y_label") = nullptr, py::arg("x_flags") = 0, py::arg("y_flags") = 0);
    m.def(
        "setup_axis",
        [](int axis, const char* label, int flags) {
            ImPlot::SetupAxis(static_cast<ImAxis>(axis), label, static_cast<ImPlotAxisFlags>(flags));
        },
        py::arg("axis"), py::arg("label") = nullptr, py::arg("flags") = 0);
    m.def(
        "setup_axis_limits",
        [](int axis, double v_min, double v_max, int cond) {
            ImPlot::SetupAxisLimits(static_cast<ImAxis>(axis), v_min, v_max, static_cast<ImPlotCond>(cond));
        },
        py::arg("axis"), py::arg("v_min"), py::arg("v_max"), py::arg("cond") = static_cast<int>(ImPlotCond_Once));
    m.def(
        "setup_axes_limits",
        [](double x_min, double x_max, double y_min, double y_max, int cond) {
            ImPlot::SetupAxesLimits(x_min, x_max, y_min, y_max, static_cast<ImPlotCond>(cond));
        },
        py::arg("x_min"), py::arg("x_max"), py::arg("y_min"), py::arg("y_max"),
        py::arg("cond") = static_cast<int>(ImPlotCond_Once));
    m.def(
        "setup_axis_format",
        [](int axis, const char* fmt) { ImPlot::SetupAxisFormat(static_cast<ImAxis>(axis), fmt); },
        py::arg("axis"), py::arg("fmt"));
    m.def(
        "setup_axis_scale",
        [](int axis, int scale) { ImPlot::SetupAxisScale(static_cast<ImAxis>(axis), static_cast<ImPlotScale>(scale)); },
        py::arg("axis"), py::arg("scale"));
    m.def(
        "setup_axis_ticks",
        [](int axis, std::vector<double> values, std::vector<std::string> labels, bool keep_default) {
            if (values.empty()) {
                return;
            }
            std::vector<const char*> clabels = CStrs(labels);
            const char* const* labels_ptr = clabels.empty() ? nullptr : clabels.data();
            ImPlot::SetupAxisTicks(static_cast<ImAxis>(axis), values.data(), static_cast<int>(values.size()),
                                   labels_ptr, keep_default);
        },
        py::arg("axis"), py::arg("values"), py::arg("labels") = std::vector<std::string>(),
        py::arg("keep_default") = false);
    m.def(
        "setup_axis_ticks_range",
        [](int axis, double v_min, double v_max, int n_ticks, std::vector<std::string> labels, bool keep_default) {
            std::vector<const char*> clabels = CStrs(labels);
            const char* const* labels_ptr = clabels.empty() ? nullptr : clabels.data();
            ImPlot::SetupAxisTicks(static_cast<ImAxis>(axis), v_min, v_max, n_ticks, labels_ptr, keep_default);
        },
        py::arg("axis"), py::arg("v_min"), py::arg("v_max"), py::arg("n_ticks"),
        py::arg("labels") = std::vector<std::string>(), py::arg("keep_default") = false);
    m.def(
        "setup_axis_limits_constraints",
        [](int axis, double v_min, double v_max) {
            ImPlot::SetupAxisLimitsConstraints(static_cast<ImAxis>(axis), v_min, v_max);
        },
        py::arg("axis"), py::arg("v_min"), py::arg("v_max"));
    m.def(
        "setup_axis_zoom_constraints",
        [](int axis, double z_min, double z_max) {
            ImPlot::SetupAxisZoomConstraints(static_cast<ImAxis>(axis), z_min, z_max);
        },
        py::arg("axis"), py::arg("z_min"), py::arg("z_max"));
    m.def(
        "setup_legend",
        [](int location, int flags) {
            ImPlot::SetupLegend(static_cast<ImPlotLocation>(location), static_cast<ImPlotLegendFlags>(flags));
        },
        py::arg("location"), py::arg("flags") = 0);
    m.def(
        "setup_mouse_text",
        [](int location, int flags) {
            ImPlot::SetupMouseText(static_cast<ImPlotLocation>(location), static_cast<ImPlotMouseTextFlags>(flags));
        },
        py::arg("location"), py::arg("flags") = 0);
    m.def("setup_finish", &ImPlot::SetupFinish);

    m.def(
        "set_next_axis_limits",
        [](int axis, double v_min, double v_max, int cond) {
            ImPlot::SetNextAxisLimits(static_cast<ImAxis>(axis), v_min, v_max, static_cast<ImPlotCond>(cond));
        },
        py::arg("axis"), py::arg("v_min"), py::arg("v_max"), py::arg("cond") = static_cast<int>(ImPlotCond_Once));
    m.def(
        "set_next_axis_to_fit", [](int axis) { ImPlot::SetNextAxisToFit(static_cast<ImAxis>(axis)); },
        py::arg("axis"));
    m.def(
        "set_next_axes_limits",
        [](double x_min, double x_max, double y_min, double y_max, int cond) {
            ImPlot::SetNextAxesLimits(x_min, x_max, y_min, y_max, static_cast<ImPlotCond>(cond));
        },
        py::arg("x_min"), py::arg("x_max"), py::arg("y_min"), py::arg("y_max"),
        py::arg("cond") = static_cast<int>(ImPlotCond_Once));
    m.def("set_next_axes_to_fit", &ImPlot::SetNextAxesToFit);

    // ─────────────────────────────────────────────────────────────────────
    // Items — lines
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_line",
        [](const char* label, std::vector<float> values, double xscale, double xstart, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotLine(label, values.data(), static_cast<int>(values.size()), xscale, xstart, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("xscale") = 1.0, py::arg("xstart") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_line_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotLine(label, xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — scatter
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_scatter",
        [](const char* label, std::vector<float> values, double xscale, double xstart, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotScatter(label, values.data(), static_cast<int>(values.size()), xscale, xstart, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("xscale") = 1.0, py::arg("xstart") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_scatter_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotScatter(label, xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — stairs
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_stairs",
        [](const char* label, std::vector<float> values, double xscale, double xstart, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotStairs(label, values.data(), static_cast<int>(values.size()), xscale, xstart, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("xscale") = 1.0, py::arg("xstart") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_stairs_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotStairs(label, xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — shaded (filled areas)
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_shaded",
        [](const char* label, std::vector<float> values, double yref, double xscale, double xstart, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotShaded(label, values.data(), static_cast<int>(values.size()), yref, xscale, xstart, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("yref") = 0.0, py::arg("xscale") = 1.0, py::arg("xstart") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_shaded_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, double yref, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotShaded(label, xs.data(), ys.data(), static_cast<int>(xs.size()), yref, spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("yref") = 0.0, py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_shaded_between",
        [](const char* label, std::vector<float> xs, std::vector<float> ys1, std::vector<float> ys2, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys1.size() || xs.size() != ys2.size()) {
                return;
            }
            ImPlot::PlotShaded(label, xs.data(), ys1.data(), ys2.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys1"), py::arg("ys2"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — bars / bar groups
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_bars",
        [](const char* label, std::vector<float> values, double bar_size, double shift, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotBars(label, values.data(), static_cast<int>(values.size()), bar_size, shift, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("bar_size") = 0.67, py::arg("shift") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_bars_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, double bar_size, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotBars(label, xs.data(), ys.data(), static_cast<int>(xs.size()), bar_size, spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("bar_size") = 0.67, py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_bar_groups",
        [](std::vector<std::string> label_ids, std::vector<float> values, int item_count, int group_count,
           double group_size, double shift, ImPlotSpec spec) {
            if (label_ids.empty() || values.empty() ||
                static_cast<int>(values.size()) < item_count * group_count) {
                return;
            }
            std::vector<const char*> clabels = CStrs(label_ids);
            ImPlot::PlotBarGroups(clabels.data(), values.data(), item_count, group_count, group_size, shift, spec);
        },
        py::arg("label_ids"), py::arg("values"), py::arg("item_count"), py::arg("group_count"),
        py::arg("group_size") = 0.67, py::arg("shift") = 0.0, py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — error bars, stems, inf lines
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_error_bars",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, std::vector<float> err, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size() || xs.size() != err.size()) {
                return;
            }
            ImPlot::PlotErrorBars(label, xs.data(), ys.data(), err.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("err"), py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_error_bars_asym",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, std::vector<float> neg,
           std::vector<float> pos, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size() || xs.size() != neg.size() || xs.size() != pos.size()) {
                return;
            }
            ImPlot::PlotErrorBars(label, xs.data(), ys.data(), neg.data(), pos.data(),
                                  static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("neg"), py::arg("pos"),
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_stems",
        [](const char* label, std::vector<float> values, double ref, double scale, double start, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotStems(label, values.data(), static_cast<int>(values.size()), ref, scale, start, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("ref") = 0.0, py::arg("scale") = 1.0, py::arg("start") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_stems_xy",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, double ref, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotStems(label, xs.data(), ys.data(), static_cast<int>(xs.size()), ref, spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("ref") = 0.0, py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_inf_lines",
        [](const char* label, std::vector<float> values, ImPlotSpec spec) {
            if (values.empty()) {
                return;
            }
            ImPlot::PlotInfLines(label, values.data(), static_cast<int>(values.size()), spec);
        },
        py::arg("label"), py::arg("values"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — pie chart, heatmap, histograms
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_pie_chart",
        [](std::vector<std::string> label_ids, std::vector<float> values, double x, double y, double radius,
           const std::string& label_fmt, double angle0, ImPlotSpec spec) {
            if (label_ids.empty() || values.empty()) {
                return;
            }
            std::vector<const char*> clabels = CStrs(label_ids);
            ImPlot::PlotPieChart(clabels.data(), values.data(), static_cast<int>(values.size()), x, y, radius,
                                 label_fmt.c_str(), angle0, spec);
        },
        py::arg("label_ids"), py::arg("values"), py::arg("x"), py::arg("y"), py::arg("radius"),
        py::arg("label_fmt") = "%.1f", py::arg("angle0") = 90.0, py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_heatmap",
        [](const char* label, std::vector<float> values, int rows, int cols, double scale_min, double scale_max,
           const std::string& label_fmt, double bounds_min_x, double bounds_min_y, double bounds_max_x,
           double bounds_max_y, ImPlotSpec spec) {
            if (values.empty() || static_cast<int>(values.size()) < rows * cols) {
                return;
            }
            ImPlot::PlotHeatmap(label, values.data(), rows, cols, scale_min, scale_max, label_fmt.c_str(),
                                ImPlotPoint(bounds_min_x, bounds_min_y), ImPlotPoint(bounds_max_x, bounds_max_y),
                                spec);
        },
        py::arg("label"), py::arg("values"), py::arg("rows"), py::arg("cols"), py::arg("scale_min") = 0.0,
        py::arg("scale_max") = 0.0, py::arg("label_fmt") = "%.1f", py::arg("bounds_min_x") = 0.0,
        py::arg("bounds_min_y") = 0.0, py::arg("bounds_max_x") = 1.0, py::arg("bounds_max_y") = 1.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_histogram",
        [](const char* label, std::vector<float> values, int bins, double bar_scale, double range_min,
           double range_max, ImPlotSpec spec) {
            if (values.empty()) {
                return 0.0;
            }
            ImPlotRange range = (range_max > range_min) ? ImPlotRange(range_min, range_max) : ImPlotRange();
            return ImPlot::PlotHistogram(label, values.data(), static_cast<int>(values.size()), bins, bar_scale,
                                         range, spec);
        },
        py::arg("label"), py::arg("values"), py::arg("bins") = static_cast<int>(ImPlotBin_Sturges),
        py::arg("bar_scale") = 1.0, py::arg("range_min") = 0.0, py::arg("range_max") = 0.0,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_histogram_2d",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, int x_bins, int y_bins,
           double x_min, double x_max, double y_min, double y_max, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return 0.0;
            }
            ImPlotRect range = (x_max > x_min && y_max > y_min) ? ImPlotRect(x_min, x_max, y_min, y_max)
                                                                : ImPlotRect();
            return ImPlot::PlotHistogram2D(label, xs.data(), ys.data(), static_cast<int>(xs.size()), x_bins,
                                           y_bins, range, spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("x_bins") = static_cast<int>(ImPlotBin_Sturges),
        py::arg("y_bins") = static_cast<int>(ImPlotBin_Sturges), py::arg("x_min") = 0.0, py::arg("x_max") = 0.0,
        py::arg("y_min") = 0.0, py::arg("y_max") = 0.0, py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Items — digital, text, dummy
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "plot_digital",
        [](const char* label, std::vector<float> xs, std::vector<float> ys, ImPlotSpec spec) {
            if (xs.empty() || xs.size() != ys.size()) {
                return;
            }
            ImPlot::PlotDigital(label, xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
        },
        py::arg("label"), py::arg("xs"), py::arg("ys"), py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_text",
        [](const char* text, double x, double y, float pix_offset_x, float pix_offset_y, ImPlotSpec spec) {
            ImPlot::PlotText(text, x, y, ImVec2(pix_offset_x, pix_offset_y), spec);
        },
        py::arg("text"), py::arg("x"), py::arg("y"), py::arg("pix_offset_x") = 0.0f, py::arg("pix_offset_y") = 0.0f,
        py::arg("spec") = ImPlotSpec());
    m.def(
        "plot_dummy", [](const char* label, ImPlotSpec spec) { ImPlot::PlotDummy(label, spec); },
        py::arg("label"), py::arg("spec") = ImPlotSpec());

    // ─────────────────────────────────────────────────────────────────────
    // Plot utils / queries / coordinate conversion
    // ─────────────────────────────────────────────────────────────────────
    m.def("set_axis", [](int axis) { ImPlot::SetAxis(static_cast<ImAxis>(axis)); }, py::arg("axis"));
    m.def(
        "set_axes", [](int x_axis, int y_axis) { ImPlot::SetAxes(static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis)); },
        py::arg("x_axis"), py::arg("y_axis"));
    m.def(
        "pixels_to_plot",
        [](float x, float y, int x_axis, int y_axis) {
            return ImPlot::PixelsToPlot(x, y, static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis));
        },
        py::arg("x"), py::arg("y"), py::arg("x_axis") = IMPLOT_AUTO, py::arg("y_axis") = IMPLOT_AUTO);
    m.def(
        "plot_to_pixels",
        [](double x, double y, int x_axis, int y_axis) {
            ImVec2 p = ImPlot::PlotToPixels(x, y, static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis));
            return FromVec2(p);
        },
        py::arg("x"), py::arg("y"), py::arg("x_axis") = IMPLOT_AUTO, py::arg("y_axis") = IMPLOT_AUTO);
    m.def("get_plot_pos", []() { return FromVec2(ImPlot::GetPlotPos()); });
    m.def("get_plot_size", []() { return FromVec2(ImPlot::GetPlotSize()); });
    m.def(
        "get_plot_mouse_pos",
        [](int x_axis, int y_axis) {
            return ImPlot::GetPlotMousePos(static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis));
        },
        py::arg("x_axis") = IMPLOT_AUTO, py::arg("y_axis") = IMPLOT_AUTO);
    m.def(
        "get_plot_limits",
        [](int x_axis, int y_axis) {
            return ImPlot::GetPlotLimits(static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis));
        },
        py::arg("x_axis") = IMPLOT_AUTO, py::arg("y_axis") = IMPLOT_AUTO);
    m.def("is_plot_hovered", &ImPlot::IsPlotHovered);
    m.def("is_axis_hovered", [](int axis) { return ImPlot::IsAxisHovered(static_cast<ImAxis>(axis)); }, py::arg("axis"));
    m.def("is_subplots_hovered", &ImPlot::IsSubplotsHovered);
    m.def("is_plot_selected", &ImPlot::IsPlotSelected);
    m.def(
        "get_plot_selection",
        [](int x_axis, int y_axis) {
            return ImPlot::GetPlotSelection(static_cast<ImAxis>(x_axis), static_cast<ImAxis>(y_axis));
        },
        py::arg("x_axis") = IMPLOT_AUTO, py::arg("y_axis") = IMPLOT_AUTO);
    m.def("cancel_plot_selection", &ImPlot::CancelPlotSelection);
    m.def(
        "hide_next_item",
        [](bool hidden, int cond) { ImPlot::HideNextItem(hidden, static_cast<ImPlotCond>(cond)); },
        py::arg("hidden") = true, py::arg("cond") = static_cast<int>(ImPlotCond_Once));

    // ─────────────────────────────────────────────────────────────────────
    // Legend utils
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "begin_legend_popup",
        [](const char* label_id, int mouse_button) { return ImPlot::BeginLegendPopup(label_id, mouse_button); },
        py::arg("label_id"), py::arg("mouse_button") = 1);
    m.def("end_legend_popup", &ImPlot::EndLegendPopup);
    m.def("is_legend_entry_hovered", [](const char* label_id) { return ImPlot::IsLegendEntryHovered(label_id); },
          py::arg("label_id"));

    // ─────────────────────────────────────────────────────────────────────
    // Drag tools — return (changed_or_held, new value(s))
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "drag_point",
        [](int id, double x, double y, Color col, float size, int flags) {
            bool held = ImPlot::DragPoint(id, &x, &y, ToVec4(col), size, static_cast<ImPlotDragToolFlags>(flags));
            return std::make_tuple(held, x, y);
        },
        py::arg("id"), py::arg("x"), py::arg("y"), py::arg("col"), py::arg("size") = 4.0f, py::arg("flags") = 0);
    m.def(
        "drag_line_x",
        [](int id, double x, Color col, float thickness, int flags) {
            bool held = ImPlot::DragLineX(id, &x, ToVec4(col), thickness, static_cast<ImPlotDragToolFlags>(flags));
            return std::make_tuple(held, x);
        },
        py::arg("id"), py::arg("x"), py::arg("col"), py::arg("thickness") = 1.0f, py::arg("flags") = 0);
    m.def(
        "drag_line_y",
        [](int id, double y, Color col, float thickness, int flags) {
            bool held = ImPlot::DragLineY(id, &y, ToVec4(col), thickness, static_cast<ImPlotDragToolFlags>(flags));
            return std::make_tuple(held, y);
        },
        py::arg("id"), py::arg("y"), py::arg("col"), py::arg("thickness") = 1.0f, py::arg("flags") = 0);
    m.def(
        "drag_rect",
        [](int id, double x1, double y1, double x2, double y2, Color col, int flags) {
            bool held = ImPlot::DragRect(id, &x1, &y1, &x2, &y2, ToVec4(col),
                                         static_cast<ImPlotDragToolFlags>(flags));
            return std::make_tuple(held, x1, y1, x2, y2);
        },
        py::arg("id"), py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"),
        py::arg("flags") = 0);

    // ─────────────────────────────────────────────────────────────────────
    // Annotations & tags (text variants pass user text through "%s" safely)
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "annotation",
        [](double x, double y, Color col, float off_x, float off_y, bool clamp, bool round) {
            ImPlot::Annotation(x, y, ToVec4(col), ImVec2(off_x, off_y), clamp, round);
        },
        py::arg("x"), py::arg("y"), py::arg("col"), py::arg("off_x") = 0.0f, py::arg("off_y") = 0.0f,
        py::arg("clamp") = false, py::arg("round") = false);
    m.def(
        "annotation_text",
        [](double x, double y, Color col, float off_x, float off_y, bool clamp, const std::string& text) {
            ImPlot::Annotation(x, y, ToVec4(col), ImVec2(off_x, off_y), clamp, "%s", text.c_str());
        },
        py::arg("x"), py::arg("y"), py::arg("col"), py::arg("off_x"), py::arg("off_y"), py::arg("clamp"),
        py::arg("text"));
    m.def("tag_x", [](double x, Color col, bool round) { ImPlot::TagX(x, ToVec4(col), round); },
          py::arg("x"), py::arg("col"), py::arg("round") = false);
    m.def("tag_x_text", [](double x, Color col, const std::string& text) { ImPlot::TagX(x, ToVec4(col), "%s", text.c_str()); },
          py::arg("x"), py::arg("col"), py::arg("text"));
    m.def("tag_y", [](double y, Color col, bool round) { ImPlot::TagY(y, ToVec4(col), round); },
          py::arg("y"), py::arg("col"), py::arg("round") = false);
    m.def("tag_y_text", [](double y, Color col, const std::string& text) { ImPlot::TagY(y, ToVec4(col), "%s", text.c_str()); },
          py::arg("y"), py::arg("col"), py::arg("text"));

    // ─────────────────────────────────────────────────────────────────────
    // Styling
    // ─────────────────────────────────────────────────────────────────────
    m.def("get_style", []() -> ImPlotStyle& { return ImPlot::GetStyle(); }, py::return_value_policy::reference);
    m.def("style_colors_auto", []() { ImPlot::StyleColorsAuto(nullptr); });
    m.def("style_colors_classic", []() { ImPlot::StyleColorsClassic(nullptr); });
    m.def("style_colors_dark", []() { ImPlot::StyleColorsDark(nullptr); });
    m.def("style_colors_light", []() { ImPlot::StyleColorsLight(nullptr); });
    m.def(
        "push_style_color", [](int idx, Color col) { ImPlot::PushStyleColor(static_cast<ImPlotCol>(idx), ToVec4(col)); },
        py::arg("idx"), py::arg("col"));
    m.def("pop_style_color", [](int count) { ImPlot::PopStyleColor(count); }, py::arg("count") = 1);
    m.def(
        "push_style_var", [](int idx, float val) { ImPlot::PushStyleVar(static_cast<ImPlotStyleVar>(idx), val); },
        py::arg("idx"), py::arg("val"));
    m.def(
        "push_style_var_int", [](int idx, int val) { ImPlot::PushStyleVar(static_cast<ImPlotStyleVar>(idx), val); },
        py::arg("idx"), py::arg("val"));
    m.def(
        "push_style_var_vec2",
        [](int idx, float x, float y) { ImPlot::PushStyleVar(static_cast<ImPlotStyleVar>(idx), ImVec2(x, y)); },
        py::arg("idx"), py::arg("x"), py::arg("y"));
    m.def("pop_style_var", [](int count) { ImPlot::PopStyleVar(count); }, py::arg("count") = 1);
    m.def("get_style_color_name", [](int idx) { return std::string(ImPlot::GetStyleColorName(static_cast<ImPlotCol>(idx))); },
          py::arg("idx"));
    m.def("get_marker_name", [](int idx) { return std::string(ImPlot::GetMarkerName(static_cast<ImPlotMarker>(idx))); },
          py::arg("idx"));

    // ─────────────────────────────────────────────────────────────────────
    // Colormaps
    // ─────────────────────────────────────────────────────────────────────
    m.def(
        "add_colormap",
        [](const char* name, std::vector<Color> cols, bool qual) {
            if (cols.empty()) {
                return -1;
            }
            std::vector<ImVec4> vecs;
            vecs.reserve(cols.size());
            for (const Color& c : cols) {
                vecs.push_back(ToVec4(c));
            }
            return static_cast<int>(ImPlot::AddColormap(name, vecs.data(), static_cast<int>(vecs.size()), qual));
        },
        py::arg("name"), py::arg("colors"), py::arg("qual") = true);
    m.def("get_colormap_count", &ImPlot::GetColormapCount);
    m.def("get_colormap_name", [](int cmap) { return std::string(ImPlot::GetColormapName(cmap)); }, py::arg("cmap"));
    m.def("get_colormap_index", [](const char* name) { return static_cast<int>(ImPlot::GetColormapIndex(name)); },
          py::arg("name"));
    m.def("push_colormap", [](int cmap) { ImPlot::PushColormap(static_cast<ImPlotColormap>(cmap)); }, py::arg("cmap"));
    m.def("push_colormap_by_name", [](const char* name) { ImPlot::PushColormap(name); }, py::arg("name"));
    m.def("pop_colormap", [](int count) { ImPlot::PopColormap(count); }, py::arg("count") = 1);
    m.def("next_colormap_color", []() { return FromVec4(ImPlot::NextColormapColor()); });
    m.def("get_colormap_size", [](int cmap) { return ImPlot::GetColormapSize(cmap); }, py::arg("cmap") = IMPLOT_AUTO);
    m.def(
        "get_colormap_color", [](int idx, int cmap) { return FromVec4(ImPlot::GetColormapColor(idx, cmap)); },
        py::arg("idx"), py::arg("cmap") = IMPLOT_AUTO);
    m.def(
        "sample_colormap", [](float t, int cmap) { return FromVec4(ImPlot::SampleColormap(t, cmap)); },
        py::arg("t"), py::arg("cmap") = IMPLOT_AUTO);
    m.def(
        "colormap_scale",
        [](const char* label, double scale_min, double scale_max, float width, float height,
           const std::string& format, int flags, int cmap) {
            ImPlot::ColormapScale(label, scale_min, scale_max, ImVec2(width, height), format.c_str(),
                                  static_cast<ImPlotColormapScaleFlags>(flags), cmap);
        },
        py::arg("label"), py::arg("scale_min"), py::arg("scale_max"), py::arg("width") = 0.0f,
        py::arg("height") = 0.0f, py::arg("format") = "%g", py::arg("flags") = 0, py::arg("cmap") = IMPLOT_AUTO);
    m.def(
        "colormap_slider",
        [](const char* label, float t, const std::string& format, int cmap) {
            ImVec4 out(0, 0, 0, 0);
            bool changed = ImPlot::ColormapSlider(label, &t, &out, format.c_str(), cmap);
            return std::make_tuple(changed, t, FromVec4(out));
        },
        py::arg("label"), py::arg("t"), py::arg("format") = "", py::arg("cmap") = IMPLOT_AUTO);
    m.def(
        "colormap_button",
        [](const char* label, float width, float height, int cmap) {
            return ImPlot::ColormapButton(label, ImVec2(width, height), cmap);
        },
        py::arg("label"), py::arg("width") = 0.0f, py::arg("height") = 0.0f, py::arg("cmap") = IMPLOT_AUTO);

    // ─────────────────────────────────────────────────────────────────────
    // Demo / diagnostics (handy while wiring plots up)
    // ─────────────────────────────────────────────────────────────────────
    m.def("show_demo_window", []() { ImPlot::ShowDemoWindow(nullptr); });
    m.def("show_metrics_window", []() { ImPlot::ShowMetricsWindow(nullptr); });
    m.def("show_style_editor", []() { ImPlot::ShowStyleEditor(nullptr); });
    m.def("show_user_guide", &ImPlot::ShowUserGuide);
    m.def("show_style_selector", [](const char* label) { return ImPlot::ShowStyleSelector(label); }, py::arg("label"));
    m.def("show_colormap_selector", [](const char* label) { return ImPlot::ShowColormapSelector(label); },
          py::arg("label"));
    m.def("bust_color_cache", []() { ImPlot::BustColorCache(nullptr); });
}

}  // namespace PY4GW::imgui_bindings

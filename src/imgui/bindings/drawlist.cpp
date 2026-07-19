#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>

#include <pybind11/stl.h>

#include <string>
#include <vector>

// NOTE: this translation unit sees the full (obsolete-enabled) ImGui API, so
// several ImDrawList methods are overloaded. We bind every method through a
// lambda to avoid taking the address of an overloaded member function.

namespace PY4GW::imgui_bindings {

namespace {

// Always resolve the live draw list at call time; never cache across frames.
ImDrawList* WindowDrawList() { return ImGui::GetWindowDrawList(); }
ImDrawList* ForegroundDrawList() { return ImGui::GetForegroundDrawList(); }
ImDrawList* BackgroundDrawList() { return ImGui::GetBackgroundDrawList(); }

// ImGui 1.92.8 moved every ImDrawFlags value. Corner flags shifted out of the low
// nibble (legacy ImDrawCornerFlags TopLeft=0x01 .. All=0x0F) up to bits 4-7, and
// ImDrawFlags_Closed moved from 1<<0 to 1<<9. Any legacy value left in the low
// nibble now lands in ImDrawFlags_InvalidMask_ (0x8000000F) and trips the
// "Incorrect parameter. Did you swap 'thickness' and 'flags'?" assert. Normalize
// legacy values so existing Python callers keep working without edits. New-style
// values (corners >= 1<<4, Closed == 1<<9) have nothing in the low nibble and pass
// through untouched.
ImDrawFlags NormalizeCornerFlags(int flags) {
    if (flags & 0x0F)
        flags = (flags & ~0x0F) | ((flags & 0x0F) << 4);
    return static_cast<ImDrawFlags>(flags);
}
ImDrawFlags NormalizeStrokeFlags(int flags) {
    if (flags & 0x01)
        flags = (flags & ~0x01) | ImDrawFlags_Closed;
    return static_cast<ImDrawFlags>(flags);
}

}  // namespace

void register_drawlist(py::module_& m) {
    py::class_<ImDrawList>(m, "DrawList")
        // ── clipping ──
        .def("push_clip_rect", [](ImDrawList& d, const ImVec2& a, const ImVec2& b, bool intersect) { d.PushClipRect(a, b, intersect); },
             py::arg("clip_min"), py::arg("clip_max"), py::arg("intersect_with_current") = false)
        .def("push_clip_rect_full_screen", [](ImDrawList& d) { d.PushClipRectFullScreen(); })
        .def("pop_clip_rect", [](ImDrawList& d) { d.PopClipRect(); })
        .def("get_clip_rect_min", [](ImDrawList& d) { return d.GetClipRectMin(); })
        .def("get_clip_rect_max", [](ImDrawList& d) { return d.GetClipRectMax(); })

        // ── primitives ──
        .def("add_line", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, ImU32 col, float t) { d.AddLine(p1, p2, col, t); },
             py::arg("p1"), py::arg("p2"), py::arg("col"), py::arg("thickness") = 1.0f)
        .def("add_line_h", [](ImDrawList& d, float min_x, float max_x, float y, ImU32 col, float t) { d.AddLineH(min_x, max_x, y, col, t); },
             py::arg("min_x"), py::arg("max_x"), py::arg("y"), py::arg("col"), py::arg("thickness") = 1.0f)
        .def("add_line_v", [](ImDrawList& d, float x, float min_y, float max_y, ImU32 col, float t) { d.AddLineV(x, min_y, max_y, col, t); },
             py::arg("x"), py::arg("min_y"), py::arg("max_y"), py::arg("col"), py::arg("thickness") = 1.0f)
        // Python-facing order is legacy (rounding, flags, thickness) to match the stubs, the
        // draw_list_add_rect free helper, and all existing callers; we reorder to ImGui 1.92.8's
        // (thickness, flags) internally. Do NOT "fix" this to ImGui's order — it silently swaps
        // thickness/flags for every caller. Same convention for add_polyline / path_stroke below.
        .def("add_rect", [](ImDrawList& d, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int flags, float thickness) { d.AddRect(a, b, col, rounding, thickness, NormalizeCornerFlags(flags)); },
             py::arg("p_min"), py::arg("p_max"), py::arg("col"),
             py::arg("rounding") = 0.0f, py::arg("flags") = 0, py::arg("thickness") = 1.0f)
        .def("add_rect_filled", [](ImDrawList& d, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int flags) { d.AddRectFilled(a, b, col, rounding, NormalizeCornerFlags(flags)); },
             py::arg("p_min"), py::arg("p_max"), py::arg("col"),
             py::arg("rounding") = 0.0f, py::arg("flags") = 0)
        .def("add_rect_filled_multi_color", [](ImDrawList& d, const ImVec2& a, const ImVec2& b, ImU32 ul, ImU32 ur, ImU32 br, ImU32 bl) { d.AddRectFilledMultiColor(a, b, ul, ur, br, bl); },
             py::arg("p_min"), py::arg("p_max"),
             py::arg("col_upr_left"), py::arg("col_upr_right"), py::arg("col_bot_right"), py::arg("col_bot_left"))
        .def("add_quad", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float t) { d.AddQuad(p1, p2, p3, p4, col, t); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("col"), py::arg("thickness") = 1.0f)
        .def("add_quad_filled", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) { d.AddQuadFilled(p1, p2, p3, p4, col); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("col"))
        .def("add_triangle", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float t) { d.AddTriangle(p1, p2, p3, col, t); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("col"), py::arg("thickness") = 1.0f)
        .def("add_triangle_filled", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) { d.AddTriangleFilled(p1, p2, p3, col); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("col"))
        .def("add_circle", [](ImDrawList& d, const ImVec2& c, float r, ImU32 col, int segs, float t) { d.AddCircle(c, r, col, segs, t); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("num_segments") = 0, py::arg("thickness") = 1.0f)
        .def("add_circle_filled", [](ImDrawList& d, const ImVec2& c, float r, ImU32 col, int segs) { d.AddCircleFilled(c, r, col, segs); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("num_segments") = 0)
        .def("add_ngon", [](ImDrawList& d, const ImVec2& c, float r, ImU32 col, int segs, float t) { d.AddNgon(c, r, col, segs, t); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("num_segments"), py::arg("thickness") = 1.0f)
        .def("add_ngon_filled", [](ImDrawList& d, const ImVec2& c, float r, ImU32 col, int segs) { d.AddNgonFilled(c, r, col, segs); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("num_segments"))
        .def("add_ellipse", [](ImDrawList& d, const ImVec2& c, const ImVec2& r, ImU32 col, float rot, int segs, float t) { d.AddEllipse(c, r, col, rot, segs, t); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("rot") = 0.0f, py::arg("num_segments") = 0, py::arg("thickness") = 1.0f)
        .def("add_ellipse_filled", [](ImDrawList& d, const ImVec2& c, const ImVec2& r, ImU32 col, float rot, int segs) { d.AddEllipseFilled(c, r, col, rot, segs); },
             py::arg("center"), py::arg("radius"), py::arg("col"), py::arg("rot") = 0.0f, py::arg("num_segments") = 0)
        .def("add_text", [](ImDrawList& d, const ImVec2& pos, ImU32 col, const std::string& text) { d.AddText(pos, col, text.c_str()); },
             py::arg("pos"), py::arg("col"), py::arg("text"))
        .def("add_bezier_cubic", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float t, int segs) { d.AddBezierCubic(p1, p2, p3, p4, col, t, segs); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("col"), py::arg("thickness"), py::arg("num_segments") = 0)
        .def("add_bezier_quadratic", [](ImDrawList& d, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float t, int segs) { d.AddBezierQuadratic(p1, p2, p3, col, t, segs); },
             py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("col"), py::arg("thickness"), py::arg("num_segments") = 0)
        .def("add_polyline", [](ImDrawList& d, const std::vector<ImVec2>& pts, ImU32 col, int flags, float thickness) { d.AddPolyline(pts.data(), static_cast<int>(pts.size()), col, thickness, NormalizeStrokeFlags(flags)); },
             py::arg("points"), py::arg("col"), py::arg("flags") = 0, py::arg("thickness") = 1.0f)
        .def("add_convex_poly_filled", [](ImDrawList& d, const std::vector<ImVec2>& pts, ImU32 col) { d.AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), col); },
             py::arg("points"), py::arg("col"))
        .def("add_concave_poly_filled", [](ImDrawList& d, const std::vector<ImVec2>& pts, ImU32 col) { d.AddConcavePolyFilled(pts.data(), static_cast<int>(pts.size()), col); },
             py::arg("points"), py::arg("col"))

        // ── stateful path API ──
        .def("path_clear", [](ImDrawList& d) { d.PathClear(); })
        .def("path_line_to", [](ImDrawList& d, const ImVec2& pos) { d.PathLineTo(pos); }, py::arg("pos"))
        .def("path_fill_convex", [](ImDrawList& d, ImU32 col) { d.PathFillConvex(col); }, py::arg("col"))
        .def("path_stroke", [](ImDrawList& d, ImU32 col, int flags, float thickness) { d.PathStroke(col, thickness, NormalizeStrokeFlags(flags)); },
             py::arg("col"), py::arg("flags") = 0, py::arg("thickness") = 1.0f)
        .def("path_arc_to", [](ImDrawList& d, const ImVec2& c, float r, float a_min, float a_max, int segs) { d.PathArcTo(c, r, a_min, a_max, segs); },
             py::arg("center"), py::arg("radius"), py::arg("a_min"), py::arg("a_max"), py::arg("num_segments") = 0)
        .def("path_arc_to_fast", [](ImDrawList& d, const ImVec2& c, float r, int a_min, int a_max) { d.PathArcToFast(c, r, a_min, a_max); },
             py::arg("center"), py::arg("radius"), py::arg("a_min_of_12"), py::arg("a_max_of_12"))
        .def("path_elliptical_arc_to", [](ImDrawList& d, const ImVec2& c, const ImVec2& r, float rot, float a_min, float a_max, int segs) { d.PathEllipticalArcTo(c, r, rot, a_min, a_max, segs); },
             py::arg("center"), py::arg("radius"), py::arg("rot"), py::arg("a_min"), py::arg("a_max"), py::arg("num_segments") = 0)
        .def("path_bezier_cubic_curve_to", [](ImDrawList& d, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, int segs) { d.PathBezierCubicCurveTo(p2, p3, p4, segs); },
             py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("num_segments") = 0)
        .def("path_bezier_quadratic_curve_to", [](ImDrawList& d, const ImVec2& p2, const ImVec2& p3, int segs) { d.PathBezierQuadraticCurveTo(p2, p3, segs); },
             py::arg("p2"), py::arg("p3"), py::arg("num_segments") = 0)
        .def("path_rect", [](ImDrawList& d, const ImVec2& a, const ImVec2& b, float rounding, int flags) { d.PathRect(a, b, rounding, NormalizeCornerFlags(flags)); },
             py::arg("rect_min"), py::arg("rect_max"), py::arg("rounding") = 0.0f, py::arg("flags") = 0)

        // ── channel splitting (draw out of order within one list) ──
        .def("channels_split", [](ImDrawList& d, int count) { d.ChannelsSplit(count); }, py::arg("count"))
        .def("channels_merge", [](ImDrawList& d) { d.ChannelsMerge(); })
        .def("channels_set_current", [](ImDrawList& d, int n) { d.ChannelsSetCurrent(n); }, py::arg("n"));

    m.def("get_window_draw_list", &WindowDrawList, py::return_value_policy::reference,
          "Draw list for the current window (clipped to it).");
    m.def("get_foreground_draw_list", &ForegroundDrawList, py::return_value_policy::reference,
          "Full-screen draw list rendered on top of every window. Use for overlays/ESP.");
    m.def("get_background_draw_list", &BackgroundDrawList, py::return_value_policy::reference,
          "Full-screen draw list rendered behind every window.");

    // Legacy flat draw_list_* helpers (ported from py_imgui.cpp): each operates on the
    // current window's draw list, taking loose x/y coordinates instead of a Vec2. The
    // library (ImGui.py, dNodes, WindowModule) drives these directly. Same call surface
    // as legacy; NB ImGui 1.92.8 swapped AddRect's trailing (thickness, flags) so we pass
    // them in the new order here.
    m.def("draw_list_add_line", [](float x1, float y1, float x2, float y2, ImU32 col, float thickness) {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, thickness);
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"), py::arg("thickness") = 1.0f);

    m.def("draw_list_add_rect", [](float x1, float y1, float x2, float y2, ImU32 col,
                                   float rounding, int rounding_corners_flags, float thickness) {
        ImGui::GetWindowDrawList()->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding,
                                            thickness, NormalizeCornerFlags(rounding_corners_flags));
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"),
       py::arg("rounding") = 0.0f, py::arg("rounding_corners_flags") = 0, py::arg("thickness") = 1.0f);

    m.def("draw_list_add_rect_filled", [](float x1, float y1, float x2, float y2, ImU32 col,
                                          float rounding, int rounding_corners_flags) {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding,
                                                  NormalizeCornerFlags(rounding_corners_flags));
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"),
       py::arg("rounding") = 0.0f, py::arg("rounding_corners_flags") = 0);

    m.def("draw_list_add_circle", [](float x, float y, float radius, ImU32 col, int num_segments, float thickness) {
        ImGui::GetWindowDrawList()->AddCircle(ImVec2(x, y), radius, col, num_segments, thickness);
    }, py::arg("x"), py::arg("y"), py::arg("radius"), py::arg("col"),
       py::arg("num_segments") = 0, py::arg("thickness") = 1.0f);

    m.def("draw_list_add_circle_filled", [](float x, float y, float radius, ImU32 col, int num_segments) {
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x, y), radius, col, num_segments);
    }, py::arg("x"), py::arg("y"), py::arg("radius"), py::arg("col"), py::arg("num_segments") = 0);

    m.def("draw_list_add_text", [](float x, float y, ImU32 col, const std::string& text) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), col, text.c_str());
    }, py::arg("x"), py::arg("y"), py::arg("col"), py::arg("text"));

    m.def("draw_list_add_triangle", [](float x1, float y1, float x2, float y2, float x3, float y3,
                                       ImU32 col, float thickness) {
        ImGui::GetWindowDrawList()->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col, thickness);
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
       py::arg("col"), py::arg("thickness") = 1.0f);

    m.def("draw_list_add_triangle_filled", [](float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col) {
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col);
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"), py::arg("col"));

    m.def("draw_list_add_quad", [](float x1, float y1, float x2, float y2, float x3, float y3,
                                   float x4, float y4, ImU32 col, float thickness) {
        ImGui::GetWindowDrawList()->AddQuad(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col, thickness);
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
       py::arg("x4"), py::arg("y4"), py::arg("col"), py::arg("thickness") = 1.0f);

    m.def("draw_list_add_quad_filled", [](float x1, float y1, float x2, float y2, float x3, float y3,
                                          float x4, float y4, ImU32 col) {
        ImGui::GetWindowDrawList()->AddQuadFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col);
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
       py::arg("x4"), py::arg("y4"), py::arg("col"));
}

}  // namespace PY4GW::imgui_bindings

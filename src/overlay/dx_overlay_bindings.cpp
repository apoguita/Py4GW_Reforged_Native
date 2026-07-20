#include "base/py_bindings.h"

#include "overlay/dx_overlay.h"

#include <memory>
#include <utility>

namespace py = pybind11;

using PY4GW::overlay::DXOverlay;

void bind_dx_overlay(py::module_& m) {
    py::class_<DXOverlay>(m, "DXOverlay")
        .def(py::init<>())
        .def("set_primitives", &DXOverlay::set_primitives, py::arg("primitives"), py::arg("draw_color") = 0xFFFFFFFF)
        .def("build_pathing_trapezoid_geometry", &DXOverlay::build_pathing_trapezoid_geometry, py::arg("color") = 0xFF00FF00)
        .def("inverse_rendering", &DXOverlay::inverse_rendering, py::arg("enabled"))
        .def("set_world_zoom_x", &DXOverlay::set_world_zoom_x, py::arg("zoom"))
        .def("set_world_zoom_y", &DXOverlay::set_world_zoom_y, py::arg("zoom"))
        .def("set_world_pan", &DXOverlay::set_world_pan, py::arg("x"), py::arg("y"))
        .def("set_world_rotation", &DXOverlay::set_world_rotation, py::arg("r"))
        .def("set_world_space", &DXOverlay::set_world_space, py::arg("enabled"))
        .def("set_world_scale", &DXOverlay::set_world_scale, py::arg("scale"))

        .def("set_screen_offset", &DXOverlay::set_screen_offset, py::arg("x"), py::arg("y"))
        .def("set_screen_zoom_x", &DXOverlay::set_screen_zoom_x, py::arg("zoom"))
        .def("set_screen_zoom_y", &DXOverlay::set_screen_zoom_y, py::arg("zoom"))
        .def("set_screen_rotation", &DXOverlay::set_screen_rotation, py::arg("r"))

        .def("set_circular_mask", &DXOverlay::set_circular_mask, py::arg("enabled"))
        .def("set_circular_mask_radius", &DXOverlay::set_circular_mask_radius, py::arg("radius"))
        .def("set_circular_mask_center", &DXOverlay::set_circular_mask_center, py::arg("x"), py::arg("y"))

        .def("set_rectangle_mask", &DXOverlay::set_rectangle_mask, py::arg("enabled"))
        .def("set_rectangle_mask_bounds", &DXOverlay::set_rectangle_mask_bounds, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"))

        .def("render", &DXOverlay::render)

        .def("DrawLine", &DXOverlay::DrawLine, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle", &DXOverlay::DrawTriangle, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangleFilled", &DXOverlay::DrawTriangleFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuad", &DXOverlay::DrawQuad, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuadFilled", &DXOverlay::DrawQuadFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawPoly", &DXOverlay::DrawPoly, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("segments") = 3, py::arg("thickness") = 1.0f)
        .def("DrawPolyFilled", &DXOverlay::DrawPolyFilled, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("segments") = 3)
        .def("DrawCubeOutline", &DXOverlay::DrawCubeOutline, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true)
        .def("DrawCubeFilled", &DXOverlay::DrawCubeFilled, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true)

        .def("DrawLine3D", &DXOverlay::DrawLine3D, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawTriangle3D", &DXOverlay::DrawTriangle3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawTriangleFilled3D", &DXOverlay::DrawTriangleFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawQuad3D", &DXOverlay::DrawQuad3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawQuadFilled3D", &DXOverlay::DrawQuadFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawPoly3D", &DXOverlay::DrawPoly3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 3, py::arg("autoZ") = true, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
        .def("DrawPolyFilled3D", &DXOverlay::DrawPolyFilled3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 3, py::arg("autoZ") = true, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)

        .def("Setup3DView", &DXOverlay::Setup3DView)
        .def("DrawBeam3D", &DXOverlay::DrawBeam3D,
            py::arg("x"), py::arg("y"), py::arg("z"),
            py::arg("height") = 300.0f, py::arg("radius") = 60.0f, py::arg("argb") = 0xFF20FF40,
            py::arg("top_alpha") = 0.0f, py::arg("additive") = true,
            "Shader-based light beam (no texture). top_alpha 0..1 = top fade; "
            "additive = light blend. Call from a PyWorldRender.register_draw callback to occlude.")
        .def("draw_shaded_3d", &DXOverlay::DrawShaded3D,
            py::arg("vertices"), py::arg("additive") = true, py::arg("use_occlusion") = true,
            "Draw shader-colored 3D triangles occluded in the world pass. vertices = list "
            "of (x, y, z, argb) world-space tuples (up = -z), 3 per triangle. The alpha in "
            "argb + additive (light) vs alpha blend make it read as glow. Build the geometry "
            "in Python (billboards, gradients) and call this. Occludes + rides the lifecycle.")
        .def("draw_shaded_3d_max", &DXOverlay::DrawShaded3DMax,
            py::arg("vertices"), py::arg("use_occlusion") = true,
            "Like draw_shaded_3d but with a MAX blend op: overlapping fragments take the "
            "brightest of source/dest instead of summing, so crossed/overlapping quads do "
            "NOT accumulate at their seam (each quad reads as if drawn alone). Alpha still "
            "fades where the vertex alpha is low.")
        .def("set_occlusion_tuning", &DXOverlay::SetOcclusionTuning,
            py::arg("near_z"), py::arg("far_z"), py::arg("zfunc"), py::arg("reverse_z"))
        .def("get_depth_diagnostics", &DXOverlay::GetDepthDiagnostics)
        .def("draw_self_occlusion_test", &DXOverlay::DrawSelfOcclusionTest)
        .def("ApplyStencilMask", &DXOverlay::ApplyStencilMask)
        .def("ResetStencilMask", &DXOverlay::ResetStencilMask)
        .def("DrawTexture", &DXOverlay::DrawTexture, py::arg("file_path"), py::arg("screen_pos_x"), py::arg("screen_pos_y"), py::arg("width") = 100.0f, py::arg("height") = 100.0f, py::arg("int_tint") = 0xFFFFFFFF)
        .def("DrawTexture3D", &DXOverlay::DrawTexture3D, py::arg("file_path"), py::arg("world_pos_x"), py::arg("world_pos_y"), py::arg("world_pos_z"), py::arg("width") = 100.0f, py::arg("height") = 100.0f, py::arg("use_occlusion") = true, py::arg("int_tint") = 0xFFFFFFFF)
        .def("DrawQuadTextured3D", &DXOverlay::DrawQuadTextured3D, py::arg("file_path"),
            py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"),
            py::arg("use_occlusion") = true, py::arg("int_tint") = 0xFFFFFFFF)
        .def("SaveGeometryToFile", &DXOverlay::SaveGeometryToFile, py::arg("filename"), py::arg("min_x"), py::arg("min_y"), py::arg("max_x"), py::arg("max_y"));

    // Singleton accessor: returns the ONE shared DXOverlay (non-owning reference, so
    // Python never destroys it). Prefer this over PyDXOverlay.DXOverlay(); construction
    // stays bound for backward compatibility.
    m.def("get_overlay", []() -> DXOverlay& { return DXOverlay::Instance(); },
          py::return_value_policy::reference,
          "Shared DXOverlay singleton (same object every call).");
}

PYBIND11_EMBEDDED_MODULE(PyDXOverlay, m) {
    bind_dx_overlay(m);
}

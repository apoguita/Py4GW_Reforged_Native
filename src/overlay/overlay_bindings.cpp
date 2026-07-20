#include "base/py_bindings.h"

#include "GW/common/game_pos.h"
#include "overlay/overlay.h"
#include "overlay/screen_overlay.h"

namespace py = pybind11;

using PY4GW::overlay::Overlay;
using PY4GW::overlay::ScreenOverlay;

// Legacy PyOverlay bound Point2D/Point3D; the port exposes the project's
// GW::Vec2f/GW::Vec3f instead (user-directed deviation).
void bind_Vec2f(py::module_& m) {
    py::class_<GW::Vec2f>(m, "Vec2f")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &GW::Vec2f::x, "X coordinate")
        .def_readwrite("y", &GW::Vec2f::y, "Y coordinate");
}

void bind_Vec3f(py::module_& m) {
    py::class_<GW::Vec3f>(m, "Vec3f")
        .def(py::init<>())
        .def(py::init<float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z") = 0.0f)
        .def_readwrite("x", &GW::Vec3f::x, "X coordinate")
        .def_readwrite("y", &GW::Vec3f::y, "Y coordinate")
        .def_readwrite("z", &GW::Vec3f::z, "Z coordinate");
}

void bind_ScreenOverlay(py::module_& m) {
    py::class_<ScreenOverlay>(m, "ScreenOverlay")
        .def(py::init<>())
        .def("create_overlay", &ScreenOverlay::CreatePrimary,
            py::arg("ms") = 0, py::arg("destroy") = false,
            "Create a transparent, click-through, topmost desktop overlay on the primary monitor")
        .def("destroy", &ScreenOverlay::Destroy,
            "Destroy the desktop overlay and free resources")
        .def("show", &ScreenOverlay::Show, py::arg("show"),
            "Show or hide the desktop overlay window without activating it")
        .def("begin", &ScreenOverlay::Begin,
            "Begin a new frame (clears to fully transparent)")
        .def("draw_rect", &ScreenOverlay::DrawRect,
            py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"),
            py::arg("argb"), py::arg("thickness") = 1.0f,
            "Draw a rectangle in desktop pixels. Color is 0xAARRGGBB.")
        .def("draw_rect_filled", &ScreenOverlay::DrawRectFilled,
            py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"),
            py::arg("argb"),
            "Draw a filled rectangle in desktop pixels. Color is 0xAARRGGBB.")
        .def("draw_text_box", &ScreenOverlay::DrawTextBox,
            py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"),
            py::arg("text"), py::arg("argb"), py::arg("px_size"),
            py::arg("family") = L"Segoe UI",
            py::arg("hcenter") = false, py::arg("vcenter") = false,
            "Draw text within a rectangle in desktop pixels. Color is 0xAARRGGBB. "
            "Text is clipped and ellipsized if too long. Font size is in pixels.")
        .def("end", &ScreenOverlay::End,
            "Present the frame to the desktop using UpdateLayeredWindow")
        .def("get_desktop_size", &ScreenOverlay::GetDesktopSize,
            "Get the size of the entire virtual desktop (all monitors). Returns (width, height).")
        .def("set_auto_expire", &ScreenOverlay::SetAutoExpire,
            py::arg("ms"), py::arg("destroy") = false,
            "Set auto-expire timeout in milliseconds. If ms=0, auto-expire is disabled. "
            "If destroy=true, the overlay window is destroyed upon expiry; otherwise it is hidden.");
}

void bind_overlay(py::module_& m) {
    py::class_<Overlay>(m, "Overlay")
        .def(py::init<>())
        .def("RefreshDrawList", &Overlay::RefreshDrawList)
        .def("GetMouseCoords", &Overlay::GetMouseCoords)
        // FindZ resolves the TOPMOST surface across all planes by default (bridges/
        // slopes are planes > 0), independent of the player's plane. Pass
        // multi_plane=False for the legacy player-plane behaviour. Single-plane maps
        // take a fast path. See docs/RE/altitude_plane_reverse_engineering.md.
        .def("FindZ", &Overlay::findZ,
             py::arg("x"), py::arg("y"), py::arg("pz") = 0, py::arg("multi_plane") = true,
             "Ground height at (x, y). multi_plane=True (default): topmost surface "
             "across all planes, stable regardless of the player's plane. "
             "multi_plane=False: legacy altitude at the player's plane.")
        .def("FindZBatch", &Overlay::FindZBatch,
             py::arg("points"), py::arg("multi_plane") = true,
             "Batched FindZ for many points (path display, dozens/frame): points = "
             "list of (x, y) -> list of z. Plane list fetched once; single-plane fast path.")
        .def("FindZPlane", &Overlay::FindZPlane, py::arg("x"), py::arg("y"), py::arg("z") = 0)
        // Low-level / specialist helpers (FindZ covers the common cases).
        .def("GetZPlaneCount", &Overlay::GetZPlaneCount,
             "Number of z-planes (pmaps) in the map; 0 if not ready. <= 1 means "
             "single-plane, so FindZ's fast path applies (cheap overlap gate).")
        .def("QueryAltitudeAt", &Overlay::QueryAltitudeAt, py::arg("x"), py::arg("y"), py::arg("plane"),
             "Altitude at a SPECIFIC plane -> (covered, z). covered=False if that "
             "plane has no surface at (x,y).")
        .def("GroundZNearest", &Overlay::GroundZNearest, py::arg("x"), py::arg("y"), py::arg("ref_z"),
             "Specialist: covered surface whose altitude is nearest ref_z (feed the "
             "object's own z to pick bridge-over vs under-bridge; FindZ's topmost can't).")
        .def("GroundZNearestBatch", &Overlay::GroundZNearestBatch, py::arg("points"), py::arg("ref_zs"),
             "Batched GroundZNearest: points = list of (x, y), ref_zs = list of "
             "reference z (same length); returns list of z.")
        .def("GroundZWalkable", &Overlay::GroundZWalkable, py::arg("x"), py::arg("y"),
             "Option B: plane-less walkable height. MapQueryAltitude at plane 0 -> the "
             "engine's PathEngineQueryAltitude (navmesh) where a path engine exists, "
             "else base terrain. Plane-independent (stable, unlike FindZ); does NOT add "
             "bridge/slope props (use FindZ multi_plane for those). One cheap call/point.")
        .def("GroundZWalkableBatch", &Overlay::GroundZWalkableBatch, py::arg("points"),
             "Batched GroundZWalkable: points = list of (x, y) -> list of z.")
        .def("WorldToScreen", &Overlay::WorldToScreen, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("GetMouseWorldPos", &Overlay::GetMouseWorldPos)
        // Game <-> World
        .def("GamePosToWorldMap", &Overlay::GamePosToWorldMap, py::arg("x"), py::arg("y"), "Convert game position to world map coordinates")
        .def("WorldMapToGamePos", &Overlay::WorlMapToGamePos, py::arg("x"), py::arg("y"), "Convert world map position to game coordinates")
        // World <-> Screen
        .def("WorldMapToScreen", &Overlay::WorldMapToScreen, py::arg("x"), py::arg("y"), "Convert world map position to screen coordinates")
        .def("ScreenToWorldMap", &Overlay::ScreenToWorldMap, py::arg("x"), py::arg("y"), "Convert screen position to world map coordinates")
        // Game <-> Screen (combined)
        .def("GameMapToScreen", &Overlay::GameMapToScreen, py::arg("x"), py::arg("y"), "Convert game map position to screen coordinates")
        .def("ScreenToGameMapPos", &Overlay::ScreenToGameMapPos, py::arg("x"), py::arg("y"), "Convert screen position to game map coordinates")
        // NormalizedScreen <-> Screen
        .def("NormalizedScreenToScreen", &Overlay::NormalizedScreenToScreen, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to screen coordinates")
        .def("ScreenToNormalizedScreen", &Overlay::ScreenToNormalizedScreen, py::arg("screen_x"), py::arg("screen_y"), "Convert screen coordinates to normalized screen coordinates")
        // NormalizedScreen <-> World / Game
        .def("NormalizedScreenToWorldMap", &Overlay::NormalizedScreenToWorldMap, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to world map coordinates")
        .def("NormalizedScreenToGameMap", &Overlay::NormalizedScreenToGameMap, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to game map coordinates")
        .def("GamePosToNormalizedScreen", &Overlay::GamePosToNormalizedScreen, py::arg("x"), py::arg("y"), "Convert game position to normalized screen coordinates")

        .def("BeginDraw", py::overload_cast<>(&Overlay::BeginDraw), "BeginDraw with no arguments")
        .def("BeginDraw", py::overload_cast<std::string>(&Overlay::BeginDraw), py::arg("name"), "BeginDraw with name")
        .def("BeginDraw", py::overload_cast<std::string, float, float, float, float>(&Overlay::BeginDraw),
            py::arg("name"), py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"), "BeginDraw with name and bounds")
        .def("EndDraw", &Overlay::EndDraw)
        .def("DrawLine", &Overlay::DrawLine, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawLine3D", &Overlay::DrawLine3D, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle", &Overlay::DrawTriangle, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle3D", &Overlay::DrawTriangle3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangleFilled", &Overlay::DrawTriangleFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawTriangleFilled3D", &Overlay::DrawTriangleFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuad", &Overlay::DrawQuad, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuad3D", &Overlay::DrawQuad3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuadFilled", &Overlay::DrawQuadFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuadFilled3D", &Overlay::DrawQuadFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawPoly", &Overlay::DrawPoly, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("thickness") = 1.0f)
        .def("DrawPoly3D", &Overlay::DrawPoly3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("thickness") = 1.0f, py::arg("autoZ") = true)
        .def("DrawPolyFilled", &Overlay::DrawPolyFilled, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12)
        .def("DrawPolyFilled3D", &Overlay::DrawPolyFilled3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("autoZ") = true)
        .def("DrawCubeOutline", &Overlay::DrawCubeOutline, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawCubeFilled", &Overlay::DrawCubeFilled, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF)

        .def("DrawText", &Overlay::DrawText2D, py::arg("position"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("centered") = true, py::arg("scale") = 1.0f)
        .def("DrawText3D", &Overlay::DrawText3D, py::arg("position3D"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("autoZ") = true, py::arg("centered") = true, py::arg("scale") = 1.0f)
        .def("GetDisplaySize", &Overlay::GetDisplaySize)
        .def("IsMouseClicked", &Overlay::IsMouseClicked, py::arg("button") = 0)
        .def("PushClipRect", &Overlay::PushClipRect, py::arg("x"), py::arg("y"), py::arg("x2"), py::arg("y2"))
        .def("PopClipRect", &Overlay::PopClipRect)
        // ==== DrawTexture overloads ====
        .def("DrawTexture",
            static_cast<void(Overlay::*)(const std::string&, float, float)>(&Overlay::DrawTexture),
            py::arg("path"),
            py::arg("width") = 32.0f,
            py::arg("height") = 32.0f)

        .def("DrawTexture",
            static_cast<void(Overlay::*)(const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>,
                std::tuple<int, int, int, int>)>(&Overlay::DrawTexture),
            py::arg("path"),
            py::arg("size") = std::make_tuple(32.0f, 32.0f),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255),
            py::arg("border_col") = std::make_tuple(0, 0, 0, 0))

        // ==== DrawTexturedRect overloads ====
        .def("DrawTexturedRect",
            static_cast<void(Overlay::*)(float, float, float, float, const std::string&)>(&Overlay::DrawTexturedRect),
            py::arg("x"),
            py::arg("y"),
            py::arg("width"),
            py::arg("height"),
            py::arg("texture_path"))

        .def("DrawTexturedRect",
            static_cast<void(Overlay::*)(std::tuple<float, float>,
                std::tuple<float, float>,
                const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>)>(&Overlay::DrawTexturedRect),
            py::arg("pos"),
            py::arg("size"),
            py::arg("texture_path"),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255))

        .def("UpkeepTextures", &Overlay::UpkeepTextures, py::arg("timeout") = 30)
        .def("ImageButton",
            static_cast<bool(Overlay::*)(const std::string&, const std::string&, float, float, int)>(&Overlay::ImageButton),
            py::arg("caption"),
            py::arg("file_path"),
            py::arg("width") = 32.0f,
            py::arg("height") = 32.0f,
            py::arg("frame_padding") = 0)

        .def("ImageButton",
            static_cast<bool(Overlay::*)(const std::string&, const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>,
                std::tuple<int, int, int, int>,
                int)>(&Overlay::ImageButton),
            py::arg("caption"),
            py::arg("file_path"),
            py::arg("size") = std::make_tuple(32.0f, 32.0f),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("bg_color") = std::make_tuple(0, 0, 0, 0),
            py::arg("tint_color") = std::make_tuple(255, 255, 255, 255),
            py::arg("frame_padding") = 0)

        .def("DrawTextureInForegound",
            &Overlay::DrawTextureInForegound,
            py::arg("pos") = std::make_tuple(0.0f, 0.0f),
            py::arg("size") = std::make_tuple(100.0f, 100.0f),
            py::arg("texture_path") = "",
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255))

        .def("DrawTextureInDrawlist",
            &Overlay::DrawTextureInDrawlist,
            py::arg("pos") = std::make_tuple(0.0f, 0.0f),
            py::arg("size") = std::make_tuple(100.0f, 100.0f),
            py::arg("texture_path") = "",
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255));
}

PYBIND11_EMBEDDED_MODULE(PyOverlay, m) {
    bind_Vec2f(m);
    bind_Vec3f(m);
    bind_overlay(m);
    bind_ScreenOverlay(m);
}

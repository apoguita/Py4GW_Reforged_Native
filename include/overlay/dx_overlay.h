#pragma once

#include "base/error_handling.h"

#include "GW/common/game_pos.h"
#include "overlay/overlay.h"

#include <cstdint>
#include <string>
#include <vector>

#include <d3d9.h>

// Direct3D 9 primitive handler migrated from legacy py_2d_renderer.h/.cpp
// (legacy class Py2DRenderer, renamed DXOverlay by user direction - it is a
// full DirectX overlay, not just a 2D renderer). Retained-mode path
// (set_primitives/render with world/screen transforms, stencil masks and
// inverse rendering) plus immediate-mode 2D/3D draw calls. Legacy D3DX usage
// was replaced: DirectXMath for matrices, D3DCompile for the inverse-render
// pixel shader, WIC for the PNG export.

namespace PY4GW::overlay {

class DXOverlay {
private:
    Overlay overlay;
    std::vector<std::vector<GW::Vec2f>> primitives;

public:
    void set_primitives(const std::vector<std::vector<GW::Vec2f>>& prims, D3DCOLOR color);
    void set_world_zoom_x(float zoom);
    void set_world_zoom_y(float zoom);
    void set_world_pan(float x, float y);
    void set_world_rotation(float r);
    void set_world_space(bool enabled);
    void set_world_scale(float x);

    // Screen-space offset (pixel units)
    void set_screen_offset(float x, float y);
    void set_screen_zoom_x(float zoom);
    void set_screen_zoom_y(float zoom);
    void set_screen_rotation(float r);

    void set_circular_mask(bool enabled);
    void set_circular_mask_radius(float radius);
    void set_circular_mask_center(float x, float y);

    void set_rectangle_mask(bool enabled);
    void set_rectangle_mask_bounds(float x, float y, float width, float height);

    void build_pathing_trapezoid_geometry(D3DCOLOR color);
    void inverse_rendering(bool enabled);

    void render();

    void Setup3DView();

    // Shader-based additive light beam (the GWToolbox GameWorldRenderer approach:
    // colored geometry, NOT a texture, drawn with a vertex/pixel shader that gets
    // the view/projection as constants so it lands correctly in GW's HDR world
    // buffer). Draw this from a world_render (in-world) callback so it occludes.
    // base at (x,y,z=ground); beam rises `height` (up = -z here); `radius` = width.
    // top_alpha (0..1) = the top's alpha relative to base (0 = fade out); additive =
    // additive light blend vs. normal alpha blend.
    void DrawBeam3D(float x, float y, float z, float height, float radius, uint32_t argb,
                    float top_alpha = 0.0f, bool additive = true);

    // Runtime depth/occlusion tuning for Setup3DView (diagnostic-driven, so the
    // exact GW depth convention can be dialed in live). zfunc is a D3DCMPFUNC
    // value (LESSEQUAL=4, GREATEREQUAL=7, ...). reverse_z swaps near/far.
    void SetOcclusionTuning(float near_z, float far_z, int zfunc, bool reverse_z);

    // Reports live render-target / depth-stencil / viewport state (for
    // diagnosing why occlusion fails: mismatched/absent depth surface, etc.).
    std::string GetDepthDiagnostics();

    // Self-contained depth-hardware test (clears depth, draws overlapping quads
    // at explicit depths). Proves whether our depth read+write path works,
    // independent of GW's scene depth. See the .cpp for the expected result.
    void DrawSelfOcclusionTest();

    void DrawLine(GW::Vec2f from, GW::Vec2f to, D3DCOLOR color, float thickness);
    void DrawLine3D(GW::Vec3f from, GW::Vec3f to, D3DCOLOR color, bool use_occlusion = true, int segments = 1, float floor_offset = 0.0f);
    void DrawTriangle(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, D3DCOLOR color, float thickness = 1.0f);
    void DrawTriangle3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawTriangleFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, D3DCOLOR color);
    void DrawTriangleFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawQuad(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, D3DCOLOR color, float thickness = 1.0f);
    void DrawQuad3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawQuadFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, D3DCOLOR color);
    void DrawQuadFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawPoly(GW::Vec2f center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f);
    void DrawPolyFilled(GW::Vec2f center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32);
    void DrawPoly3D(GW::Vec3f center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true, bool use_occlusion = true, int segments = 1, float floor_offset = 0.0f);
    void DrawPolyFilled3D(GW::Vec3f center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true, bool use_occlusion = true, int segments = 1, float floor_offset = 0.0f);
    void DrawCubeOutline(GW::Vec3f center, float size, D3DCOLOR color, bool use_occlusion = true);
    void DrawCubeFilled(GW::Vec3f center, float size, D3DCOLOR color, bool use_occlusion = true);
    void DrawTexture(const std::string& file_path, float screen_pos_x, float screen_pos_y, float width, float height, uint32_t int_tint);
    void DrawTexture3D(const std::string& file_path, float world_pos_x, float world_pos_y, float world_pos_z, float width, float height, bool use_occlusion, uint32_t int_tint);
    void DrawQuadTextured3D(const std::string& file_path, GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, bool use_occlusion, uint32_t int_tint);
    int SaveGeometryToFile(
        const std::wstring& filename,
        float min_x, float min_y,
        float max_x, float max_y
    );
    void ApplyStencilMask();
    void ResetStencilMask();

private:
    void SetupProjection();

    struct D3DVertex {
        float x, y, z, rhw;
        D3DCOLOR color;
    };

    struct D3DVertex3D {
        float x, y, z;           // No RHW
        D3DCOLOR color;
    };

    D3DCOLOR color = 0xFFFFFFFF;
    float world_zoom_x = 1.0f;
    float world_zoom_y = 1.0f;
    float world_pan_x = 0.0f;
    float world_pan_y = 0.0f;
    float world_scale = 1.0f;
    float world_rotation = 0.0f;
    bool world_space = true;

    // Screen transform (pixels)
    float screen_offset_x = 0.0f;
    float screen_offset_y = 0.0f;
    float screen_zoom_x = 1.0f;
    float screen_zoom_y = 1.0f;
    float screen_rotation = 0.0f;

    bool use_circular_mask = false;
    float mask_radius = 5000.0f;
    float mask_center_x = 0.0f;
    float mask_center_y = 0.0f;

    bool use_rectangle_mask = false;
    float mask_rect_x = 0.0f;
    float mask_rect_y = 0.0f;
    float mask_rect_width = 0.0f;
    float mask_rect_height = 0.0f;

    bool inverse_rendering_enabled = false;
};

}  // namespace PY4GW::overlay

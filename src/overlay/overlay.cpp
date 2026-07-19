#include "base/error_handling.h"

#include "overlay/overlay.h"

#include "GW/agent/agent.h"
#include "GW/context/agent.h"
#include "GW/context/camera.h"
#include "GW/context/context.h"
#include "GW/context/gameplay.h"
#include "GW/context/map.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/render/render.h"
#include "GW/textures/texture_manager.h"
#include "GW/ui/ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <d3d9.h>

using namespace DirectX;

// Raw raycast bridge owned by the map module (defined in map_patterns.cpp).
// Same local extern shape as map_bindings.cpp; the legacy overlay scanned for
// MapIntersect itself, the port reuses the map module's resolved symbol.
namespace GW::map {
using MapIntersectFn = uint32_t(__cdecl*)(Vec3f* origin, Vec3f* unit_direction, Vec3f* hit_point, int* prop_layer);
extern MapIntersectFn g_map_intersect_func;
bool ResolveMapIntersectFunction();
}

namespace PY4GW::overlay {

GW::Vec3f MouseWorldPos;
GW::Vec2f MouseScreenPos;

void GlobalMouseClass::SetMouseWorldPos(float x, float y, float z) {
    MouseWorldPos.x = x;
    MouseWorldPos.y = y;
    MouseWorldPos.z = z;
}

GW::Vec3f GlobalMouseClass::GetMouseWorldPos() {
    return MouseWorldPos;
}

void GlobalMouseClass::SetMouseCoords(float x, float y) {
    MouseScreenPos.x = x;
    MouseScreenPos.y = y;
}

GW::Vec2f GlobalMouseClass::GetMouseCoords() {
    return MouseScreenPos;
}

namespace {

// UTF-8 -> UTF-16 for TextureManager keys (same helper shape as
// texture_bindings.cpp; the legacy byte-widening loop broke non-ASCII paths).
std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

ImTextureID ToTextureId(IDirect3DTexture9* tex) {
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex));
}

}  // namespace

void Overlay::RefreshDrawList() {
    drawList = ImGui::GetWindowDrawList();
}

GW::Vec2f Overlay::GetMouseCoords() {
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    return GW::Vec2f(mouse_pos.x, mouse_pos.y);
}

XMMATRIX Overlay::CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction) {
    XMMATRIX viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye_pos), XMLoadFloat3(&look_at_pos), XMLoadFloat3(&up_direction));
    return viewMatrix;
}

XMMATRIX Overlay::CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane) {
    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_plane, far_plane);
    return projectionMatrix;
}

// Function to convert world coordinates to screen coordinates
GW::Vec3f Overlay::GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height) {
    GW::Vec3f res;

    // Transform the point into camera space
    XMVECTOR pos = XMVectorSet(world_position.x, world_position.y, world_position.z, 1.0f);
    pos = XMVector3TransformCoord(pos, mat_view);

    // Transform the point into projection space
    pos = XMVector3TransformCoord(pos, mat_proj);

    // Perform perspective division
    XMFLOAT4 projected;
    XMStoreFloat4(&projected, pos);
    if (projected.w < 0.1f) return res; // Point is behind the camera

    projected.x /= projected.w;
    projected.y /= projected.w;
    projected.z /= projected.w;

    // Transform to screen space
    res.x = ((projected.x + 1.0f) / 2.0f) * viewport_width;
    res.y = ((-projected.y + 1.0f) / 2.0f) * viewport_height;
    res.z = projected.z;

    return res;
}

void Overlay::GetScreenToWorld() {
    GW::game_thread::Enqueue([]() {
        GlobalMouseClass setmousepos;

        // Legacy rescanned these patterns on every call; the port resolves
        // them once through offsets/overlay.json (and reuses the map module's
        // MapCliQueryIntersection instead of a duplicate scan).
        if (!ResolveNdcScreenCoords() || !g_ndc_screen_coords) {
            setmousepos.SetMouseWorldPos(0, 0, 0);
            return;
        }
        if (!ResolveScreenToWorldPointFunc() || !g_screen_to_world_point_func) {
            setmousepos.SetMouseWorldPos(0, 0, 0);
            return;
        }
        if (!GW::map::ResolveMapIntersectFunction() || !GW::map::g_map_intersect_func) {
            setmousepos.SetMouseWorldPos(0, 0, 0);
            return;
        }

        GW::Vec2f* screen_coord = g_ndc_screen_coords;

        GW::Vec3f origin;
        GW::Vec3f p2;
        g_screen_to_world_point_func(&origin, screen_coord->x, screen_coord->y, 0);
        g_screen_to_world_point_func(&p2, screen_coord->x, screen_coord->y, 1);
        GW::Vec3f dir = p2 - origin;
        GW::Vec3f ray_dir = GW::Normalize(dir);
        GW::Vec3f hit_point;
        int layer = 0;
        uint32_t ret = GW::map::g_map_intersect_func(&origin, &ray_dir, &hit_point, &layer); // needs to run in game thread

        if (ret) { setmousepos.SetMouseWorldPos(hit_point.x, hit_point.y, hit_point.z); }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); }
    });
}

namespace {

// ---------- map rect ----------
bool InMapBounds(float x, float y) {
    auto* map = GW::Context::GetMapContext();
    if (!map) return true;

    float minx = map->map_boundaries[1];
    float miny = map->map_boundaries[2];
    float maxx = map->map_boundaries[3];
    float maxy = map->map_boundaries[4];

    return x >= minx && x <= maxx && y >= miny && y <= maxy;
}

std::vector<uint32_t> GetValidZPlanes() {
    std::vector<uint32_t> planes;

    auto* map = GW::Context::GetMapContext();
    if (!map || !map->sub1 || !map->sub1->sub2)
        return planes;

    auto& pmaps = map->sub1->sub2->pmaps;
    for (size_t i = 0; i < pmaps.size(); ++i) {
        planes.push_back(static_cast<uint32_t>(i));
    }

    // Always include player's current plane (safety)
    if (auto* player = GW::agent::GetControlledCharacter()) {
        planes.push_back(static_cast<uint32_t>(player->plane));
    }

    // dedupe
    std::sort(planes.begin(), planes.end());
    planes.erase(std::unique(planes.begin(), planes.end()), planes.end());

    return planes;
}

float QueryZ(float x, float y, uint32_t plane) {
    auto* map = GW::Context::GetMapContext();
    if (!map || !map->sub1 || !map->sub1->sub2)
        return 0.0f;

    if (map->sub1->sub2->pmaps.size() <= plane)
        return 0.0f;

    GW::GamePos gp(x, y, plane);
    float z = 0.0f;
    GW::map::QueryAltitude(gp, 5.0f, z);
    return z;
}

// Coverage-aware per-plane query: unlike QueryZ, reports whether the plane
// actually has a surface at (x,y) (QueryAltitude returns 0 when it doesn't), so
// the resolvers can ignore planes that don't cover the point.
bool QueryPlaneAltitude(float x, float y, uint32_t plane, float& out) {
    auto* map = GW::Context::GetMapContext();
    if (!map || !map->sub1 || !map->sub1->sub2)
        return false;
    if (map->sub1->sub2->pmaps.size() <= plane)
        return false;

    GW::GamePos gp(x, y, plane);
    float z = 0.0f;
    if (GW::map::QueryAltitude(gp, 5.0f, z) == 0)
        return false;
    out = z;
    return true;
}

// Topmost covered surface across `planes` at (x,y). up = -z, so topmost = min z
// (this is the same rule MapQueryAltitude uses internally when it min-combines
// terrain and prop). Falls back to plain plane-0 altitude if nothing covers it.
float ResolveTopZ(float x, float y, const std::vector<uint32_t>& planes) {
    bool any = false;
    float best = 0.0f;
    for (uint32_t p : planes) {
        float z = 0.0f;
        if (QueryPlaneAltitude(x, y, p, z) && (!any || z < best)) {
            best = z;
            any = true;
        }
    }
    return any ? best : QueryZ(x, y, 0);
}

// Covered surface whose altitude is nearest ref_z (bridge-over vs under-bridge).
float ResolveNearestZ(float x, float y, float ref_z, const std::vector<uint32_t>& planes) {
    bool any = false;
    float best = 0.0f;
    float best_d = 0.0f;
    for (uint32_t p : planes) {
        float z = 0.0f;
        if (QueryPlaneAltitude(x, y, p, z)) {
            float d = std::abs(z - ref_z);
            if (!any || d < best_d) {
                best = z;
                best_d = d;
                any = true;
            }
        }
    }
    return any ? best : QueryZ(x, y, 0);
}

struct ZResult {
    float z;
    uint32_t plane;
};

ZResult FindZ(float x, float y) {
    // Guard
    if (!InMapBounds(x, y)) {
        if (auto* p = GW::agent::GetControlledCharacter())
            return { p->z, static_cast<uint32_t>(p->plane) };
        return { 0.0f, 0 };
    }

    auto planes = GetValidZPlanes();
    if (planes.empty())
        return { 0.0f, 0 };

    // Base reference
    float base_z = QueryZ(x, y, 0);

    float best_z = base_z;
    uint32_t best_plane = 0;
    float best_delta = 0.0f;

    for (uint32_t plane : planes) {
        float z = QueryZ(x, y, plane);
        float d = std::abs(z - base_z);
        if (d > best_delta) {
            best_delta = d;
            best_z = z;
            best_plane = plane;
        }
    }

    return { best_z, best_plane };
}

bool IsMapReady() {
    auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving() && instance_type != GW::Constants::InstanceType::Loading;
}

}  // namespace

// ---------------- PUBLIC API ----------------
float Overlay::findZ(float x, float y, uint32_t /*zplane_hint*/, bool multi_plane) {
    if (!IsMapReady()) {
        return 0.0f;
    }

    if (multi_plane) {
        // Topmost surface across all planes -- independent of the player's plane,
        // so a fixed point stays on its slope/bridge no matter where the player is.
        return ResolveTopZ(x, y, GetValidZPlanes());
    }

    // Legacy: altitude at the player's plane.
    auto* player = GW::agent::GetControlledCharacter();
    if (!player)
        return 0.0f;
    return QueryZ(x, y, static_cast<uint32_t>(player->plane));
}

std::vector<float> Overlay::FindZBatch(const std::vector<std::pair<float, float>>& pts,
                                       bool multi_plane) {
    std::vector<float> out;
    out.reserve(pts.size());
    if (!IsMapReady()) {
        out.assign(pts.size(), 0.0f);
        return out;
    }
    if (!multi_plane) {
        auto* player = GW::agent::GetControlledCharacter();
        uint32_t pl = player ? static_cast<uint32_t>(player->plane) : 0;
        for (const auto& p : pts)
            out.push_back(QueryZ(p.first, p.second, pl));
        return out;
    }
    // Fetch the plane list ONCE; single-plane maps take the cheap plane-0 path.
    auto planes = GetValidZPlanes();
    if (planes.size() <= 1) {
        for (const auto& p : pts)
            out.push_back(QueryZ(p.first, p.second, 0));
        return out;
    }
    for (const auto& p : pts)
        out.push_back(ResolveTopZ(p.first, p.second, planes));
    return out;
}

uint32_t Overlay::FindZPlane(float x, float y, uint32_t /*zplane_hint*/) {
    if (!IsMapReady()) {
        return 0;
    }

    return FindZ(x, y).plane;
}

uint32_t Overlay::GetZPlaneCount() {
    auto* map = GW::Context::GetMapContext();
    if (!map || !map->sub1 || !map->sub1->sub2)
        return 0;
    return static_cast<uint32_t>(map->sub1->sub2->pmaps.size());
}

std::pair<bool, float> Overlay::QueryAltitudeAt(float x, float y, uint32_t plane) {
    if (!IsMapReady())
        return { false, 0.0f };
    float z = 0.0f;
    bool covered = QueryPlaneAltitude(x, y, plane, z);
    return { covered, covered ? z : 0.0f };
}

float Overlay::GroundZNearest(float x, float y, float ref_z) {
    if (!IsMapReady())
        return 0.0f;
    return ResolveNearestZ(x, y, ref_z, GetValidZPlanes());
}

std::vector<float> Overlay::GroundZNearestBatch(const std::vector<std::pair<float, float>>& pts,
                                                const std::vector<float>& ref_zs) {
    std::vector<float> out;
    out.reserve(pts.size());
    if (!IsMapReady()) {
        out.assign(pts.size(), 0.0f);
        return out;
    }
    auto planes = GetValidZPlanes();
    const bool single = planes.size() <= 1;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (single) {
            out.push_back(QueryZ(pts[i].first, pts[i].second, 0));
        } else {
            float ref = i < ref_zs.size() ? ref_zs[i] : 0.0f;
            out.push_back(ResolveNearestZ(pts[i].first, pts[i].second, ref, planes));
        }
    }
    return out;
}

float Overlay::GroundZWalkable(float x, float y) {
    if (!IsMapReady())
        return 0.0f;
    // Plane 0: MapQueryAltitude routes through PathEngineQueryAltitude when a path
    // engine exists (walkable navmesh height), else base terrain. Plane-independent.
    return QueryZ(x, y, 0);
}

std::vector<float> Overlay::GroundZWalkableBatch(const std::vector<std::pair<float, float>>& pts) {
    std::vector<float> out;
    out.reserve(pts.size());
    if (!IsMapReady()) {
        out.assign(pts.size(), 0.0f);
        return out;
    }
    for (const auto& p : pts)
        out.push_back(QueryZ(p.first, p.second, 0));
    return out;
}

GW::Vec2f Overlay::WorldToScreen(float x, float y, float z) {
    GW::Vec3f world_position = { x, y, z };

    // Legacy cached the camera pointer in a function-local static (stale
    // across map loads); the port fetches it fresh each call.
    auto* cam = GW::Context::GetCamera();
    if (!cam)
        return GW::Vec2f(0.f, 0.f);

    XMFLOAT3 eyePos = { cam->position.x, cam->position.y, cam->position.z };
    XMFLOAT3 lookAtPos = { cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z };
    XMFLOAT3 upDir = { 0.0f, 0.0f, -1.0f };  // Up Vector

    // Create view matrix
    XMMATRIX viewMatrix = CreateViewMatrix(eyePos, lookAtPos, upDir);

    // Define projection parameters
    float fov = GW::render::GetFieldOfView();
    float aspectRatio = static_cast<float>(GW::render::GetViewportWidth()) / static_cast<float>(GW::render::GetViewportHeight());
    float nearPlane = 0.1f;
    float farPlane = 100000.0f;

    // Create projection matrix
    XMMATRIX projectionMatrix = CreateProjectionMatrix(fov, aspectRatio, nearPlane, farPlane);
    GW::Vec3f res = GetWorldToScreen(world_position, viewMatrix, projectionMatrix, static_cast<float>(GW::render::GetViewportWidth()), static_cast<float>(GW::render::GetViewportHeight()));
    return GW::Vec2f(res.x, res.y);
}

GW::Vec3f Overlay::GetMouseWorldPos() {
    GetScreenToWorld();
    GlobalMouseClass mousepos;
    return mousepos.GetMouseWorldPos();
}

namespace {

struct ImRect {
    ImVec2 Min;
    ImVec2 Max;

    ImRect() : Min(), Max() {}
    ImRect(const ImVec2& min, const ImVec2& max) : Min(min), Max(max) {}
    ImRect(float x1, float y1, float x2, float y2) : Min(x1, y1), Max(x2, y2) {}

    ImVec2 GetCenter() const { return ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
    ImVec2 GetSize()   const { return ImVec2(Max.x - Min.x, Max.y - Min.y); }
    bool Contains(const ImVec2& p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y; }
};

bool GetMapWorldMapBounds(GW::Context::AreaInfo* map, ImRect* out) {
    if (!map) return false;
    auto bounds = &map->icon_start_x;
    if (*bounds == 0)
        bounds = &map->icon_start_x_dupe;

    // NB: Even though area info holds map bounds as uints, the world map uses signed floats anyway - a cast should be fine here.
    *out = {
        { static_cast<float>(bounds[0]), static_cast<float>(bounds[1]) },
        { static_cast<float>(bounds[2]), static_cast<float>(bounds[3]) }
    };
    return true;
}

}  // namespace

GW::Vec2f Overlay::GamePosToWorldMap(float x, float y) {
    ImRect map_bounds;
    if (!GetMapWorldMapBounds(GW::map::GetCurrentMapInfo(), &map_bounds))
        return GW::Vec2f(0, 0);
    const auto current_map_context = GW::Context::GetMapContext();
    if (!current_map_context)
        return GW::Vec2f(0, 0);

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    const auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (std::abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (std::abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    // Convert the game coordinates to world map coordinates
    float result_x = (x / gwinches_per_unit) + map_mid_world_point.x;
    float result_y = ((y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
    return GW::Vec2f(result_x, result_y);
}

GW::Vec2f Overlay::WorlMapToGamePos(float x, float y) {
    ImRect map_bounds;
    if (!GetMapWorldMapBounds(GW::map::GetCurrentMapInfo(), &map_bounds))
        return GW::Vec2f(0, 0);
    if (!map_bounds.Contains({ x, y }))
        return GW::Vec2f(0, 0); // Current map doesn't contain these coords; we can't plot a position

    const auto current_map_context = GW::Context::GetMapContext();
    if (!current_map_context)
        return GW::Vec2f(0, 0);

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    constexpr auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (std::abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (std::abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    float result_x = (x - map_mid_world_point.x) * gwinches_per_unit;
    float result_y = ((y - map_mid_world_point.y) * gwinches_per_unit) * -1.f; // Inverted Y Axis
    return GW::Vec2f(result_x, result_y);
}

GW::Vec2f Overlay::WorldMapToScreen(float x, float y) {
    GW::Vec2f position(x, y);
    const auto gameplay_context = GW::Context::GetGameplayContext();
    const auto mission_map_context = GW::Context::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::ui::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible())) return GW::Vec2f(0, 0);

    const auto root = GW::ui::GetRootFrame();
    auto mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
    auto mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    auto mission_map_scale = mission_map_frame->position.GetViewportScale(root);
    auto mission_map_zoom = gameplay_context->mission_map_zoom;
    auto current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;
    auto mission_map_screen_pos = mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;

    const auto offset = (position - current_pan_offset);
    const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
    auto result(scaled_offset * mission_map_zoom + mission_map_screen_pos);
    return GW::Vec2f(result.x, result.y);
}

GW::Vec2f Overlay::ScreenToWorldMap(float screen_x, float screen_y) {
    const auto gameplay = GW::Context::GetGameplayContext();
    const auto mission_map_ctx = GW::Context::GetMissionMapContext();
    const auto frame = mission_map_ctx ? GW::ui::GetFrameById(mission_map_ctx->frame_id) : nullptr;
    if (!(frame && frame->IsVisible())) return GW::Vec2f(0, 0);

    const auto root = GW::ui::GetRootFrame();
    auto top_left = frame->position.GetContentTopLeft(root);
    auto bottom_right = frame->position.GetContentBottomRight(root);
    auto center = top_left + (bottom_right - top_left) / 2;
    auto scale = frame->position.GetViewportScale(root);
    auto zoom = gameplay->mission_map_zoom;
    auto pan_offset = mission_map_ctx->h003c->mission_map_pan_offset;

    GW::Vec2f offset = {
        (screen_x - center.x) / (zoom * scale.x),
        (screen_y - center.y) / (zoom * scale.y)
    };

    auto result = pan_offset + offset;
    return GW::Vec2f(result.x, result.y);
}

GW::Vec2f Overlay::GameMapToScreen(float x, float y) {
    GW::Vec2f world_map_pos = GamePosToWorldMap(x, y);
    return WorldMapToScreen(world_map_pos.x, world_map_pos.y);
}

GW::Vec2f Overlay::ScreenToGameMapPos(float screen_x, float screen_y) {
    GW::Vec2f world = ScreenToWorldMap(screen_x, screen_y);
    return WorlMapToGamePos(world.x, world.y);
}

GW::Vec2f Overlay::NormalizedScreenToScreen(float norm_x, float norm_y) {
    const auto mission_map_context = GW::Context::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::ui::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible()))
        return GW::Vec2f(0.f, 0.f);

    const auto root = GW::ui::GetRootFrame();

    GW::Vec2f top_left = mission_map_frame->position.GetContentTopLeft(root);
    GW::Vec2f bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    GW::Vec2f size = bottom_right - top_left;

    // Convert from [-1, 1] to [0, 1], and invert Y
    float adjusted_x = (norm_x + 1.0f) / 2.0f;
    float adjusted_y = (1.0f - norm_y) / 2.0f;

    float screen_x = top_left.x + adjusted_x * size.x;
    float screen_y = top_left.y + adjusted_y * size.y;

    return GW::Vec2f(screen_x, screen_y);
}

GW::Vec2f Overlay::ScreenToNormalizedScreen(float screen_x, float screen_y) {
    const auto mission_map_context = GW::Context::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::ui::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible()))
        return GW::Vec2f(0.f, 0.f);

    const auto root = GW::ui::GetRootFrame();

    GW::Vec2f top_left = mission_map_frame->position.GetContentTopLeft(root);
    GW::Vec2f bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    GW::Vec2f size = bottom_right - top_left;

    float rel_x = (screen_x - top_left.x) / size.x;
    float rel_y = (screen_y - top_left.y) / size.y;

    // Convert to normalized range [-1, 1], with Y inverted
    float norm_x = rel_x * 2.0f - 1.0f;
    float norm_y = (1.0f - rel_y) * 2.0f - 1.0f;

    return GW::Vec2f(norm_x, norm_y);
}

GW::Vec2f Overlay::NormalizedScreenToWorldMap(float norm_x, float norm_y) {
    GW::Vec2f screen = NormalizedScreenToScreen(norm_x, norm_y);
    return ScreenToWorldMap(screen.x, screen.y);
}

GW::Vec2f Overlay::NormalizedScreenToGameMap(float norm_x, float norm_y) {
    GW::Vec2f world = NormalizedScreenToWorldMap(norm_x, norm_y);
    return WorlMapToGamePos(world.x, world.y);
}

GW::Vec2f Overlay::GamePosToNormalizedScreen(float x, float y) {
    GW::Vec2f screen = GameMapToScreen(x, y);
    return ScreenToNormalizedScreen(screen.x, screen.y);
}

void Overlay::BeginDraw() {
    ImGuiIO& io = ImGui::GetIO();

    // Make the panel cover the whole screen
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    // Create a transparent and click-through panel
    ImGui::Begin("HeroOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs);
}

void Overlay::BeginDraw(std::string name) {
    ImGuiIO& io = ImGui::GetIO();

    // Make the panel cover the whole screen
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    // Create a transparent and click-through panel
    std::string internal_name = "##" + name;
    ImGui::Begin(internal_name.c_str(), nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs);
}

void Overlay::BeginDraw(std::string name, float x, float y, float width, float height) {
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(x, y));
    // Create a transparent and click-through panel
    std::string internal_name = "##" + name;
    ImGui::Begin(internal_name.c_str(), nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs);
}

void Overlay::EndDraw() {
    ImGui::End();
}

void Overlay::DrawLine(GW::Vec2f from, GW::Vec2f to, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 from_imvec = ImVec2(from.x, from.y);
    ImVec2 to_imvec = ImVec2(to.x, to.y);
    drawList->AddLine(from_imvec, to_imvec, color, thickness);
}

void Overlay::DrawLine3D(GW::Vec3f from, GW::Vec3f to, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    GW::Vec2f from3D = WorldToScreen(from.x, from.y, from.z);
    GW::Vec2f to3D = WorldToScreen(to.x, to.y, to.z);

    drawList->AddLine(ImVec2(from3D.x, from3D.y), ImVec2(to3D.x, to3D.y), color, thickness);
}

void Overlay::DrawTriangle(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 v1(p1.x, p1.y);
    ImVec2 v2(p2.x, p2.y);
    ImVec2 v3(p3.x, p3.y);
    drawList->AddTriangle(v1, v2, v3, color, thickness);
}

void Overlay::DrawTriangle3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    GW::Vec2f a = WorldToScreen(p1.x, p1.y, p1.z);
    GW::Vec2f b = WorldToScreen(p2.x, p2.y, p2.z);
    GW::Vec2f c = WorldToScreen(p3.x, p3.y, p3.z);
    drawList->AddTriangle(ImVec2(a.x, a.y), ImVec2(b.x, b.y), ImVec2(c.x, c.y), color, thickness);
}

void Overlay::DrawTriangleFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    drawList->AddTriangleFilled(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), ImVec2(p3.x, p3.y), color);
}

void Overlay::DrawTriangleFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    GW::Vec2f p1Screen = WorldToScreen(p1.x, p1.y, p1.z);
    GW::Vec2f p2Screen = WorldToScreen(p2.x, p2.y, p2.z);
    GW::Vec2f p3Screen = WorldToScreen(p3.x, p3.y, p3.z);
    drawList->AddTriangleFilled(ImVec2(p1Screen.x, p1Screen.y), ImVec2(p2Screen.x, p2Screen.y), ImVec2(p3Screen.x, p3Screen.y), color);
}

void Overlay::DrawQuad(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 points[4] = {
        ImVec2(p1.x, p1.y),
        ImVec2(p2.x, p2.y),
        ImVec2(p3.x, p3.y),
        ImVec2(p4.x, p4.y)
    };
    drawList->AddPolyline(points, 4, color, thickness, ImDrawFlags_Closed);
}

void Overlay::DrawQuad3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();

    GW::Vec2f s1 = WorldToScreen(p1.x, p1.y, p1.z);
    GW::Vec2f s2 = WorldToScreen(p2.x, p2.y, p2.z);
    GW::Vec2f s3 = WorldToScreen(p3.x, p3.y, p3.z);
    GW::Vec2f s4 = WorldToScreen(p4.x, p4.y, p4.z);

    ImVec2 screenPoints[4] = {
        ImVec2(s1.x, s1.y),
        ImVec2(s2.x, s2.y),
        ImVec2(s3.x, s3.y),
        ImVec2(s4.x, s4.y)
    };

    drawList->AddPolyline(screenPoints, 4, color, thickness, ImDrawFlags_Closed);
}

void Overlay::DrawQuadFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 points[4] = {
        ImVec2(p1.x, p1.y),
        ImVec2(p2.x, p2.y),
        ImVec2(p3.x, p3.y),
        ImVec2(p4.x, p4.y)
    };
    drawList->AddConvexPolyFilled(points, 4, color);
}

void Overlay::DrawQuadFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();

    GW::Vec2f s1 = WorldToScreen(p1.x, p1.y, p1.z);
    GW::Vec2f s2 = WorldToScreen(p2.x, p2.y, p2.z);
    GW::Vec2f s3 = WorldToScreen(p3.x, p3.y, p3.z);
    GW::Vec2f s4 = WorldToScreen(p4.x, p4.y, p4.z);

    ImVec2 screenPoints[4] = {
        ImVec2(s1.x, s1.y),
        ImVec2(s2.x, s2.y),
        ImVec2(s3.x, s3.y),
        ImVec2(s4.x, s4.y)
    };

    drawList->AddConvexPolyFilled(screenPoints, 4, color);
}

void Overlay::DrawPoly(GW::Vec2f center, float radius, ImU32 color, int numSegments, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        points.emplace_back(ImVec2(x, y));
    }

    drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, thickness, ImDrawFlags_Closed);
}

void Overlay::DrawPolyFilled(GW::Vec2f center, float radius, ImU32 color, int numSegments) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        points.emplace_back(ImVec2(x, y));
    }

    drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), color);
}

void Overlay::DrawPoly3D(GW::Vec3f center, float radius, ImU32 color, int numSegments, float thickness, bool autoZ) {
    drawList = ImGui::GetWindowDrawList();
    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    std::vector<ImVec2> points;
    points.reserve(numSegments);

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + radius * std::cos(angle);
        float y = center.y + radius * std::sin(angle);
        float z = autoZ ? findZ(x, y, static_cast<uint32_t>(center.z)) : center.z;

        GW::Vec2f screen = WorldToScreen(x, y, z);
        points.emplace_back(ImVec2(screen.x, screen.y));
    }

    drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, thickness, ImDrawFlags_Closed);
}

void Overlay::DrawPolyFilled3D(GW::Vec3f center, float radius, ImU32 color, int numSegments, bool autoZ) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        float z = autoZ ? findZ(x, y, static_cast<uint32_t>(center.z)) : center.z;

        GW::Vec2f screen = WorldToScreen(x, y, z);
        points.emplace_back(ImVec2(screen.x, screen.y));
    }

    drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), color);
}

void Overlay::DrawCubeOutline(GW::Vec3f center, float size, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    float h = size / 2.0f;

    GW::Vec3f corners[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h},
    };

    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (auto& edge : edges) {
        GW::Vec2f p1 = WorldToScreen(corners[edge[0]].x, corners[edge[0]].y, corners[edge[0]].z);
        GW::Vec2f p2 = WorldToScreen(corners[edge[1]].x, corners[edge[1]].y, corners[edge[1]].z);
        drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, thickness);
    }
}

void Overlay::DrawCubeFilled(GW::Vec3f center, float size, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    float h = size / 2.0f;

    GW::Vec3f corners[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h},
    };

    int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4},
        {2,3,7,6}, {0,3,7,4}, {1,2,6,5}
    };

    for (auto& face : faces) {
        ImVec2 screenPts[4];
        for (int i = 0; i < 4; ++i) {
            GW::Vec2f screen = WorldToScreen(corners[face[i]].x, corners[face[i]].y, corners[face[i]].z);
            screenPts[i] = ImVec2(screen.x, screen.y);
        }
        drawList->AddConvexPolyFilled(screenPts, 4, color);
    }
}

void Overlay::DrawText2D(GW::Vec2f position, std::string text, ImU32 color, bool centered, float scale) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 screenPos(position.x, position.y);

    // ImGui 1.92 removed ImFont::Scale (legacy scaled via Scale + Push/PopFont);
    // the equivalent is passing an explicit size to AddText/CalcTextSizeA.
    ImFont* currentFont = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * scale;

    if (centered) {
        ImVec2 textSizeVec = currentFont->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str());
        screenPos.x -= textSizeVec.x / 2;
    }

    drawList->AddText(currentFont, font_size, screenPos, color, text.c_str());
}

void Overlay::DrawText3D(GW::Vec3f position3D, std::string text, ImU32 color, bool autoZ, bool centered, float scale) {
    drawList = ImGui::GetWindowDrawList();
    // Project the 3D position to 2D screen coordinates
    GW::Vec2f screenPosition = { 0, 0 };

    if (autoZ) {
        // Optionally adjust Z if autoZ is true
        float z = findZ(position3D.x, position3D.y, static_cast<uint32_t>(position3D.z));
        screenPosition = WorldToScreen(position3D.x, position3D.y, z);
    }
    else {
        screenPosition = WorldToScreen(position3D.x, position3D.y, position3D.z);
    }

    ImVec2 screenPos(screenPosition.x, screenPosition.y);

    ImFont* currentFont = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * scale;

    if (centered) {
        ImVec2 textSizeVec = currentFont->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str());
        screenPos.x -= textSizeVec.x / 2;
    }

    drawList->AddText(currentFont, font_size, screenPos, color, text.c_str());
}

GW::Vec2f Overlay::GetDisplaySize() {
    ImGuiIO& io = ImGui::GetIO();
    return GW::Vec2f(io.DisplaySize.x, io.DisplaySize.y);
}

bool Overlay::IsMouseClicked(int button) {
    ImGuiIO& io = ImGui::GetIO();
    return io.MouseDown[button];
}

void Overlay::PushClipRect(float x, float y, float x2, float y2) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 min(x, y);
    ImVec2 max(x2, y2);
    drawList->PushClipRect(min, max, true);
}

void Overlay::UpkeepTextures(int timeout) {
    GW::textures::TextureManager::Instance().CleanupOldTextures(timeout);
}

void Overlay::DrawTexture(const std::string& path, float width, float height) {
    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(path));
    if (!tex)
        return;

    ImGui::Image(ToTextureId(tex), ImVec2(width, height));
}

void Overlay::DrawTexture(const std::string& path,
    std::tuple<float, float> size,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> tint,
    std::tuple<int, int, int, int> border_col) {
    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(path));
    if (!tex)
        return;

    ImVec2 im_size(std::get<0>(size), std::get<1>(size));
    ImVec2 im_uv0(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 im_uv1(std::get<0>(uv1), std::get<1>(uv1));

    ImVec4 color = ImVec4(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    ImVec4 border_color = ImVec4(
        std::get<0>(border_col) / 255.0f,
        std::get<1>(border_col) / 255.0f,
        std::get<2>(border_col) / 255.0f,
        std::get<3>(border_col) / 255.0f
    );

    // ImGui 1.92 removed the Image(tint, border) overload; tint goes through
    // ImageWithBg and the border through the border style color + size (this
    // vendored ImGui predates the dedicated ImGuiCol_ImageBorder entry).
    const bool has_border = border_color.w > 0.0f;
    if (has_border) {
        ImGui::PushStyleColor(ImGuiCol_Border, border_color);
        ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1.0f);
    }
    ImGui::ImageWithBg(ToTextureId(tex), im_size, im_uv0, im_uv1, ImVec4(0, 0, 0, 0), color);
    if (has_border) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void Overlay::DrawTexturedRect(float x, float y, float width, float height, const std::string& texture_path) {
    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(texture_path));
    if (!tex)
        return;
    ImGui::GetWindowDrawList()->AddImage(ToTextureId(tex), ImVec2(x, y), ImVec2(x + width, y + height));
}

void Overlay::DrawTexturedRect(std::tuple<float, float> pos,
    std::tuple<float, float> size,
    const std::string& texture_path,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> tint) {
    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(texture_path));
    if (!tex) return;

    const float x = std::get<0>(pos), y = std::get<1>(pos);
    const float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    ImGui::GetWindowDrawList()->AddImage(ToTextureId(tex), ImVec2(x, y), ImVec2(x + w, y + h), uv_start, uv_end, ImGui::ColorConvertFloat4ToU32(tint_color));
}

bool Overlay::ImageButton(const std::string& caption, const std::string& file_path, float width, float height, int frame_padding) {
    auto* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(file_path));
    if (!tex)
        return false;

    ImVec2 size(width, height);

    // Optional: allow frame padding override
    if (frame_padding >= 0)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));

    bool result = ImGui::ImageButton(
        caption.c_str(),     // required unique ID
        ToTextureId(tex),
        size,
        ImVec2(0, 0), ImVec2(1, 1),
        ImVec4(0, 0, 0, 0),  // bg color
        ImVec4(1, 1, 1, 1)   // tint color
    );

    if (frame_padding >= 0)
        ImGui::PopStyleVar();

    return result;
}

bool Overlay::ImageButton(const std::string& caption, const std::string& file_path,
    std::tuple<float, float> size,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> bg_color,
    std::tuple<int, int, int, int> tint_color,
    int frame_padding) {
    auto* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(file_path));
    if (!tex)
        return false;

    ImVec2 sz(std::get<0>(size), std::get<1>(size));
    ImVec2 uv0f(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv1f(std::get<0>(uv1), std::get<1>(uv1));

    ImVec4 bg(
        std::get<0>(bg_color) / 255.0f,
        std::get<1>(bg_color) / 255.0f,
        std::get<2>(bg_color) / 255.0f,
        std::get<3>(bg_color) / 255.0f
    );

    ImVec4 tint(
        std::get<0>(tint_color) / 255.0f,
        std::get<1>(tint_color) / 255.0f,
        std::get<2>(tint_color) / 255.0f,
        std::get<3>(tint_color) / 255.0f
    );

    if (frame_padding >= 0)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));

    bool result = ImGui::ImageButton(
        caption.c_str(),
        ToTextureId(tex),
        sz,
        uv0f,
        uv1f,
        bg,
        tint
    );

    if (frame_padding >= 0)
        ImGui::PopStyleVar();

    return result;
}

void Overlay::DrawTextureInForegound(
    const std::tuple<float, float>& pos,
    const std::tuple<float, float>& size,
    const std::string& texture_path,
    const std::tuple<float, float>& uv0,
    const std::tuple<float, float>& uv1,
    const std::tuple<int, int, int, int>& tint) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(texture_path));
    if (!tex || !draw_list)
        return;

    float x = std::get<0>(pos), y = std::get<1>(pos);
    float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    draw_list->AddImage(
        ToTextureId(tex),
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        uv_start,
        uv_end,
        ImGui::ColorConvertFloat4ToU32(tint_color)
    );
}

void Overlay::DrawTextureInDrawlist(
    const std::tuple<float, float>& pos,
    const std::tuple<float, float>& size,
    const std::string& texture_path,
    const std::tuple<float, float>& uv0,
    const std::tuple<float, float>& uv1,
    const std::tuple<int, int, int, int>& tint) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();  // Draw in current ImGui window

    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(texture_path));
    if (!tex || !draw_list)
        return;

    float x = std::get<0>(pos), y = std::get<1>(pos);
    float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    draw_list->AddImage(
        ToTextureId(tex),
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        uv_start,
        uv_end,
        ImGui::ColorConvertFloat4ToU32(tint_color)
    );
}

}  // namespace PY4GW::overlay

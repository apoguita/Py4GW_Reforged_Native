#include "base/py_bindings.h"

#include "GW/common/game_pos.h"
#include "GW/context/context.h"
#include "GW/context/map.h"
#include "GW/context/pathing.h"
#include "GW/map/map.h"

#include <array>
#include <cstdint>
#include <string>

namespace py = pybind11;

// Raw raycast bridge symbols owned by the map module (defined in map_patterns.cpp).
namespace GW::map {
using MapIntersectFn = uint32_t(__cdecl*)(Vec3f* origin, Vec3f* unit_direction, Vec3f* hit_point, int* prop_layer);
using TerrainQueryIntersectionFn = uint32_t(__cdecl*)(void* terrain, Vec3f* src, Vec3f* dest, float unk, float* dist);
using GrModelIntersectConeFn = uint32_t(__cdecl*)(void* model, Vec3f* origin, Vec3f* unit_dir, float cone, float* distance, int flag);

extern MapIntersectFn g_map_intersect_func;
extern TerrainQueryIntersectionFn g_terrain_query_intersection_func;
extern GrModelIntersectConeFn g_gr_model_intersect_cone_func;

bool ResolveMapIntersectFunction();
bool ResolveTerrainQueryIntersection();
bool ResolveGrModelIntersectCone();
}

namespace {

py::tuple MapQueryAltitude(float x, float y, float radius) {
    float altitude = 0.0f;
    GW::GamePos pos{};
    pos.x = x;
    pos.y = y;
    pos.zplane = 0;
    GW::Vec3f terrain_normal{};
    const int result = GW::map::QueryAltitude(pos, radius, altitude, &terrain_normal);
    return py::make_tuple(result != 0 ? 1 : 0,
        altitude,
        terrain_normal.x,
        terrain_normal.y,
        terrain_normal.z);
}

// ============================================================================
// RAW map collision raycast bridge (ported from legacy py_map.cpp, PR #23).
//
// A faithful RAW bridge to the engine's world-intersect primitives plus the live
// prop-memory walk. It deliberately does NO derived math -- normalize, segment
// length, the blocked decision, hit-point reconstruction and the prop-mesh v*M
// transform all live on the Python side (Py4GWCoreLib Map.Raycast). C++ here is
// limited to the engine intersect calls and the live-memory walking only native
// can do, returning raw engine numbers verbatim.
//
// Everything runs on the Py4GW script thread (the game main loop), where the
// engine state these read is stable -- same as the rest of the read APIs.
// ============================================================================

// ioDist seed used where the engine takes an in/out distance argument. Set well beyond
// any real segment so the engine reports the raw nearest hit and the Python side applies
// the actual segment-length bound + slack in its blocked decision.
constexpr float kRayMaxDistance = 1.0e7f;

// Raw reads off a live engine struct by byte offset.
inline const void* ReadPtr(const void* base, std::size_t off) {
    return *reinterpret_cast<const void* const*>(reinterpret_cast<const char*>(base) + off);
}
inline uint32_t ReadU32(const void* base, std::size_t off) {
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(base) + off);
}
inline float ReadF32(const void* base, std::size_t off) {
    return *reinterpret_cast<const float*>(reinterpret_cast<const char*>(base) + off);
}

// RAW: the engine writes a world hit POINT via outHit (it does not expose a fraction),
// so this returns that point verbatim plus prop_layer. Python supplies the unit direction
// it computed and derives the hit distance + blocked decision.
//   Returns (has_hit, hit_x, hit_y, hit_z, prop_layer):
//     has_hit    : the engine returned a contact (within ~25000 units).
//     prop_layer : 0 -> nearest hit was TERRAIN (or no hit); != 0 -> nearest hit was a
//                  PROP (PathGetMap-packed layer<<16|index).
py::tuple RayCast(std::array<float, 3> start, std::array<float, 3> unit_dir) {
    if (!GW::map::ResolveMapIntersectFunction())
        return py::make_tuple(false, 0.0f, 0.0f, 0.0f, 0);

    GW::Vec3f origin{ start[0], start[1], start[2] };
    GW::Vec3f dir{ unit_dir[0], unit_dir[1], unit_dir[2] };
    GW::Vec3f hit{};
    int prop_layer = 0;
    const uint32_t has_hit = GW::map::g_map_intersect_func(&origin, &dir, &hit, &prop_layer);
    return py::make_tuple((bool)has_hit, hit.x, hit.y, hit.z, prop_layer);
}

// Terrain-only collision raycast from start to end (world (x,y,z)). RAW: returns the
// engine's hit fraction only; Python scales by seg_len, reconstructs the hit point and
// decides blocked.
//   Returns (has_hit, frac): has_hit = engine contact flag; frac = parametric hit
//   position in [0,1] along start->end (NOT world units; only meaningful when has_hit).
//   The bound terrain kernel writes this fraction; the public rescaling wrapper is not
//   bound, so Python multiplies frac by seg_len to get the world distance.
py::tuple RayCastTerrain(std::array<float, 3> start, std::array<float, 3> end) {
    GW::Context::MapContext* ctx = GW::Context::GetMapContext();
    if (!GW::map::ResolveTerrainQueryIntersection() || !ctx || !ctx->terrain)
        return py::make_tuple(false, 0.0f);

    GW::Vec3f origin{ start[0], start[1], start[2] };
    GW::Vec3f target{ end[0], end[1], end[2] };
    // The endpoints define the ray; the kernel ignores this ioDist in-value and overwrites
    // it with the hit fraction [0,1].
    float frac = kRayMaxDistance;
    const uint32_t hit = GW::map::g_terrain_query_intersection_func(ctx->terrain, &origin, &target, 0.0f, &frac);
    return py::make_tuple((bool)hit, frac);
}

// One prop's interactive collision geosets vs the ray. `distance` is in/out (in = max
// range, out = nearest hit distance, world units along unit_dir). Returns 1 on hit.
// Mirrors IProps::IntersectGeometry (EXE 0x00736cb0) with guards.
//
// PropsContext extended fields: recArray=*(props+0x1c0), recArrayCount=*(props+0x1c8),
//   epochToken=*(props+0x1e0).  MapProp: interactive_model(+0x58), token(+0x60),
//   recOffset(+0x64), recCount(+0x68).
uint32_t PropIntersectGeometry(const GW::Context::PropsContext* props, const GW::Context::MapProp* prop,
                               GW::Vec3f& origin, GW::Vec3f& unit_dir, float* distance) {
    if (!GW::map::g_gr_model_intersect_cone_func || !prop || !distance) return 0;
    if (!ReadPtr(prop, 0x58)) return 0;                              // interactive_model
    const uint32_t rec_count = ReadU32(prop, 0x68);
    if (rec_count == 0) return 0;                                    // no geosets
    if (ReadU32(prop, 0x60) != ReadU32(props, 0x1e0)) return 0;      // epoch / validity token
    const uint32_t rec_off = ReadU32(prop, 0x64);
    if (rec_off + rec_count > ReadU32(props, 0x1c8)) return 0;       // bounds vs recArray count
    const char* rec_base = reinterpret_cast<const char*>(ReadPtr(props, 0x1c0));
    if (!rec_base) return 0;
    void** rec = reinterpret_cast<void**>(const_cast<char*>(rec_base) + rec_off * 4);
    for (uint32_t i = 0; i < rec_count; ++i, ++rec) {
        if (*rec && GW::map::g_gr_model_intersect_cone_func(*rec, &origin, &unit_dir, 0.0f, distance, 1))
            return 1;
    }
    return 0;
}

// Loops all props, OR-ing the interactive-object geoset test; threads the nearest hit
// distance. RAW: returns the engine's threaded nearest distance + winning prop; Python
// reconstructs the hit point and decides blocked. Python supplies the unit direction and
// max_range (the ioDist in-value).
//   Returns (has_hit, dist, prop_id, n_scanned):
//     has_hit   : an interactive prop's mesh was hit within max_range.
//     dist      : nearest hit distance along unit_dir (only meaningful when has_hit).
//     prop_id   : propArray index of that prop (-1 if none).
//     n_scanned : interactive props tested when the probe ran (0 => the map carries no
//                 interactive-prop meshes, a legitimate clear). -1 => the probe could NOT
//                 run (function unresolved / no map-props context / non-unit dir) --
//                 distinct from a clear result so callers don't read it as line of sight.
py::tuple RayCastInteractive(std::array<float, 3> start, std::array<float, 3> unit_dir,
                             float max_range) {
    if (!GW::map::ResolveGrModelIntersectCone())
        return py::make_tuple(false, 0.0f, -1, -1);

    GW::Context::MapContext* ctx = GW::Context::GetMapContext();
    if (!ctx || !ctx->props)
        return py::make_tuple(false, 0.0f, -1, -1);
    GW::Context::PropsContext* props = ctx->props;
    GW::Context::MapProp** buf = props->propArray.m_buffer;
    const uint32_t count = props->propArray.size();
    if (!buf)
        return py::make_tuple(false, 0.0f, -1, -1);

    GW::Vec3f origin{ start[0], start[1], start[2] };
    GW::Vec3f dir{ unit_dir[0], unit_dir[1], unit_dir[2] };

    // Precondition guard: CIGrModel::IntersectCone hard-asserts on a non-unit direction
    // ("Invalid unit vector" -> no-return abort, client crash). Map.Raycast always passes a
    // normalized dir, but reject a degenerate/non-unit dir here too so a raw PyMap caller
    // cannot crash the client. n_scanned == -1 => the probe could not run.
    const float m2 = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    if (!(m2 > 0.999f && m2 < 1.001f))
        return py::make_tuple(false, 0.0f, -1, -1);

    float dist = max_range;          // ioDist: in = max range, out = nearest hit distance
    int best_prop = -1;
    int n_scanned = 0;

    for (uint32_t i = 0; i < count; ++i) {
        GW::Context::MapProp* prop = buf[i];
        if (!prop || !ReadPtr(prop, 0x58)) continue;   // only props with an interactive_model
        ++n_scanned;
        if (PropIntersectGeometry(props, prop, origin, dir, &dist))
            best_prop = (int)i;                        // threaded dist => last write is the nearest
    }

    return py::make_tuple(best_prop >= 0, dist, best_prop, n_scanned);
}

// Enumerate every map prop with world position + collision flags, for overlay debugging
// ("draw the props"). Reuses the propArray iteration of RayCastInteractive. One tuple per
// prop:
//   (prop_id, x, y, z, is_interactive, rec_count)
//     prop_id        : propArray index (matches RayCastInteractive's prop_id)
//     x, y, z        : prop world position (center +0x20/+0x24, vertZ +0x28)
//     is_interactive : prop carries an interactive_model (+0x58) -> the set
//                      RayCastInteractive scans
//     rec_count      : collision geoset count (+0x68); >0 means it has meshes
py::list GetProps() {
    py::list out;
    GW::Context::MapContext* ctx = GW::Context::GetMapContext();
    if (!ctx || !ctx->props) return out;
    GW::Context::PropsContext* props = ctx->props;
    GW::Context::MapProp** buf = props->propArray.m_buffer;
    const uint32_t count = props->propArray.size();
    if (!buf) return out;
    for (uint32_t i = 0; i < count; ++i) {
        const GW::Context::MapProp* prop = buf[i];
        if (!prop) continue;
        const bool interactive = ReadPtr(prop, 0x58) != nullptr;
        out.append(py::make_tuple((int)i,
                                  ReadF32(prop, 0x20), ReadF32(prop, 0x24), ReadF32(prop, 0x28),
                                  interactive, ReadU32(prop, 0x68)));
    }
    return out;
}

// One prop's COLLISION MESH as RAW LOCAL triangles + per-submodel world matrices, for
// wireframe drawing ("draw the prop's whole shape"). Walks the SAME chain
// RayCastInteractive / IProps::IntersectGeometry tests, so the drawn shape == the tested
// shape:
//   prop -> recArray handle -> CIGrModel -> submodels -> CIGrGeoset -> geom -> u16 index
//   triples over the local Coord3f vertex buffer + the submodel world matrix.
// RAW: emits the local vertices and the submodel's row-major 3x4 world matrix; the
// per-vertex v*M transform is done in Python (Map.Raycast.GetPropGeometry). This keeps the
// matrix orientation/transpose question a Python-side tweak with no rebuild.
// Pure hand-reads, no engine calls / no byte-pattern scanners (the handle "deref" is just
// record[0]; HandleDereference @EXE 0x0046fd00 returns *handle after an access-key check).
// Returns a list of (matrix12, tris_local) per submodel, where matrix12 = 12 floats
// (row-major 3x4 affine) and tris_local = list of (lx1,ly1,lz1, lx2,ly2,lz2, lx3,ly3,lz3)
// LOCAL triangles.
py::list GetPropGeometry(int prop_id) {
    py::list out;
    GW::Context::MapContext* ctx = GW::Context::GetMapContext();
    if (!ctx || !ctx->props || prop_id < 0)
        return out;
    GW::Context::PropsContext* props = ctx->props;
    GW::Context::MapProp** buf = props->propArray.m_buffer;
    const uint32_t prop_count = props->propArray.size();
    if (!buf || (uint32_t)prop_id >= prop_count)
        return out;
    GW::Context::MapProp* prop = buf[prop_id];
    if (!prop || !ReadPtr(prop, 0x58))                      // model-present flag
        return out;

    const uint32_t rec_count = ReadU32(prop, 0x68);
    if (rec_count == 0) return out;
    if (ReadU32(prop, 0x60) != ReadU32(props, 0x1e0)) return out;  // epoch / validity token
    const uint32_t rec_off = ReadU32(prop, 0x64);
    if (rec_off + rec_count > ReadU32(props, 0x1c8)) return out;
    const char* rec_base = reinterpret_cast<const char*>(ReadPtr(props, 0x1c0));
    if (!rec_base) return out;

    const int MAX_TRIS = 20000;                             // safety cap (one prop)
    int emitted = 0;

    for (uint32_t r = 0; r < rec_count && emitted < MAX_TRIS; ++r) {
        // recArray entry = pointer to a handle record; record[0] = object ptr (CIGrModel).
        const void* hrec = ReadPtr(rec_base, (rec_off + r) * 4);
        if (!hrec) continue;
        const char* model = reinterpret_cast<const char*>(ReadPtr(hrec, 0x0));
        if (!model || ReadU32(model, 0xc) != 5) continue;   // CIGrModel type tag == 5

        const uint32_t sub_count = ReadU32(model, 0xa4);
        const char* sub_base = reinterpret_cast<const char*>(ReadPtr(model, 0x9c));
        const int* idx_arr = reinterpret_cast<const int*>(ReadPtr(model, 0x38));   // transform-index array
        const uint32_t idx_len = ReadU32(model, 0x40);
        const char* mat_base = reinterpret_cast<const char*>(ReadPtr(model, 0x44)); // CIGrMatrix array (stride 0x34)
        const uint32_t mat_len = ReadU32(model, 0x4c);
        if (!sub_base || !idx_arr || !mat_base) continue;

        for (uint32_t s = 0; s < sub_count && emitted < MAX_TRIS; ++s) {
            const char* sub = sub_base + s * 0x7c;
            if (!ReadU32(sub, 0x78)) continue;              // geoset-present guard
            const char* geoset = reinterpret_cast<const char*>(ReadPtr(sub, 0x70));
            if (!geoset) continue;

            // World matrix for this submodel: row-major 3x4 affine, 12 floats at +0x00.
            if (s >= idx_len) continue;
            const int ti = idx_arr[s];
            if (ti < 0 || (uint32_t)ti >= mat_len) continue;
            const float* M = reinterpret_cast<const float*>(mat_base + (size_t)ti * 0x34);

            const char* geom = reinterpret_cast<const char*>(ReadPtr(geoset, 0x10));
            if (!geom) continue;
            const uint16_t* idx = reinterpret_cast<const uint16_t*>(ReadPtr(geom, 0x08));
            const uint32_t idx_count = ReadU32(geom, 0x10);
            const uint32_t vert_count = ReadU32(geom, 0x18);
            const char* vbase = reinterpret_cast<const char*>(ReadPtr(geom, 0x1c));
            const uint32_t stride = ReadU32(geom, 0x54);
            if (!idx || !vbase || idx_count < 3 || vert_count == 0 || stride < 12)
                continue;

            // Per-submodel: raw local triangles + the 12-float world matrix. Python does v*M.
            py::tuple matrix = py::make_tuple(M[0], M[1], M[2], M[3],
                                              M[4], M[5], M[6], M[7],
                                              M[8], M[9], M[10], M[11]);
            py::list tris_local;
            const uint32_t tri_count = idx_count / 3;
            for (uint32_t t = 0; t < tri_count && emitted < MAX_TRIS; ++t) {
                const uint16_t tri[3] = { idx[3 * t + 0], idx[3 * t + 1], idx[3 * t + 2] };
                if (tri[0] >= vert_count || tri[1] >= vert_count || tri[2] >= vert_count)
                    continue;
                const float* v0 = reinterpret_cast<const float*>(vbase + (size_t)tri[0] * stride);
                const float* v1 = reinterpret_cast<const float*>(vbase + (size_t)tri[1] * stride);
                const float* v2 = reinterpret_cast<const float*>(vbase + (size_t)tri[2] * stride);
                tris_local.append(py::make_tuple(v0[0], v0[1], v0[2],
                                                 v1[0], v1[1], v1[2],
                                                 v2[0], v2[1], v2[2]));
                ++emitted;
            }
            if (py::len(tris_local) > 0)
                out.append(py::make_tuple(matrix, tris_local));
        }
    }
    return out;
}

} // namespace

PYBIND11_EMBEDDED_MODULE(PyMap, m) {
    m.doc() = "Py4GW Map bindings";

    m.def("travel", [](uint32_t map_id, int region, int district_number, int language) {
        return GW::map::Travel(
            static_cast<GW::Constants::MapID>(map_id),
            static_cast<GW::Constants::ServerRegion>(region),
            district_number,
            static_cast<GW::Constants::Language>(language));
    }, py::arg("map_id"), py::arg("region") = -2, py::arg("district_number") = 0, py::arg("language") = 0);

    m.def("travel_to_district", [](uint32_t map_id, int district, int district_number) {
        return GW::map::Travel(
            static_cast<GW::Constants::MapID>(map_id),
            static_cast<GW::Constants::District>(district),
            district_number);
    }, py::arg("map_id"), py::arg("district") = 0, py::arg("district_number") = 0);

    m.def("map_test_start", [](uint32_t map_id, uint32_t alt_map_id, int number, uint32_t count, uint32_t delay_ms, uint32_t timeout_ms, uint32_t message_id) {
        GW::map::MapTestStart(map_id, alt_map_id, number, count, delay_ms, timeout_ms, message_id);
    }, py::arg("map_id"), py::arg("alt_map_id"), py::arg("number") = 2, py::arg("count") = 3,
       py::arg("delay_ms") = 0, py::arg("timeout_ms") = 10000, py::arg("message_id") = 0x10000098);

    m.def("map_test_stop", []() { GW::map::MapTestStop(); });

    m.def("map_test_get_status", []() -> std::string {
        auto* status = GW::map::MapTestGetStatus();
        return status ? status : "";
    });

    m.def("map_test_is_active", []() -> bool { return GW::map::MapTestIsActive(); });

    m.def("map_test_get_count", []() -> uint32_t { return GW::map::MapTestGetCount(); });

    m.def("enter_challenge", []() -> bool { return GW::map::EnterChallenge(); });

    m.def("cancel_enter_challenge", []() -> bool { return GW::map::CancelEnterChallenge(); });

    m.def("query_altitude", &MapQueryAltitude, py::arg("x"), py::arg("y"), py::arg("radius") = 100.0f);

    m.def("get_is_map_loaded", []() -> bool { return GW::map::GetIsMapLoaded(); });

    m.def("get_map_id", []() -> uint32_t { return static_cast<uint32_t>(GW::map::GetMapID()); });

    m.def("get_is_map_unlocked", [](uint32_t map_id) -> bool {
        return GW::map::GetIsMapUnlocked(static_cast<GW::Constants::MapID>(map_id));
    }, py::arg("map_id"));

    m.def("get_region", []() -> int { return static_cast<int>(GW::map::GetRegion()); });

    m.def("get_language", []() -> int { return static_cast<int>(GW::map::GetLanguage()); });

    m.def("get_is_observing", []() -> bool { return GW::map::GetIsObserving(); });

    m.def("get_district", []() -> int { return GW::map::GetDistrict(); });

    m.def("get_instance_time", []() -> uint32_t { return GW::map::GetInstanceTime(); });

    m.def("get_instance_type", []() -> uint32_t { return static_cast<uint32_t>(GW::map::GetInstanceType()); });

    m.def("get_foes_killed", []() -> uint32_t { return GW::map::GetFoesKilled(); });

    m.def("get_foes_to_kill", []() -> uint32_t { return GW::map::GetFoesToKill(); });

    m.def("get_is_in_cinematic", []() -> bool { return GW::map::GetIsInCinematic(); });

    m.def("skip_cinematic", []() -> bool { return GW::map::SkipCinematic(); });

    m.def("region_from_district", [](int district) -> int {
        return static_cast<int>(GW::map::RegionFromDistrict(static_cast<GW::Constants::District>(district)));
    }, py::arg("district"));

    m.def("language_from_district", [](int district) -> int {
        return static_cast<int>(GW::map::LanguageFromDistrict(static_cast<GW::Constants::District>(district)));
    }, py::arg("district"));

    // RAW collision raycast bridge (math done in Python).
    m.def("RayCast", &RayCast, py::arg("start"), py::arg("unit_dir"),
          "RAW combined terrain + walkable-prop raycast (MapCliQueryIntersection). "
          "start = world (x,y,z) origin; unit_dir = unit direction (caller-normalized). "
          "Returns (has_hit: bool, hit_x: float, hit_y: float, hit_z: float, prop_layer: int): "
          "the engine's raw contact point and prop_layer (0 = terrain, != 0 = prop). "
          "No blocked decision / no distance bound -- the Python side (Map.Raycast.Cast) "
          "computes the hit distance and decides line of sight.");
    m.def("RayCastTerrain", &RayCastTerrain, py::arg("start"), py::arg("end"),
          "RAW terrain-only raycast (ITerrain::QueryIntersection) from start to end (world x,y,z). "
          "Returns (has_hit: bool, frac: float): the engine contact flag and the parametric hit "
          "position in [0,1] along start->end (NOT world units -- the bound terrain kernel writes "
          "a fraction). Python (Map.Raycast.CastTerrain) multiplies frac by seg_len, reconstructs "
          "the hit point and decides blocked against the segment length.");
    m.def("RayCastInteractive", &RayCastInteractive, py::arg("start"), py::arg("unit_dir"),
          py::arg("max_range"),
          "RAW interactive-object mesh probe: tests start + t*unit_dir (t in [0, max_range]) "
          "against the collision geosets of props that carry an interactive_model (doors, gates, "
          "gadgets) -- which the walkable-prop raycast does not always cover. Mirrors "
          "IProps::IntersectGeometry via the per-geoset CIGrModel::IntersectCone (stable assertion "
          "anchor). Returns (has_hit: bool, dist: float, prop_id: int, n_scanned: int): the threaded "
          "nearest hit distance + winning prop. n_scanned == -1 means the probe could not run "
          "(unresolved symbol / no map-props context / non-unit dir); 0 means it ran and the map "
          "has no interactive-prop meshes (a clear result). Python (Map.Raycast.CastInteractive) "
          "supplies unit_dir/max_range, reconstructs the point and decides blocked.");
    m.def("GetProps", &GetProps,
          "Enumerate all map props for overlay debugging. Returns a list of "
          "(prop_id: int, x: float, y: float, z: float, is_interactive: bool, rec_count: int) "
          "per prop. is_interactive props with rec_count>0 are the ones RayCastInteractive scans.");
    m.def("GetPropGeometry", &GetPropGeometry, py::arg("prop_id"),
          "RAW collision MESH of one prop (the same geosets RayCastInteractive tests), for "
          "wireframe drawing. Returns a list of (matrix12, tris_local) per submodel, where "
          "matrix12 is 12 floats (row-major 3x4 affine world matrix) and tris_local is a list of "
          "(lx1,ly1,lz1, lx2,ly2,lz2, lx3,ly3,lz3) LOCAL-space triangles. The v*M world transform "
          "is applied in Python (Map.Raycast.GetPropGeometry). Empty if the prop has no geometry.");
}

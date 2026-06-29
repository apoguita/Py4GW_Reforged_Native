#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_patcher.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/constants/constants.h"
#include "GW/common/game_pos.h"
#include "GW/context/map.h"
#include "GW/context/pathing.h"
#include "GW/ui/ui.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace GW::map {

struct MapDimensions {
    uint32_t unk;
    uint32_t start_x;
    uint32_t start_y;
    uint32_t end_x;
    uint32_t end_y;
    uint32_t unk1;
};

struct InstanceInfo {
    MapDimensions* terrain_info1;
    GW::Constants::InstanceType instance_type;
    GW::Context::AreaInfo* current_map_info;
    uint32_t terrain_count;
    MapDimensions* terrain_info2;
};

struct MapTypeInstanceInfo {
    uint32_t request_instance_map_type;
    bool is_outpost;
    Context::RegionType map_region_type;
};

struct MissionMapSubContext {
    uint32_t h0000[0xE];
};

struct MissionMapSubContext2 {
    uint32_t h0000;
    GW::Vec2f player_mission_map_pos;
    uint32_t h000c;
    GW::Vec2f mission_map_size;
    float unk;
    GW::Vec2f mission_map_pan_offset;
    GW::Vec2f mission_map_pan_offset2;
    float unk2[2];
    uint32_t unk3[9];
};
static_assert(sizeof(MissionMapSubContext2) == 0x58, "MissionMapSubContext2 size mismatch");

struct MissionMapContext {
    GW::Vec2f size;
    uint32_t h0008;
    GW::Vec2f last_mouse_location;
    uint32_t frame_id;
    GW::Vec2f player_mission_map_pos;
    GW::GWArray<MissionMapSubContext*> h0020;
    uint32_t h0030;
    uint32_t h0034;
    uint32_t h0038;
    MissionMapSubContext2* h003c;
    uint32_t h0040;
    uint32_t h0044;
};
static_assert(sizeof(MissionMapContext) == 0x48, "MissionMapContext size mismatch");

struct WorldMapContext {
    uint32_t frame_id;
    uint32_t h0004;
    uint32_t h0008;
    float h000c;
    float h0010;
    uint32_t h0014;
    float h0018;
    float h001c;
    float h0020;
    float h0024;
    float h0028;
    float h002c;
    float h0030;
    float h0034;
    float zoom;
    GW::Vec2f top_left;
    GW::Vec2f bottom_right;
    uint32_t h004c[7];
    float h0068;
    float h006c;
    uint32_t params[0x6D];
};
static_assert(sizeof(WorldMapContext) == 0x224, "WorldMapContext size mismatch");

bool Initialize();
void Shutdown();

using QueryAltitudeFn = int(__cdecl*)(
    const GamePos* point,
    float radius,
    float* altitude,
    Vec3f* terrain_normal);
using VoidFn = void(__cdecl*)();
using DoActionFn = void(__cdecl*)(uint32_t);

MissionMapContext* GetMissionMapContext();
WorldMapContext* GetWorldMapContext();
bool Travel(GW::Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number = 0, GW::Constants::Language language = static_cast<GW::Constants::Language>(0));
bool Travel(GW::Constants::MapID map_id, GW::Constants::District district = static_cast<GW::Constants::District>(0), int district_number = 0);
bool MapTestStart(uint32_t map_id, uint32_t alt_map_id, int number = 2, uint32_t count = 3, uint32_t delay_ms = 0, uint32_t timeout_ms = 10000, uint32_t message_id = 0x10000098);
void MapTestStop();
const char* MapTestGetStatus();
bool MapTestIsActive();
uint32_t MapTestGetCount();
bool EnterChallenge();

int QueryAltitude(const GamePos& pos, float radius, float& altitude, Vec3f* terrain_normal = nullptr);
bool GetIsMapLoaded();
GW::Constants::MapID GetMapID();
bool GetIsMapUnlocked(GW::Constants::MapID map_id);
GW::Constants::ServerRegion GetRegion();
uintptr_t GetServerRegionPtr();
MapTypeInstanceInfo* GetMapTypeInstanceInfo(Context::RegionType map_type);
GW::Constants::Language GetLanguage();
bool GetIsObserving();
int GetDistrict();
uint32_t GetInstanceTime();
GW::Constants::InstanceType GetInstanceType();
GW::Constants::ServerRegion RegionFromDistrict(GW::Constants::District district);
GW::Constants::Language LanguageFromDistrict(GW::Constants::District district);
Context::MissionMapIconArray* GetMissionMapIconArray();
Context::PathingMapArray* GetPathingMap();
uint32_t GetFoesKilled();
uint32_t GetFoesToKill();
Context::AreaInfo* GetMapInfo(GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0));
uintptr_t GetInstanceInfoPtr();
inline Context::AreaInfo* GetCurrentMapInfo() {
    return GetMapInfo(GetMapID());
}
bool GetIsInCinematic();
bool SkipCinematic();
bool CancelEnterChallenge();

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void OnEnterChallengeMission_Hook(uint32_t identifier);
void OnEnterChallengeMission_UIMessage(PY4GW::HookStatus* status, ui::UIMessage msg_id, void* wparam, void*);

extern QueryAltitudeFn g_query_altitude_func;
extern VoidFn g_skip_cinematic_func;
extern VoidFn g_cancel_enter_challenge_mission_func;
extern DoActionFn g_enter_challenge_mission_func;
extern DoActionFn g_enter_challenge_mission_original;
extern GW::Constants::ServerRegion* g_region_id_addr;
extern Context::AreaInfo* g_area_info_addr;
extern MapTypeInstanceInfo* g_map_type_instance_infos;
extern uint32_t g_map_type_instance_infos_size;
extern uintptr_t g_instance_info_ptr;
extern InstanceInfo* g_instance_info;
extern PY4GW::MemoryPatcher g_bypass_tolerance_patch;
extern PY4GW::HookEntry g_enter_challenge_mission_hook_entry;
extern std::atomic<bool> g_initialized;

extern WorldMapContext* g_world_map_context;
extern MissionMapContext* g_mission_map_context;

struct MapTestState {
    bool active = false;
    uint32_t map_id = 0;
    uint32_t alt_map_id = 0;
    int number = 2;
    uint32_t count = 3;
    uint32_t delay_ms = 0;
    uint32_t timeout_ms = 10000;
    uint32_t message_id = 0;
    uint32_t tries = 0;
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t t2 = 0;
    bool seen = false;
    uint32_t phase = 0;
    std::string status = "idle";
};
extern MapTestState g_map_test_state;
void MapTestStep0();
void MapTestSetPhase(uint32_t phase, const char* status_text);

inline bool ResolveSkipCinematic() {
    CrashContextScope context("startup", "map", "resolve_skip_cinematic");
    const auto* pattern = PY4GW::Patterns::Get("map.skip_cinematic");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.skip_cinematic", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    g_skip_cinematic_func = reinterpret_cast<VoidFn>(address);
    return Logger::AssertAddress("SkipCinematic_Func", reinterpret_cast<uintptr_t>(g_skip_cinematic_func), "map");
}

inline bool ResolveRegionId() {
    CrashContextScope context("startup", "map", "resolve_region_id");
    const auto* pattern = PY4GW::Patterns::Get("map.region_id_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.region_id_ref", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("RegionId_Ref", address, "map")) {
        return false;
    }
    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("RegionId_Ptr", candidate, "map")) {
        return false;
    }
    g_region_id_addr = reinterpret_cast<GW::Constants::ServerRegion*>(candidate);
    return true;
}

inline bool ResolveAreaInfo() {
    CrashContextScope context("startup", "map", "resolve_area_info");
    const auto* pattern = PY4GW::Patterns::Get("map.area_info_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.area_info_ref", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("AreaInfo_Ref", address, "map")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("AreaInfo_Ptr", candidate, "map")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate, PY4GW::ScannerSection::RData)) {
        Logger::Instance().LogError("Area info pointer is outside the expected rdata section.", "map");
        return false;
    }

    g_area_info_addr = reinterpret_cast<GW::Context::AreaInfo*>(candidate);
    return true;
}

inline bool ResolveInstanceInfo() {
    CrashContextScope context("startup", "map", "resolve_instance_info");
    const auto* pattern = PY4GW::Patterns::Get("map.instance_info_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.instance_info_ref", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("InstanceInfo_Ref", address, "map")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("InstanceInfo_Ptr", candidate, "map")) {
        return false;
    }

    g_instance_info = reinterpret_cast<InstanceInfo*>(candidate);
    g_instance_info_ptr = address;
    return true;
}

inline bool ResolveQueryAltitude() {
    CrashContextScope context("startup", "map", "resolve_query_altitude");
    const auto* pattern = PY4GW::Patterns::Get("map.query_altitude_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.query_altitude_callsite", "map");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("QueryAltitude_Callsite", callsite, "map")) {
        return false;
    }

    g_query_altitude_func = reinterpret_cast<QueryAltitudeFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress("QueryAltitude_Func", reinterpret_cast<uintptr_t>(g_query_altitude_func), "map");
}

inline bool ResolveBypassTolerancePatch() {
    CrashContextScope context("startup", "map", "resolve_bypass_tolerance_patch");
    const auto* pattern = PY4GW::Patterns::Get("map.bypass_tolerance_patch");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.bypass_tolerance_patch", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("BypassTolerancePatch_Addr", address, "map")) {
        return false;
    }

    static constexpr char patch[] = "\xEB";
    g_bypass_tolerance_patch.SetPatch(address, patch, 1);
    return g_bypass_tolerance_patch.IsValid();
}

inline bool ResolveEnterChallengeFunctions() {
    CrashContextScope context("startup", "map", "resolve_enter_challenge_functions");
    const auto* pattern = PY4GW::Patterns::Get("map.enter_challenge_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.enter_challenge_anchor", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("EnterChallenge_Anchor", address, "map")) {
        return false;
    }

    g_cancel_enter_challenge_mission_func = reinterpret_cast<VoidFn>(
        PY4GW::Scanner::FunctionFromNearCall(address + 0x1B));
    g_enter_challenge_mission_func = reinterpret_cast<DoActionFn>(
        PY4GW::Scanner::FunctionFromNearCall(address + 0x43));
    return Logger::AssertAddress(
        "CancelEnterChallengeMission_Func",
        reinterpret_cast<uintptr_t>(g_cancel_enter_challenge_mission_func),
        "map") &&
        Logger::AssertAddress(
        "EnterChallengeMission_Func",
        reinterpret_cast<uintptr_t>(g_enter_challenge_mission_func),
        "map");
}

inline bool ResolveMapTypeInstanceInfos() {
    CrashContextScope context("startup", "map", "resolve_map_type_instance_infos");
    const auto* pattern = PY4GW::Patterns::Get("map.map_type_instance_infos_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: map.map_type_instance_infos_ref", "map");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("MapTypeInstanceInfos_Anchor", address, "map")) {
        return false;
    }

    g_map_type_instance_infos_size = (*reinterpret_cast<const uint32_t*>(address + 5)) / sizeof(MapTypeInstanceInfo);
    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address + 0x1A);
    if (!Logger::AssertAddress("MapTypeInstanceInfos_Ptr", candidate, "map")) {
        return false;
    }

    g_map_type_instance_infos = reinterpret_cast<MapTypeInstanceInfo*>(candidate);
    return true;
}

}  // namespace GW::map

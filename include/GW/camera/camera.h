#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/context/camera.h"
#include "base/memory_patcher.h"

#include <atomic>
#include <cstdint>

namespace GW::camera {

bool Initialize();
void Shutdown();

Context::Camera* GetCamera();

bool ForwardMovement(float amount, bool true_forward);
bool VerticalMovement(float amount);
bool RotateMovement(float angle);
bool SideMovement(float amount);

bool SetMaxDist(float dist = 900.0f);
bool SetFieldOfView(float fov);
Vec3f ComputeCamPos(float dist = 0.0f);
bool UpdateCameraPos();

float GetFieldOfView();
float GetYaw();

bool UnlockCam(bool flag);
bool GetCameraUnlock();
bool SetFog(bool flag);

extern Context::Camera* g_camera;
extern PY4GW::MemoryPatcher g_patch_cam_update;
extern PY4GW::MemoryPatcher g_patch_fog;
extern std::atomic<bool> g_initialized;

inline bool ResolveCameraPointer() {
    CrashContextScope context("startup", "camera", "resolve_camera_pointer");
    const auto* anchor_pattern = PY4GW::Patterns::Get("camera.camera_ptr_anchor");
    const auto* ptr_scan_pattern = PY4GW::Patterns::Get("camera.camera_ptr_scan");
    if (!anchor_pattern || !ptr_scan_pattern) {
        Logger::Instance().LogError("Missing or invalid camera pointer pattern.", "camera");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        anchor_pattern->assertion_file.c_str(),
        anchor_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(anchor_pattern->line_number),
        anchor_pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to resolve camera assertion anchor.", "camera");
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        ptr_scan_pattern->pattern.c_str(),
        ptr_scan_pattern->mask.c_str(),
        ptr_scan_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("camera_ptr_scan", address, "camera")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("camera_ptr", candidate, "camera")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate)) {
        Logger::Instance().LogError("Camera pointer is outside the expected data section.", "camera");
        return false;
    }

    g_camera = reinterpret_cast<Context::Camera*>(candidate);
    return true;
}

inline bool ResolveFogPatch() {
    CrashContextScope context("startup", "camera", "resolve_fog_patch");
    const auto* pattern = PY4GW::Patterns::Get("camera.fog_patch");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: camera.fog_patch", "camera");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("camera_fog_patch", address, "camera")) {
        return false;
    }

    static constexpr uint8_t kFogPatch[] = {0x00};
    g_patch_fog.SetPatch(address, kFogPatch, sizeof(kFogPatch));
    return Logger::AssertAddress("camera_fog_patch_target", g_patch_fog.GetAddress(), "camera");
}

inline bool ResolveCameraUpdatePatch() {
    CrashContextScope context("startup", "camera", "resolve_camera_update_patch");
    const auto* vs2017_pattern = PY4GW::Patterns::Get("camera.camera_update_patch_vs2017");
    const auto* vs2022_pattern = PY4GW::Patterns::Get("camera.camera_update_patch_vs2022");
    if (!vs2017_pattern || !vs2022_pattern) {
        Logger::Instance().LogError("Missing or invalid camera update patch pattern.", "camera");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::Find(
        vs2017_pattern->pattern.c_str(),
        vs2017_pattern->mask.c_str(),
        vs2017_pattern->offset,
        vs2017_pattern->section);
    if (address) {
        static constexpr uint8_t kPatchVs2017[] = {0xEB, 0x0C};
        g_patch_cam_update.SetPatch(address, kPatchVs2017, sizeof(kPatchVs2017));
        return Logger::AssertAddress("camera_update_patch", g_patch_cam_update.GetAddress(), "camera");
    }

    address = PY4GW::Scanner::Find(
        vs2022_pattern->pattern.c_str(),
        vs2022_pattern->mask.c_str(),
        vs2022_pattern->offset,
        vs2022_pattern->section);
    if (!Logger::AssertAddress("camera_update_patch", address, "camera")) {
        return false;
    }

    static constexpr uint8_t kPatchVs2022[] = {0xEB, 0x0F};
    g_patch_cam_update.SetPatch(address, kPatchVs2022, sizeof(kPatchVs2022));
    return Logger::AssertAddress("camera_update_patch_target", g_patch_cam_update.GetAddress(), "camera");
}

void Exit();

}  // namespace GW::camera

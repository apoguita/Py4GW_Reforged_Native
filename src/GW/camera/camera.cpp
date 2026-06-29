#include "base/error_handling.h"

#include "GW/camera/camera.h"

#include "base/CrashHandler.h"

namespace GW::camera {

Context::Camera* g_camera = nullptr;
PY4GW::MemoryPatcher g_patch_cam_update = {};
PY4GW::MemoryPatcher g_patch_fog = {};
std::atomic<bool> g_initialized = false;

void Exit() {
    CrashContextScope context("shutdown", "camera", "exit");
    g_patch_cam_update.Reset();
    g_patch_fog.Reset();
    g_camera = nullptr;
}

bool Initialize() {
    CrashContextScope context("startup", "camera", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    if (!ResolveCameraPointer() || !ResolveFogPatch() || !ResolveCameraUpdatePatch()) {
        Exit();
        return false;
    }

    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "camera", "shutdown");
    if (!g_initialized) {
        return;
    }

    Exit();
    g_initialized = false;
}

}  // namespace GW::camera

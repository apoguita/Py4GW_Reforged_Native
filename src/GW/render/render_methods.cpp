#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "GW/camera/camera.h"
#include "GW/render/render.h"

#include <cmath>

namespace GW::render {

HWND GetWindowHandle() {
    return g_window_handle_ptr ? *reinterpret_cast<HWND*>(g_window_handle_ptr) : nullptr;
}

IDirect3DDevice9* GetDevice() {
    return g_dx_context ? g_dx_context->device : nullptr;
}

bool GetIsInRenderLoop() {
    if (!g_render_lock_initialized) {
        return false;
    }
    ::EnterCriticalSection(&g_render_lock);
    const bool ret = g_in_render_loop;
    ::LeaveCriticalSection(&g_render_lock);
    return ret;
}

int GetIsFullscreen() {
    if (!g_dx_context) {
        return -1;
    }
    return g_dx_context->viewport_height == g_dx_context->window_height &&
            g_dx_context->viewport_width == g_dx_context->window_width
        ? 1
        : 0;
}

uint32_t GetViewportWidth() {
    return g_dx_context ? g_dx_context->viewport_width : 0;
}

uint32_t GetViewportHeight() {
    return g_dx_context ? g_dx_context->viewport_height : 0;
}

Mat4x3f* GetTransform(Transform transform) {
    return g_get_transform_func ? g_get_transform_func(static_cast<int>(transform)) : nullptr;
}

float GetFieldOfView() {
    const Context::Camera* camera = camera::GetCamera();
    if (!camera) {
        return 0.0f;
    }

    constexpr float kDividend = 2.0f / 3.0f + 1.0f;
    return std::atan2(1.0f, kDividend / std::tan(camera->GetFieldOfView() * 0.5f)) * 2.0f;
}

RenderCallback GetRenderCallback() {
    return g_render_callback;
}

void SetRenderCallback(RenderCallback callback) {
    CrashContextScope context("runtime", "render", "set_render_callback", callback ? "assign" : "clear");
    if (g_shutting_down && callback) {
        return;
    }
    g_render_callback = callback;
}

void SetResetCallback(RenderCallback callback) {
    CrashContextScope context("runtime", "render", "set_reset_callback", callback ? "assign" : "clear");
    if (g_shutting_down && callback) {
        return;
    }
    g_reset_callback = callback;
}

}  // namespace GW::render

#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/game_pos.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

struct IDirect3DDevice9;

namespace GW::render {

bool Initialize();
void Shutdown();

using RenderCallback = void(__cdecl*)(IDirect3DDevice9*);

enum class Transform : int {
    ProjectionMatrix = 0,
    ModelMatrix = 1,
    Count = 5,
};

HWND GetWindowHandle();
IDirect3DDevice9* GetDevice();
bool GetIsInRenderLoop();
int GetIsFullscreen();
uint32_t GetViewportWidth();
uint32_t GetViewportHeight();
Mat4x3f* GetTransform(Transform transform);
float GetFieldOfView();

RenderCallback GetRenderCallback();
void SetRenderCallback(RenderCallback callback);
void SetResetCallback(RenderCallback callback);

struct GwDxContext {
    uint8_t h0000_1[0x128];
    uint8_t h0000[24];
    uint32_t h0018;
    uint8_t h001C[44];
    wchar_t gpu_name[32];
    uint8_t h0088[8];
    IDirect3DDevice9* device;
    uint8_t h0094[12];
    uint32_t framecount;
    uint8_t h00A4[2936];
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint8_t h0C24[148];
    uint32_t window_width;
    uint32_t window_height;
    uint8_t h0CC0[952];
};

using EndSceneFn = bool(__cdecl*)(GwDxContext* ctx, void* unk);
using ResetFn = bool(__cdecl*)(GwDxContext* ctx);
using GetTransformFn = Mat4x3f*(__cdecl*)(int transform);

extern GwDxContext* g_dx_context;
extern uintptr_t g_window_handle_ptr;
extern EndSceneFn g_end_scene_func;
extern EndSceneFn g_end_scene_original;
extern EndSceneFn g_screen_capture_func;
extern EndSceneFn g_screen_capture_original;
extern ResetFn g_reset_func;
extern ResetFn g_reset_original;
extern GetTransformFn g_get_transform_func;

extern CRITICAL_SECTION g_render_lock;
extern std::atomic<int> g_active_render_hooks;
extern std::atomic<bool> g_in_render_loop;
extern bool g_render_lock_initialized;
extern bool g_hooks_enabled;
extern std::atomic<bool> g_initialized;
extern std::atomic<bool> g_shutting_down;

extern RenderCallback g_render_callback;
extern RenderCallback g_reset_callback;

inline bool ResolveWindowHandlePointer() {
    CrashContextScope context("startup", "render", "resolve_window_handle_ptr");
    const auto* pattern = PY4GW::Patterns::Get("render.window_handle_ptr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.window_handle_ptr", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("window_handle_ptr_scan", scan, "render")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(scan);
    if (!Logger::AssertAddress("window_handle_ptr", candidate, "render")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate, PY4GW::ScannerSection::Data)) {
        Logger::Instance().LogError("window_handle_ptr is outside the expected data section.", "render");
        return false;
    }

    g_window_handle_ptr = candidate;
    return true;
}

inline bool ResolveResetHook() {
    CrashContextScope context("startup", "render", "resolve_reset_hook");
    const auto* pattern = PY4GW::Patterns::Get("render.reset_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.reset_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        0,
        pattern->section);
    if (!Logger::AssertAddress("GwReset_Scan", scan, "render")) {
        return false;
    }

    g_reset_func = reinterpret_cast<ResetFn>(
        PY4GW::Scanner::ToFunctionStart(scan, static_cast<uint32_t>(pattern->offset)));
    return Logger::AssertAddress("GwReset_Func", reinterpret_cast<uintptr_t>(g_reset_func), "render");
}

inline bool ResolveEndSceneHook() {
    CrashContextScope context("startup", "render", "resolve_end_scene_hook");
    const auto* pattern = PY4GW::Patterns::Get("render.end_scene_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.end_scene_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("GwEndScene_Scan", scan, "render")) {
        return false;
    }

    g_end_scene_func =
        reinterpret_cast<EndSceneFn>(PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress("GwEndScene_Func", reinterpret_cast<uintptr_t>(g_end_scene_func), "render");
}

inline bool ResolveGetTransformFunction() {
    CrashContextScope context("startup", "render", "resolve_get_transform");
    const auto* pattern = PY4GW::Patterns::Get("render.get_transform_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.get_transform_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("GwGetTransform_scan", scan, "render")) {
        return false;
    }

    g_get_transform_func =
        reinterpret_cast<GetTransformFn>(PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress(
        "GwGetTransform_func",
        reinterpret_cast<uintptr_t>(g_get_transform_func),
        "render");
}

inline bool ResolveScreenCapture() {
    CrashContextScope context("startup", "render", "resolve_screen_capture");
    const auto* pattern = PY4GW::Patterns::Get("render.screen_capture_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.screen_capture_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("ScreenCapture_Scan", scan, "render")) {
        return false;
    }

    g_screen_capture_func = reinterpret_cast<EndSceneFn>(
        PY4GW::Scanner::ToFunctionStart(scan, 0xFFF));
    return Logger::AssertAddress(
        "ScreenCapture_Func",
        reinterpret_cast<uintptr_t>(g_screen_capture_func),
        "render");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();
bool WaitForRenderHooksToDrain();

bool __cdecl OnEndScene(GwDxContext* ctx, void* unk);
bool __cdecl OnReset(GwDxContext* ctx);
void __cdecl OnScreenCapture(GwDxContext* ctx, void* unk);

}  // namespace GW::render

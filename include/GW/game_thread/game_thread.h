#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <atomic>
#include <functional>
#include <vector>
#include <windows.h>

namespace GW::game_thread {

using GameThreadCallback = PY4GW::HookCallback<>;

using LeaveGameThreadFn = void(__cdecl*)(void*);

struct CallbackEntry {
    int altitude = 0;
    PY4GW::HookEntry* entry = nullptr;
    GameThreadCallback callback;
};

bool Initialize();
void Shutdown();

void ClearCalls();
void Enqueue(std::function<void()> callback);
void RegisterGameThreadCallback(
    PY4GW::HookEntry* entry,
    const GameThreadCallback& callback,
    int altitude = 0x4000);
void RemoveGameThreadCallback(PY4GW::HookEntry* entry);
bool IsInGameThread();

extern CRITICAL_SECTION g_mutex;
extern bool g_mutex_initialized;
extern LeaveGameThreadFn g_leave_game_thread_func;
extern LeaveGameThreadFn g_leave_game_thread_original;
extern std::atomic<bool> g_initialized;
extern std::atomic<bool> g_in_game_thread;
extern std::vector<std::function<void()>> g_singleshot_callbacks;
extern std::vector<CallbackEntry> g_callbacks;

inline bool ResolveLeaveGameThreadTarget() {
    CrashContextScope context("startup", "game_thread", "resolve_leave_game_thread_target");
    const auto* pattern = PY4GW::Patterns::Get("game_thread.leave_game_thread_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: game_thread.leave_game_thread_target", "game_thread");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    g_leave_game_thread_func = reinterpret_cast<LeaveGameThreadFn>(address);
    return Logger::AssertAddress(
        "LeaveGameThread_Func",
        reinterpret_cast<uintptr_t>(g_leave_game_thread_func),
        "game_thread");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void CallFunctions();
void __cdecl OnLeaveGameThread(void* unk);

}  // namespace GW::game_thread

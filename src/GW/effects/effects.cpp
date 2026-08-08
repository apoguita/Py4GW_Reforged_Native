#include "base/error_handling.h"

#include "GW/effects/effects.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include "GW/player/player.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>

namespace GW::effects {

using PostProcessEffectFn = void(__cdecl*)(uint32_t intensity, uint32_t tint);
using DropBuffFn = void(__cdecl*)(uint32_t buff_id);

bool ResolvePostProcessEffect();
bool ResolveDropBuff();
bool Init();
void EnableHooks();
void DisableHooks();
void Exit();
void __cdecl OnPostProcessEffect(uint32_t intensity, uint32_t tint);

PostProcessEffectFn g_post_process_effect_func = nullptr;
PostProcessEffectFn g_post_process_effect_original = nullptr;
DropBuffFn g_drop_buff_func = nullptr;
std::atomic<uint32_t> g_alcohol_level = 0;
std::mutex g_alcohol_timer_mutex;
uint64_t g_alcohol_expires_at_ms = 0;
uint32_t g_prev_packet_tint_6_level = 0;
uint32_t g_prev_alcohol_title_points = 0;
bool g_alcohol_title_points_initialized = false;
std::atomic<bool> g_initialized = false;

namespace {

uint64_t SteadyNowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint32_t GetAlcoholTitlePoints() {
    const auto* title = GW::player::GetTitleTrack(GW::Constants::TitleID::Drunkard);
    return title ? title->current_points : 0;
}

}  // namespace

void __cdecl OnPostProcessEffect(uint32_t intensity, uint32_t tint) {
    PY4GW::HookBase::EnterHook();

    const uint64_t now_ms = SteadyNowMs();
    const uint32_t current_title_points = GetAlcoholTitlePoints();
    uint32_t title_points_gained = 0;
    uint32_t old_level = 0;
    uint64_t remaining_before_ms = 0;
    uint64_t remaining_after_ms = 0;
    const char* action = "reset";

    {
        std::lock_guard<std::mutex> lock(g_alcohol_timer_mutex);
        if (!g_alcohol_title_points_initialized) {
            g_prev_alcohol_title_points = current_title_points;
            g_alcohol_title_points_initialized = true;
        }
        else if (current_title_points > g_prev_alcohol_title_points) {
            title_points_gained = current_title_points - g_prev_alcohol_title_points;
        }
        g_prev_alcohol_title_points = current_title_points;

        old_level = g_alcohol_level.load();
        remaining_before_ms = g_alcohol_expires_at_ms > now_ms
            ? g_alcohol_expires_at_ms - now_ms
            : 0;

        bool ignore_event = false;
        if (tint == 8 && intensity == 5) {
            action = "ignored_pahnai_salad";
            ignore_event = true;
        }
        else if (tint == 6) {
            if (intensity == 5 &&
                (g_prev_packet_tint_6_level < intensity - 1 ||
                 (g_prev_packet_tint_6_level == 5 && title_points_gained < 1))) {
                action = "ignored_lunar_effect";
                ignore_event = true;
            }
            g_prev_packet_tint_6_level = intensity;
        }

        if (!ignore_event) {
            if (intensity > old_level) {
                remaining_after_ms = remaining_before_ms +
                    (static_cast<uint64_t>(intensity - old_level) * 60000ULL);
                action = old_level == 0 ? "started" : "level_increase";
            }
            else {
                remaining_after_ms = static_cast<uint64_t>(intensity) * 60000ULL;
                action = intensity == old_level && intensity > 0 ? "same_level_topoff" : "reset";
            }

            g_alcohol_expires_at_ms = now_ms + remaining_after_ms;
            g_alcohol_level = intensity;
        }
        else {
            remaining_after_ms = remaining_before_ms;
        }
    }

    std::ostringstream diagnostic;
    diagnostic << "event intensity=" << intensity
               << " tint=" << tint
               << " old_level=" << old_level
               << " title_points_gained=" << title_points_gained
               << " remaining_before_ms=" << remaining_before_ms
               << " remaining_after_ms=" << remaining_after_ms
               << " action=" << action;
    Logger::Instance().WriteFileLine("effects.alcohol", "DEBUG", diagnostic.str());

    if (g_post_process_effect_original) {
        g_post_process_effect_original(intensity, tint);
    }

    PY4GW::HookBase::LeaveHook();
}

bool Init() {
    CrashContextScope context("startup", "effects", "init");

    if (!ResolvePostProcessEffect() || 
        !ResolveDropBuff()) {
        return false;
    }

    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_post_process_effect_func),
        reinterpret_cast<void*>(&OnPostProcessEffect),
        reinterpret_cast<void**>(&g_post_process_effect_original));
    return Logger::AssertHook("PostProcessEffect_Func", status, "effects");
}

void EnableHooks() {
    CrashContextScope context("runtime", "effects", "enable_hooks");
    if (g_post_process_effect_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_post_process_effect_func));
    }
}

void DisableHooks() {
    CrashContextScope context("shutdown", "effects", "disable_hooks");
    if (g_post_process_effect_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_post_process_effect_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "effects", "exit");
    if (g_post_process_effect_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_post_process_effect_func));
    }

    g_post_process_effect_func = nullptr;
    g_post_process_effect_original = nullptr;
    g_drop_buff_func = nullptr;
    g_alcohol_level = 0;
    {
        std::lock_guard<std::mutex> lock(g_alcohol_timer_mutex);
        g_alcohol_expires_at_ms = 0;
        g_prev_packet_tint_6_level = 0;
        g_prev_alcohol_title_points = 0;
        g_alcohol_title_points_initialized = false;
    }
}

bool Initialize() {
    CrashContextScope context("startup", "effects", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    PY4GW::HookBase::Initialize();
    if (!Init()) {
        Exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    EnableHooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "effects", "shutdown");
    if (!g_initialized) {
        return;
    }

    DisableHooks();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::effects

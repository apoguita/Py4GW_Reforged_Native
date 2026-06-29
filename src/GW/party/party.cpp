#include "base/error_handling.h"

#include "GW/party/party.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <cstdio>

namespace GW::party {

ui::UIInteractionCallback g_tick_button_ui_callback = nullptr;
ui::UIInteractionCallback g_tick_button_ui_callback_original = nullptr;
ui::UIInteractionCallback g_party_player_member_ui_callback = nullptr;
PartySearchSeekFn g_party_search_seek_func = nullptr;
PartySearchButtonCallbackFn g_party_search_button_callback_func = nullptr;
PartySearchButtonCallbackFn g_party_window_button_callback_func = nullptr;
DoActionFn g_set_ready_status_func = nullptr;
DoActionFn g_set_difficulty_func = nullptr;
FlagHeroAgentFn g_flag_hero_agent_func = nullptr;
FlagAllFn g_flag_all_func = nullptr;
SetHeroBehaviorFn g_set_hero_behavior_func = nullptr;
LockPetTargetFn g_lock_pet_target_func = nullptr;
CommandHotKeyDisableAiFn g_command_hot_key_disable_ai_func = nullptr;
bool g_tick_work_as_toggle = false;
std::atomic<bool> g_initialized = false;

static void ScanLog(const char* name, uintptr_t address) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[SCAN] %s = %p", name, reinterpret_cast<void*>(address));
    Logger::Instance().LogInfo(buf);
}

void OnTickButtonUICallback(ui::InteractionMessage* message, void* wParam, void* lParam) {
    PY4GW::HookBase::EnterHook();
    bool blocked = false;
    if (!g_tick_work_as_toggle)
        goto finish;
    switch (static_cast<int>(message->message_id)) {
    case 0x22:  // Ready state icon clicked
        tick(!get_is_player_ticked());
        blocked = true;
        break;
    case 0x2c:  // Show ready state dropdown
        blocked = true;
        break;
    }
finish:
    if (!blocked) {
        g_tick_button_ui_callback_original(message, wParam, lParam);
    }
    PY4GW::HookBase::LeaveHook();
}

bool init() {
    CrashContextScope context("startup", "party", "init");

    const auto try_resolve = [](const char* name, bool(*resolver)()) {
        if (!resolver()) {
            Logger::Instance().LogWarning(std::string("Optional resolver failed: ") + name, "party");
        }
    };

    try_resolve("resolve_tick_button_ui_callback", &resolve_tick_button_ui_callback);
    try_resolve("resolve_set_difficulty_func", &resolve_set_difficulty_func);
    try_resolve("resolve_party_search_seek_func", &resolve_party_search_seek_func);
    try_resolve("resolve_party_search_button_callback_func", &resolve_party_search_button_callback_func);
    try_resolve("resolve_party_window_button_callback_func", &resolve_party_window_button_callback_func);
    try_resolve("resolve_party_player_member_ui_callback", &resolve_party_player_member_ui_callback);
    try_resolve("resolve_set_ready_status_func", &resolve_set_ready_status_func);
    try_resolve("resolve_flag_functions", &resolve_flag_functions);
    try_resolve("resolve_set_hero_behavior_funcs", &resolve_set_hero_behavior_funcs);
    try_resolve("resolve_command_hotkey_disable_ai_func", &resolve_command_hotkey_disable_ai_func);

    if (g_tick_button_ui_callback) {
        const int result = PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&g_tick_button_ui_callback),
            reinterpret_cast<void*>(&OnTickButtonUICallback),
            reinterpret_cast<void**>(&g_tick_button_ui_callback_original));
        Logger::AssertHook("TickButtonUICallback", result, "party");
    } else {
        Logger::Instance().LogWarning("TickButtonUICallback is unavailable; party tick hook will remain disabled.", "party");
    }

    ScanLog("TickButtonUICallback", reinterpret_cast<uintptr_t>(g_tick_button_ui_callback));
    ScanLog("SetDifficulty_Func", reinterpret_cast<uintptr_t>(g_set_difficulty_func));
    ScanLog("PartySearchSeek_Func", reinterpret_cast<uintptr_t>(g_party_search_seek_func));
    ScanLog("SetReadyStatus_Func", reinterpret_cast<uintptr_t>(g_set_ready_status_func));
    ScanLog("FlagHeroAgent_Func", reinterpret_cast<uintptr_t>(g_flag_hero_agent_func));
    ScanLog("FlagAll_Func", reinterpret_cast<uintptr_t>(g_flag_all_func));
    ScanLog("SetHeroBehavior_Func", reinterpret_cast<uintptr_t>(g_set_hero_behavior_func));
    ScanLog("LockPetTarget_Func", reinterpret_cast<uintptr_t>(g_lock_pet_target_func));
    ScanLog("CommandHotKeyDisableAi_Func", reinterpret_cast<uintptr_t>(g_command_hot_key_disable_ai_func));
    ScanLog("PartyWindowButtonCallback_Func", reinterpret_cast<uintptr_t>(g_party_window_button_callback_func));
    ScanLog("PartySearchButtonCallback_Func", reinterpret_cast<uintptr_t>(g_party_search_button_callback_func));

    return true;
}

void enable_hooks() {
    CrashContextScope context("runtime", "party", "enable_hooks");
    if (g_tick_button_ui_callback) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_tick_button_ui_callback));
    }
}

void disable_hooks() {
    CrashContextScope context("shutdown", "party", "disable_hooks");
    if (g_tick_button_ui_callback) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_tick_button_ui_callback));
    }
}

void exit() {
    CrashContextScope context("shutdown", "party", "exit");
    if (g_tick_button_ui_callback) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_tick_button_ui_callback));
    }

    g_tick_button_ui_callback = nullptr;
    g_tick_button_ui_callback_original = nullptr;
    g_party_player_member_ui_callback = nullptr;
    g_party_search_seek_func = nullptr;
    g_party_search_button_callback_func = nullptr;
    g_party_window_button_callback_func = nullptr;
    g_set_ready_status_func = nullptr;
    g_set_difficulty_func = nullptr;
    g_flag_hero_agent_func = nullptr;
    g_flag_all_func = nullptr;
    g_set_hero_behavior_func = nullptr;
    g_lock_pet_target_func = nullptr;
    g_command_hot_key_disable_ai_func = nullptr;
    g_tick_work_as_toggle = false;
}

bool Initialize() {
    CrashContextScope context("startup", "party", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    PY4GW::HookBase::Initialize();
    if (!init()) {
        exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    enable_hooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "party", "shutdown");
    if (!g_initialized) {
        return;
    }

    disable_hooks();
    exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::party

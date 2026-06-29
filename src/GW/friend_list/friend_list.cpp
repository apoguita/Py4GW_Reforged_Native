#include "base/error_handling.h"

#include "GW/friend_list/friend_list.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <cstdlib>
#include <cstring>

namespace GW::friend_list {

#pragma warning(push)
#pragma warning(disable : 4200)
struct EventData {
    uint32_t event_id;
    uint32_t unk;
    uint32_t data_size;
    uint32_t data[];
};
#pragma warning(pop)

FriendEventHandlerFn g_friend_event_handler_func = nullptr;
FriendEventHandlerFn g_friend_event_handler_original = nullptr;
SetOnlineStatusFn g_set_online_status_func = nullptr;
AddFriendFn g_add_friend_func = nullptr;
RemoveFriendFn g_remove_friend_func = nullptr;
uintptr_t g_friend_list_addr = 0;
std::unordered_map<PY4GW::HookEntry*, FriendStatusCallback> g_friend_status_callbacks;
std::atomic<bool> g_initialized = false;

void __cdecl OnFriendEventHandler(void* unk, void* evt_info) {
    PY4GW::HookBase::EnterHook();
    auto* event_info = static_cast<EventData*>(evt_info);
    uint8_t* uuid = nullptr;
    const wchar_t* alias = nullptr;
    switch (event_info->event_id) {
    case 0x26:
        uuid = reinterpret_cast<uint8_t*>(&event_info->data[2]);
        alias = reinterpret_cast<wchar_t*>(&event_info->data[6]);
        break;
    case 0x28:
        alias = reinterpret_cast<wchar_t*>(&event_info->data[0]);
        break;
    case 0x2C:
        alias = reinterpret_cast<wchar_t*>(&event_info->data[4]);
        break;
    default:
        break;
    }

    if (!uuid && !alias) {
        if (g_friend_event_handler_original) {
            g_friend_event_handler_original(unk, event_info);
        }
        PY4GW::HookBase::LeaveHook();
        return;
    }

    bool uuid_valid = false;
    for (size_t i = 0; uuid && i < 16 && !uuid_valid; ++i) {
        uuid_valid = uuid[i] != 0;
    }
    if (!uuid_valid) {
        uuid = nullptr;
    }
    if (!(alias && alias[0])) {
        alias = nullptr;
    }

    Context::Friend* current_state = nullptr;
    if (uuid) {
        current_state = GetFriend(uuid);
    } else if (alias) {
        current_state = GetFriend(alias, nullptr, Context::FriendType::Unknow);
    }

    PY4GW::HookStatus hook_status = {};
    Context::Friend* old_state = nullptr;
    if (current_state) {
        old_state = static_cast<Context::Friend*>(std::malloc(sizeof(*old_state)));
        PY4GW_ASSERT(old_state && std::memcpy(old_state, current_state, sizeof(*old_state)));
    }

    if (g_friend_event_handler_original) {
        g_friend_event_handler_original(unk, event_info);
    }

    if (uuid) {
        current_state = GetFriend(uuid);
    } else if (alias) {
        current_state = GetFriend(alias, nullptr, Context::FriendType::Unknow);
    }

    for (auto& [entry, callback] : g_friend_status_callbacks) {
        static_cast<void>(entry);
        callback(&hook_status, old_state, current_state);
        ++hook_status.altitude;
    }

    if (old_state) {
        std::free(old_state);
    }

    PY4GW::HookBase::LeaveHook();
}

bool Init() {
    CrashContextScope context("startup", "friend_list", "init");
    if (!ResolveFriendListPointer() ||
        !ResolveFriendEventHandler() ||
        !ResolveSetOnlineStatus() ||
        !ResolveAddFriend() ||
        !ResolveRemoveFriend()) {
        return false;
    }

    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_friend_event_handler_func),
        reinterpret_cast<void*>(&OnFriendEventHandler),
        reinterpret_cast<void**>(&g_friend_event_handler_original));
    return Logger::AssertHook("FriendEventHandler_Func", status, "friend_list");
}

void EnableHooks() {
    CrashContextScope context("runtime", "friend_list", "enable_hooks");
    if (g_friend_event_handler_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_friend_event_handler_func));
    }
}

void DisableHooks() {
    CrashContextScope context("shutdown", "friend_list", "disable_hooks");
    if (g_friend_event_handler_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_friend_event_handler_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "friend_list", "exit");
    if (g_friend_event_handler_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_friend_event_handler_func));
    }

    g_friend_event_handler_func = nullptr;
    g_friend_event_handler_original = nullptr;
    g_set_online_status_func = nullptr;
    g_add_friend_func = nullptr;
    g_remove_friend_func = nullptr;
    g_friend_list_addr = 0;
    g_friend_status_callbacks.clear();
}

bool Initialize() {
    CrashContextScope context("startup", "friend_list", "initialize");
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
    CrashContextScope context("shutdown", "friend_list", "shutdown");
    if (!g_initialized) {
        return;
    }

    DisableHooks();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::friend_list

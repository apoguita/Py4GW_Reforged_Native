#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/context/friend_list.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace GW::friend_list {

bool Initialize();
void Shutdown();

using FriendStatusCallback = PY4GW::HookCallback<const Context::Friend*, const Context::Friend*>;
using FriendEventHandlerFn = void(__cdecl*)(void*, void*);
using SetOnlineStatusFn = void(__cdecl*)(Context::FriendStatus status);
using AddFriendFn = void(__cdecl*)(const wchar_t* name, const wchar_t* alias, Context::FriendType type);
using RemoveFriendFn = void(__cdecl*)(const uint8_t* uuid, const wchar_t* name, uint32_t arg8);

Context::FriendList* GetFriendList();

Context::Friend* GetFriend(const wchar_t* alias, const wchar_t* charname, Context::FriendType type = Context::FriendType::Friend);
Context::Friend* GetFriend(uint32_t index);
Context::Friend* GetFriend(const uint8_t* uuid);

uint32_t GetNumberOfFriends(Context::FriendType type = Context::FriendType::Friend);
uint32_t GetNumberOfIgnores();
uint32_t GetNumberOfPartners();
uint32_t GetNumberOfTraders();

Context::FriendStatus GetMyStatus();
bool SetFriendListStatus(Context::FriendStatus status);

void RegisterFriendStatusCallback(
    PY4GW::HookEntry* entry,
    const FriendStatusCallback& callback);
void RemoveFriendStatusCallback(PY4GW::HookEntry* entry);

bool AddFriend(const wchar_t* name, const wchar_t* alias = nullptr);
bool AddIgnore(const wchar_t* name, const wchar_t* alias = nullptr);
bool RemoveFriend(Context::Friend* friend_entry);

extern FriendEventHandlerFn g_friend_event_handler_func;
extern FriendEventHandlerFn g_friend_event_handler_original;
extern SetOnlineStatusFn g_set_online_status_func;
extern AddFriendFn g_add_friend_func;
extern RemoveFriendFn g_remove_friend_func;
extern uintptr_t g_friend_list_addr;
extern std::unordered_map<PY4GW::HookEntry*, FriendStatusCallback> g_friend_status_callbacks;
extern std::atomic<bool> g_initialized;

inline bool ResolveFriendListPointer() {
    CrashContextScope context("startup", "friend_list", "resolve_friend_list_pointer");
    const auto* anchor_pattern = PY4GW::Patterns::Get("friend_list.friend_list_anchor");
    const auto* list_pattern = PY4GW::Patterns::Get("friend_list.friend_list_scan");
    if (!anchor_pattern || !list_pattern) {
        Logger::Instance().LogError("Missing or invalid friend list pointer pattern.", "friend_list");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        anchor_pattern->assertion_file.c_str(),
        anchor_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(anchor_pattern->line_number),
        anchor_pattern->offset);
    if (!Logger::AssertAddress("FriendList_Anchor", address, "friend_list")) {
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        list_pattern->pattern.c_str(),
        list_pattern->mask.c_str(),
        list_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("FriendList_PointerAddress", address, "friend_list")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogError("Friend list pointer is outside the expected data section.", "friend_list");
        return false;
    }

    g_friend_list_addr = *reinterpret_cast<uintptr_t*>(address);
    return Logger::AssertAddress("FriendList_Addr", g_friend_list_addr, "friend_list");
}

inline bool ResolveFriendEventHandler() {
    CrashContextScope context("startup", "friend_list", "resolve_friend_event_handler");
    const auto* pattern = PY4GW::Patterns::Get("friend_list.friend_event_handler");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: friend_list.friend_event_handler", "friend_list");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("FriendEventHandler_Scan", scan, "friend_list")) {
        return false;
    }

    g_friend_event_handler_func = reinterpret_cast<FriendEventHandlerFn>(
        PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress(
        "FriendEventHandler_Func",
        reinterpret_cast<uintptr_t>(g_friend_event_handler_func),
        "friend_list");
}

inline bool ResolveSetOnlineStatus() {
    CrashContextScope context("startup", "friend_list", "resolve_set_online_status");
    const auto* pattern = PY4GW::Patterns::Get("friend_list.set_online_status");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: friend_list.set_online_status", "friend_list");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("SetOnlineStatus_Scan", scan, "friend_list")) {
        return false;
    }

    g_set_online_status_func = reinterpret_cast<SetOnlineStatusFn>(
        PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress(
        "SetOnlineStatus_Func",
        reinterpret_cast<uintptr_t>(g_set_online_status_func),
        "friend_list");
}

inline bool ResolveAddFriend() {
    CrashContextScope context("startup", "friend_list", "resolve_add_friend");
    const auto* pattern = PY4GW::Patterns::Get("friend_list.add_friend");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: friend_list.add_friend", "friend_list");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("AddFriend_Scan", scan, "friend_list")) {
        return false;
    }

    g_add_friend_func = reinterpret_cast<AddFriendFn>(
        PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress(
        "AddFriend_Func",
        reinterpret_cast<uintptr_t>(g_add_friend_func),
        "friend_list");
}

inline bool ResolveRemoveFriend() {
    CrashContextScope context("startup", "friend_list", "resolve_remove_friend");
    const auto* anchor_pattern = PY4GW::Patterns::Get("friend_list.remove_friend_anchor");
    const auto* call_pattern = PY4GW::Patterns::Get("friend_list.remove_friend_call");
    if (!anchor_pattern || !call_pattern) {
        Logger::Instance().LogError("Missing or invalid remove friend pattern.", "friend_list");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::Find(
        anchor_pattern->pattern.c_str(),
        anchor_pattern->mask.c_str(),
        anchor_pattern->offset,
        anchor_pattern->section);
    if (!Logger::AssertAddress("RemoveFriend_Anchor", address, "friend_list")) {
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        call_pattern->pattern.c_str(),
        call_pattern->mask.c_str(),
        call_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("RemoveFriend_Callsite", address, "friend_list")) {
        return false;
    }

    g_remove_friend_func = reinterpret_cast<RemoveFriendFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress(
        "RemoveFriend_Func",
        reinterpret_cast<uintptr_t>(g_remove_friend_func),
        "friend_list");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __cdecl OnFriendEventHandler(void* unk, void* event_info);

}  // namespace GW::friend_list

#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace GW::events {

bool Initialize();
void Shutdown();

enum class EventID {
    kRecvPing = 0x8,
    kSendFriendState = 0x26,
    kRecvFriendState = 0x2c
};

using EventCallback = PY4GW::HookCallback<EventID, void*, uint32_t>;
using SendEventMessageFn = uint32_t(__cdecl*)(void* event_context, uint32_t unk1, EventID event_id, void* data_buffer, uint32_t data_length);

struct CallbackEntry {
    int altitude = 0;
    PY4GW::HookEntry* entry = nullptr;
    EventCallback callback;
};

void RegisterEventCallback(
    PY4GW::HookEntry* entry,
    EventID event_id,
    const EventCallback& callback,
    int altitude = -0x8000);
void RemoveEventCallback(PY4GW::HookEntry* entry);

extern SendEventMessageFn g_send_event_message_func;
extern SendEventMessageFn g_send_event_message_original;
extern std::unordered_map<EventID, std::vector<CallbackEntry>> g_callbacks;
extern std::atomic<bool> g_initialized;

inline bool ResolveSendEventMessageTarget() {
    CrashContextScope context("startup", "events", "resolve_send_event_message_target");
    const auto* pattern = PY4GW::Patterns::Get("events.send_event_message_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: events.send_event_message_callsite", "events");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("SendEventMessage_Callsite", callsite, "events")) {
        return false;
    }

    g_send_event_message_func = reinterpret_cast<SendEventMessageFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress(
        "SendEventMessage_Func",
        reinterpret_cast<uintptr_t>(g_send_event_message_func),
        "events");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

uint32_t __cdecl OnSendEventMessage(
    void* event_context,
    uint32_t unk1,
    EventID event_id,
    void* data_buffer,
    uint32_t data_length);

}  // namespace GW::events

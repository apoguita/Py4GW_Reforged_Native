#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/gw_array.h"
#include "GW/common/stoc.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <windows.h>

namespace GW::StoC {

bool Initialize();
void Shutdown();

constexpr uint32_t kStoCHeaderCount = 0x1e7;

using PacketCallback = PY4GW::HookCallback<Packet::StoC::PacketBase*>;
using StoCHandlerFn = bool(__cdecl*)(Packet::StoC::PacketBase* packet);

struct StoCHandler {
    uint32_t* packet_template = nullptr;
    uint32_t template_size = 0;
    StoCHandlerFn handler_func = nullptr;
};

using StoCHandlerArray = GW::GWArray<StoCHandler>;

struct GameServer {
    uint8_t h0000[8];
    struct {
        uint8_t h0000[12];
        struct {
            uint8_t h0000[12];
            void* next;
            uint8_t h0010[12];
            uint32_t client_codec_array[4];
            StoCHandlerArray handlers;
        }* ls_codec;
        uint8_t h0010[12];
        uint32_t client_codec_array[4];
        StoCHandlerArray handlers;
    }* gs_codec;
};

struct CallbackEntry {
    int altitude = 0;
    PY4GW::HookEntry* entry = nullptr;
    PacketCallback callback;
};

bool RegisterPacketCallback(
    PY4GW::HookEntry* entry,
    uint32_t header,
    const PacketCallback& callback,
    int altitude = -0x8000);
bool RegisterPostPacketCallback(
    PY4GW::HookEntry* entry,
    uint32_t header,
    const PacketCallback& callback);

template <typename T>
bool RegisterPacketCallback(PY4GW::HookEntry* entry, const PY4GW::HookCallback<T*>& handler, int altitude = -0x8000) {
    const uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
    return RegisterPacketCallback(
        entry,
        header,
        [handler](PY4GW::HookStatus* status, Packet::StoC::PacketBase* packet_value) -> void {
            handler(status, static_cast<T*>(packet_value));
        },
        altitude);
}

template <typename T>
bool RegisterPostPacketCallback(PY4GW::HookEntry* entry, const PY4GW::HookCallback<T*>& handler) {
    const uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
    return RegisterPostPacketCallback(
        entry,
        header,
        [handler](PY4GW::HookStatus* status, Packet::StoC::PacketBase* packet_value) -> void {
            handler(status, static_cast<T*>(packet_value));
        });
}

size_t RemoveCallback(uint32_t header, PY4GW::HookEntry* entry);
size_t RemoveCallbacks(PY4GW::HookEntry* entry);

template <typename T>
void RemoveCallback(PY4GW::HookEntry* entry) {
    RemoveCallback(Packet::StoC::Packet<T>::STATIC_HEADER, entry);
}

void RemovePostCallback(uint32_t header, PY4GW::HookEntry* entry);

template <typename T>
void RemovePostCallback(PY4GW::HookEntry* entry) {
    RemovePostCallback(Packet::StoC::Packet<T>::STATIC_HEADER, entry);
}

bool EmulatePacket(Packet::StoC::PacketBase* packet);

template <typename T>
bool EmulatePacket(Packet::StoC::Packet<T>* packet_value) {
    packet_value->header = Packet::StoC::Packet<T>::STATIC_HEADER;
    return EmulatePacket(static_cast<Packet::StoC::PacketBase*>(packet_value));
}

void Init();
void EnableHooks();
void DisableHooks();
void Exit();

bool __cdecl StoCHandler_Func(Packet::StoC::PacketBase* packet);

extern CRITICAL_SECTION g_mutex;
extern bool g_mutex_initialized;
extern bool g_hooks_enabled;
extern std::atomic<bool> g_initialized;
extern size_t g_stoc_handler_count;
extern StoCHandlerArray* g_game_server_handlers;
extern StoCHandler* g_original_functions;
extern std::vector<std::vector<CallbackEntry>> g_packet_entries;

inline bool ResolveGameServerHandlers() {
    CrashContextScope context("startup", "stoc", "resolve_game_server_handlers");
    const auto* pattern = PY4GW::Patterns::Get("stoc.handler_table_pointer");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: stoc.handler_table_pointer", "stoc");
        return false;
    }

    const uintptr_t pointer_location = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("StoCHandler_PointerLocation", pointer_location, "stoc")) {
        return false;
    }

    const uintptr_t handlers_addr = *reinterpret_cast<uintptr_t*>(pointer_location);
    if (!Logger::AssertAddress("StoCHandler_Addr", handlers_addr, "stoc")) {
        return false;
    }

    auto** game_server = reinterpret_cast<GameServer**>(handlers_addr);
    if (!(game_server && *game_server && (*game_server)->gs_codec)) {
        Logger::Instance().LogError("Game server handler table is not fully initialized.", "stoc");
        return false;
    }

    g_game_server_handlers = &(*game_server)->gs_codec->handlers;
    return g_game_server_handlers != nullptr;
}

}  // namespace GW::StoC

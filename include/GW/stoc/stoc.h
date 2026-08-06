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

// Returns the wire byte size of `packet` by walking the header's resolved
// field template from the live handler table. The template's field_count is
// a count of uint32_t field descriptors, not a byte size (a handler with N
// template fields is not an N-byte packet), so fixed- and variable-width
// fields (blob/string16/array8-16-32/nested-struct) are decoded individually
// and their wire widths summed. Falls back to sizeof(PacketBase) when the
// header is out of range, the table is not resolved, or the walk fails.
// Exposed for consumers that need accurate per-header packet sizes (e.g. the
// packet sniffer) without re-scanning the handler table.
uint32_t GetPacketSize(uint32_t header, const Packet::StoC::PacketBase* packet);

bool EmulatePacket(Packet::StoC::PacketBase* packet);

template <typename T>
bool EmulatePacket(Packet::StoC::Packet<T>* packet_value) {
    packet_value->header = Packet::StoC::Packet<T>::STATIC_HEADER;
    return EmulatePacket(static_cast<Packet::StoC::PacketBase*>(packet_value));
}

}  // namespace GW::StoC

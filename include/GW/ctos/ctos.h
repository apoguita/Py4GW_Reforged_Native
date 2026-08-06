#pragma once

#include "base/error_handling.h"
#include "base/patterns.h"

#include <cstdint>
#include <vector>

namespace GW::CToS {

// The game send routine mutates the packet buffer while it packs the message.
// Callers must therefore provide writable storage and invoke SendPacket on the
// Guild Wars game thread.
using SendPacketFn = void(__cdecl*)(uint32_t context, uint32_t size, void* packet);

constexpr uint32_t kMinimumPacketSize = sizeof(uint32_t);
constexpr uint32_t kMaximumPacketSize = 4096;

bool Initialize();
void Shutdown();

// Send one already-marshalled packet. This is a game-thread operation; Python
// callers should use QueuePacket so the buffer lifetime and thread affinity are
// owned by this module.
bool SendPacket(uint32_t size, void* buffer);

// Queue a packet represented as dwords. words[0] is the raw opcode; the game
// applies its wire-level header transform while sending.
bool QueuePacket(std::vector<uint32_t> words);

// These resolvers are module-owned so the sender's pattern data and runtime
// symbol ownership remain separate from packet capture.
inline bool ResolveSendTarget(uintptr_t* out_target) {
    if (!out_target) {
        return false;
    }
    *out_target = 0;
    return PY4GW::Patterns::Resolve("ctos.send_target", out_target) && *out_target != 0;
}

inline bool ResolveGameServerObject(uintptr_t* out_address) {
    if (!out_address) {
        return false;
    }
    *out_address = 0;
    return PY4GW::Patterns::Resolve("ctos.game_server_object", out_address) && *out_address != 0;
}

}  // namespace GW::CToS

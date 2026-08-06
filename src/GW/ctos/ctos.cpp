#include "base/error_handling.h"

#include "GW/ctos/ctos.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"

#include <limits>
#include <memory>

namespace GW::CToS {

namespace {

constexpr const char* kModule = "ctos";

SendPacketFn g_send_packet = nullptr;
uintptr_t g_game_server_object_addr = 0;
bool g_initialized = false;

void LogError(const char* message) {
    Logger::Instance().LogError(message, kModule);
}

bool IsSendable() {
    return GW::map::GetIsMapLoaded() &&
        !GW::map::GetIsObserving() &&
        GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;
}

bool ReadConnection(uintptr_t& out_connection) {
    out_connection = 0;
    if (!g_game_server_object_addr) {
        return false;
    }

    __try {
        out_connection = *reinterpret_cast<const uintptr_t*>(g_game_server_object_addr);
        return out_connection != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsConnectionReady(uintptr_t connection) {
    if (!connection) {
        return false;
    }

    __try {
        return *reinterpret_cast<const uint32_t*>(connection + 0x60) == 2 &&
            *reinterpret_cast<const uint32_t*>(connection + 0x38) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsValidSendHeader(uintptr_t connection, uint32_t header) {
    if (!connection) {
        return false;
    }

    __try {
        const uintptr_t channel = *reinterpret_cast<const uintptr_t*>(connection + 0x8);
        if (!channel) {
            return false;
        }

        const uint32_t message_count = *reinterpret_cast<const uint32_t*>(channel + 0x24);
        if (header >= message_count) {
            return false;
        }

        const uintptr_t format_table = *reinterpret_cast<const uintptr_t*>(channel + 0x1C);
        if (!format_table) {
            return false;
        }

        return *reinterpret_cast<const uintptr_t*>(format_table + (header * 8) + 4) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace

bool Initialize() {
    CrashContextScope context("startup", "ctos", "initialize");
    if (g_initialized) {
        return true;
    }

    uintptr_t send_target = 0;
    uintptr_t game_server_object = 0;
    if (!ResolveSendTarget(&send_target) ||
        !ResolveGameServerObject(&game_server_object)) {
        LogError("CToS initialization aborted because a sender symbol could not be resolved.");
        return false;
    }

    g_send_packet = reinterpret_cast<SendPacketFn>(send_target);
    g_game_server_object_addr = game_server_object;
    g_initialized = true;
    Logger::Instance().LogInfo("[ctos] CToS sender initialized.");
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "ctos", "shutdown");
    g_send_packet = nullptr;
    g_game_server_object_addr = 0;
    g_initialized = false;
}

bool SendPacket(uint32_t size, void* buffer) {
    if (!g_initialized || !g_send_packet || !buffer ||
        size < kMinimumPacketSize || size > kMaximumPacketSize ||
        !GW::game_thread::IsInGameThread() || !IsSendable()) {
        return false;
    }

    uintptr_t connection = 0;
    if (!ReadConnection(connection) || !IsConnectionReady(connection)) {
        return false;
    }

    uint32_t header = 0;
    __try {
        header = *reinterpret_cast<const uint32_t*>(buffer);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    if (!IsValidSendHeader(connection, header)) {
        return false;
    }

    g_send_packet(static_cast<uint32_t>(connection), size, buffer);
    return true;
}

bool QueuePacket(std::vector<uint32_t> words) {
    if (words.empty() || words.size() > (kMaximumPacketSize / sizeof(uint32_t))) {
        return false;
    }

    GW::game_thread::Enqueue([words = std::move(words)]() mutable {
        if (!IsSendable()) {
            return;
        }

        const uint32_t size = static_cast<uint32_t>(words.size() * sizeof(uint32_t));
        SendPacket(size, words.data());
    });
    return true;
}

}  // namespace GW::CToS

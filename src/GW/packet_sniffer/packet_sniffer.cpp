#include "base/error_handling.h"

#include "GW/packet_sniffer/packet_sniffer.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "GW/stoc/stoc.h"
#include "system/system.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include <windows.h>

namespace GW::packet_sniffer {

namespace {

constexpr const char* kModule = "packet_sniffer";

void LogError(const std::string& message) {
    Logger::Instance().LogError(message, kModule);
}

void LogInfo(const std::string& message) {
    Logger::Instance().LogInfo("[" + std::string(kModule) + "] " + message);
}

// Outbound send trampoline. Owned by the CToS detour below.
SendPacketFn g_ctos_trampoline = nullptr;

bool SafeReadCToSHeader(const void* src, uint32_t& out_header) {
    if (!src) {
        return false;
    }

    __try {
        const uint32_t raw_header = *reinterpret_cast<const uint32_t*>(src);
        out_header = static_cast<uint16_t>(raw_header & 0xFFFF);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out_header = 0;
        return false;
    }
}

uint32_t SafeCopyCToSPacket(const void* src, uint32_t packet_size, std::vector<uint8_t>& out_data) {
    if (!src || packet_size == 0 || packet_size > kMaxReasonableCToSPacketSize) {
        out_data.clear();
        return 0;
    }

    __try {
        out_data.resize(packet_size);
        memcpy(out_data.data(), src, packet_size);
        return packet_size;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out_data.clear();
        return 0;
    }
}

uint32_t SafeCopyStoCPacket(const void* src, uint32_t copy_size, std::vector<uint8_t>& out_data) {
    __try {
        out_data.resize(copy_size);
        memcpy(out_data.data(), src, copy_size);
        return copy_size;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out_data.resize(sizeof(Packet::StoC::PacketBase));
        memcpy(out_data.data(), src, sizeof(Packet::StoC::PacketBase));
        return sizeof(Packet::StoC::PacketBase);
    }
}

void __cdecl Detour_SendPacket(void* ctx, uint32_t size, const void* packet) {
    if (packet && size >= sizeof(uint32_t) && size <= kMaxReasonableCToSPacketSize) {
        PacketLogEntry entry{};
        entry.tick = PY4GW::System::GetTickCount64();
        entry.direction = PacketDirection::CToS;

        if (SafeReadCToSHeader(packet, entry.header)) {
            entry.size = SafeCopyCToSPacket(packet, size, entry.data);
            if (entry.size >= sizeof(uint32_t) && !entry.data.empty()) {
                LogPacket(std::move(entry));
            }
        }
    }

    if (g_ctos_trampoline) {
        g_ctos_trampoline(ctx, size, packet);
    }
}

}  // namespace

// StoC hook registration handles, one per header. Owned here; consumed by the
// StoC callback registration in InitializeStoC / removal in TerminateStoC.
std::vector<PY4GW::HookEntry> g_stoc_hook_entries;

// Cached CToS resolution so teardown does not re-scan during shutdown.
uintptr_t g_ctos_target = 0;

bool g_stoc_initialized = false;
bool g_ctos_initialized = false;

bool Initialize() {
    const bool stoc_ok = InitializeStoC();
    const bool ctos_ok = InitializeCToS();
    if (!stoc_ok || !ctos_ok) {
        std::ostringstream oss;
        oss << "Unified packet sniffer initialization failed. StoC=" << stoc_ok << " CToS=" << ctos_ok;
        LogError(oss.str());
    }
    return stoc_ok && ctos_ok;
}

bool InitializeStoC() {
    CrashContextScope context("startup", "packet_sniffer", "initialize_stoc");
    if (g_stoc_initialized) {
        return true;
    }

    g_stoc_hook_entries.resize(GW::StoC::kStoCHeaderCount);

    for (uint32_t header = 0; header < GW::StoC::kStoCHeaderCount; ++header) {
        GW::StoC::RegisterPacketCallback(
            &g_stoc_hook_entries[header],
            header,
            [](PY4GW::HookStatus*, Packet::StoC::PacketBase* pak) {
                PacketLogEntry entry{};
                entry.tick = PY4GW::System::GetTickCount64();
                entry.direction = PacketDirection::StoC;
                entry.header = pak->header;

                const uint32_t packet_size = GW::StoC::GetPacketSize(pak->header, pak);
                const uint32_t copy_size = std::min<uint32_t>(packet_size, static_cast<uint32_t>(kMaxStoCPacketBuffer));
                const uint32_t copied = SafeCopyStoCPacket(pak, copy_size, entry.data);
                entry.size = packet_size ? packet_size : copied;

                LogPacket(std::move(entry));
            },
            -0x8000);
    }

    g_stoc_initialized = true;
    LogInfo("StoC packet sniffer initialized successfully.");
    return true;
}

bool InitializeCToS() {
    CrashContextScope context("startup", "packet_sniffer", "initialize_ctos");
    if (g_ctos_initialized) {
        return true;
    }

    uintptr_t target = 0;
    if (!ResolveCToSSendTarget(&target) || !target) {
        LogError("CToS initialization aborted because no packet send target was resolved.");
        return false;
    }

    void* trampoline = nullptr;
    const int result = PY4GW::HookBase::CreateHookRaw(
        reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(&Detour_SendPacket),
        &trampoline);

    if (result != 0 || !trampoline) {
        std::ostringstream oss;
        oss << "Failed to create CToS hook. target=0x" << std::hex << target
            << " result=" << std::dec << result
            << " trampoline=" << trampoline;
        LogError(oss.str());
        return false;
    }

    g_ctos_trampoline = reinterpret_cast<SendPacketFn>(trampoline);
    g_ctos_target = target;
    PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(target));
    g_ctos_initialized = true;
    {
        std::ostringstream oss;
        oss << "CToS packet sniffer initialized successfully. target=0x" << std::hex << target
            << " trampoline=" << trampoline;
        LogInfo(oss.str());
    }
    return true;
}

void Terminate() {
    TerminateCToS();
    TerminateStoC();
}

void TerminateStoC() {
    CrashContextScope context("shutdown", "packet_sniffer", "terminate_stoc");
    if (!g_stoc_initialized) {
        return;
    }

    for (uint32_t header = 0; header < GW::StoC::kStoCHeaderCount && header < g_stoc_hook_entries.size(); ++header) {
        GW::StoC::RemoveCallback(header, &g_stoc_hook_entries[header]);
    }

    g_stoc_initialized = false;
}

void TerminateCToS() {
    CrashContextScope context("shutdown", "packet_sniffer", "terminate_ctos");
    if (!g_ctos_initialized) {
        return;
    }

    if (g_ctos_target) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_ctos_target));
    }

    g_ctos_trampoline = nullptr;
    g_ctos_target = 0;
    g_ctos_initialized = false;
}

}  // namespace GW::packet_sniffer

#include "base/error_handling.h"

#include "GW/stoc/stoc.h"

namespace GW::StoC {

using StoCHandlerFn = bool(__cdecl*)(Packet::StoC::PacketBase* packet);

struct StoCHandler {
    uint32_t* packet_template = nullptr;
    uint32_t field_count = 0;  // number of uint32_t descriptors in packet_template, NOT a byte size
    StoCHandlerFn handler_func = nullptr;
};

using StoCHandlerArray = GW::GWArray<StoCHandler>;

struct CallbackEntry {
    int altitude = 0;
    PY4GW::HookEntry* entry = nullptr;
    PacketCallback callback;
};

extern CRITICAL_SECTION g_mutex;
extern bool g_mutex_initialized;
extern StoCHandlerArray* g_game_server_handlers;
extern std::vector<std::vector<CallbackEntry>> g_packet_entries;

void SafeInitializeCriticalSection(CRITICAL_SECTION* mtx);
bool __cdecl StoCHandler_Func(Packet::StoC::PacketBase* packet);
bool OriginalHandler(Packet::StoC::PacketBase* packet);

bool RegisterPacketCallback(
    PY4GW::HookEntry* entry,
    uint32_t header,
    const PacketCallback& callback,
    int altitude) {
    bool success = false;
    SafeInitializeCriticalSection(&g_mutex);

    ::EnterCriticalSection(&g_mutex);
    RemoveCallback(header, entry);
    if (g_packet_entries.size() <= header) {
        g_packet_entries.resize(header + 1);
    }

    auto it = g_packet_entries[header].begin();
    while (it != g_packet_entries[header].end()) {
        if (it->altitude > altitude) {
            break;
        }
        ++it;
    }
    g_packet_entries[header].insert(it, CallbackEntry{altitude, entry, callback});

    if (g_game_server_handlers && g_game_server_handlers->size() > header) {
        g_game_server_handlers->at(header).handler_func = &StoCHandler_Func;
        success = true;
    }
    ::LeaveCriticalSection(&g_mutex);
    return success;
}

bool RegisterPostPacketCallback(
    PY4GW::HookEntry* entry,
    uint32_t header,
    const PacketCallback& callback) {
    return RegisterPacketCallback(entry, header, callback, 0x8000);
}

size_t RemoveCallback(uint32_t header, PY4GW::HookEntry* entry) {
    size_t removed = 0;
    SafeInitializeCriticalSection(&g_mutex);
    ::EnterCriticalSection(&g_mutex);
    if (header < g_packet_entries.size()) {
        auto it = g_packet_entries[header].begin();
        while (it != g_packet_entries[header].end()) {
            if (it->entry == entry) {
                g_packet_entries[header].erase(it);
                ++removed;
                break;
            }
            ++it;
        }
    }
    ::LeaveCriticalSection(&g_mutex);
    return removed;
}

size_t RemoveCallbacks(PY4GW::HookEntry* entry) {
    size_t removed = 0;
    SafeInitializeCriticalSection(&g_mutex);
    ::EnterCriticalSection(&g_mutex);
    for (auto& header_entries : g_packet_entries) {
        auto it = header_entries.begin();
        while (it != header_entries.end()) {
            if (it->entry == entry) {
                it = header_entries.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
    }
    ::LeaveCriticalSection(&g_mutex);
    return removed;
}

void RemovePostCallback(uint32_t header, PY4GW::HookEntry* entry) {
    RemoveCallback(header, entry);
}

namespace {

// Field kinds as encoded in each packet_template uint32_t descriptor:
// bits 0-3 = type, bits 4-7 = size, bits 8-23 = count. Mirrors GWCA's
// PacketLogger field decoding (Examples/PacketLogger/main.cpp: GetField),
// which is the authoritative reference for this format.
enum class FieldKind : uint32_t {
    AgentId,
    Float,
    Vect2,
    Vect3,
    Fixed4,  // Byte/Word/Dword: all three are serialized as a 4-byte value
    Blob,
    Ignore,  // template bookkeeping entries (e.g. array-end markers); no wire bytes
    String16,
    Array8,
    Array16,
    Array32,
    NestedStruct,
    Unknown,
};

FieldKind DecodeFieldKind(uint32_t descriptor) {
    const uint32_t type = descriptor & 0xF;
    const uint32_t size = (descriptor >> 4) & 0xF;
    switch (type) {
        case 0: return FieldKind::AgentId;
        case 1: return FieldKind::Float;
        case 2: return FieldKind::Vect2;
        case 3: return FieldKind::Vect3;
        case 4:
        case 8:
            return FieldKind::Fixed4;
        case 5:
        case 9:
            return FieldKind::Blob;
        case 6:
        case 10:
            return FieldKind::Ignore;
        case 7:
            return FieldKind::String16;
        case 11:
            switch (size) {
                case 1: return FieldKind::Array8;
                case 2: return FieldKind::Array16;
                case 4: return FieldKind::Array32;
                default: return FieldKind::Unknown;
            }
        case 12:
            return FieldKind::NestedStruct;
        default:
            return FieldKind::Unknown;
    }
}

// Hard ceilings on the walk below, purely defensive: they bound how far a
// corrupt/misread template or a garbage in-packet repeat count can push the
// computed size or the nested-struct replay loop before we give up and let
// the caller fall back to sizeof(PacketBase).
constexpr uint32_t kMaxComputedPacketBytes = 8192;
constexpr uint32_t kMaxNestedRepeatCount = 4096;

// Walks `field_count` template field descriptors against the live packet
// bytes starting at `base + *offset`, advancing *offset by what each field
// actually occupies on the wire. Fixed-width fields (AgentId/Float/Vect2/
// Vect3/Fixed4) and template-sized regions (Blob/String16/Array8/16/32) are
// resolved from the descriptor alone; a trailing NestedStruct's repeat count
// is packet data (not template data), so this reads live bytes at *offset
// and must only be called from inside the SEH guard installed by the caller.
bool WalkFields(const uint32_t* fields, uint32_t field_count, const uint8_t* base, uint32_t* offset) {
    for (uint32_t i = 0; i < field_count; ++i) {
        const uint32_t descriptor = fields[i];
        const uint32_t count = (descriptor >> 8) & 0xFFFF;

        switch (DecodeFieldKind(descriptor)) {
            case FieldKind::AgentId:
            case FieldKind::Float:
            case FieldKind::Fixed4:
                *offset += 4;
                break;
            case FieldKind::Vect2:
                *offset += 8;
                break;
            case FieldKind::Vect3:
                *offset += 12;
                break;
            case FieldKind::Blob:
            case FieldKind::Array8:
                *offset += count;
                break;
            case FieldKind::String16:
            case FieldKind::Array16:
                *offset += count * 2;
                break;
            case FieldKind::Array32:
                *offset += count * 4;
                break;
            case FieldKind::Ignore:
            case FieldKind::Unknown:
                break;
            case FieldKind::NestedStruct: {
                const uint32_t repeat = *reinterpret_cast<const uint32_t*>(base + *offset);
                *offset += 4;
                if (repeat > kMaxNestedRepeatCount || *offset > kMaxComputedPacketBytes) {
                    return false;
                }
                const uint32_t* nested_fields = fields + i + 1;
                const uint32_t nested_field_count = field_count - (i + 1);
                for (uint32_t rep = 0; rep < repeat; ++rep) {
                    if (!WalkFields(nested_fields, nested_field_count, base, offset)) {
                        return false;
                    }
                }
                // GW always places at most one nested struct, and always last.
                return true;
            }
        }

        if (*offset > kMaxComputedPacketBytes) {
            return false;
        }
    }
    return true;
}

}  // namespace

uint32_t GetPacketSize(uint32_t header, const Packet::StoC::PacketBase* packet) {
    uint32_t size = sizeof(Packet::StoC::PacketBase);
    SafeInitializeCriticalSection(&g_mutex);
    ::EnterCriticalSection(&g_mutex);
    if (g_game_server_handlers && g_game_server_handlers->size() > header) {
        const StoCHandler& handler = g_game_server_handlers->at(header);
        // Field index 0 describes the header itself, which the caller's
        // packet pointer already accounts for (offset starts past it), so
        // the walk starts at field 1, matching GWCA's PrintNestedField call.
        if (handler.packet_template && handler.field_count > 1 && packet) {
            uint32_t offset = sizeof(uint32_t);
            const uint8_t* base = reinterpret_cast<const uint8_t*>(packet);
            __try {
                if (WalkFields(handler.packet_template + 1, handler.field_count - 1, base, &offset)) {
                    size = offset;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                size = sizeof(Packet::StoC::PacketBase);
            }
        }
    }
    ::LeaveCriticalSection(&g_mutex);
    return size;
}

bool EmulatePacket(Packet::StoC::PacketBase* packet) {
    return OriginalHandler(packet);
}

}  // namespace GW::StoC

#include "GW/ui/native_dialog_host.h"

#include "GW/ui/ui.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_manager.h"
#include "base/patterns.h"

#include <cstdio>
#include <string>

namespace GW::ui::DialogHost {
namespace {

Natives g_natives{};

// One record per live host window, found by the frame id from msg[0]. Measurement (2026-10-12)
// showed the engine does NOT hand our create_param back as msg[2] — that slot is engine-owned — so
// a record is bound to the id carried in the create message rather than recovered from the param.
constexpr uint32_t kMaxHosts = 16;
HostRecord* g_hosts[kMaxHosts]{};
uint32_t g_host_count = 0;

// Most recently created host frame id. Recorded because a create issued from Python is queued
// onto the game thread and therefore cannot return its id to the caller.
uint32_t g_last_frame_id = 0;

// The record handed to the engine by the in-flight create call. The record is now also published
// in g_hosts before that call, so this is only a belt-and-braces path for the creation window.
HostRecord* g_pending = nullptr;

// The dispatcher's first messages are logged so an in-client run can prove whether the proc is
// installed at all, and which message ids and param values actually arrive. Absence of these lines
// during a create means the proc was never invoked as the frame's FrameProc.
constexpr uint32_t kMaxLoggedMessages = 64;
uint32_t g_messages_logged = 0;

// The nine natives the OWNING path needs (see HostResolvedMask for the bit order). Geometry is a
// separate capability: reported in the mask, but it must never gate ownership.
constexpr uint32_t kRequiredMask = 0x1FF;
constexpr uint32_t kGeometryBit  = 1u << 9;
constexpr uint32_t kChromeBit    = 1u << 10;
constexpr uint32_t kAllMask      = kRequiredMask | kGeometryBit | kChromeBit;

uint32_t ComputeResolvedMask() {
    uint32_t mask = 0;
    if (g_natives.frame_msg_call_base)       mask |= 1u << 0;
    if (g_natives.frame_schedule_size)       mask |= 1u << 1;
    if (g_natives.frame_native_size_changed) mask |= 1u << 2;
    if (g_natives.ctl_layout_ctor)           mask |= 1u << 3;
    if (g_natives.ctl_layout_dtor)           mask |= 1u << 4;
    if (g_natives.ctl_layout_set_frame_id)   mask |= 1u << 5;
    if (g_natives.ctl_layout_clear)          mask |= 1u << 6;
    if (g_natives.ctl_layout_size)           mask |= 1u << 7;
    if (g_natives.ctl_layout_size_query)     mask |= 1u << 8;
    // bit 9 is the geometry facade: reported, but deliberately not part of kRequiredMask, so a
    // missing geometry native cannot disable ownership.
    if (g_natives.frame_set_position)        mask |= 1u << 9;
    if (g_natives.frame_new_subclass)        mask |= 1u << 10;
    return mask;
}

// msg[2] is an ENGINE-owned slot, not our create_param — measured in-client, which is why
// ownership is keyed on msg[0] instead. This helper is retained only so the dispatcher's trace can
// report whether msg[2] happens to be one of ours (it is not, and that is expected).
//
// The pointer is validated against our own table first: trusting msg[2] blindly would let a
// foreign or stale param be reinterpreted as a HostRecord, and its instance_slot would then be
// dereferenced as an Instance inside the layout calls. Checking is a few comparisons and turns a
// whole crash class into a no-op.
bool IsKnownRecord(const HostRecord* record) {
    if (!record) {
        return false;
    }
    if (record == g_pending) {
        return true;
    }
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (g_hosts[i] == record) {
            return true;
        }
    }
    return false;
}

HostRecord* FindRecordById(uint32_t frame_id) {
    if (frame_id == 0) {
        return nullptr;
    }
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (g_hosts[i] && g_hosts[i]->frame_id == frame_id) {
            return g_hosts[i];
        }
    }
    return nullptr;
}

// Frames are identified by the id the engine puts in msg[0] — NOT by msg[2].
//
// Measured in-client 2026-10-12: the dispatcher receives the create message (msg[1] == 0x09) with
// msg[0] == 17 and msg[2] == an engine-owned pointer that is not the value passed as
// CreateUIComponent's 5th argument, and the frame's readable user param is 0. The earlier reading
// of `hdr.param` as "the FrameCreate userdata we supply" is therefore wrong for this path, and
// keying ownership on msg[2] meant the record was never found — which left the instance slot empty
// and crashed the engine's destroy path.
HostRecord* RecordFrom(const uint32_t* msg) {
    return msg ? FindRecordById(msg[0]) : nullptr;
}

Instance* InstanceFrom(const uint32_t* msg) {
    HostRecord* record = RecordFrom(msg);
    if (!record || record->instance_slot == 0) {
        return nullptr;
    }
    return reinterpret_cast<Instance*>(record->instance_slot);
}

HostRecord* FindRecord(uint32_t frame_id) {
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (g_hosts[i] && g_hosts[i]->frame_id == frame_id) {
            return g_hosts[i];
        }
    }
    return nullptr;
}

int FreeSlot() {
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (!g_hosts[i]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ReleaseRecord(HostRecord* record) {
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (g_hosts[i] == record) {
            g_hosts[i] = nullptr;
            if (g_host_count > 0) {
                --g_host_count;
            }
            return;
        }
    }
}

// The create arm: claim the record's slot by allocating the instance the engine expects to find
// there. Mirrors TCtlInstance<T>::MsgProc's msg-9 arm (same allocation size, same embedded
// layout ctor, same SetFrameId, same post-create order).
void CreateInstance(uint32_t* msg) {
    HostRecord* record = RecordFrom(msg);

    if (!record) {
        // The create message carries the frame id in msg[0], but the record's id is not bound yet
        // at that moment. Creation is serialized (one in-flight create), so bind the pending
        // record to the id we just learned. After this, every later message finds it by id.
        record = g_pending;
        if (record) {
            record->frame_id = msg[0];
        }
    }

    if (!record) {
        char line[176];
        std::snprintf(line, sizeof(line),
            "[dialog-host] create for frame %u rejected: no record to bind (msg[2]=0x%08X)",
            msg[0], msg[2]);
        Logger::Instance().LogWarning(std::string(line));
        return;
    }

    if (record->instance_slot != 0) {
        return;  // already owned
    }

    auto* instance = static_cast<Instance*>(PY4GW::MemoryManager::MemAlloc(kInstanceSize));
    if (!instance) {
        Logger::Instance().LogWarning(std::string("[dialog-host] instance allocation failed"));
        return;
    }

    instance->vtable = nullptr;  // we are not a C++ TCtlInstance subclass; nothing virtuals us
    instance->frame_id = msg[0];

    if (!g_natives.ctl_layout_ctor(instance->layout)) {
        // The engine ctor returns the buffer it constructed; a null return means we must not keep
        // this instance, and must leave the slot null so the frame stays un-owned. Logged because
        // a silent return here is indistinguishable from the dispatcher never being called.
        char line[160];
        std::snprintf(line, sizeof(line),
            "[dialog-host] frame %u create abandoned: CCtlLayout ctor returned null", msg[0]);
        Logger::Instance().LogWarning(std::string(line));
        PY4GW::MemoryManager::MemFree(instance);
        return;
    }
    // THE engine contract, verified against TCtlInstance<T>::MsgProc and now against a live
    // client: msg[2] points at a slot the ENGINE owns, and the frame's proc is expected to store
    // its instance through it — the decompiled create arm does `*(int**)param1[2] = instance`.
    // TCtlInstance<T>::Ptr then reads it back with one dereference, and Ptr asserts
    // `hdr.param == NULL || *hdr.param != NULL`. The engine's destroy path dereferences that slot.
    //
    // An earlier revision kept the instance only in our own record and never wrote this slot, so
    // the assertion was violated and destroy read a null base plus frame field 0x19B:
    // "Memory at address 0000019b could not be read".
    if (msg[2]) {
        *reinterpret_cast<uint32_t*>(msg[2]) = reinterpret_cast<uint32_t>(instance);
    }

    g_natives.ctl_layout_set_frame_id(instance->layout, instance->frame_id);

    record->instance_slot = reinterpret_cast<uint32_t>(instance);
    if (record->frame_id == 0) {
        record->frame_id = instance->frame_id;
    }

    if (record->build_content) {
        record->build_content(instance->frame_id, instance, record->user);
    }

    // Engine order, taken from the decompiled create path: OnFrameCreate, then Clear, then the
    // size pass. Clear before the schedule is what makes the first measure pass recompute.
    g_natives.ctl_layout_clear(instance->layout);
    if (record->build_layout) {
        record->build_layout(instance, instance->layout, record->user);
    }

    // Both halves are required: without the schedule call the engine never measures the frame,
    // and without the native-size notify direct children keep stale rects.
    g_natives.frame_schedule_size(instance->frame_id);
    g_natives.frame_native_size_changed(instance->frame_id);

    // Logged on success so an in-client run can prove the owning path executed, rather than
    // inferring it from the mere existence of a frame id.
    char line[192];
    std::snprintf(line, sizeof(line),
        "[dialog-host] frame %u OWNED: instance 0x%08X, layout 0x%08X, size pass scheduled",
        instance->frame_id,
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(instance)),
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(instance->layout)));
    Logger::Instance().LogInfo(std::string(line));
}

// The destroy arm. Returns the record so the caller can free it AFTER chaining to the base proc:
// the base proc receives the same message and may still read hdr.param, so freeing it here would
// be a use-after-free.
HostRecord* DestroyInstance(uint32_t* msg) {
    HostRecord* record = RecordFrom(msg);
    Instance* instance = InstanceFrom(msg);

    uint32_t released_frame_id = 0;
    if (instance) {
        // Read the id before the instance is freed.
        released_frame_id = instance->frame_id;
        // The engine's own teardown is ~T (which runs Ui::CLayout::~CLayout on the embedded
        // layout) then operator delete. Without the dtor the layout's node array leaks.
        if (g_natives.ctl_layout_dtor) {
            g_natives.ctl_layout_dtor(instance->layout);
        }
        PY4GW::MemoryManager::MemFree(instance);
    }

    if (record) {
        record->instance_slot = 0;
        ReleaseRecord(record);
    }

    if (released_frame_id) {
        char line[160];
        std::snprintf(line, sizeof(line),
            "[dialog-host] frame %u RELEASED: layout dtor ran, instance freed", released_frame_id);
        Logger::Instance().LogInfo(std::string(line));
    }
    return record;
}

}  // namespace

bool Ready() {
    // SUBSET test, not equality. An equality test here silently disabled ownership the moment the
    // geometry bit was added: measured in-client on 2026-10-12, the mask read 0x3FF with
    // "unresolved bits 0x0" yet still reported "ownership unavailable", because 0x3FF != 0x1FF.
    return (ComputeResolvedMask() & kRequiredMask) == kRequiredMask;
}

const Natives& Resolved() {
    return g_natives;
}

uint32_t HostResolvedMask() {
    return ComputeResolvedMask();
}

uint32_t HostWindowCount() {
    return g_host_count;
}

uint32_t LastHostFrameId() {
    return g_last_frame_id;
}

bool SetFrameRect(uint32_t frame_id, float x, float y, float w, float h) {
    CrashContextScope context("ui", "dialog_host", "set_frame_rect");

    if (!g_natives.frame_set_position) {
        // Geometry never resolved. Report it rather than silently leaving a 0x0 rect and letting
        // the caller conclude that nothing renders.
        return false;
    }
    if (frame_id == 0) {
        return false;
    }

    // The facade takes POINTERS to two Coord2f — position (x,y) and size (w,h) — with anchor 6
    // hardcoded inside it. Ghidra-verified field order: anchor +8, x +0xc, y +0x10, w +0x14, h +0x18.
    const float position[2] = { x, y };
    const float size[2] = { w, h };
    g_natives.frame_set_position(frame_id, position, size);
    return true;
}

// Layers a subclass descriptor onto a frame. The descriptor address is BUILD-SPECIFIC: the June-era
// constant 0x851180 in the older notes belongs to a different build, so it is never hardcoded here —
// the caller resolves it for the running client and passes it in.
bool AttachFrameSubclass(uint32_t frame_id, void* subclass_descriptor, uint32_t msg_id) {
    CrashContextScope context("ui", "dialog_host", "attach_frame_subclass");

    if (!g_natives.frame_new_subclass || !subclass_descriptor || frame_id == 0) {
        return false;
    }
    g_natives.frame_new_subclass(frame_id, subclass_descriptor, msg_id);
    return true;
}

uint32_t CreateOwnedWindow(uint32_t parent_frame_id, float x, float y, float w, float h,
                           uint32_t style, const wchar_t* title) {
    CrashContextScope context("ui", "dialog_host", "create_owned_window");

    // The frame is created first (owned: instance + embedded CCtlLayout + size pass), then given a
    // real rect through the anchor-6 setter. `style` must carry 0x40000 or the frame's paint pass
    // early-returns and nothing is drawn, no matter how correct the geometry is.
    const uint32_t frame_id = CreateHostWindow(
        parent_frame_id, style, 0, title, nullptr, nullptr, nullptr);
    if (frame_id == 0) {
        return 0;
    }

    if (!SetFrameRect(frame_id, x, y, w, h)) {
        char warn[176];
        std::snprintf(warn, sizeof(warn),
            "[dialog-host] window %u has no geometry: anchor-6 setter unavailable", frame_id);
        Logger::Instance().LogWarning(std::string(warn));
        // The frame exists and is owned, so report it rather than pretending the create failed.
    }

    char line[192];
    std::snprintf(line, sizeof(line),
        "[dialog-host] owned window %u (parent %u, style 0x%X, rect %.0f,%.0f %.0fx%.0f, bg bit %s)",
        frame_id, parent_frame_id, style, x, y, w, h, (style & 0x40000u) ? "set" : "MISSING");
    Logger::Instance().LogInfo(std::string(line));
    return frame_id;
}

bool ResolveDialogHostNatives() {
    CrashContextScope context("startup", "ui", "resolve_dialog_host");

    // Every entry below is a unique byte-pattern match against the installed client; the layout
    // cluster is a contiguous run of small __thiscall "ret 4" methods, which is why the patterns
    // are prologue-anchored rather than assertion-anchored.
    PY4GW::Patterns::Resolve("ui.frame_msg_call_base_func", &g_natives.frame_msg_call_base);
    PY4GW::Patterns::Resolve("ui.frame_schedule_size_func", &g_natives.frame_schedule_size);
    PY4GW::Patterns::Resolve("ui.frame_native_size_changed_func", &g_natives.frame_native_size_changed);
    PY4GW::Patterns::Resolve("ui.layout_ctor_func", &g_natives.ctl_layout_ctor);
    PY4GW::Patterns::Resolve("ui.layout_dtor_func", &g_natives.ctl_layout_dtor);
    PY4GW::Patterns::Resolve("ui.ctl_layout_set_frame_id_func", &g_natives.ctl_layout_set_frame_id);
    PY4GW::Patterns::Resolve("ui.ctl_layout_clear_func", &g_natives.ctl_layout_clear);
    PY4GW::Patterns::Resolve("ui.ctl_layout_size_func", &g_natives.ctl_layout_size);
    PY4GW::Patterns::Resolve("ui.ctl_layout_size_query_func", &g_natives.ctl_layout_size_query);

    // Anchor-6 geometry facade, Ghidra-verified as FUN_00634cc0(frameId, Coord2f* pos,
    // Coord2f* size) -> GetFrame(frameId) then CRect::SetPosition(frame+0xD0, pos, size, anchor=6).
    // The existing pattern anchors on that prologue and its bytes decode to exactly that function's
    // entry (push ebp; mov ebp,esp; mov eax,[ebp+8]; test eax,eax; jnz; push 0x840), so pattern and
    // decompiler agree on the same address.
    PY4GW::Patterns::Resolve("ui.frame_set_position_pos_size_func", &g_natives.frame_set_position);

    // FrameNewSubclass — the chrome attach. Two attempts in the table: an assertion anchor
    // (FrApi.cpp line 0x467) then a byte-pattern fallback. The fallback is the trustworthy one here,
    // because this build's assert macro has its operands swapped relative to what
    // FileScanner::FindAssertion builds (see §8), so assertion anchors only match by luck. The
    // pattern resolves to VA 0x6346A0 on the installed client, inside the same FrApi cluster as
    // FrameNativeSizeChanged (0x634660) and FrameScheduleSize (0x6349C0) — not the June-era
    // 0x62F150 the older notes quote, which is why nothing here is hardcoded.
    PY4GW::Patterns::Resolve("ui.frame_new_subclass_func", &g_natives.frame_new_subclass);

    const uint32_t mask = ComputeResolvedMask();
    const bool ownership = (mask & kRequiredMask) == kRequiredMask;
    const bool geometry = (mask & kGeometryBit) != 0;

    // Reports ownership and geometry separately, so a fully-resolved mask can no longer sit next to
    // an "unavailable" verdict without the log making the contradiction obvious.
    char line[224];
    std::snprintf(line, sizeof(line),
        "[dialog-host] resolved mask 0x%X/0x%X; ownership %s; geometry %s",
        mask, kAllMask,
        ownership ? "available" : "unavailable (pass-through)",
        geometry ? "available" : "MISSING");
    // export_to_disk defaults to true; an earlier revision passed false, which is why this
    // diagnostic never appeared in the client log at all.
    Logger::Instance().LogInfo(std::string(line));

    const uint32_t missing = kRequiredMask & ~mask;
    if (missing != 0) {
        char warn[96];
        std::snprintf(warn, sizeof(warn),
            "[dialog-host] unresolved ownership bits 0x%X", missing);
        Logger::Instance().LogWarning(std::string(warn));
    }

    return ownership;
}

uint32_t CreateHostWindow(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index,
                          const wchar_t* component_label, ContentCreateFn build_content,
                          ContentLayoutFn build_layout, void* user) {
    CrashContextScope context("ui", "dialog_host", "create_host_window");

    if (!Ready()) {
        char warn[128];
        std::snprintf(warn, sizeof(warn),
            "[dialog-host] create refused: ownership natives incomplete (mask 0x%X, need 0x%X)",
            ComputeResolvedMask(), kRequiredMask);
        Logger::Instance().LogWarning(std::string(warn));
        return 0;
    }

    // Reserve the table slot BEFORE creating: after the engine has seen the record it is
    // referenced by the frame, so there is no safe moment to free it on a late failure.
    const int slot = FreeSlot();
    if (slot < 0) {
        Logger::Instance().LogWarning(std::string("[dialog-host] host table full"));
        return 0;
    }

    auto* record = static_cast<HostRecord*>(PY4GW::MemoryManager::MemAlloc(sizeof(HostRecord)));
    if (!record) {
        Logger::Instance().LogWarning(std::string("[dialog-host] record allocation failed"));
        return 0;
    }

    // instance_slot MUST be zero here: the engine's Ptr asserts that a non-null param already
    // holds an instance, and the null slot is how the create arm knows to allocate.
    *record = HostRecord{};
    record->build_content = build_content;
    record->build_layout = build_layout;
    record->user = user;

    // Publish the record BEFORE the create call, and keep it published afterwards.
    //
    // An earlier revision gated the dispatcher's record lookup on a `g_pending` pointer that was
    // only set for the duration of this call, on the assumption that the create message arrives
    // synchronously from inside it. That assumption is not safe: if the engine delivers the
    // message at any other time, g_pending is already null, the dispatcher rejects the message,
    // no instance is allocated, and the frame's param slot stays zero — which then crashed the
    // client inside the engine's destroy path (null base + frame field 0x19B).
    g_hosts[slot] = record;
    ++g_host_count;
    g_pending = record;

    // Logged so the dispatcher's msg[2] can be compared directly against our own record address.
    // If they differ, msg[2] is engine-owned and ownership must stay keyed on msg[0] (the id).
    {
        char rline[176];
        std::snprintf(rline, sizeof(rline),
            "[dialog-host] record 0x%08X published at slot %d (parent %u, flags 0x%X)",
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(record)), slot,
            parent_frame_id, component_flags);
        Logger::Instance().LogInfo(std::string(rline));
    }

    const uint32_t frame_id = GW::ui::CreateUIComponent(
        parent_frame_id,
        component_flags,
        child_index,
        reinterpret_cast<GW::ui::UIInteractionCallback>(&HostProc),
        record,
        component_label);

    g_pending = nullptr;

    if (frame_id == 0) {
        ReleaseRecord(record);
        PY4GW::MemoryManager::MemFree(record);
        return 0;
    }

    // The create arm records the id it saw in msg[0]; prefer that, since it is the id the
    // dispatcher actually owns.
    if (record->frame_id == 0) {
        record->frame_id = frame_id;
    }
    g_last_frame_id = frame_id;

    char line[160];
    std::snprintf(line, sizeof(line),
        "[dialog-host] host window created: frame %u (parent %u, flags 0x%X, slot %d)",
        frame_id, parent_frame_id, component_flags, slot);
    Logger::Instance().LogInfo(std::string(line));
    return frame_id;
}

bool DestroyHostWindow(uint32_t frame_id) {
    CrashContextScope context("ui", "dialog_host", "destroy_host_window");

    HostRecord* record = FindRecord(frame_id);
    if (!record) {
        return false;
    }

    if (record->instance_slot == 0) {
        // The frame exists but was never claimed: the dispatcher never received its create
        // message, so the param slot is still zero. Calling the engine's destroy path in this
        // state dereferences that empty slot and crashes the client (observed 2026-10-12:
        // "Memory at address 0000019b could not be read", null base + frame field 0x19B).
        // Refuse instead, and deliberately do NOT free the record: the live frame still points
        // at it through hdr.param, so freeing it would leave a dangling param.
        char line[176];
        std::snprintf(line, sizeof(line),
            "[dialog-host] refusing to destroy frame %u: instance slot empty (never owned), "
            "engine destroy would fault", frame_id);
        Logger::Instance().LogWarning(std::string(line));
        return false;
    }

    Frame* frame = GW::ui::GetFrameById(frame_id);
    if (!frame) {
        // The frame is gone but the record was never released (e.g. the engine destroyed it
        // without a destroy message reaching us). Reclaim the record rather than leak it.
        ReleaseRecord(record);
        PY4GW::MemoryManager::MemFree(record);
        return false;
    }
    // The destroy message runs DestroyInstance, which frees the instance, runs the layout dtor
    // and releases the record.
    return GW::ui::DestroyUIComponent(frame);
}

uint32_t DestroyAllHostWindows() {
    // Snapshot the frame ids first: DestroyHostWindow mutates the table as it goes.
    uint32_t ids[kMaxHosts]{};
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxHosts; ++i) {
        if (g_hosts[i]) {
            ids[count++] = g_hosts[i]->frame_id;
        }
    }

    uint32_t destroyed = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (DestroyHostWindow(ids[i])) {
            ++destroyed;
        }
    }
    return destroyed;
}

void __cdecl HostProc(uint32_t* msg, void* wparam, void* lparam) {
    if (!msg) {
        return;
    }

    // Logged BEFORE any early-out, so a foreign or unclaimed message is still visible. Only the
    // first three dwords are read: the header is known to carry frameId/msgId/param, and reading
    // past that would risk going beyond the struct.
    if (g_messages_logged < kMaxLoggedMessages) {
        ++g_messages_logged;
        char trace[176];
        std::snprintf(trace, sizeof(trace),
            "[dialog-host] proc msg[0]=%u msg[1]=0x%X msg[2]=0x%08X ours=%s",
            msg[0], msg[1], msg[2],
            IsKnownRecord(reinterpret_cast<HostRecord*>(msg[2])) ? "yes" : "no");
        Logger::Instance().LogInfo(std::string(trace));
    }

    const uint32_t msg_id = msg[1];
    HostRecord* doomed = nullptr;

    if (Ready()) {
        if (msg_id == kMsgCreate) {
            CreateInstance(msg);
        }
        else if (msg_id == kMsgSize) {
            if (Instance* instance = InstanceFrom(msg)) {
                // The engine reads the incoming Rect4f inline at wparam + 8, not at wparam.
                auto* rect = reinterpret_cast<void*>(
                    reinterpret_cast<uint8_t*>(wparam) + kSizeParmOffset);
                g_natives.ctl_layout_size(instance->layout, rect);
            }
        }
        else if (msg_id == kMsgSizeQuery) {
            if (Instance* instance = InstanceFrom(msg)) {
                // Verified: wparam + 8 holds the OUTPUT Coord2f pointer, and the entity the engine
                // passes as `constraints` is the message pointer itself; the engine's own arm
                // writes the result only when it is non-zero.
                auto* out = *reinterpret_cast<void**>(
                    reinterpret_cast<uint8_t*>(wparam) + kSizeParmOffset);
                if (out) {
                    g_natives.ctl_layout_size_query(instance->layout, out, wparam);
                }
            }
        }
        else if (msg_id == kMsgDestroy) {
            doomed = DestroyInstance(msg);
        }
    }

    // Always chain: we are layered over the engine's own proc chain, and the base proc is what
    // keeps the chrome and every other message working.
    if (g_natives.frame_msg_call_base) {
        g_natives.frame_msg_call_base(msg, wparam, lparam);
    }

    if (doomed) {
        PY4GW::MemoryManager::MemFree(doomed);
    }
}

}  // namespace GW::ui::DialogHost

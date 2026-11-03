#pragma once

// Py4GW native dialog host — the "owner" every control-hosting Guild Wars window has and
// Py4GW never did. Recovered by decompiling Gw.wasm for two independent dialogs; see
// docs/ui/research/native-dialog-host-owner-contract.md for the evidence.
//
// Contract reproduced from the engine's own TCtlInstance<T>::MsgProc:
//   * a FrameProc is `void __cdecl(uint32_t* msg, void* wparam, void* lparam)`
//   * msg[0] = frameId, msg[1] = msgId, msg[2] (byte +0x08) = the param slot
//   * on create the engine passes param pointing at a (possibly null) slot; the owner
//     allocates its instance, stores it through *param, and reaches it the same way later
//   * the instance is per class: vtable +0x00, frameId +0x04, embedded CCtlLayout +0x08
//   * after the content builder runs, the owner requests the size pass through
//     FrameScheduleSize + FrameNativeSizeChanged (a CRect operation on frame+0xD0)
//   * teardown destroys the instance and chains the message to the base proc
//
// HOST RECORD DESIGN (as measured in-client 2026-10-12). Ownership is keyed on the frame id:
//
//   * Before creating, the host publishes a HostRecord and marks it as the in-flight create.
//   * The frame's proc then receives msg[1] == 0x09 carrying the new frame id in msg[0], and the
//     host binds the in-flight record to that id.
//   * Every later message is matched by msg[0]; instance_slot in the record holds the per-frame
//     instance the host allocated, and the host frees it on the destroy message.
//
// msg[0] is the authoritative identity. An earlier revision keyed this on msg[2] instead, reading
// it as "the create_param we pass to CreateUIComponent". Measurement falsified that: msg[2] holds
// an engine-owned pointer which does NOT carry that value, and the frame's readable user param is
// 0. The record was therefore never found, no instance was allocated, and the engine's destroy
// path later dereferenced the empty slot and crashed the client ("Memory at address 0000019b
// could not be read"). Message ids from the same run confirmed the rest of the contract:
// 0x4 class declaration, 0x9 create, 0xA, 0x32, 0x38 size query, 0x15, 0x34.

#include <cstddef>
#include <cstdint>

namespace GW::ui::DialogHost {

// Message ids, evidence-backed from the decompiled dispatcher (see the findings doc §1.1).
inline constexpr uint32_t kMsgCreate = 0x09;
inline constexpr uint32_t kMsgSize = 0x37;
inline constexpr uint32_t kMsgSizeQuery = 0x38;
inline constexpr uint32_t kMsgDestroy = 0x0b;
inline constexpr uint32_t kMsgNotify = 0x31;
inline constexpr uint32_t kMsgRefresh = 0x20;

// Payload geometry for the size arms, read off the decompiled dispatcher:
//   0x37: the Rect4f is INLINE at wparam + 8 (four consecutive floats)
//   0x38: wparam + 8 holds a POINTER to the output Coord2f, and the constraints argument
//         the engine passes is the message pointer itself
inline constexpr uint32_t kSizeParmOffset = 0x08;

// Instance geometry for a General-options-style content class. NOTE: instance size is
// per class — a second dispatcher in the same client frees its instance at 0x5C, not 0xD4.
inline constexpr uint32_t kInstanceSize = 0xD4;
inline constexpr uint32_t kLayoutOffset = 0x08;

struct Instance {
    void* vtable;          // +0x00
    uint32_t frame_id;     // +0x04
    uint8_t layout[0x18];  // +0x08  embedded CCtlLayout
    uint8_t reserved[kInstanceSize - 0x20];
};

static_assert(sizeof(Instance) == kInstanceSize, "instance size must match the engine's 0xD4");
static_assert(offsetof(Instance, layout) == kLayoutOffset, "layout must sit at instance+8");

// Content callbacks. `build_content` should FrameCreate controls as DIRECT CHILDREN of the
// hosting frame and seed values crash-safe; captions must be encoded payloads, not raw wide
// strings. `build_layout` builds the CCtlLayout tree for the size pass.
using ContentCreateFn = void(__cdecl*)(uint32_t frame_id, Instance* instance, void* user);
using ContentLayoutFn = void(__cdecl*)(Instance* instance, void* ctl_layout, void* user);

// The record handed to the engine as the create call's user data. NOTE: measurement showed the
// engine does NOT expose this pointer back to the proc — msg[2] is an engine-owned slot — so a
// record is found by the frame id from msg[0], not through msg[2] (see the design note above).
struct HostRecord {
    uint32_t instance_slot;  // +0x00  the Instance* once created (mirrors the engine slot)
    uint32_t frame_id;       // ours from here down
    ContentCreateFn build_content;
    ContentLayoutFn build_layout;
    void* user;
    uint8_t reserved[16];
};

struct Natives {
    void(__cdecl* frame_msg_call_base)(uint32_t* msg, void* wparam, void* lparam) = nullptr;
    void(__cdecl* frame_schedule_size)(uint32_t frame_id) = nullptr;
    void(__cdecl* frame_native_size_changed)(uint32_t frame_id) = nullptr;
    void*(__thiscall* ctl_layout_ctor)(void* self) = nullptr;
    // The engine's own MsgProc runs ~T, which runs Ui::CLayout::~CLayout() on the embedded
    // layout. Skipping it leaks the layout's node array, so it is resolved and called.
    void(__thiscall* ctl_layout_dtor)(void* self) = nullptr;
    void(__thiscall* ctl_layout_set_frame_id)(void* self, uint32_t frame_id) = nullptr;
    void(__thiscall* ctl_layout_clear)(void* self) = nullptr;
    void(__thiscall* ctl_layout_size)(void* self, void* rect4f) = nullptr;
    void(__thiscall* ctl_layout_size_query)(void* self, void* out_coord2f, const void* constraints) = nullptr;

    // Anchor-6 geometry facade, Ghidra-verified on the current build as
    //   FUN_00634cc0(frameId, Coord2f* pos, Coord2f* size):
    //     assert frameId != 0                      (line 0x840)
    //     GetFrame(frameId)
    //     CRect::SetPosition(frame+0xD0, pos, size, anchor=6)
    // The rect fields it writes are anchor +8, x +0xc, y +0x10, w +0x14, h +0x18; the inner setter
    // asserts 0x238 on an inverted container rect.
    //
    // A plain FrameSetSize is deliberately NOT used: the parent's layout pass resets such a rect
    // back to 0x0, which is why a frame sized that way stays invisible.
    void(__cdecl* frame_set_position)(uint32_t frame_id, const float* position, const float* size) = nullptr;

    // FrameNewSubclass(frameId, subclassProc, msgId) — layers a subclass onto a frame. This is how a
    // window gets CHROME: the legacy window creator attaches the composite-root control descriptor
    // here, and attaching it twice double-subclasses and crashes. Resolved by byte pattern rather
    // than a constant address, because the June-era addresses in the notes are from an older build.
    uint32_t(__cdecl* frame_new_subclass)(uint32_t frame_id, void* subclass_proc, uint32_t msg_id) = nullptr;
};

// Resolves the natives above. Safe to call more than once.
bool ResolveDialogHostNatives();

// True when the host has everything it needs to take ownership of a frame.
bool Ready();

const Natives& Resolved();

// Bitmask of resolved natives, for diagnostics.
// bit 0 FrameMsgCallBase, 1 FrameScheduleSize, 2 FrameNativeSizeChanged, 3 CtlLayout ctor,
// bit 4 CtlLayout dtor, 5 SetFrameId, 6 Clear, 7 Size, 8 SizeQuery, 9 FrameSetPosition.
// The first nine (0x1FF) are what Ready() requires for ownership; bit 9 is the geometry facade and
// is reported separately, so a missing geometry native cannot disable ownership.
uint32_t HostResolvedMask();

// Gives a frame a real rect through the anchor-6 facade: `position` is (x,y), `size` is (w,h).
// Returns false when the geometry native did not resolve. Unlike a plain size setter this survives
// the parent's layout pass.
bool SetFrameRect(uint32_t frame_id, float x, float y, float w, float h);

// ---------------------------------------------------------------------------
// Window creation.
//
// A GW window is an ARCHETYPE, not a plain container: the chrome comes from the frame's style and
// chain, and a container created with 0x300 alone draws NOTHING because the paint pass tests
// `FrameTestStyles(frame, 0x40000)` and early-returns when that background-template bit is clear.
// 0x254000 — the style the engine's own confirmation-dialog archetype uses — carries 0x40000, and
// that archetype was in-client verified as a working window with a button.
//
// This creates such a frame OWNED by HostProc, so the instance + embedded CCtlLayout + size pass are
// present too. Those are exactly what the notes say direct children require and "a bare window
// lacks", so window chrome and owner machinery are complementary, not alternatives.
// ---------------------------------------------------------------------------

// Creates an owned window frame at a real rect. Returns the frame id (0 on failure).
uint32_t CreateOwnedWindow(uint32_t parent_frame_id, float x, float y, float w, float h,
                           uint32_t style, const wchar_t* title);

// Layers an arbitrary subclass descriptor onto a frame. Exposed so a caller that can resolve the
// composite-root descriptor for THIS build can attach window chrome; the descriptor address is
// build-specific and must never be hardcoded from older notes.
bool AttachFrameSubclass(uint32_t frame_id, void* subclass_descriptor, uint32_t msg_id);

// The authored dispatcher. File scope + __cdecl so its address is a plain proc pointer,
// which is what the create primitive's callback parameter expects.
void __cdecl HostProc(uint32_t* msg, void* wparam, void* lparam);

// ---------------------------------------------------------------------------
// Entry points.
//
// The record handoff rests on one source-verified fact: CreateUIComponent *is* the engine's
// FrameCreate (RE map: ram:809a13ea == create_ui_component_func), whose signature is
// FrameCreate(parent, flags, child, proc, create_param, label). Py4GW's wrapper happens to name
// that 5th argument `name_enc` (and its Python binding calls it `wparam`), but it is forwarded
// verbatim as a raw pointer (ui_methods.cpp), so whatever pointer we pass there arrives as
// `hdr.param` == msg[2]. A HostRecord can therefore carry the registration itself, which is the
// only design that works: at create time no frame id exists yet, so a frame-id lookup cannot.
// ---------------------------------------------------------------------------

// Creates a frame owned by HostProc and returns its frame id (0 on failure).
// `component_flags` are the engine's FrameCreate flags (0 is the plain-container value).
// `component_label` may be null. The HostRecord is allocated internally: callers must NOT pass
// their own create_param, because that argument is reserved for the record.
uint32_t CreateHostWindow(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index,
                          const wchar_t* component_label, ContentCreateFn build_content,
                          ContentLayoutFn build_layout, void* user);

// Destroys a frame created by CreateHostWindow and releases its record.
bool DestroyHostWindow(uint32_t frame_id);

// Destroys every live host window. Test/cleanup convenience: a host created off the game thread
// cannot report its frame id back synchronously, so this is the reliable way to reclaim it.
// Returns how many were destroyed.
uint32_t DestroyAllHostWindows();

// Number of live host windows.
uint32_t HostWindowCount();

// Frame id of the most recently created host window (0 if none). Python scripts do NOT run on the
// game thread, so a create issued from Python is queued and cannot report its id synchronously —
// poll this (and HostWindowCount) instead of trusting the create return value.
uint32_t LastHostFrameId();

}  // namespace GW::ui::DialogHost

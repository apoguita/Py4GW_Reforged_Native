#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <string>

// Native UI module: the heavy reverse-engineering layer over Guild Wars' own UI
// system, migrated from the legacy py_ui.h custom subsystems (NOT the
// GWCA-mapped surface, which lives in GW/ui + PyUIManager bindings). This is
// the Py4GW-authored native-frame tooling: engine FrameProc resolution, window
// clone/title/dialog hijacks, devtext hosting, safe teardown, and the
// message-driven control swarm.
//
// Scan policy: plain byte-pattern and assertion scans resolve through
// offsets/native_ui.json; call-site derivation walks and prologue validation
// stay in native_ui_patterns.cpp (the call site owns the meaning of the scan).
namespace GW::native_ui {

/* ---------------- Resolved-symbol surface (module-owned) ---------------- */
// Batch 1: the shared engine FrameProc / helper resolver bank. Each symbol is
// resolved lazily on first use; the resolver bodies live in
// native_ui_patterns.cpp and are declared here as the module-owned base.

// Ui_CreateEncodedText(style_id, layout_profile, wide_text, reserved) -> frame text handle.
using CreateEncodedText_pt = uint32_t(__cdecl*)(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved);
// CtlFrameListCreateItem(frame_list_id, flags, child_code, ctx, out) -> item frame id (sends msg 0x57).
using CtlFrameListCreateItem_pt = uint32_t(__cdecl*)(uint32_t, uint32_t, uint32_t, void*, void*);
// FrameNewSubclass(frame_id, proc, flags) -> registers a subclass FrameProc on a frame.
using FrameNewSubclass_pt = uint32_t(__cdecl*)(uint32_t, void*, uint32_t);
// CtlText / CtlTextButton / CtlTextSelectable / page / slider / button / list procs are
// raw FrameProc addresses passed as component procs; typed as opaque uintptr_t.
// CtlFrameListSelectableGetSelection(frame_list_id, out_item_code) -> nonzero if a row is selected.
using CtlFrameListSelectableGetSelection_pt = uint32_t(__cdecl*)(uint32_t, uint32_t*);
// Hover-target setter FUN_00630cd0(frame, subitem): the sole writer of the hovered-frame
// global; call (0, -1) to clear hover before destroying a control-hosting window.
using SetHoverTarget_pt = void(__cdecl*)(int, int);
// Focus/tooltip-frame clear FUN_0064e920(frame): call (0) to drop the keyboard-focus global.
using ClearFocusFrame_pt = void(__cdecl*)(int);
// IUi_FrameMsgCallBase(msg, wparam, lparam): walks the FrameProc chain and invokes the next proc.
using FrameMsgCallBase_pt = void(__cdecl*)(uint32_t*, void*, void*);

extern CreateEncodedText_pt g_create_encoded_text_func;
extern CtlFrameListCreateItem_pt g_ctl_frame_list_create_item_func;
extern FrameNewSubclass_pt g_frame_new_subclass_func;
extern uintptr_t g_ctl_text_proc;                       // TextLabelFrame_Callback / CtlTextProc
extern uintptr_t g_ctl_text_button_proc;                // CtlTextBtnProc
extern uintptr_t g_ctl_text_selectable_proc;            // CCtlTextSelectable::FrameProc
extern uintptr_t g_ui_ctl_page_proc;                    // CUiCtlPage::FrameProc (styled tab page)
extern uintptr_t g_ui_ctl_slider_proc;                  // UiCtlSliderProc (styled slider wrapper)
extern SetHoverTarget_pt g_set_hover_target_func;
extern ClearFocusFrame_pt g_clear_focus_frame_func;
extern uintptr_t g_plain_container_proc;                // FUN_0051d8e0 pass-through content host
extern uintptr_t g_ctl_frame_list_selectable_proc;      // CCtlFrameListSelectable::FrameProc
extern CtlFrameListSelectableGetSelection_pt g_ctl_frame_list_selectable_get_selection_func;
extern uintptr_t g_ctl_btn_proc;                        // CtlBtnProc (flat engine button)
extern FrameMsgCallBase_pt g_frame_msg_call_base_func;

// Resolver ownership (bodies in native_ui_patterns.cpp). Each is idempotent and
// returns true once its symbol pointer is valid. create_encoded_text also runs
// the legacy prologue re-validation (55 8B EC 51 56 57).
bool ResolveCreateEncodedText();
bool ResolveCtlFrameListCreateItem();
bool ResolveFrameNewSubclass();
bool ResolveCtlTextProc();
bool ResolveCtlTextButtonProc();
bool ResolveCtlTextSelectableProc();
bool ResolveUiCtlPageProc();
bool ResolveUiCtlSliderProc();
bool ResolveSetHoverTarget();
bool ResolveClearFocusFrame();
bool ResolvePlainContainerProc();
bool ResolveCtlFrameListSelectableProc();
bool ResolveCtlFrameListSelectableGetSelection();
bool ResolveCtlBtnProc();
bool ResolveFrameMsgCallBase();

/* ---------------- Lifecycle ---------------- */
// The module's resolvers are lazy, but the runtime hooks (window-title hook,
// dialog-title hijack, ...) must be torn down on shutdown, so the module owns a
// lifecycle step. Initialize is a no-op arm point; Shutdown removes every hook
// this module installed.
bool Initialize();
void Shutdown();

/* ---------------- Batch 2: window-title hook (legacy UIManagerTitleHook) --------- */
// Overrides the title Guild Wars renders for the NEXT composite window it
// creates, by detouring the DevText title path (Ui_CreateEncodedText /
// Ui_SetFrameText / Ui_SetFrameEncodedTextResource) and swapping the caption
// only for the armed create. Hooks install lazily on first
// SetNextCreatedWindowTitle.
namespace title_hook {

bool SetNextCreatedWindowTitle(const std::wstring& title);
void ClearNextCreatedWindowTitle();
bool HasNextCreatedWindowTitle();
// Arm/Cancel move the pending next-title into a (parent, child) slot; the clone
// builders (later batch) call these around a native create.
void ArmNextCreatedWindowTitle(uint32_t parent_frame_id, uint32_t child_index);
void CancelArmedWindowTitle(uint32_t parent_frame_id, uint32_t child_index);
uint32_t GetLastAppliedFrameId();
std::wstring GetLastAppliedTitle();
bool IsInstalled();

// Build a game-encoded text string from literal UTF-16 `text` via the engine's
// Ui_CreateEncodedText (style 8 / layout 7 = literal-text wrapper, the window-
// title form). Returns the encoded wchar_t* (owned by the engine; do not free),
// or nullptr if the encoder could not be resolved. Used to inject literal
// CtlTextMl markup (e.g. <c=#RRGGBB>...</c>) that the game renders. Ensures the
// encoder is resolved on first use; safe to call whether or not the title hook
// is installed.
const wchar_t* CreateEncodedTextLiteral(const wchar_t* text);

}  // namespace title_hook

}  // namespace GW::native_ui

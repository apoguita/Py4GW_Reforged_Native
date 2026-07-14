#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <functional>
#include <string>

struct IDirect3DDevice9;

// World-render compositor.
// -----------------------------------------------------------------------------
// Lets overlays draw with correct depth occlusion against world geometry. Drawing
// from the D3D9 EndScene hook is too late/wrong. The correct point (as newer GWCA
// does it) is GW's Dx9 DDI command dispatcher FUN_006c6c40 (Dx9Ddi.cpp), which is
// hooked; on DDI opcode 0x0F (PRESENT: the handler calls device EndScene @vtbl+0xA8
// and Present @vtbl+0x44), the whole frame is rendered and the depth buffer is
// fully populated, so overlays drawn there occlude correctly.
//
// GWCA reference: RenderMgr hooks FUN_006c6c40 (found via "B9 10 19 00 00" ->
// ToFunctionStart) and invokes its render callback with device = *(ddi_ctx+0x1B8)
// when *(cmd+4) == 0x0F, before the original. We replicate exactly.
// RE: docs/RE/loot_beam_occlusion_plan.md. Scan inputs: offsets/world_render.json.

namespace GW::world_render {

bool Initialize();
void Shutdown();

// Callbacks run on the render thread at DDI opcode 0x0F (pre-Present), with the
// world depth buffer fully populated. Draw 3D geometry (e.g. via DXOverlay) here.
using DrawCallback = std::function<void(IDirect3DDevice9*)>;

int RegisterDraw(DrawCallback callback);  // returns a token (>=0), or -1 on failure
void UnregisterDraw(int token);
void ClearDraws();
bool IsActive();  // hook installed and enabled

// Runtime diagnostics: detour/opcode/callback counts + device pointer.
std::string GetDiagnostics();

// Enable/disable drawing without removing the hook (A/B testing).
void SetEnabled(bool enabled);

// Set which DDI opcode we draw at (default 0x1E). The depth scan (GetDiagnostics
// best_op) finds where the depth buffer actually holds the scene.
void SetDrawOpcode(int opcode);

// Enable/disable the diagnostic per-opcode depth scan (off by default; it issues a
// GPU occlusion query per frame and is only for finding the draw opcode).
void SetScanEnabled(bool enabled);

// GW Dx9 DDI command dispatcher (FUN_006c6c40):
//   uint32_t __cdecl(void* p1, void* ddi_ctx, uint32_t* cmd, uint32_t* out)
//   cmd[1] = DDI opcode (0x0F = present); device = *(IDirect3DDevice9**)(ddi_ctx+0x1B8).
using DdiDispatchFn = uint32_t(__cdecl*)(void* p1, void* ddi_ctx, uint32_t* cmd, uint32_t* out);

extern DdiDispatchFn g_ddi_func;      // hook target
extern DdiDispatchFn g_ddi_original;  // trampoline

bool ResolveDdiDispatch();  // body in world_render_patterns.cpp

}  // namespace GW::world_render

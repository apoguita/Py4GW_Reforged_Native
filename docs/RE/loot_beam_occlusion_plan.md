# In-world occluded overlay rendering (loot beams / agent highlights)

Goal: draw 3D overlays (loot beams, agent highlight rings/columns) that are
correctly OCCLUDED by world geometry (walls, terrain, props), in the current
Guild Wars build.

## Why the current DXOverlay approach fails

`DXOverlay` draws from the D3D9 `EndScene` hook. After a GW client update (added
OpenGL support), the depth buffer bound at `EndScene` no longer contains the 3D
scene's depth (device state is otherwise textbook: RT/DS both 1678x1368, DS =
D24S8, VP minZ/maxZ = 0/1). So any depth compare speckles; only `ZFUNC ALWAYS`
(no occlusion) looks clean. The projection math in `Setup3DView` is now CORRECT
(LH `LookAtLH` eye=cam.pos, target=cam.look_at_target, up=(0,0,-1);
`PerspectiveFovLH(GetFieldOfView, aspect, 46.875, 48000)`), it just runs too late.

Reference that works TODAY: GWToolbox++ `Utils/GameWorldCompositor.cpp` +
`Modules/LootBeaconsModule.cpp`. It draws INSIDE GW's world render pass.

## The mechanism (from GWToolbox, confirmed against Gw.exe)

Hook GW's world render dispatcher and run overlay callbacks after the world
block but before the HUD, while depth is still live. Matrices/states:
- View  = `XMMatrixLookAtLH(eye=cam.pos, target=cam.look_at_target, up=(0,0,-1))`
- Proj  = `XMMatrixPerspectiveFovLH(GetFieldOfView(), w/h, kZNear=46.875, kZFar=48000)`
- Depth = `ZENABLE=TRUE, ZWRITEENABLE=FALSE, ZFUNC=LESSEQUAL, CULL=NONE`,
          alpha `SRCALPHA/INVSRCALPHA`.
- Beams: fixed-function (POSITION + D3DCOLOR). Rings: shaders + a small negative
  `SLOPESCALEDEPTHBIAS`/`DEPTHBIAS` to avoid ground z-fight.

## Gw.exe (06-14) reverse engineering findings

FrCache render module lives ~0x635000. Anchored via the referenced log strings
"FrCache: ignored invalid client/frame viewport rect" at 0x00a51dd8 / 0x00a51e20
(the "...FrCache.cpp" assert path at 0x00a51da6/0x00a51dbd has NO absolute xref
in this build, so use the log-string refs to anchor instead).

### Caller: FrCache_Render = FUN_00634eb0  (single caller of FUN_00634f20)
    void __cdecl FUN_00634eb0(u32 param1, u32 param2) {
      if (DAT_00bef6b8 == 0) { ... resets DAT_00bef6e8 (count) = 0; }
      else if (DAT_00bef6e8 != 0) goto call;
      FUN_00634330();
    call:
      FUN_00634f20(param1, param2);   // <-- passes TWO args
    }

### FrCache_RenderAll = FUN_00634f20  (0x00634f20 .. 0x006351cc)
*** SIGNATURE: void __cdecl FUN_00634f20(u32 param1 [EBP+8], float param2 [EBP+0xC]) ***
It has TWO 4-byte stack params (read at [EBP+8] and [EBP+0xC] in the viewport
cases). The detour/trampoline MUST match this arity and FORWARD both params to
every original() call. Hooking it as void() made the original read garbage
viewport data -> GW UI/handle corruption -> stale-frame assert crash
(py4gw-20260713-020233). This was THE crash root cause (RE-confirmed via the
caller + the [EBP+8]/[EBP+0xC] reads), not the split logic or hook mechanism.

Iterates a global render-entry list and dispatches by type:

    base  = DAT_00bef6e0            // pointer to entry array
    count = DAT_00bef6e8            // number of entries
    entry stride = 3 dwords (12 bytes): [ type(dword), idx(dword), arg(dword) ]

    for each entry:
      switch(type):
        case 0: GPU_RENDER -> FUN_00651f40(0, entry.arg, DAT_00bef6d0 + entry.idx*4)  // WORLD draw
        case 1: draw            -> FUN_0064dce0 / FUN_00647c80
        case 2: client viewport rect (validated, FUN_006540f0)
        case 3: frame  viewport rect (validated)
    // tail: resets viewport to (0,0,1,1) via FUN_006540f0

Other referenced globals: DAT_00bef6d0 (GPU cmd buffer base), DAT_00bef6d8 (its
count), DAT_00bef6c0 (viewport buffer), DAT_00bef6c8 (its count).

### Split plan for the detour (OnFrCacheRenderAll)
Manipulate the two list globals around calls to the ORIGINAL trampoline:

    base  = DAT_00bef6e0; count = DAT_00bef6e8;
    boundary = count;
    for i in 0..count: if entry[i].type == 0 { boundary = i+1; break; }   // after first GPU_RENDER

    DAT_00bef6e8 = boundary;                 // world portion [0, boundary)
    original();
    DAT_00bef6e0 = base; DAT_00bef6e8 = count;

    GW::Render::FlushCommandQueue();          // TODO: resolve address (GWCA)
    RunOverlayCallbacks(device);              // our beams/highlights, depth LIVE

    DAT_00bef6e0 = base + boundary*12;         // HUD portion [boundary, count)
    DAT_00bef6e8 = count - boundary;
    original();
    DAT_00bef6e0 = base; DAT_00bef6e8 = count; // restore

(GWToolbox guards with drawn_this_frame reset in BeginFrame; single execution.)

## CORRECTED CONSENSUS - the FrCache split + flush was WRONG (2026-07-13)

Empirically: hooking FrCache_RenderAll, splitting at ANY index, and calling the
flush (mode==3, flush_ret==1 all confirmed) still produced NO occlusion. So that
whole approach is abandoned. The ACTUAL mechanism newer GWCA uses (RE of gwca.dll
RenderMgr, ground truth):

GWCA hooks GW's **Dx9 DDI command dispatcher FUN_006c6c40** (Dx9Ddi.cpp; found via
`B9 10 19 00 00` = MOV ECX,0x1910 @ 0x006c6e0d -> ToFunctionStart ~0x300 ->
0x006c6c40). Signature: `u32 __cdecl(void* p1, void* ddi_ctx, u32* cmd, u32* out)`;
cmd[1] = DDI opcode. GWCA (gwca.dll FUN_1000f85d) invokes its render callback with
device = *(ddi_ctx + 0x1B8) when cmd[1] == **0x0F**, inside a crit-section, BEFORE
the original. Opcode 0x0F = PRESENT: its handler FUN_006cadf0 calls device
EndScene (vtbl+0xA8) then Present (vtbl+0x44) + frame limiter + device-lost check.
So at opcode 0x0F the WHOLE frame is rendered and the depth buffer is fully
populated -> overlays drawn there occlude. No FrCache split, no manual flush.
(gwca flush hook FUN_1000f77c only captures ctx for the manual FlushCommandQueue
export; the DRAW point is the DDI dispatcher opcode 0x0F.)

IMPLEMENTED: world_render now hooks FUN_006c6c40 (offsets/world_render.json
ddi_dispatch), and on cmd[1]==0x0F draws callbacks with device=*(ddi_ctx+0x1B8),
then original. Instrumented: dispatch/present/cbs counts + device + last_opcode.
Py4GW's own EndScene hook fires at the wrong point (depth gone) - that is why
DXOverlay never occluded there.

## (SUPERSEDED) earlier RENDER MODEL notes (Gw.exe + gwca.dll, 2026-07-13)

The GW render is DEFERRED. FrCache_RenderAll's type-0 (GPU_RENDER) entries call
FUN_00651f40, which does NOT draw - it BUILDS command queues (sorts geometry into
buckets, writes opcode buffers into the render context). Nothing reaches the depth
buffer until the queues are FLUSHED. That is why splitting the entry list at ANY
index never occludes: at our callback the world is still queued, not rendered.

Runtime list this build: count=46, 36 type-0 entries (world is many passes).

### The flush (this is the missing piece)
GW::Render::FlushCommandQueue (gwca.dll export) -> GW flush = **FUN_00654930**:
    bool __fastcall FUN_00654930(context)   // ECX = render context
      require context[0x1cc] == 3           // GR_QUEUES; assert "m_queueFlushing == GR_QUEUES"
      device = context[0x08]
      for q in 3 queues at context+0x1a4 (stride 0x10):
        backend = context[0x34]             // 1=D3D9(FUN_006c71c0) 2=(FUN_006e43a0) 3=GL(FUN_00701100)
        submit q to backend                 // <-- actually renders to depth/color
      context[0x1cc] = 0..3
Found via assertion file "GrDev.cpp" msg "m_queueFlushing == GR_QUEUES"
(FindAssertion -> ToFunctionStart), OR the prologue
55 8B EC 51 53 56 8B F1 57 89 75 FC 81 BE CC 01 00 00 03 00 00 00.
context[0x34] = active backend = the OpenGL support that broke EndScene occlusion.

### The render context
context = *(uint32_t*)0x00c0ba4c (pointer global). Resolve build-robustly via the
device accessor FUN_006545b0: `A1 <ctx_global> 85 C0 74 04 8B 40 08 C3`
(MOV EAX,[ctx_global]; TEST; JZ; MOV EAX,[EAX+8]; RET) -> read imm32 at +1.

### Correct algorithm (GWToolbox GameWorldCompositor consensus)
Hook FrCache_RenderAll -> render WORLD entries (build+ready queues, context[0x1cc]
reaches 3) -> **call FUN_00654930(context) to FLUSH world to the depth buffer** ->
run our overlay callbacks (now occluded) -> render HUD entries. Guard the flush on
context && context[0x1cc]==3 (else skip, no crash). GWCA does the same: it HOOKS
FUN_00654930 to capture the context, and FlushCommandQueue() calls it manually.

## Open items to resolve
1. `GW::Render::FlushCommandQueue` address in Gw.exe (pull pattern from newest
   GWCA at C:\Users\Apo\Desktop\ghidra_12.0.4_PUBLIC\GWCA, or find in Gw.exe).
2. Confirm calling convention / that FUN_00634f20 is void()/reads globals (it is).
3. A stable scan pattern for FUN_00634f20: anchor on the ref to the frame-viewport
   log string (0x00a51e20 @ 0x0063518c) then to_function_start.
4. Whether hooking FUN_00634f20 directly is safe vs. hooking its caller.

## Implementation shape (Py4GW_Reforged_Native)
New in-world compositor, likely under GW/render (or a sibling), exposing a
`RegisterWorldDraw(callback)` API + resolving FUN_00634f20 via offsets JSON.
DXOverlay's 3D draws get routed through the world-draw callback (matrices/states
already correct) instead of EndScene. Keep EndScene path for true 2D/HUD overlays.

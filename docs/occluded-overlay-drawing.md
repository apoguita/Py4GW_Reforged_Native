# Occluded overlay drawing (in-world 3D that hides behind geometry)

This is the mechanism that lets 3D overlays (agent rings, range circles, loot
beams, danger zones) be **occluded by world geometry** - hidden behind walls,
terrain, and characters - instead of always drawing on top.

## Why there are two draw phases

GW's renderer is deferred and (since the graphics/OpenGL-support update) the
**depth buffer only exists during the world render pass** - it is discarded before
the frame is presented. So *where* you draw determines whether depth-testing works:

| Phase | When | Depth buffer | Use for |
|-------|------|--------------|---------|
| **EndScene** (existing Py4GW render callback) | end of frame / present | **empty** | 2D / UI / ImGui / anything "always on top" |
| **world_render** (`GW::world_render`, this module) | inside the world pass (DDI opcode `0x1E`) | **live** | 3D overlays that must be **occluded** |

Drawing a 3D primitive at EndScene will never occlude (depth is gone). Drawing it
in the world_render phase occludes correctly.

## How it works (RE summary)

GW executes the frame through a **Dx9 DDI command dispatcher**, `FUN_006c6c40`
(Dx9Ddi.cpp). `GW::world_render` hooks it (resolved from `offsets/world_render.json`
via `B9 10 19 00 00` -> function start) and, at DDI **opcode `0x1E`** (where the
world is drawn and depth is live), invokes registered draw callbacks with the D3D9
device (`*(ddi_ctx + 0x1B8)`), then calls the original. This mirrors how GWCA /
GWToolbox's `GameWorldRenderer` draws its occluded loot beacons. The draw opcode is
settable (`PyWorldRender.set_draw_opcode`) but `0x1E` is the confirmed default.
Full RE trail: `docs/RE/loot_beam_occlusion_plan.md`.

## How to draw an occluded overlay (Python)

Register a no-arg callback with `PyWorldRender`; it runs in the world pass. Inside
it, draw with `PyDXOverlay`'s 3D methods (they draw immediately at that point, so
they occlude). Register once, unregister when done.

    import PyWorldRender, PyDXOverlay, PyOverlay
    dx = PyDXOverlay.DXOverlay()

    def draw_world():
        x, y = get_item_xy()
        z = PyOverlay.Overlay().FindZ(x, y, 0)
        dx.DrawBeam3D(x, y, z, height=300.0, radius=60.0, argb=0xFF20FF40)

    token = PyWorldRender.register_draw(draw_world)   # installs the hook lazily
    # ... later ...
    PyWorldRender.unregister_draw(token)

`use_occlusion` on the primitive methods (`True`/`False`) picks the depth compare -
`LESS_EQUAL` (hidden by geometry) vs depth-test-off (visible through everything).
Same method, same draw point, one render state.

The callback runs on the render thread inside a saved D3D state block, so it can't
corrupt GW's rendering. `PyWorldRender.get_diagnostics()` reports dispatch/present/
callback counts and the device pointer.

**Note:** the callback is retained until you `unregister_draw` it - if a script
exits without unregistering, its last draws stay on screen. Unregister on teardown.

## IMPORTANT: fixed-function vs shader draws

The world buffer at this stage is **HDR and shader-driven**, so *how* you draw
matters:

- **Fixed-function `DXOverlay` 3D primitives** (`DrawPolyFilled3D`, `DrawPoly3D`,
  `DrawLine3D`, `DrawCube*`, ...) **occlude correctly**, but their COLOR is off
  (they write LDR fixed-function output into an HDR buffer that then gets tone-
  mapped: colors shift, look semi-transparent, blend oddly). Fine for rough
  debug/markers where exact color doesn't matter; not for polished visuals.
- **Shader-based draws** land correctly. `DXOverlay::DrawBeam3D` is the reference:
  it draws **colored geometry (no texture)** with a small vertex/pixel shader that
  gets view (`c0`) / projection (`c4`) as constants and uses additive blending - a
  clean glowing light beam, exactly like GWToolbox's `GameWorldRenderer`. New
  polished occluded visuals should follow this shader pattern.
- **Textures**: a loot beam is NOT a texture - it is additive colored geometry.
  Stretching an opaque bitmap (`DrawQuadTextured3D`) into the HDR world buffer will
  not look right; use `DrawBeam3D` / shader geometry instead.

## Depth tuning (usually unnecessary)

`Setup3DView` / `DrawBeam3D` use the proven LH view + `PerspectiveFovLH(fov, aspect,
46.875, 48000)` and `ZFUNC LESSEQUAL`, matching the pipeline. These are runtime-
tunable via `DXOverlay::SetOcclusionTuning(near, far, zfunc, reverse_z)` if a future
GW build shifts the depth convention.

## Practical takeaway

The occluded-draw phase is genuinely useful and is the intended path for 3D
overlays that should respect world geometry. Draw **occlusion-critical primitives**
(rings on the ground behind hills, beams behind walls, agent highlights) here.
Prefer **shader geometry** (`DrawBeam3D`-style) for anything where color fidelity
matters; fixed-function primitives are fine for debug markers. Keep 2D/UI on the
EndScene path.

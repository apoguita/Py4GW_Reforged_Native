# Client Build Drift: Recovering Lost UI Pointers

Status: current, offline- and live-verified for the 2026-11-03 client build.
Scope: `offsets/ui.json` in `Py4GW_Reforged_Native` (authoritative), its mirror in
`Py4GW_Reforged`, and the port copy in `Py4GW_Stealth`.

## Symptom

The client updated. The injection log for the new build
(`F:\GW\GW1\Py4GW_injection_log.txt`, 2026-11-03 08:34:48) reports five failed
`ui` resolutions:

| Resolver | Failure | Policy |
|---|---|---|
| `ui.get_child_frame_id_func` | `resolve_target` -> `near-call target is null` at `0x430C77` | halt, error |
| `ui.frame_schedule_size_func` | `scan_target` -> `scan returned null` | continue, warning |
| `ui.frame_native_size_changed_func` | `scan_target` -> `scan returned null` | continue, warning |
| `ui.frame_set_position_pos_size_func` | `scan_target` -> `scan returned null` | continue, warning |
| `ui.frame_get_native_size_func` | `scan_target` -> `scan returned null` | continue, warning |

The fourth one is what turns `[dialog-host]` geometry into `MISSING`.

## What actually changed

Two independent causes, and only the first is a regression.

### 1. `get_child_frame_id` — the compiler dropped one instruction (regression)

The resolver is assertion-anchored: `FileScanner::FindAssertion` finds the
`CtlView.cpp` / `pageId` assert, applies the pattern's `offset`, and
`FunctionFromNearCall` follows the call it lands on. The offset was `0x19`, which
is what legacy GWCA used. On the 2026-10-25 and 2026-11-02 builds the code after
the assert was:

```
0x610A70  BA <file> B9 <msg>          ; assert operands
0x610A7A  E8 <assert handler>
0x610A7F  8B 45 08                    ; mov eax,[ebp+8]
0x610A82  6A 00  6A 00  FF 70 04      ; push 0; push 0; push [eax+4]
0x610A89  E8 -> GetChildFrameId       ; anchor + 0x19
```

On the new build the compiler reused `edi` instead of reloading `[ebp+8]`, so the
call arrives three bytes earlier:

```
0x610C5E  BA <file> B9 <msg>
0x610C68  E8 <assert handler>
0x610C6D  6A 00  6A 00  FF 77 04      ; push [edi+4]
0x610C74  E8 -> GetChildFrameId       ; anchor + 0x16
```

A fixed `+0x19` therefore landed on the `04` byte of `FF 77 04`, which is not a
call, and `FunctionFromNearCall` returned null — exactly the logged failure.

### 2. The four `frame_*` patterns were already stale (not a regression)

Those patterns pinned bytes that a rebuilt `FrApi.cpp` changes:

- the assert **line number** immediate (`68 28/33/40 08 00 00`), and
- the **preferred-base pointers** to the assert's file and message literals.

`FrApi.cpp` gained six lines, so every pinned line shifted by exactly `+6`, and
`.rdata` moved, so the literal addresses changed too. The old forms do not match
on the 2026-10-24, 2026-10-25 or 2026-11-02 builds either; only the 2026-10-11
build they were written against. They were work-in-progress that had never
resolved on any build present on this machine.

| Symbol (WASM name) | 2026-10-11 (recorded) | assert line then | 2026-11-03 | assert line now |
|---|---|---|---|---|
| `FrameNativeSizeChanged` | 0x634660 | 0x828 | 0x631EA0 | 0x82E |
| `FrameNewSubclass` | 0x6346A0 | - | 0x631EE0 | 0x49B |
| `FrameScheduleSize` | 0x6349C0 | 0x833 | 0x632200 | 0x839 |
| `FrameSetPosition(f,pos,size,anchor)` | 0x634CC0 | 0x840 | 0x632500 | 0x846 |
| `FrameGetNativeSize(f,Coord2f&)` | 0x6327F0 | 0x7FD | 0x630030 | 0x803 |

The 2026-10-11 addresses for `FrameNewSubclass`/`FrameNativeSizeChanged`/
`FrameScheduleSize`/`FrameSetPosition` are recorded in the comments of
`src/GW/ui/native_dialog_host.cpp`; `FrameSetPosition` (`0x634CC0`) and
`FrameGetNativeSize` (`0x6327F0`) also appear as resolved addresses in
`Py4GW_Reforged/_ghidra/out/resolver_health_corrected.txt`.

## How the new addresses were established

Three independent lines of evidence agree, which is why they are trusted:

1. **Line shift.** Every `FrApi.cpp` assert that the old patterns pinned is `+6`
   in the 2026-10-25 build, so the old line numbers identify their successors
   unambiguously.
2. **Positional relation.** The four functions live in one translation unit in a
   fixed order. Measured directly, `FrameNativeSizeChanged = FrameNewSubclass -
   0x40`, `FrameScheduleSize = FrameNewSubclass + 0x320`, `FrameSetPosition =
   FrameNewSubclass + 0x620` on both the 2026-10-25 and 2026-11-03 builds, and
   the 2026-10-11 recording keeps the same order. `FrameNewSubclass` resolves
   independently through its own fallback pattern, so it is a safe base.
3. **Semantics of the neighbouring code.** Each function's trailing block is
   distinctive and matches the WASM symbol: `5D E9 <tail-jump>` for the
   argument-less `CRect::NotifyNativeSizeChanged`, `6A 01 E8` for
   `CRect::ScheduleSize(1)`, and `6A 06 FF 75 10 FF 75 0C E8` for
   `CRect::SetPosition(..., anchor = 6)` with the two `Coord2f` parameters.

Caller counts corroborate that the entries are real functions and not fragments:
`FrameNativeSizeChanged` 134, `FrameNewSubclass` 256, `FrameScheduleSize` 187,
`FrameSetPosition` 177, `FrameGetNativeSize` 383 call sites. The blocks that
follow each entry have zero call sites, so they are not entry points.

## The fix

`offsets/ui.json` only; no C++ changed.

1. `get_child_frame_id_anchor`: `offset` `0x19` -> `0x0`, so the anchor now
   answers with the assert site itself.
2. `get_child_frame_id_func`: the fixed offset became a bounded search the
   resolver engine already supports — `scan_in_range` over `near_call_opcode`
   from `anchor + 0xF` (immediately after the assert call, whose length is fixed:
   `BA`+`B9`+`E8 rel32` = 0xF) to `anchor + 0x40`, then
   `function_from_near_call`. This resolves correctly on all four builds present,
   because it does not care where the call sits inside that window.
3. `frame_native_size_changed`, `frame_schedule_size`,
   `frame_set_position_pos_size`: rewritten to compare the function prologue and
   its trailing block while **wildcarding** the assert line immediate, both
   literal pointers and every `rel32`. Line renumbering and address drift no
   longer break them; only a change to the function's own code does, and that
   fails loudly as `scan returned null`.
4. `frame_get_native_size`: line immediate `0x7FD` -> `0x803`.

`Py4GW_Reforged/offsets/ui.json` is byte-identical to the native table by
construction (verified by SHA-256). `Py4GW_Stealth/offsets/ui.json` carries only
items 1 and 2: that port's table predates the dialog-host work and has no
`frame_*` resolvers, and its port has no dialog host to feed.

### Why no rebuild was needed

`Patterns::Initialize()` loads `offsets/*.json` from the module directory at
startup, so the tables are runtime data, not compiled-in constants. Changing the
table and restarting the client is sufficient; the DLL binary does not have to be
rebuilt for a pattern-only fix.

## Verification

Offline, against `Gw.exe` on disk (2026-11-03 build, 10497216 bytes):

- every `ui` resolver: 94 of 94 resolve (was 89 of 94).
- every table in `offsets/`: 256 of 256 resolve.
- the same `ui` table also resolves 94 of 94 on the 2026-10-25 build, so the fix
  does not trade one build for another.
- each rewritten pattern is a unique match on the 2026-10-24, 2026-10-25,
  2026-11-02 and 2026-11-03 builds.
- the independent `_ghidra/scripts/verify_patterns.ps1` reports all four as
  unique with **zero loader-dependent bytes** — the earlier `frame_schedule_size`
  and `frame_native_size_changed` forms embedded rewritten pointers, which is why
  they could never match at runtime.

Live, against the running client (pid 29192, module base `0x00220000`):

- Stealth's own `tools/resolve_live.py` resolves the `ui` namespace against live
  memory **and** against the file, then compares by RVA: **72 of 72 agree, 0
  differ, 0 unresolved.**
- `ui.get_child_frame_id_func` live: `0x0044FD50` = base `0x00220000` + RVA
  `0x22FD50`, bytes `55 8b ec 8b 45 08 85 c0 74 0b 50 e8 ...` — a real entry,
  and the same RVA the file image yields.
- `python main.py --self-test`: PASS 25, FAIL 0 (6 GUI checks skipped).
- `pyright` on the added tools: 0 errors.

The live base confirms the arithmetic independently: the failed in-client address
in the log, `0x430C77`, is `0x00220000 + 0x210C77`, i.e. the assert site of the
2026-11-03 build on the live image.

## Tools added

Both live in `Py4GW_Reforged_Native/tools/pattern_lab/`:

- `resolve_offsets.py` — replays the `resolvers` programs of `offsets/*.json`
  against a `Gw.exe` image through the same `FileScanner` semantics the DLL uses
  (including the reverse-scan and end-bound quirks), and reports the failing step
  and its trace for each resolver. This is what turns "the pattern looks correct"
  into a measurement.
- `scan_asserts.py` — inventories every assert-macro call site in a build,
  resolving the file and message literals and the line number, with
  `--func-entry` to report the enclosing prologue. This is how a pin on an assert
  line is re-derived after a client update.

In the Stealth port, `tools/resolve_live.py` is the live counterpart of the
existing `tools/resolve_offline.py`.

## Remaining assumptions and limits

- The three rewritten `frame_*` patterns end by comparing the first bytes of the
  block that follows each function. That block is stable inside this translation
  unit across every build examined, but it is neighbouring code rather than the
  function itself; if the client reorders or deletes it, those three resolvers
  fail with `scan returned null` rather than resolving anywhere wrong.
- `frame_get_native_size_func` still pins an assert line and has **no consumer**
  anywhere in `src/` or `include/`. It was repaired to keep the log honest, not
  because anything reads it. Consider retiring it.
- Native-side in-client confirmation is still outstanding: this work could not
  inject into the client. The dialog-host geometry path should be exercised once
  the updated table is in place next to the injected DLL, and
  `[dialog-host] resolved mask 0x7FF/0x7FF; ownership available; geometry
  available` is the expected line.
- `Py4GW_Stealth/external/py4gw_stealth_game_thread_bridge_*` carry their own
  older `offsets/ui.json` (2026-07-31 / 2026-08-01 snapshots). They were not
  touched; if they are ever run directly they will need the same two-item fix.

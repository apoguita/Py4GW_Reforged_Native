# Handover: Python library migration — restart from zero

Date: 2026-07-04. This documents the state after a failed Vec2-unification attempt,
so the next session can restart the Python-library migration cleanly.

## TL;DR

- The **Python** project (`C:\Users\Apo\Py4GW_Reforged`) was reverted to HEAD
  `81593e2a` (legacy Py4GW). **All prior Python-migration progress is gone on
  purpose.** We restart the Python migration from the legacy baseline.
- The **Native** project (`C:\Users\Apo\Py4GW_Reforged_Native`) at HEAD `20bffb8`
  keeps its uncommitted native-side migration (managers/bindings/crash handler/
  callback isolation/scanner). The **Vec2 C++ rework was reverted** to HEAD.
- A Vec2 "unification" attempt (make one Vec2 type span ImGui `ImVec2`, overlay
  `GW::Vec2f`, and the ctypes shared-memory `Vec2f`) caused a hard crash at
  injection and was fully reverted. Do not repeat that approach (see Lessons).

## Hard rules (from the user — do not violate)

1. **NEVER touch the native callback system** (`src/callback/callback.cpp`,
   `PyCallback::ExecutePhase`, register/remove). It is migrated correctly and is
   off-limits. The Python codemods made ZERO changes to callback registration,
   so callbacks are not the migration's concern. Do not "fix", snapshot, or
   refactor it. This was stated repeatedly and firmly.
2. **No compatibility shims** anywhere (C++ bindings, facade, or call sites).
   Fix-forward: adapt the code to the real types/API.
3. **Be faithful to the native bindings and to the legacy structure.** Do not
   invent taxonomies, do not scatter things into new modules, keep legacy names.
4. **Do not run broad/blind codemods.** Migrate incrementally; verify each step.
   Get explicit approval before sweeping changes. Broad AST sweeps that guessed
   at tuple-grouping are what mangled the code last time.
5. **Trust the user's empirical diagnosis** over speculative C++ theories. When
   they say "it worked before X, it breaks after X," bisect around X first.

## What was attempted this session and why it failed

Goal (mine, over-reach): make every ImGui/overlay/ctypes Vec2 a single unified
type with a shared interface (indexing/iteration/operators/implicit tuple
conversion), and rewrite Python call sites to feed tuples/Vec2 instead of
separate floats.

What broke it:
- Added a sequence protocol (`__len__`/`__getitem__`/`__iter__`) to the **ctypes
  `Vec2f`/`Vec3f`** in `Py4GWCoreLib/native_src/internals/types.py` so they'd
  "double as" a Vec2. Those structs define **shared-memory layout** and must NOT
  be repurposed as Vec2. This is the misuse the user identified.
- Added the same interface to the C++ `ImVec2`/`ImVec4` (`types.cpp`) and
  `GW::Vec2f`/`Vec3f` (`overlay_bindings.cpp`), plus more implicit conversions.
- Ran broad AST codemods that grouped separate-float call args into tuples
  (`button(label, w, h)` -> `button(label, (w,h))`, and a buggy "sweep" that
  produced wrong groupings like `begin_child(id, (size, border), flags)`).

Result: deterministic access violation at **injection/first frames**. Every
crash dump was byte-identical:

```
#00 python313.dll!PyObject_Vectorcall  (fault reading [callable+4] at addr 0x00000004)
#02 pybind11 unpacking_collector<1>::call  (cast.h:2254)
#04 PY4GW::PyCallback::ExecutePhase        (callback.cpp:174  -> t->fn())
#05 DrawLoop / UpdateLoopStep
```

i.e. a callback's callable resolved to NULL/garbage. The user is certain the
Vec2 rework is the cause; per direction, the Vec2 rework (C++ + ctypes) was
reverted rather than diagnosed further. Root cause was NOT independently
confirmed — establish a clean injecting baseline first next session.

## Current repo state (verified)

Native `Py4GW_Reforged_Native` @ `20bffb8`, uncommitted (KEEP — native migration):
- `include/base/CrashHandler.h`, `src/base/CrashHandler.cpp` (crash sidecar +
  Python-stack capture)
- `src/GW/item/item_bindings.cpp`, `src/GW/ui/ui_bindings.cpp`,
  `src/GW/ui/ui_methods.cpp` (UI/item bindings; ui_methods has `IsFrameValid`
  guards for `0xFFFFFFFF` frame sentinels)
- `src/callback/callback.cpp` (native-migration **isolation** delta, +18/-2 —
  per-callback try/catch + crash context + `discard_as_unraisable`. LEAVE IT.)
- `src/imgui/bindings/enums.cpp`, `src/imgui/bindings/style.cpp`
- `src/base/python_runtime.cpp` (minor delta from HEAD)
- untracked `src/base/scanner_bindings.cpp` (remember: adding a `.cpp` requires
  re-running CMake configure — globbed sources)
- `Vec2` C++ rework reverted: `types.cpp` / `overlay_bindings.cpp` /
  `imgui_bindings.cpp` == HEAD.

Python `Py4GW_Reforged` @ `81593e2a`: clean (reverted to HEAD).

## Recommended restart plan (incremental, verify each step)

1. **Baseline first.** Build native as-is, inject with Python @ HEAD. Confirm it
   loads WITHOUT the crash. Do not migrate anything until this baseline injects.
   (Build: `cmake --build build --config RelWithDebInfo`; user injects/tests —
   never auto-build.) If it still crashes at baseline, the cause is native, not
   Python — bisect the native uncommitted files.
2. **Enumerate the real API delta.** For each legacy Python module, list exactly
   which `Py4GW.*` / `PyImGui.*` / `Py*` symbols it calls and whether the native
   build provides them (import each embedded module; check the binding surface).
   Migrate the smallest leaf module first.
3. **One module at a time, then inject.** Port a single module's call sites to
   the actual native signatures (faithful to the bindings — no shims), rebuild
   only if C++ changed, inject, confirm, commit. Never batch many modules.
4. **Vec2 specifically:** the native `PyImGui.Vec2/Vec4` at HEAD ALREADY accept
   tuples/lists via `implicitly_convertible<tuple/list, ImVec2/ImVec4>` and a
   sequence constructor. So Python may pass tuples where a native Vec2 is
   expected — that already works. **Do NOT** add a sequence interface to the
   ctypes shared-memory `Vec2f/Vec3f`, and **do NOT** run a blanket sweep to
   re-shape call args. Change call sites only when a specific signature demands
   it, function by function, verifying each.
5. Keep the migration guide + style guide open: `docs/module-migration-guide.md`,
   `docs/12-project-style-guide.md`. Native module anatomy and pattern/scanner
   rules are in `CLAUDE.md`.

## Debug notes if the injection crash returns

- Crash sidecars: `F:\GW\GW1\crashes\py4gw-*` (`-stack.txt`, `-pytrace.txt`,
  `.json`, GW `-gwtext.txt`). `-pytrace.txt` "No Python frame" + a NULL callable
  in `ExecutePhase` means the invoked callable itself is bad, not the callback
  body. That is a symptom; find what Python registered or what corrupted the
  heap earlier — do NOT modify the callback dispatcher.
- `python313.dll` loads from a PyInstaller temp dir (`_MEI...`), i.e. the
  injector bundles Python 3.13 (32-bit). Not itself a bug.

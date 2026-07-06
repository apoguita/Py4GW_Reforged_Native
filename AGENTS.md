# AGENTS.md

## What This Repo Is

`Py4GW_Reforged_Native` is a Windows-only, 32-bit injected DLL for Guild Wars. It hosts an embedded Python runtime, hooks the DX9 render/reset path, renders an ImGui overlay, and is actively migrating legacy Py4GW/GWCA subsystems into a cleaner native layout.

## Critical Rules

- Windows and Win32 are mandatory.
- 32-bit matters for both build assumptions and runtime behavior.
- The DLL is emitted into the repo root beside `fonts/`, `scripts/`, and `offsets/`.
- Do not treat `build/` as source-of-truth architecture.
- There is no automated test suite; validation is targeted and build-centric.
- Docs are ASCII-only.
- Never introduce structural migration changes without rereading the migration guide and style guide in the same turn.

## Read First Docs

- `docs/README.md`
- `docs/01-repository-overview.md`
- `docs/02-build-and-dependencies.md`
- `docs/03-runtime-lifecycle.md`
- `docs/12-project-style-guide.md`
- `docs/module-migration-guide.md`

If the task touches Python migration instability or startup crashes, read:

- `docs/callback-crash-isolation-plan.md`

## Build Facts

- Preset flow:
  - `cmake --preset vs2022-win32`
  - `cmake --build --preset vs2022-win32-relwithdebinfo`
- Plain flow:
  - `cmake -S . -B build -A Win32`
  - `cmake --build build --config RelWithDebInfo`
- Adding a new `.cpp` requires re-running CMake configure because sources are globbed with `CONFIGURE_DEPENDS`.

## Architecture Anchors

- `src/Py4GW.cpp` owns the coarse runtime lifecycle.
- `src/dllmain.cpp` owns process attach/detach entry.
- `src/GW/GuildWars.cpp` owns GW subsystem init/shutdown ordering.
- `include/GW/<module>/` + `src/GW/<module>/` is the native manager/module pattern.
- Shared GW data belongs in `GW/context/`.
- Packets/opcodes/shared protocol declarations belong in `GW/common/`.
- Project-wide infrastructure belongs in `base/`.

## Migration Discipline

This repo is under parity migration.

Before any structural migration work:
- reread `docs/module-migration-guide.md`
- reread `docs/12-project-style-guide.md`
- inspect the closest migrated sibling
- inspect current `GW/context/` layout

Do not auto-add Python bindings, convenience wrappers, or behavioral changes unless explicitly requested.

## Validation Guidance

Validation is usually:
- successful Win32 build
- forced startup/shutdown path surviving
- targeted smoke checks for the touched subsystem

For render, Python, hook, or shutdown-sensitive work, be conservative about lifecycle assumptions.

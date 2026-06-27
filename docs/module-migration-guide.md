# Module Migration Guide

This note captures the current migration pattern used when moving Guild Wars subsystems from the legacy `Py4GW` tree into `Py4GW_Reforged`.

## Scope Rule

Default to migrating the native manager or module first.

Do not automatically migrate:

- Python bindings
- legacy wrapper classes such as `PyCamera`
- unrelated convenience layers

That keeps the first port focused on runtime correctness, build integration, and dependency unblocking.

Before choosing the file layout, classify the legacy code correctly:

- manager or module: subsystem with its own `Module` entry in GWCA and explicit `init_module` or `enable_hooks` ownership
- shared infrastructure: utility used by several managers and coordinated from top-level GWCA lifecycle

Current example:

- `CameraMgr` is a true module
- `MemoryPatcher` is shared infrastructure

Practical naming rule for this repo:

- if the legacy component is a `*Mgr` and is backed by a GWCA `Module`, migrate it as a module
- if it is not a `*Mgr`, treat it as a shared tool by default unless legacy usage proves otherwise

Use the name rule as the fast first pass, then confirm it from the legacy code.

## Target File Layout

For a migrated Guild Wars module, prefer this four-file layout:

- `include/GW/<module>/<module>.h`
  Public lifecycle entry points such as `Initialize()` and `Shutdown()`.
- `include/GW/<module>/<module>_methods.h`
  Reverse-engineered structs, typedefs, globals, and callable methods used by other project code.
- `src/GW/<module>/<module>.cpp`
  Pointer resolution, pattern scans, hook setup, patch setup, and lifecycle orchestration.
- `src/GW/<module>/<module>_methods.cpp`
  Runtime methods operating on already-resolved state.

This is the pattern for true migrated modules such as `render` and `camera`.

## Responsibility Split

Keep responsibilities separated consistently:

- `<module>.cpp` owns discovery and setup
- `<module>_methods.cpp` owns behavior once setup has already succeeded

Examples of code that belongs in `<module>.cpp`:

- `Scanner::Find(...)`
- `Scanner::FindAssertion(...)`
- `Scanner::ToFunctionStart(...)`
- dereferencing located pointers
- creating hooks
- selecting patch sites

Examples of code that belongs in `<module>_methods.cpp`:

- math helpers
- getters and setters over resolved game structures
- patch enable and disable calls
- simple state queries

## Functional Scope Rule

Migration work must not add new functionality, new behavior, or new shutdown/runtime policy beyond:

- the legacy module behavior
- the project integration required by this repo
- explicit rules already documented in this file or other project docs

That means:

- do not add safety systems, wait paths, counters, state machines, or behavior changes just because they seem cleaner
- do not redesign module behavior while claiming parity migration
- do not change runtime semantics unless the legacy code already does it, or a project doc explicitly requires it

Allowed migration adaptations:

- project logger/assert usage
- project scanner/pattern JSON usage
- project file layout and naming
- project lifecycle entry points
- shared struct organization when explicitly requested

If a migration needs behavior that does not exist in legacy code, it must be treated as a separate intentional deviation and called out explicitly instead of being folded into the parity port.

## Prerequisite Migration Rule

Do not bridge missing legacy dependencies with compatibility shims.

If a manager or module needs shared structs, helper methods, or supporting legacy code that is not migrated yet, migrate that prerequisite first and then use it from the manager port.

Required order:

1. identify the real legacy dependency
2. migrate the shared prerequisite into its proper shared location
3. verify the prerequisite builds in project shape
4. migrate the manager or module that consumes it

Important consequences:

- do not invent replacement helpers just to keep the current manager moving
- do not add local stopgap behavior inside a manager because another legacy file is still missing
- do not hide unmigrated dependencies behind "temporary" parity code
- if parity requires another legacy file first, stop and migrate that file first

Parity must remain traceable:

- every non-project adaptation used by a migrated manager should already exist in migrated legacy code
- if the source behavior comes from another legacy file, that file should exist in the repo before the manager consumes it
- parity assessment should compare migrated code against the real legacy implementation, not against compatibility glue added during porting

## Shared Infrastructure Rule

Not everything under legacy GWCA should become its own migrated module.

If the legacy code is used by multiple managers and coordinated centrally, migrate it as shared infrastructure instead of forcing a fake manager or module split.

Current example:

- `MemoryPatcher` is included directly by `CameraMgr.cpp`, `MapMgr.cpp`, and `ChatMgr.cpp`
- top-level `GWCA.cpp` calls `MemoryPatcher::EnableHooks()` and `MemoryPatcher::DisableHooks()`
- there is no standalone `MemoryPatcherModule`

That means the appropriate port shape is shared project infrastructure with top-level lifecycle coordination, not a `GW/<module>/` manager.

Rule of thumb:

- one owner subsystem with its own lifecycle: migrate as a module
- multiple subsystems plus top-level orchestration: migrate as shared infrastructure

## Shared GW Data Rule

Not all shared GW code belongs under `include/base/` or `src/base/`.

If the legacy code is GW-facing shared data, shared structs, or shared helper methods on GW types, keep it under `GW/` but do not force it into a module-shaped folder.

Use this split:

- GW manager or module with lifecycle: `include/GW/<module>/` and `src/GW/<module>/`
- shared project infrastructure: `include/base/` and `src/base/`
- shared GW entities, structs, context layouts, and helper methods with no standalone lifecycle: `include/GW/context/` and matching `src/GW/context/` implementation files as needed

Examples:

- `Skill.cpp` shared helper behavior belongs under shared GW context/type code, not as a fake manager
- `Effect` and `SkillbarSkill` helper methods belong under shared GW context/type code, not inside `EffectMgr`
- `WorldContext` layout belongs under shared GW context code, not inside a manager implementation
- shared GW container types such as `GwArray` belong under shared GW context/type code, not at the top of `GW/`

Important:

- do not create a module-style folder just because a legacy file has behavior
- if the code has no `Initialize()`/`Shutdown()` ownership, it is not a module
- shared GW type behavior should be placed by domain, not by whichever manager happened to need it first
- if a manager needs shared GW type behavior, migrate that shared code first instead of embedding a local substitute in the manager

## Scan Translation Rule

Do not over-normalize scanner logic while porting.

The current project uses the pattern JSON system for scanner inputs. The call site still owns the meaning of scan results.

When porting scans from legacy code:

- preserve whether the source used assertion scans, direct byte scans, near-call resolution, or pointer dereferences
- preserve any asymmetry in how offsets are interpreted
- keep validation local with `Logger::AssertAddress(...)`
- move raw byte patterns, masks, sections, and integer offsets into the pattern JSON system instead of leaving them as string literals in code
- move assertion file names, assertion messages, line numbers, and scanner helper ranges into the pattern JSON system as well

Important reinforcement:

- `FindInRange(...)` is not an exception to the pattern rule
- `FindAssertion(...)` is not an exception to the pattern rule
- `FindUseOfAddress(...)` and `ToFunctionStart(...)` numeric inputs are not exceptions to the pattern rule
- if a call site uses `FindInRange(...)`, the byte pattern and mask should still come from `Patterns::Get(...)`
- if a call site uses `FindAssertion(...)`, the assertion file and assertion message should still come from `Patterns::Get(...)`
- the call site still owns scan windows, control flow, and semantic meaning

## Dependency Unblocking

Before migrating a new module, search the repo for commented placeholders or parity notes that mention it.

Example:

- `render` carried a commented `GetFieldOfView()` placeholder until `CameraMgr` existed

This matters because one migration often unlocks immediate cleanup in another subsystem.

Also search the legacy tree for include and call-site usage before deciding shape. The fastest way to make a wrong migration is to assume that every legacy header under `Utilities` or every reverse-engineered type deserves a manager or module wrapper.

## Integration Checklist

For each migrated module:

1. Add source files to `CMakeLists.txt`
2. Wire lifecycle into `src/GW/GuildWars.cpp`
3. Assert or log every resolved address and patch site
4. Reset hooks, patches, and pointers on shutdown
5. Re-enable any dependent placeholder code now unblocked by the migration
6. Build the target DLL to catch signature or include drift immediately
7. Add crash-context attribution around module initialization, hook installation, patch toggles, callback registration, and shutdown steps

For shared infrastructure:

1. Place the code under `include/base/` and `src/base/` unless there is already a better shared home
2. Wire any global enable and disable behavior into the top-level lifecycle such as `gw::Initialize()` and `gw::Shutdown()`
3. Avoid inventing a synthetic module wrapper unless the legacy project had one

## Legacy Parity Notes

Document inactive legacy pieces before porting them blindly.

Also document every intentional deviation from legacy behavior.

Do not use undocumented compatibility code to keep a migration moving. If the migrated manager only works because of newly invented glue, parity has not been established yet.

Required format for any deviation:

- what changed
- why it changed
- whether it is required by an existing project rule
- whether parity can still be assessed independently of that change

Current examples:

- `CameraMgr.cpp` declares `patch_max_dist` and `patch_fov`, but the legacy implementation never resolves or uses them in active code
- `CameraMgr::EnableHooks()` in the legacy project is effectively empty

That means the current port is not missing active behavior by omitting those inactive pieces.

For `MemoryPatcher`, the meaningful migrated surface is:

- patch registration
- patch restore on disable and reset
- global `EnableHooks()` and `DisableHooks()`
- optional `SetRedirect()` support

That is the actual legacy behavior other managers depend on.

## Validation Checklist

Minimum validation after a migration:

- the project builds successfully
- `Initialize()` is idempotent
- `Shutdown()` is safe when partially initialized state exists
- hooks or patches restore original bytes on teardown
- no Python surface was added unless explicitly requested
- crashes during startup or shutdown identify the active module and operation in the crash sidecar

## Crash Attribution Rule

Migration is not complete if the new code cannot be attributed clearly in crash logs.

Required practices:

- use `CrashHandler::SetContext(...)` at top-level lifecycle boundaries such as `gw::Initialize()`, `gw::Shutdown()`, `Py4GW_Initialize()`, and `Py4GW_Shutdown()`
- use `CrashContextScope` inside module internals so temporary operations stamp `phase`, `module`, `operation`, and optional `detail`
- wrap shared hook and patch infrastructure once instead of duplicating ad-hoc log lines in each module
- cover shutdown paths with the same care as startup paths because teardown crashes are common in injected code

Preferred module-level coverage:

- pointer and function resolution
- hook creation and removal
- patch registration and toggle paths
- callback registration points
- `Initialize()` and `Shutdown()` orchestration
- wait paths that drain in-flight detours or callbacks before teardown destroys shared state

The goal is simple: if the client crashes, the sidecar should identify which migrated subsystem was active and what it was doing.

## Bootstrap Rule

Migration work must respect project bootstrap ordering.

Current requirements:

- initialize the logger first
- initialize `Scanner` as early as possible
- initialize `Patterns` immediately after the scanner
- initialize the crash handler only after the pieces it depends on are ready
- terminate the crash handler last on process detach so shutdown crashes still produce artifacts

Do not move module-specific initialization earlier than the shared bootstrap unless there is a documented dependency reason.

## Shutdown Order Rule

Migration work must preserve a strict shutdown contract.

Important limit:

- do not invent new shutdown behavior for a migrated module unless legacy code already has it, or a project rule explicitly requires it
- if a stricter shutdown contract is required by project policy, document that as an intentional deviation instead of silently folding it into the migration

Required teardown shape:

1. mark the module as shutting down
2. stop new callback or hook-driven work from entering the module
3. disable hooks or restore patches
4. wait for in-flight work owned by that module to drain
5. remove hooks
6. delete synchronization primitives and clear pointers

Important consequences:

- do not destroy a critical section while a detour may still enter it
- do not clear trampoline or callback state before preventing new entries
- do not rely on a short sleep as a correctness boundary
- prefer module-local in-flight counters when global hook counts are too coarse

## Practical Speedups

These steps cut migration time the most:

- inspect the already-migrated sibling module first
- inspect the legacy usage graph before deciding whether something is a module or shared infrastructure
- port the smallest native surface that unblocks dependent code
- migrate shared helpers once, early, when they are clearly reusable
- document scan meaning at the call site instead of inventing abstractions during the port
- copy the crash-context pattern from an already-migrated sibling module instead of inventing one-off logging
- test one forced startup path and one shutdown path after each migration so attribution regressions are caught immediately

## Documentation Rules Learned During Migration

- do not add mojibake to project documentation
- keep migration notes in plain ASCII unless there is a clear reason not to
- when a rule already exists in another doc, reinforce it here if missing migrations keep violating it

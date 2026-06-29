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
- if the legacy component comes from `GWCA/GameEntities/`, treat it as shared GW context or entity code by default and migrate it under `GW/context/`

Use the name rule as the fast first pass, then confirm it from the legacy code.

## Refresh Context Rule

Before each new migration, refresh the migration context from the repo instead of relying on the previous port shape from memory.

Minimum refresh pass:

1. reread this migration guide
2. reread the project style guide
3. inspect the closest already-migrated sibling module
4. inspect the current shared GW context/type layout
5. confirm whether the new migration needs public module declarations, shared GW declarations, internal module declarations, or runtime methods

Do not carry forward assumptions from the previous migration if the current module has a different shape.

Hard gate:

- if the migration context refresh pass has not been completed in the current turn, do not make structural migration edits
- do not choose file placement by convenience
- do not choose file placement by copying the previous migration blindly
- treat the migration docs as a binding checklist, not loose guidance
- if a structural decision is not explicitly justified against the current docs and repo context, stop and reassess before editing

## Current Repo Baseline

This section defines the migration split from the repo as it exists now.

Do not infer new shapes beyond this baseline.

Observed baseline modules for this rule:

- `camera`
- `render`
- `game_thread`
- `effects`
- `context`

Observed baseline after cleanup:

- shared GW structs and helpers live under `GW/context/` instead of being embedded into whichever manager needed them first
- types, structs, public callable declarations, resolver declarations, hook callback declarations, packet helper structs, and module-owned base declarations belong in the main module header
- internal-only declarations stay in `.cpp` unless they need named shared ownership in a dedicated header

Strict interpretation for future migrations:

- do not invent arbitrary split shapes
- keep the split explicit and role-based
- use the main header for declarations and module-owned base
- treat every pattern-resolved symbol, offset, assertion-owned anchor, pointer scan, callsite scan, and patch site as part of the module-owned base
- do not add a `*_methods.h` layer
- only create additional headers beyond that when an existing documented subsystem-specific rule already requires them

## Target File Layout

For a migrated Guild Wars module, use this as the default layout.

Default manager split:

- `include/GW/<module>/<module>.h`
  All module-owned base declarations:
  lifecycle declarations, public callable declarations, structs, typedefs, function-pointer aliases, enums, callback types, globals, packet helper structs, resolver declarations, hook callback declarations, and the manager's assertion-owned symbol surface.
  This includes named ownership for every pattern-resolved function, offset, anchor, pointer reference, callsite, and patch site used by the module.
  Resolver logic itself belongs here too. If the module owns the resolution path, the resolver body is part of the header-owned base and should not be left only in the `.cpp`.
- `src/GW/<module>/<module>.cpp`
  Implementations for the manager-owned base declarations:
  internal functions, hook setup, patch setup, lifecycle orchestration, trampolines, and non-accessor implementation bodies that are not resolver definitions.
- `src/GW/<module>/<module>_methods.cpp`
  Definitions for callable public accessors and operations declared in the main module header.

This is the actual current pattern used by:

- `camera`
- `render`
- `game_thread`
- `effects`

If a manager is small, keep this split minimal and role-based.
Do not add more categories beyond this unless the repo already has that split for the same kind of subsystem.

Default result for an ordinary manager migration:

1. `include/GW/<module>/<module>.h`
2. `src/GW/<module>/<module>.cpp`
3. optional `src/GW/<module>/<module>_methods.cpp`

That is the expected output.
If the module only exposes lifecycle and no real callable surface, the methods file may be omitted.
Do not treat additional files beyond this as available design space by default.

Important limit:

- do not explode a tiny manager into many files beyond the default manager split
- do not create a `<module>_methods.h` for ordinary manager migration
- do not use `_methods` files for enums, typedefs, callback signatures, globals, structs, or other declarations
- if the module only exposes lifecycle and no real public callable surface, do not force methods files into existence
- if the module does expose callable accessors or operations, declare them in `<module>.h`
- do not declare module-local types, function pointer aliases, resolver declarations, hook declarations, packet helper structs, or module state directly inside anonymous `.cpp` scope just to avoid creating the right header split
- do not leave resolver bodies in `<module>.cpp` when the module owns those resolution steps
- do not leave a pattern-resolved symbol as unnamed `.cpp`-local scan logic when that symbol is part of the manager's owned runtime surface
- if a module needs extra declarations that are not public API and are not runtime methods, create an appropriate dedicated header for that module instead of burying them in the `.cpp`
- do not create many small files for a tiny manager unless an existing repo baseline for that exact subsystem already requires it
- do not split a manager just because another older migrated file already happens to be split

## Responsibility Split

Keep responsibilities separated consistently:

- `<module>.cpp` owns discovery and setup
- `<module>.cpp` owns internal-only helpers and private implementation details
- `<module>.h` owns resolver definitions, including the scan logic and assertions for module-owned symbols
- `<module>.cpp` may call those resolver definitions during setup, but should not be the only place they exist
- `<module>_methods.cpp` owns public callable definitions when that split exists
- additional implementation files only exist when a documented subsystem-specific split already requires them

Definition to enforce during migration:

- public callable function or method: any non-lifecycle function declared in `<module>.h` that another module may call directly
- this includes registration APIs, callback add or remove functions, accessors, getters, setters, queries, packet send or emulate entry points, and other user-callable operations
- private or internal helper: any routine only used to implement that module's internals, setup, teardown, hooks, synchronization, scans, or trampolines and not part of the module header surface

Important clarification:

- "method" in these docs does not only mean class member syntax
- free functions in a module namespace still count as methods for file-placement purposes when they are part of the public callable surface
- the placement rule is determined by call-surface ownership, not by whether the function looks small, looks like an accessor, or happens to live in a `.cpp` today
- pattern resolution ownership is determined the same way: if the module owns the resolved symbol, the declaration of that ownership belongs in the module header even though the actual scan executes in the `.cpp`
  This project now requires the stronger form: the resolver body itself also belongs in the module header, typically as `inline`.

That split does not authorize creating extra headers beyond the documented roles.

Do not misuse extra files as overflow storage for:

- enums
- callback typedefs
- registration APIs
- global state declarations
- internal hook bookkeeping

If those declarations are part of the module's real public API, place them in `<module>.h`.

If the declaration is a public callable function or accessor, place it in `<module>.h`.

If the body implements a public callable function declared in `<module>.h`, place that body in `<module>_methods.cpp` when that file exists.

If they are internal-only implementation details but still need named types, aliases, or shared module state declarations, place them in a dedicated module header instead of anonymous `.cpp` scope.

Context must not be omitted when making that decision:

- shared GW declarations used across managers belong under `GW/context/` when they are part of shared GW context/type code
- module-specific declarations belong under that module's header split
- `.cpp` files should primarily contain function bodies that implement the structural declarations established in the header
- header placement decisions must be made explicitly before editing, not inferred afterward from what compiles

Observed baseline from current repo:

- declarations live in headers
- structural declarations live in `<module>.h`
- public callable functions are declared in `<module>.h`
- public callable definitions may live in `<module>_methods.cpp`
- lifecycle and hook ownership live in `<module>.cpp`
- shared GW declarations live in `GW/context/`
- small modules should not be split beyond the documented manager pattern

Examples of code that belongs in `<module>.cpp`:

- `Scanner::Find(...)`
- `Scanner::FindAssertion(...)`
- `Scanner::ToFunctionStart(...)`
- dereferencing located pointers
- creating hooks
- selecting patch sites
- private hook trampolines
- internal wait logic
- private lifecycle helpers
- synchronization helpers that are not part of the public API such as `SafeInitializeCriticalSection(...)`

Examples of code that may belong in `<module>_methods.cpp`:

- getters and setters over resolved game structures
- simple state queries
- callback registration functions that are part of the public callable surface
- state query helpers
- small accessors over resolved pointers
- user-callable operations exposed by the module
- namespace-scope free functions that are declared in `<module>.h` for other modules to call

Concrete `stoc` example:

- `RegisterPacketCallback(...)`, `RegisterPostPacketCallback(...)`, `RemoveCallback(...)`, `RemoveCallbacks(...)`, `RemovePostCallback(...)`, and `EmulatePacket(...)` are public callable methods because they are declared in `stoc.h` and are intended to be called by other modules
- those bodies belong in `stoc_methods.cpp` when the `stoc` split uses a methods file
- `SafeInitializeCriticalSection(...)`, scan resolution, hook install or uninstall logic, detours, trampolines, and lifecycle-only helpers remain in `stoc.cpp`

Examples of code that does not belong in anonymous `.cpp` scope:

- function pointer typedefs or aliases used across the module
- callback entry structs
- exported or shared module globals
- state declarations that deserve named ownership and reviewable structure
- packet or UI-message helper structs used across the module
- resolver declarations and hook callback declarations used across the module
- unnamed `Patterns::Get(...)` fetches for manager-owned symbols
- unnamed assertion-owned scan anchors for manager-owned symbols
- unnamed direct statement blocks that resolve offsets or callsites without a matching header-declared ownership point

Required explicitness for resolved symbols:

- if the module resolves `SendAgentDialog_Func`, `CameraPtr`, `AgentArrayPtr`, or any equivalent symbol, that symbol's ownership must be visible from the module header through a named declaration surface
- the `.cpp` may execute `Patterns::Get(...)`, `Find(...)`, `FindAssertion(...)`, `ToFunctionStart(...)`, `FunctionFromNearCall(...)`, pointer dereferences, and address assertions
- but the existence of that resolution path must not be discoverable only by reading the `.cpp`
- header review should make it obvious which manager-owned symbols exist and which resolver functions own them
- do not hide assertion-backed or pattern-backed resolution behind anonymous `.cpp` blocks
- do not hide resolver bodies in `.cpp` when parity requires the header to carry the manager-owned base

Examples of modules that should usually stay with declarations in a single header:

- modules whose public surface is only `Initialize()` and `Shutdown()`
- modules with no public callable methods beyond lifecycle

If you cannot point to an explicit documented reason for extra files, do not create them.

Explicit anti-regression rule:

- do not create a `*_methods.h` layer for ordinary managers
- do not use `_methods` files for structural declarations
- do not split a module into `types`, `internal`, `symbols`, `module`, or similarly named fragments beyond the documented manager pattern
- if the repo was previously left in a half-merged state, finish restoring the documented split before starting another migration

## Non-Inference Rule

For migration structure, do not invent a shape from taste, convenience, or abstract cleanliness.

Use only:

- the current repo baseline
- the current migration docs
- the current shared `GW/context` layout
- the specific legacy source being migrated

Do not generalize from the UI migration to non-UI managers.
UI is a large subsystem with its own documented split and is not the default template for small managers.
Do not generalize from older wrong splits or temporary compacting passes.
Use the documented split roles directly.

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

Not allowed under parity migration unless explicitly documented as a deviation:

- renaming legacy-local helpers, callbacks, or internal routines when the rename is not required by project integration
- changing helper signatures or parameter shapes when the legacy form already fits the project
- folding several legacy steps into a new helper split that makes one-to-one review harder
- "cleanup" refactors that make migrated code less directly comparable to the legacy source

If a migration needs behavior that does not exist in legacy code, it must be treated as a separate intentional deviation and called out explicitly instead of being folded into the parity port.

Practical enforcement:

- if code is missing because another legacy file has not been migrated yet, stop and migrate that dependency first
- do not synthesize replacement behavior in the current manager to close that gap
- do not add compatibility helpers, timing helpers, drain helpers, or convenience methods unless they already exist in migrated legacy code or are required by an explicit project rule
- parity review must be able to point from migrated code directly back to legacy source without crossing invented glue
- parity review must not need a rename table for local helpers just to understand which migrated routine corresponds to which legacy routine
- when a legacy helper name and signature can be kept, keep them

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
- do not auto-migrate a reduced or "minimum" version of a missing shared prerequisite unless the migration request explicitly includes that prerequisite work
- when a missing prerequisite is discovered during a manager migration, stop the manager port and surface the exact prerequisite to the user for direction

Parity must remain traceable:

- every non-project adaptation used by a migrated manager should already exist in migrated legacy code
- if the source behavior comes from another legacy file, that file should exist in the repo before the manager consumes it
- parity assessment should compare migrated code against the real legacy implementation, not against compatibility glue added during porting

Working interpretation:

- missing shared structs means migrate the shared structs first
- missing shared helper methods on GW types means migrate those shared helper methods first
- missing manager-owned behavior means migrate that upstream manager or source file first if parity depends on it
- the correct answer to a missing dependency is never "add a temporary local substitute"
- the correct answer to a missing dependency is also not "silently port the smallest possible prerequisite anyway"

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
- shared GW protocol declarations, packet layouts, opcodes, and similar cross-manager protocol/type surfaces with no standalone lifecycle: `include/GW/common/`

Default source-to-destination rule:

- legacy `GWCA/Managers/...` files default to manager migration under `GW/<module>/`
- legacy `GWCA/GameEntities/...` files default to shared entity or context migration under `GW/context/`
- legacy packet and protocol declaration files default to `GW/common/`

Examples:

- `Skill.cpp` shared helper behavior belongs under shared GW context/type code, not as a fake manager
- `Effect` and `SkillbarSkill` helper methods belong under shared GW context/type code, not inside `EffectMgr`
- `WorldContext` layout belongs under shared GW context code, not inside a manager implementation
- shared GW container types such as `GwArray` belong under shared GW context/type code, not at the top of `GW/`
- `GameEntities/Friendslist.h` belongs under shared GW context code, not inside `FriendListMgr`
- `Packets/Opcodes.h` and `Packets/StoC.h` belong under shared GW common declarations, not inside `StoCMgr`

Important:

- do not create a module-style folder just because a legacy file has behavior
- if the code has no `Initialize()`/`Shutdown()` ownership, it is not a module
- do not treat `GameEntities` headers as manager-owned just because a manager is their first current consumer
- shared GW type behavior should be placed by domain, not by whichever manager happened to need it first
- if a manager needs shared GW type behavior, migrate that shared code first instead of embedding a local substitute in the manager
- do not bypass `GW/context/` just because a module can technically compile with local declarations
- when a declaration is part of shared GW context/type structure, `GW/context/` is the default home and that decision should be revisited before every migration
- when a declaration is a shared GW protocol or packet surface rather than runtime memory/context structure, `GW/common/` is the default home and that decision should be revisited before every migration

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

Explicit declaration rule for pattern resolution:

- whenever a module resolves patterns, assertions, offsets, anchors, pointer references, callsites, or patch sites, that resolver ownership must be represented in the module header
- do not treat `Patterns::Get(...)` plus a local variable in `.cpp` as sufficient structure by itself
- the resolver body itself should live in the header when it belongs to the module-owned base
- the `.cpp` may invoke that resolver during initialization, but should not be the sole home of the resolver implementation
- review of `<module>.h` should expose the manager's full assertion-owned symbol surface without requiring a second pass over implementation details
- if a scan block cannot be described cleanly from the header, the module split is still wrong

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
8. Confirm before editing that the chosen file split matches the refreshed migration context and not just the previous port
9. Confirm after editing that no stale includes or stale source file registrations remain from an older split
10. Confirm that public callable declarations are in the main header
11. Build immediately so the repo is not left in a half-migrated structural state

For shared infrastructure:

1. Place the code under `include/base/` and `src/base/` unless there is already a better shared home
2. Wire any global enable and disable behavior into the top-level lifecycle such as `gw::Initialize()` and `gw::Shutdown()`
3. Avoid inventing a synthetic module wrapper unless the legacy project had one

## Legacy Parity Notes

Document inactive legacy pieces before porting them blindly.

Also document every intentional deviation from legacy behavior.

Do not use undocumented compatibility code to keep a migration moving. If the migrated manager only works because of newly invented glue, parity has not been established yet.

Do not hide parity drift behind harmless-looking cleanup. Even local helper renames can impede parity checking if they break direct source-to-source correspondence.

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
- no deleted header or source file is still referenced
- `Initialize()` is idempotent
- `Shutdown()` is safe when partially initialized state exists
- hooks or patches restore original bytes on teardown
- no Python surface was added unless explicitly requested
- crashes during startup or shutdown identify the active module and operation in the crash sidecar

Minimum structural validation after any file-layout change:

- search for stale includes to removed headers
- search for stale references in `CMakeLists.txt`
- verify the final shape still matches the documented manager split
- verify callable public functions are declared in the main header
- verify types, aliases, globals, and structs remain in the main header
- do not move on to another migration until that cleanup is complete

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

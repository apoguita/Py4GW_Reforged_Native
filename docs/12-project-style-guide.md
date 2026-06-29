# Project Style Guide

This document defines mandatory conventions for `Py4GW_Reforged` project-owned code.

## Core Include Policy

- Every project-owned header must include `base/error_handling.h` immediately after `#pragma once`.
- Build targets must force-include `base/error_handling.h` so new translation units inherit the panic layer even if a local include is missed.
- New shared macros or fatal-runtime policy belong in `base/error_handling.h` or headers it owns.
- Third-party code under `third_party/` is excluded from this rule.

## Fatal Error Policy

- Fatal internal failures must use the `PY4GW_*` panic/assert macros.
- Process-wide crash capture is a required subsystem, not an optional utility.
- Do not duplicate panic registration logic inside feature modules.

## Pattern And Hooking Policy

- Addresses and hooks must prefer the JSON-backed pattern system.
- Validate resolved addresses with project assertions before installing hooks.
- Avoid hardcoded addresses unless the module explicitly documents why they are unavoidable.
- If a module owns a resolved symbol, offset, anchor, pointer reference, callsite, or patch site, that ownership must be declared from the module header instead of being discoverable only from `.cpp` scan code.
- `Patterns::Get(...)` usage in `.cpp` must implement a header-declared resolver or named module-owned symbol surface, not replace it.
- Resolver bodies for module-owned symbols belong in the module header as part of the module-owned base. The `.cpp` may call them, but should not be their only implementation site.

## Logging Policy

- Use the project logger for runtime diagnostics.
- Use panic/assert macros for invariant failures, not plain log lines.
- If a failure must stop the process, it is not a logger-only event.

## ImGui And UI Policy

- Keep UI composition in dedicated UI functions instead of embedding large trees directly into render plumbing.
- Addons and demos must stay separated so dependency tracking remains explicit.
- Project UI code may demo functionality, but demos must not redefine core runtime policy.

## Porting Policy

- Port behavior intentionally; do not copy foreign modules blindly.
- Refit imported code to this project's scanner, logger, lifecycle, and panic systems.
- Preserve the smallest dependency surface needed for the current project stage.
- Refresh the migration context before each new port: reread the migration docs, inspect the closest migrated sibling, and inspect the current `GW/context` and shared declaration layout before deciding file shape.
- If the migration context refresh has not been completed in the current turn, do not make structural migration edits.
- Base migration structure on the current repo baseline, not on inference or cleanup preferences.
- Prefer the smallest file count that still matches the repo baseline and keeps the code readable.
- For ordinary managers, keep structural declarations, module-owned base declarations, and public callable declarations in the main module header.
- Do not add functionality or behavioral redesign during migration unless an existing project doc explicitly requires it.
- If a migration must intentionally differ from legacy behavior, document that deviation explicitly instead of mixing it into parity work.
- Do not rename legacy-local helpers, callback routines, or internal function shapes during parity migration unless the rename is required by project integration and the deviation is called out.
- Preserve legacy helper names and signatures when they already fit the repo, because parity review should stay source-to-source and not require a manual correspondence map.
- Shared GW structs and helper methods must live in shared GW locations such as `GW/context/`, not in fake module folders.
- Legacy `GWCA/GameEntities/...` files default to `GW/context/` migration because they are shared runtime entity or context declarations, not manager-owned behavior.
- Shared GW protocol declarations such as packet layouts and opcode tables should live in a shared GW location such as `GW/common/`, not in manager folders or `base/`.
- Legacy `GWCA/Managers/...` files become managers; legacy `GWCA/GameEntities/...` files become shared context or entity declarations; packet and protocol declaration files become `GW/common/`.
- If a migrated manager depends on unmigrated shared legacy code, migrate that prerequisite first instead of adding a compatibility shim or local substitute.
- Do not use temporary glue to hide missing legacy dependencies; parity work must stay directly traceable to migrated legacy code.
- Do not silently convert a manager migration request into an unsolicited reduced prerequisite migration.
- If a missing prerequisite is discovered and the request did not explicitly ask to migrate it too, stop and surface the prerequisite clearly so the next migration decision is intentional.
- Do not create `_methods.h` files for ordinary managers.
- Use `_methods.cpp` only for public callable method bodies when that split is already useful for readability.
- Do not use `_methods` files as storage for enums, callback types, typedefs, globals, structs, or other declarations.
- Do not bury named module types, function pointer aliases, or module state declarations inside anonymous `.cpp` scope; give them an appropriate header.
- Do not omit `GW/context` from the migration decision when shared GW declarations are involved.
- Do not make file-shape decisions by convenience or by reusing the previous migration without revalidation.
- Do not generalize the UI split to small managers or unrelated subsystems.
- Do not explode a small manager into many files unless the current repo baseline already uses that split for the same subsystem class.
- For a small manager with public callable methods, the default output is `<module>.h`, `<module>.cpp`, and optionally `<module>_methods.cpp`.
- If the manager has no public callable methods beyond lifecycle, the methods file may be omitted.
- Put callback types, typedefs, aliases, globals, registration APIs, packet helper structs, resolver declarations, hook callback declarations, and structural declarations in the main module header.
- Put declaration ownership for pattern-resolved functions, assertion-backed anchors, pointer scans, offsets, and patch sites in the main module header too.
- Put public callable function declarations in the main module header.
- Put setup, assertions, hooks, and private implementation bodies in the main module `.cpp`, but declare the manager-owned base they implement in the main module header.
- Put resolver definitions themselves in the main module header when they resolve module-owned symbols.
- Put public callable method bodies in the methods `.cpp` when that split exists.
- Do not leave code like `const auto* pattern = PY4GW::Patterns::Get(...)` or assertion-anchor fetches as unnamed implementation-only structure when they resolve manager-owned symbols; the header must expose the resolver ownership explicitly.
- Treat any function declared in `<module>.h` for use by other modules as a public callable method even if it is a namespace-scope free function rather than a class member.
- Do not decide "method" versus "private helper" by whether the function looks like a getter; decide it by whether it is part of the module's callable surface.
- For example, `stoc` functions such as `RegisterPacketCallback`, `RegisterPostPacketCallback`, `RemoveCallback`, `RemoveCallbacks`, `RemovePostCallback`, and `EmulatePacket` are methods and belong in `stoc_methods.cpp` when that file exists.
- Internal helpers such as `SafeInitializeCriticalSection`, scan setup, hook wiring, trampolines, and lifecycle-only routines remain in `<module>.cpp`.
- If shared GW types or helpers are needed, place them in `GW/context/` first instead of burying them in the manager.
- If a migration changes a split, complete the cleanup fully in the same pass: fix includes, fix build registration, and rebuild.
- Do not start a new migration while the repo is left in a half-migrated structural state.

## Documentation Encoding Policy

- Project documentation should use plain ASCII unless there is a clear reason not to.
- Do not introduce mojibake or mixed-encoding text into `docs/`.
- Treat mojibake as a documentation defect and fix it when found.
- When importing text from other sources, normalize punctuation and encoding before committing it.

## Review Checklist

- Does the header include `base/error_handling.h`?
- Are fatal invariants using `PY4GW_ASSERT`, `PY4GW_REQUIRE`, or `PY4GW_PANIC`?
- Does hook resolution go through project scanning and assertion paths?
- Does the module header make the owned pattern-resolution surface explicit, or are offsets and anchors hidden in unnamed `.cpp` scan blocks?
- Are resolver bodies for module-owned symbols actually in the header, instead of merely declared there?
- Does the migration avoid adding new behavior beyond documented project rules?
- If behavior changed intentionally, is the deviation documented explicitly?
- If local helper names or signatures changed, was that actually required and documented?
- If the migrated code depends on legacy shared behavior, was that prerequisite migrated first instead of replaced with a shim?
- Does the file split make sense, or was a fake `_methods` layer added without real methods to justify it?
- Did the migration keep the file count as small as practical for the module size?
- Were structural declarations kept in the main module header?
- Were public callable methods declared in the main module header?
- If a methods file exists, were bodies for public callable functions moved there instead of being left in the main module `.cpp`?
- Are named module types/state declared in the right header instead of hidden in anonymous `.cpp` scope?
- Was the migration context refreshed in this turn before structural edits were made?
- Does the split match the current repo baseline, instead of a new inferred pattern?
- If files were merged or removed, were stale includes and stale `CMakeLists.txt` entries cleaned up immediately?
- Is third-party code isolated from project-owned policy changes?
- Is the module documented if it introduces a new subsystem rule?
- Does the documentation remain free of mojibake and mixed-encoding artifacts?

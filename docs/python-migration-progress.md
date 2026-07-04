# Python Migration - Living Progress Log

Purpose: the always-current pick-up point for the Python-side migration onto the
reforged native backend. If a session dies or something regresses, START HERE.
Updated continuously as work happens - every unit migrated, every decision, every
blocker is recorded below.

Companion docs:
- `python-migration-layers-analysis.md` - the three-layer survey (bindings, shared
  memory, native contexts) and the module reconciliation table.
- `python-migration-restart-handover.md` - the reset history and hard rules.

Projects:
- Python (being migrated): `C:\Users\Apo\Py4GW_Reforged` (Py4GWCoreLib). HEAD `81593e2a`.
- Reforged native (backend): `C:\Users\Apo\Py4GW_Reforged_Native`.
- Legacy references: `C:\Users\Apo\Py4GW` (native), `C:\Users\Apo\Py4GW_python_files` (python).

## Ground rules (do not violate)

1. NO compatibility shims. No alias modules, no compat wrapper classes, no
   legacy-compat structures. Change the Python code to make the correct new native
   calls directly. If the proper call needs native C++ that does not exist, that is
   a native-build blocker to surface - not a Python stand-in.
2. This phase is a compatibility FIX so the Python side starts functioning; a full
   migration to the new system comes later. Do not over-reach into the full
   redesign now.
3. No auto builds. The user builds and injects manually and confirms. Verification
   of "it works" is gated on the user, not on me.
4. Incremental only. One unit at a time, verify, then continue. No broad/blind
   codemods or AST sweeps (that is what caused the prior revert).
5. NEVER touch the native callback system (`src/callback/callback.cpp`,
   `PyCallback::ExecutePhase`). Off-limits; crash stacks there are symptoms.
6. Do not assume a module is "missing" - the rewrite renamed/folded/retired many.
   Verify against reforged native (and legacy) before concluding.
7. Keep THIS doc current as work proceeds.

## Migration model and attack order (owner-stated)

Three Python-side data layers: bindings, shared memory, native contexts.
Order: (1) bindings first, (2) pointer sourcing (pypointers -> shared memory),
(3) contexts then load and validate.

## Current status

- Phase: IMPLEMENT (binding layer). Design spec complete + adversarially verified
  (workflow `wf_a58a07cc-6ba`, 15/15 modules, 0 misclassifications). Per-module
  specs saved under the session scratchpad `specs/<Module>.json`.
- Done this session (Python-only, compile-verified via py_compile; NOT yet runtime-
  verified - see critical path): PyKeystroke, PyOverlay, Py2DRenderer->PyDXOverlay,
  PyCombatEvents->PyAgentEvents.
- Done this session (NATIVE, authored - awaits owner CMake reconfigure + 32-bit
  rebuild + inject): PySkill embedded module. Plus Python-only: removed dead
  `from PyAgent import AttributeClass` (Skillbar.py) - the next import-time blocker
  after PySkill.

## PySkill native build (owner action required)

New/edited native files:
- `include/GW/skillbar/skill_names.h` - name-lookup declarations.
- `src/GW/skillbar/skill_names.cpp` - GENERATED tables (3031 skills / 29 types / 11
  professions) from the reforged `GW::Constants` enums. Regenerate if those enums
  change (generator logic in session history; parses skills.h/constants.h).
- `src/GW/skillbar/skill_bindings.cpp` - `PYBIND11_EMBEDDED_MODULE(PySkill)`:
  SkillID / SkillType / SkillProfession / Skill, reproducing the legacy py_skills
  surface over `GW::Context::Skill` (via `GW::skillbar::GetSkillConstantData`).
- `src/base/python_runtime.cpp` - added `"PySkill"` to kEmbeddedModules.

Because new .cpp files were added, CMake must RECONFIGURE then do a full 32-bit
build (GLOB_RECURSE). After inject, sanity checks:
- `import Py4GWCoreLib` no longer fails at PySkill.
- `PySkill.Skill("Energy_Drain").id.id == 79`; `PySkill.Skill(79).id.GetName() == "Energy_Drain"`.
- `PySkill.Skill(id).type.GetName()` and `.profession.GetName()` return expected strings.
- `PySkill.Skill(id).energy_cost` reflects the 11->15 / 12->25 remap.

## CRITICAL PATH (import blocker)

`Py4GWCoreLib/__init__.py:46` does `import PySkill`. PySkill has NO native module
yet (only `GW::Context::Skill` data exists in C++). Until a `PySkill` binding is
built natively and the DLL rebuilt, `import Py4GWCoreLib` raises ModuleNotFound at
line 46 and NOTHING in the library loads. Therefore the build-free Python edits
already made cannot be runtime-verified by re-inject until PySkill (native +
rebuild) lands. PySkill is the first native-binding unit to build.

## Injection test results (2026-07-04, first coupled run after PySkill build)

Owner rebuilt with PySkill + the earlier Python-only work and injected. Progress
was large: ALL 37 embedded modules imported (incl. new PySkill, PyAgentEvents,
PyDXOverlay, PyKeystroke), all GW modules initialized, shared memory published,
crash handler + ImGui up, "Py4GW initialized". Then Py4GWCoreLib import failed.
Errors found and fixed this session:

1. `ModuleNotFoundError: No module named 'PyPointers'` at AccAgentContext.py:1
   (chain __init__ -> Map -> Context -> AccAgentContext). FIXED (Python-only, no
   rebuild): removed the dead top-level `import PyPointers` from 11 context files
   and dead `import PyParty` (PartyContext) / `import PyPlayer` (ServerRegion) - all
   verified 0 live uses; pointers already come from shared memory. All 13 compile.
2. Script host `main()`/`draw()` bug (python_runtime.cpp ExecuteDraw): draw() ran
   as a fallback that RETURNED before main(), so main() was skipped whenever draw()
   existed. FIXED (native, needs rebuild): ExecuteDraw now calls BOTH draw() and
   main() every draw frame, ungated (equal priority). update() stays update-loop
   only (ExecuteUpdate). Matches the owner rule: update->update loop; draw+main
   both run ungated in draw.
3. Font "missing": friz-quadrata-std-bold-italic (BoldItalic, 6 sizes). Root cause:
   font_manager.cpp requested the HYPHEN name `...bold-italic...` but the repo
   tracks the UNDERSCORE asset `...bold_italic...` (legacy FontManager.h also used
   underscore). FIXED (native, needs rebuild): font_manager.cpp now uses the
   underscore name matching the tracked asset. Owner can delete the untracked
   hyphen duplicate they added as a workaround.

Next expected errors after these: call-time API-shape mismatches (PyAgent/PyItem/
etc.) as code paths execute - the error-by-error phase.

## Binding-layer ledger (status per module)

Status values: SPEC (spec drafted) / READY (verified, plan set) / IN-PROGRESS /
DONE-PENDING-INJECT (edited, awaiting user build+inject confirm) / DONE / BLOCKED.

Verified classification (all confirmed by adversarial pass). "py-only" = fixable
with Python edits + re-inject; "native" = needs C++ binding work + your rebuild.

| Module | Verified class | Kind | Status | Notes |
|---|---|---|---|---|
| PyKeystroke | NAME_FIX | py-only | DONE-PENDING-INJECT | `PyScanCodeKeystroke`->`PyKeyHandler`, Keystroke.py |
| PyOverlay | NAME_FIX | py-only | DONE-PENDING-INJECT | Point2D/3D->Vec2f/3f across Overlay/DXOverlay/Map/UIManager |
| Py2DRenderer | RE_HOMED -> PyDXOverlay | py-only | DONE-PENDING-INJECT | DXOverlay.py + __init__ import/re-export |
| PyCombatEvents | RE_HOMED -> PyAgentEvents | py-only | DONE-PENDING-INJECT | CombatEvents.py rewritten to free-fn API; tester deferred |
| PyInventory | API_SHAPE_REWRITE | py-only | TODO | Inventory.py + caches; Item.py/ItemArray.py Bag calls deferred to PyItem |
| PySkill | NEEDS_NATIVE_BINDING | native | DONE-PENDING-BUILD | module authored (3 new files + registration); names generated from reforged enums, NOT legacy table |
| PyAgent | NEEDS_NATIVE_BINDING | native+py | PARTIAL | py: removed dead AttributeClass import (done). native: get_agent_enc_name must return raw bytes (C++, still TODO) |
| PyItem | NEEDS_NATIVE_BINDING | native | BLOCKED | build PyItem data class over GW::Context::Item (~45 uses) |
| PyEffects | NEEDS_NATIVE_BINDING | native | BLOCKED | get_effects/get_buffs/EffectType/BuffType unbound |
| PyMerchant | NEEDS_NATIVE_BINDING | native | BLOCKED | |
| PyQuest | NEEDS_NATIVE_BINDING | native | BLOCKED | PyQuest class + 21 methods + QuestData + async cache |
| PyCamera | NEEDS_NATIVE_BINDING | native | BLOCKED | set_yaw/pitch/pos/look_at + needs Camera field in shared memory + CameraContext reader |
| PySkillbar | NEEDS_NATIVE_BINDING | native | BLOCKED | GetPlayerSkillbar/GetHeroSkillbar/GetHoveredSkill unbound; HeroUseSkill must be authored |
| PyParty | API_SHAPE_REWRITE | native+py | PARTIAL-BLOCKED | mostly py free-fn rewrite; UseHeroSkill needs native binding |
| PyPlayer | RE_HOMED_REPOINT | native+py | PARTIAL-BLOCKED | mostly py repoint; chat-history trio needs native binding |

## Shared memory / pointer sourcing (workstream 2 - not started)

- System A `Pointers_SHMemStruct` (15 context base pointers) is the new pointer
  source. Open task: confirm layout/field parity byte-for-byte with the C++ writer;
  confirm every field is populated.
- Deviations to resolve: `AgentContext` and `InstanceInfoContext` still use native
  pattern scans instead of shmem.

## Native contexts (workstream 3 - not started)

- 13/17 already read base pointer from shmem. Dead `import PyPointers`/`PyParty`/
  `PyPlayer` lines at top of context files must be removed (import-time crash risk).
- Validate ctypes struct offsets against native struct layouts once bindings +
  pointer sourcing are correct.

## Quality bar (owner-stated)

- Depth over breadth per unit. A module marked DONE must be ACTUALLY solved: every
  call site in that wrapper moved to the real native surface, each new call
  cross-checked against the binding, no dangling legacy symbols left behind.
- It is acceptable if the first pass does not resolve everything. It is NOT
  acceptable to skim: do not mark something solved that is only superficially done.
- If a unit cannot be fully solved this pass, mark it BLOCKED with the concrete
  reason (e.g. needs a native binding built) - never "mostly done".
- Two phases: (1) migrate the structure now, as much as can be done correctly and
  autonomously; (2) later, address remaining errors one by one during build+inject
  triage with the owner.

## Decisions log

- 2026-07-04: This phase is a compatibility fix, not the full new-system migration.
  No shims permitted (owner, strict).
- 2026-07-04: Use workflows only for read-only design/analysis; all edits stay
  incremental in the main loop with user build/inject verification.
- 2026-07-04: Owner sanctions autonomous structural migration of the wrappers;
  quality bar above governs what counts as DONE.

## Deviations (documented parity notes)

- CombatEvents.py: legacy `CombatEventQueue.SetMaxEvents` had no reforged native
  equivalent (fixed 1000-entry ring buffer) and no callers -> REMOVED rather than
  kept as an inert vestige. `GetMaxEvents` now returns `PyAgentEvents.get_capacity()`.
  `CombatEventQueue.GetQueue()` removed (no queue object in the new free-fn API).
- Widgets/Coding/Debug/Guild Wars/CombatEventsTester.py still calls
  `PyCombatEvents.GetCombatEventQueue()` directly (debug widget, outside Py4GWCoreLib).
  Deferred; migrate to `PyAgentEvents.*` when touching debug widgets.
- PySkill: skill/type/profession names are GENERATED from the reforged
  `GW::Constants` enums, NOT re-ported from legacy `SkillArray.cpp`. Rationale: the
  reforged enums are the authoritative id source (GetSkillConstantData indexes by
  the enum value); re-porting the legacy table would be a forbidden legacy-compat
  structure and could diverge from the enum ids. Risk: if a legacy name string
  differs from the reforged enum identifier for a skill some caller passes by name,
  that name->id lookup returns 0 - surface in error-by-error triage.
- PySkill: `.attribute` exposed as a raw int (legacy exposed an AttributeClass
  object). No in-repo consumer calls a method on `.attribute`; add a wrapper if a
  `.GetName()` caller appears during triage.
- PySkill placement: binding + name tables live under `GW/skillbar/` (nearest
  module; owns GetSkillConstantData), namespace `GW::skillbar`. Skill constant data
  itself is `GW::Context::Skill`.

## Next action / pick-up point

Two tracks remain:

A) Finish the remaining py-only unit: PyInventory (Inventory.py + InventoryCache.py
   + ItemCache.py) using the get_bag dict snapshots + PyItem/PyInventory free
   functions from `specs/PyInventory.json`. Defer Item.py/ItemArray.py Bag calls to
   the PyItem unit so those files are not left half-migrated.

B) Native binding work (needs owner rebuild). Recommended order, PySkill FIRST
   because it unblocks the whole import:
   1. PySkill  - AUTHORED; awaiting owner CMake reconfigure + 32-bit rebuild +
      inject to confirm import unblocks and sanity checks pass.
   2. PyAgent  - change get_agent_enc_name to return raw bytes (vector<uint8_t>).
   3. PyItem, PyEffects, PyMerchant, PyQuest, PySkillbar, PyCamera (+ shared-memory
      Camera field), PyParty.UseHeroSkill, PyPlayer chat-history.
   Each: author binding over existing GW::* methods (no shim), then owner rebuilds +
   injects, then error-by-error triage.

Per-module edit plans: session scratchpad `specs/<Module>.json` (call_mapping +
spec_markdown). DECISION NEEDED from owner: do the native bindings as one batch for
a single rebuild, or module-by-module (PySkill first) with a rebuild each.

# Python Migration - Three-Layer Analysis (current state)

Date: 2026-07-04. Draft for review.

Scope: document the Python project (`C:\Users\Apo\Py4GW_Reforged\Py4GWCoreLib`)
as it exists now, across the three data layers it uses to reach game state, and
map each layer against the reforged native project
(`C:\Users\Apo\Py4GW_Reforged_Native`). This is the pre-migration survey; it
describes current reality and the adaptation each layer needs. It does not change
any code.

Reference legacy projects: `C:\Users\Apo\Py4GW` (native/C++ legacy),
`C:\Users\Apo\Py4GW_python_files` (python legacy).

## The core architectural shift

In legacy Py4GW the native DLL handed raw game-object pointers to Python through a
binding module (`PyPointers`, plus per-manager `Get*ContextPtr()` accessors). The
context classes cast those addresses onto ctypes structs.

In the reforged native project the C++ side **no longer shares pointers through
bindings**. Every base pointer is now published into a **shared memory** block
that Python attaches to and reads. Shared memory carries more than pointers - it
also carries copied-out struct snapshots (agent array, and a separate legacy
multibox block).

So the three Python-side data layers are:

1. **Bindings** - the `Py*` embedded modules (callable native surface).
2. **Shared memory** - the block the native side publishes; Python attaches read-only.
3. **Native contexts** - `native_src/context/*.py`, ctypes struct views laid over
   pointers that (now) come from shared memory.

The migration, in the owner's stated order: fix the **bindings** first, then fix
**where pointers are sourced** (pypointers -> shared memory) so the **contexts**
load, and it is almost ready.

---

## Layer 1 - Bindings

The Python library imports `Py*` modules and calls into them. The native rewrite
changed most of these to some extent. Key correction: **do not treat a module the
Python side imports but the native list lacks as "missing"** - in every case so
far it was renamed, folded into another module, or intentionally retired. Verify
against the native project (and legacy) before concluding.

### Authoritative native embedded-module list (39)

Py4GW, PySystem, PySettings, PyProfiler, PyCallback, PyListeners,
PyAgent, PyAgentEvents, PyAgentRecolor, PyCamera, PyChat,
PyDXOverlay, PyDialog, PyEffects, PyFriendList, PyGameThread, PyGuild,
PyImGui, PyInventory, PyItem, PyKeystroke, PyListeners, PyMap,
PyMerchant, PyMouse, PyNameObfuscator, PyOverlay, PyPacketSniffer,
PyParty, PyPathing, PyPing, PyPlayer, PyProfiler, PyQuest, PyRender,
PyScanner, PySettings, PySkill, PySkillbar, PySystem, PyTexture,
PyTrade, PyUIManager.

### Reconciliation of the modules Py4GWCoreLib imports

Categories: MATCH (compatible today), API-SHAPE (module exists but surface changed
from legacy class to flat `snake_case` free functions), NAME (specific symbol
renamed), RE-HOMED (functionality moved to a differently-named module),
RETIRED (removed by design).

| Python imports | Native reality | Category | Notes |
|---|---|---|---|---|
| PyScanner | `scanner_bindings.cpp` (`PyScanner` class) | MATCH | class + static methods preserved |
| PyImGui | `imgui/imgui_bindings.cpp` | MATCH | both function-based; ImGui 1.92.x adapted |
| PyPathing | `pathing_bindings.cpp` (`PathPlanner`, `PathStatus`) | MATCH | classes present |
| PyUIManager | `ui/ui_bindings.cpp` (`UIManager`, `UIFrame`, ...) | MATCH | present |
| PyAgent | `agent/agent_bindings.cpp` | API-SHAPE | native = flat `snake_case`; no `PyAgent.PyAgent` class |
| PyPlayer | `player/player_bindings.cpp` | API-SHAPE | no `PyPlayer` class |
| PyParty | `party/party_bindings.cpp` | API-SHAPE | free funcs; no `PyParty`/`Hero` classes |
| PyItem | `item/item_bindings.cpp` | API-SHAPE | only `ItemModifier` class; no `Item`/`PyItem` |
| PyInventory | `item/inventory_bindings.cpp` | API-SHAPE | no `Bag`/`PyInventory` class |
| PySkillbar | `skillbar/skillbar_bindings.cpp` | API-SHAPE | free funcs; no `Skillbar` class |
| PyMerchant | `merchant/merchant_bindings.cpp` | API-SHAPE | no class |
| PyEffects | `effects/effects_bindings.cpp` | API-SHAPE | no class |
| PyQuest | `quest/quest_bindings.cpp` | API-SHAPE | no class |
| PyCamera | `camera/camera_bindings.cpp` | API-SHAPE | no class; Camera field added to shared memory (2026-07-06) |
| PyKeystroke | `virtual_input/virtual_input_bindings.cpp` | NAME | class is `PyKeyHandler`; Python wants `PyScanCodeKeystroke` |
| PyOverlay | `overlay/overlay_bindings.cpp` | NAME | `Overlay` OK; `Point2D`/`Point3D` -> `Vec2f`/`Vec3f` (intentional) |
| PySkill | native skill DATA exists: `GW::Context::Skill` in `include/GW/context/skill.h`; binding in `skill_bindings.cpp` | MATCH | PySkill embedded module built 2026-07-04 |
| Py2DRenderer | migrated into `DXOverlay` (`src/overlay/dx_overlay.cpp`), bound as `PyDXOverlay` | RELOCATED | Python `Py2DRenderer.Py2DRenderer()` maps to PyDXOverlay |
| PyCombatEvents | migrated + reworked into `listeners/agent_events_listener.h`, bound as `PyAgentEvents` | RELOCATED | header notes it derives from legacy `CombatEventQueue` |
| PyPointers | replaced by shared memory (`Pointers_SHMemStruct`); 11/12 getters matched, 1 gap (AreaInfo) | RELOCATED | dead `import PyPointers` lines removed from contexts |
| PyDialogCatalog | folded into `PyDialog` (`read_dialog_*` accessors) | RELOCATED | Python import is already guarded try/except |

### Binding-layer implications

- API-SHAPE is the largest chunk of work: ~10 wrapper modules (`Agent.py`,
  `Player.py`, `Party.py`, `Item.py`, `Inventory.py`, `Skillbar.py`,
  `Merchant.py`, `Effect.py`, `Quest.py`, `Camera.py`) call a legacy class API
  that the native module no longer exposes. A per-module function-by-function
  signature diff is still required (not done here - agents deliberately stopped at
  module level to avoid a giant speculative diff).
- RELOCATED modules: Python wrappers must be re-pointed at the new native
  module name (`Py2DRenderer` -> PyDXOverlay, `PyCombatEvents` -> PyAgentEvents,
  `PyPointers` -> shared memory, `PyDialogCatalog` -> PyDialog).
- PyScanner and PyTrade are registered in kEmbeddedModules (2026-07-06).
- Camera shared memory field added to Pointers_SHMemStruct (2026-07-06).

---

## Layer 2 - Shared memory

There are two independent shared-memory systems in the Python tree. Only one
publishes game-context base pointers.

### System A - System Shared Memory (native-published, reforged)

- Manager: `native_src/ShMem/SysShaMem.py`, singleton `SystemShaMemMgr`
  (instantiated + `.enable()` at import).
- Block name from the native side: `Py4GW.Game.get_shared_memory_name()`
  (`SysShaMem.py:35,56`). Python attaches read-only: `SharedMemory(name=...,
  create=False)` (`SysShaMem.py:69`). It never creates the block - it consumes
  what C++ created.
- Layout: `[SharedMemoryHeader (24B)][AgentArraySHMemStruct][Pointers_SHMemStruct]`.
- `SharedMemoryHeader` (`SysShaMem.py:9`, `_pack_=1`): `version`, `total_size`,
  `sequence` (odd = write in progress), `process_id`, `window_handle` (u64).
- Read path `get_payload` (`SysShaMem.py:96`) is a **seqlock**: reads header,
  skips if `sequence` odd, copies out agent array + pointers with
  `from_buffer_copy`, re-reads header, retries on a torn read (up to 3x). Refreshed
  every `PyCallback.Phase.PreUpdate` at priority 0 (before context consumers run).

Structs (System A):

| Struct | File | Purpose | Context pointers? |
|---|---|---|---|
| `SharedMemoryHeader` | `SysShaMem.py` | seqlock header | no |
| `Pointers_SHMemStruct` | `native_src/ShMem/structs/PointersSSM.py` | **15 game-context base pointers** | YES (all) |
| `AgentSHMemStruct` | `structs/AgentArraySSM.py` | one agent: `ptr` (c_void_p) + `agent_id` | YES (per agent) |
| `AgentRefSHMemStruct` / `AgentRefArraySHMemStruct` | `structs/AgentArraySSM.py` | lightweight id/index ref arrays | no |
| `AgentArraySHMemStruct` | `structs/AgentArraySSM.py` | full array (`AgentArray[300]`) + categorized ref arrays (All/Ally/Enemy/Neutral/Living/Item/Gadget/...) | via entries |
| `AgentArraySHMemWrapper` | `structs/AgentArraySSM.py` | non-ctypes helper (`to_int_list`, `get_ally_array`, `get_agent_by_id`) | n/a |

`Pointers_SHMemStruct` fields (all `c_void_p`): MissionMapContext, WorldMapContext,
GameplayContext, InstanceInfo, MapContext, GameContext, PreGameContext,
WorldContext, CharContext, AgentContext, CinematicContext, GuildContext,
AvailableCharacters, PartyContext, ServerRegionContext.

This is the pointer source the contexts read. `AGENT_ARRAY_MAX_SIZE = 300`.

### System B - Py4GW Shared Memory (legacy multibox, Python-managed)

- Manager: `GlobalCache/SharedMemory.py`, singleton `Py4GWSharedMemoryManager`.
- Block name: `"Py4GW_Shared_Mem"` (`SHMEM_SHARED_MEMORY_FILE_NAME`). Unlike System
  A, Python **creates** it if absent, and reads with a **live** `from_buffer`
  overlay so writes persist.
- Root struct `AllAccounts` (`GlobalCache/shared_memory_src/AllAccounts.py`):
  per-slot tables `Keys[64]`, `AccountData[64]`, `Inbox[64]`, `HeroAIOptions[64]`,
  `Intents[64]`. Contains value snapshots (agent data, inventory, skillbar, buffs,
  attributes, titles, quests, faction, mission, messaging inbox, HeroAI options,
  whiteboard intent locks) produced by each client via `from_context()` methods.
- **No game-context pointers.** This is cross-process multibox coordination, not
  part of the pointer-sourcing migration. Left as-is unless it depends on a changed
  binding (e.g. `from_context()` reading a context that must first be fixed).

### Shared-memory implications

- System A is the migration-critical one: its `Pointers_SHMemStruct` is exactly the
  new pointer source. Confirm the native side actually populates every field and
  that the Python struct layout matches the C++ writer byte-for-byte (order + types).
- The seqlock read is already implemented Python-side; the concern is layout/field
  parity with the C++ publisher, not the read mechanism.

---

## Layer 3 - Native contexts

`native_src/context/*.py` - 17 files. Each defines ctypes Structure(s) and a facade
class with `_update_ptr()` registered on `PyCallback.Phase.PreUpdate`, resolving a
base pointer and doing `cast(ptr, POINTER(<Struct>)).contents`. Only the pointer
source differs.

| Context file | Facade | Base-pointer source TODAY | Needs |
|---|---|---|---|
| WorldContext.py | WorldContext | shmem `SSM.WorldContext` | verify field/layout parity |
| PartyContext.py | PartyContext | shmem `SSM.PartyContext` | " |
| MapContext.py | MapContext | shmem `SSM.MapContext` | " |
| CharContext.py | CharContext | shmem `SSM.CharContext` | " |
| GuildContext.py | GuildContext | shmem `SSM.GuildContext` | " |
| AccAgentContext.py | AccAgentContext | shmem `SSM.AgentContext` | field named `AgentContext` in shmem |
| AvailableCharacterContext.py | AvailableCharacterArray | shmem `SSM.AvailableCharacters` | scan alt commented out |
| WorldMapContext.py | WorldMapContext | shmem `SSM.WorldMapContext` | " |
| MissionMapContext.py | MissionMapContext | shmem `SSM.MissionMapContext` | " |
| PreGameContext.py | PreGameContext | shmem `SSM.PreGameContext` | " |
| ServerRegionContext.py | ServerRegion | shmem `SSM.ServerRegionContext` | scan alt commented out |
| CinematicContext.py | Cinematic | shmem `SSM.CinematicContext` | " |
| GameplayContext.py | GameplayContext | shmem `SSM.GameplayContext` | " |
| TextContext.py | TextParser | shmem `SSM.GameContext` then +0x18 `text_parser` | offset chain off GameContext |
| InstanceInfoContext.py | InstanceInfo | **native pattern scan** `InstanceInfo_GetPtr.read_ptr()` | `SSM.InstanceInfo` exists but is commented out - deviation |
| AgentContext.py | AgentArray | **native pattern scan** `AgentArray_GetPtr.read_ptr()` for array base; shmem agent-array wrapper for `GetAgentByID` | mixed source - not in `Pointers_SHMemStruct` |
| GameContext.py | (none) | struct-only, no resolver | consumed by TextContext (+0x18) |

Findings:

- 13 of 17 contexts already read their base pointer from shared memory
  (`SystemShaMemMgr.get_pointers_struct().<Field>`). The legacy
  `PyPointers.Get*Ptr()` call survives only as a commented-out line next to each.
- 2 contexts deviate and still use a native byte-pattern scan
  (`NativeSymbol.read_ptr()`): `AgentContext` (array base) and `InstanceInfoContext`.
  `InstanceInfo` even ignores a live `SSM.InstanceInfo` field. Decide whether these
  should move to shmem for consistency or remain scan-based.
- `GameContext.py` resolves nothing itself; it is a passive struct whose `+0x18`
  `text_parser` is the base for `TextContext`.
- Residual dead `import PyPointers` / `import PyParty` / `import PyPlayer` at the top
  of context files will raise at import if those modules are absent - a concrete
  early-crash candidate independent of the pointer logic below.

Helpers: `native_src/internals/native_symbol.py` (scan+deref),
`native_src/ShMem/SysShaMem.py` + `native_src/ShMem/structs/PointersSSM.py`.

---

## Suggested attack order (per owner direction)

1. Bindings first. Reconcile the `Py*` surface: build the un-built binding
   (`PySkill`), re-point RE-HOMED wrappers (`Py2DRenderer` -> PyDXOverlay,
   `PyCombatEvents` -> PyAgentEvents), fix NAME renames (PyKeystroke, PyOverlay
   points), and work the API-SHAPE class->function conversions module by module.
   Remove dead imports (`PyPointers`, `PyDialogCatalog`).
2. Pointer sourcing. Confirm `Pointers_SHMemStruct` layout parity with the C++
   publisher; resolve the 2 scan-based deviations; confirm every field is populated.
3. Contexts load. With bindings and pointer source correct, the context ctypes
   overlays materialize; validate field offsets against the native struct layouts.

Open items that still need a decision or a deeper pass:
- Per-module function signature diff for the 10 API-SHAPE modules.
- Whether `AgentContext`/`InstanceInfo` should move fully onto shmem.
- Exact bound surface of `PyDXOverlay` vs the `Py2DRenderer` demand.

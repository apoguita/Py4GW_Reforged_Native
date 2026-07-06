# Shared Memory Re-migration — back to the legacy fixed layout (PyPointers merged)

Status: 2026-07-05. Supersedes the segment-directory rework described in
`shared-memory-handover.md` (that doc is now historical).

## Decision

The reforged C++ shared memory had been reworked into a self-describing
segment-directory model (48-byte header + `SegmentDescriptor[]` + 40 named
segments incl. `runtime.ctx.*` full-struct snapshots and `runtime.ptr.*` dynamic
lists). This was **more than the migration required** and it was never matched on
the Python side: `Py4GWCoreLib/native_src/ShMem/SysShaMem.py` still reads the
**legacy fixed layout**, so every context pointer came back `None` and the whole
context layer (Map/Char/World/Agent/Party/...) was dead -> `IsMapReady()` false
-> `GetAccountEmail()` '' -> widgets rendered nothing, silently.

Owner decision: **re-migrate the legacy shared memory faithfully and merge
PyPointers into it** (the pointer block is the single source of truth; the
separate `PyPointers` module is retired). Fix the side that over-migrated (C++),
not the side that was already correct (Python).

## Contract (fixed layout, must stay in lockstep with the Python mirrors)

```
[ SharedMemoryHeader (24B) ][ AgentArray_SHMemStruct ][ Pointers_SHMemStruct ]
```
- `SharedMemoryHeader`: version,total_size,sequence,process_id,window_handle (5 fields, natural align = 24B). seqlock on `sequence` (odd=writing).
- `AgentArray_SHMemStruct` (`pack(1)`): max_size, AgentArrayCount, AgentArray[300], then **13** ref arrays (All, Ally, Neutral, Enemy, SpiritPet, Minion, NPCMinipet, Living, Item, OwnedItem, Gadget, DeadAlly, DeadEnemy).
- `Pointers_SHMemStruct` (`pack(1)`): **15** uintptr_t (4B on Win32) in order MissionMapContext, WorldMapContext, GameplayContext, InstanceInfo, MapContext, GameContext, PreGameContext, WorldContext, CharContext, AgentContext, CinematicContext, GuildContext, AvailableCharacters, PartyContext, ServerRegionContext.
- Total region = 24 + 33660 + 60 = **33744 bytes** (== Python `expected_size`).

Python mirrors (unchanged, already faithful): `SysShaMem.py` (header + offset
math), `structs/AgentArraySSM.py` (13 arrays), `structs/PointersSSM.py` (15
fields). No Python change was required.

## Reforged accessors used (vs legacy `GW::Get*`)

All context pointers come from `GW::Context::Get*Context()`
(`include/GW/context/context.h`), except:
- InstanceInfo: `GW::Context::GetInstanceInfoPtr()` (`include/GW/context/map.h`, returns uintptr_t).
- ServerRegionContext: `GW::map::GetServerRegionPtr()` (`include/GW/map/map.h`, lowercase `map`).
- CinematicContext: `GW::Context::GetGameContext()->cinematic` (`include/GW/context/game.h:36`; no dedicated accessor).
- AvailableCharacters (**deviation**): no reforged accessor exists; publishes `GW::Context::GetPreGameContext()->chars_buffer` (`include/GW/context/pregame.h:94`).
Agent array classification ports the legacy `UpdateAgentArrayRegion` using
`GW::Context::GetAgentArray()`, `GetAgentContext()->agent_movement` stale gate,
`Agent::GetIs*Type/GetAsAgent*`, `AgentLiving::allegiance/GetIsDead`, and the
`GW::map` map-ready predicate.

## Files

- Rewritten: `include/GW/shared_memory/manager.h`, `src/GW/shared_memory/manager.cpp` (legacy-faithful `Manager`, namespace `GW::shared_memory`).
- Deleted: `include/GW/shared_memory/segments.h` + all 12 `src/GW/**/*_shared_memory.cpp` registrars.
- Rewired: `src/Py4GW.cpp` (Create/Update/Destroy; dropped `RegisterDefaultSegments`/`g_shared_memory_registered`), `src/base/python_runtime.cpp` (kept `is_ready/get_name/get_size/get_sequence`; dropped `get_frame_counter/list_segments/get_segment/set_update_interval/publish_now`). `src/system/system_bindings.cpp` unchanged (uses `Name()`).
- CMake: sources are `GLOB_RECURSE` — **reconfigure** after the file deletions.

## Follow-up / not done

- Dead snapshot types remain (harmless, unreferenced): `RuntimePointersSnapshot`
  (context.h:35), `AgentArraySnapshot` (agent.h:396), `MapContextSnapshot`
  (map.h:230) + `Context::kSharedMemoryVersion`/`kSharedMemorySegmentNameCapacity`.
  Remove in a later cleanup.
- `Map Struct Mapper.py` (standalone dev tool) still calls `PyPointers.*` incl.
  `GetAreaInfoPtr` (no shared-memory field). Not part of the runtime library.
- The dropped `runtime.ctx.*`/`runtime.ptr.*` extras were unused by any Python
  reader (SysShaMem is the only consumer and reads the fixed layout).

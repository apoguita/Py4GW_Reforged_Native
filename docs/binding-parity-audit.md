# Native Binding Parity Audit + Migration Plan

Status: 2026-07-05. Source: 22-module parity workflow (legacy py_*.cpp vs reforged
*_bindings.cpp vs Py4GWCoreLib usage). 95 total gaps, 40 blockers.

## Verdict

The reforged bindings are NOT a parity port: legacy stateful `py::class_` objects
(PyPlayer, PyCamera, PyInventory.Bag, PyMerchant, PyTrading, PyParty, PyQuest,
PyEffects, PyAgent) were flattened into snake_case module free functions, split
across siblings (PyAgent/PyChat/PyFriendList). Where Py4GWCoreLib was reworked in
lockstep, parity holds (no break). Damage is concentrated in 5 modules the Python
still drives through the legacy CLASS surface.

## Cross-cutting patterns
- Legacy `PyX.PyX()` class flattened to free funcs, but Python still does `obj = PyX.PyX(); obj.Method()` -> AttributeError at construction.
- Backing relocated to a SIBLING module (PyPlayer -> PyAgent/PyChat/PyFriendList; PyInventory item ops -> PyItem).
- CamelCase methods renamed to snake_case free funcs (harmless where Python migrated).
- Modules renamed/consolidated: PyTrading->PyTrade, PyAgentTagColor->PyAgentRecolor, Py2DRenderer->PyDXOverlay, PyCombatEvents->PyAgentEvents, PyDialogCatalog->PyDialog.
- Legacy enums dropped for Python IntEnums (low impact).
- Snapshot data-attrs replaced by ctypes context scraping (intentional).
- Truly unmigrated native: PyPlayer chat-log history (GetChatLog); PyUIManager frame-list/CtlFrameList + native-window host; PyParty UseHeroSkillInstant.

## Prioritized plan (blockers first)

1. **PyCamera** (blocker, medium) - `camera_bindings.cpp`: add `py::class_<PyCamera>("PyCamera")` `init<>()` snapshotting `GW::Context::GetCamera()` into legacy 30 attrs + PascalCase methods; delegate movement/fog/fov to `GW::camera` free funcs, SetYaw/SetPitch/SetCameraPos/SetLookAtTarget to `Context::Camera` accessors (game-thread). Unblocks Camera.py.
2. **PyPlayer** (blocker, medium) - `player_bindings.cpp`: add `py::class_<PyPlayer>("PyPlayer")` `init<>()` forwarding GetPlayerStatus/SetPlayerStatus->`GW::friend_list`; target_id/observing_id/ChangeTarget/CallTarget/InteractAgent/SendDialog->`GW::agent`; Istyping/SendChat/SendChatCommand/SendWhisper/SendFakeChat[Colored]->`GW::chat`. Skip the 41 readonly attrs + PlayerStatus enum (superseded by GWContext). Unblocks Player.py + PlayerMethods.py.
3. **PyInventory.Bag** (blocker, medium) - `inventory_bindings.cpp`: `py::class_<Bag>("Bag")` ctor(int,str) backed by `GW::item::GetBag` with GetItems/GetContext/FindItemById/GetItemByIndex/GetItemCount/GetSize + 7 readonly attrs (incl `name`). Unblocks ItemCache.py.
4. **PyUIManager quick wins** (blocker, small) - bind already-backed methods: draw_on_compass (ui_methods.cpp:1578), async_decode_str (2444), is_valid_enc_bytes (2497), set_frame_margins (866); wire set_window_position + register/remove_create_ui_component_callback.
5. **PyUIManager native-widget subsystem** (blocker, LARGE) - migrate frame-list/CtlFrameList control-swarm + native content-window host (CreateNativeWindow, create_*_content_by_frame_id, add_*_item_to_frame_list, checkbox/edit-box/progress/slider child controls, tab pages, group headers) + destroy_window_safely/clear_ui_input_targets + key mappings + logs. Unblocks GWUI.py.
6. **PyPlayer chat-history** (blocker, medium) - migrate GW::Chat GetChatLog + AsyncDecodeStr buffer; bind RequestChatHistory/IsChatHistoryReady/GetChatHistory on PyPlayer.
7. **PyInventory.PyInventory facade** (parity) - class delegating MoveItem/IdentifyItem/Salvage/UseItem to PyItem free funcs (or fix Widget call sites).
8. **PyDialogCatalog** (parity, small) - alias module re-exporting PyDialog.PyDialog, or repoint DialogCatalog.py.
9. **PyParty UseHeroSkill** (parity, small) - migrate UseHeroSkillInstant hook -> GW::party, bind use_hero_skill.
10. **Renamed-module aliases** (parity) - PyAgent.PyAgent.GetAgentEncName, PyAgentTagColor, Py2DRenderer, PyCombatEvents for out-of-scope/test scripts.
11. **Cosmetic** - dropped enums (PyItem Dye/Item/Salvage/Identify, PyPlayer PlayerStatus), SkillbarSkill init, PyQuest QuestData, PyTrading predicates - strict-parity only.
12. **Stubs + docs** - regenerate .pyi, coverage audit, update parity docs after class restorations.

## No-action modules (parity holds for the core lib)
PyMap, PyNameObfuscator, PyPacketSniffer, PyDialog, PyEffects, PySkill, PySkillbar,
PyItem (superset), PyMerchant, PyOverlay, Py2DRenderer, PyCombatEvents,
PyAgentTagColor, PyTrading, PyQuest (all optional/cosmetic only).

Batching: blockers 1-3 are independent binding files (camera/player/inventory) -
do as ONE batch, ONE rebuild. UIManager is its own (large) batch.
Full audit JSON: workflow wf_b435b977-c6b.

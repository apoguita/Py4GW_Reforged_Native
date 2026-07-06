# Legacy Py4GW -> Py4GW_Reforged Migration Checklist (file level)

Snapshot 2026-07-02. File-level overview only; function-level parity per file
is a later pass. Legacy source = `C:\Users\Apo\Py4GW\{include,src}` (Py4GW-native
layer; GWCA manager parity is tracked separately in the parity-report docs).

## Missing / remaining work

- [-] **Skills data layer** - `py_skills.cpp` + `SkillArray.cpp` (3,173 ln) +
      `SpecialSkilldata.cpp` (7,532 ln). DECISION 2026-07-02: skill data will
      be handled from Python; no C++ migration. The empty `GW/skills` and
      `GW/game_entities` scaffold folders can be removed.
- [~] **Native UI RE layer** -> new `GW/native_ui` module (started 2026-07-03,
      the legacy py_ui.h custom subsystems, ~5k ln of scanner-resolved
      procs/hooks/globals; the GWCA-mapped surface stays in
      `src/GW/ui/ui_bindings.cpp` PyUIManager). See
      [[native-ui-module-port]] for the batch plan. Progress:
      - [x] batch 1 - resolver bank (15 engine FrameProc/helper symbols) ->
            `offsets/native_ui.json` + `native_ui_patterns.cpp`.
      - [x] batch 2 - window-title hook (`title_hook::` MinHook detours on the
            DevText title path); module `Initialize`/`Shutdown` wired into
            `kInitSteps` (now 26, native_ui last).
      - [ ] batches 3-11 - dialog-title hijack (`create_dialog_with_title`),
            devtext source hosting, native window/dialog clone builders,
            safe-destroy + input-target scrubbing globals, content-panel/band/
            content-page builders, frame-controller anchor/margin helpers +
            `FrameSetSize`, content invalidate + caption Path-B readers,
            key-mapping table, the msg-driven frame-list control swarm (edit
            boxes, group headers, list tabs, sliders), and the bindings pass.
      - Not yet bound to Python (native-first). Still-open small items for the
            PyUIManager mapped module too: frame/UI-message log capture,
            `async_decode_str`, `is_valid_enc_bytes`, `set_slider_range`,
            window position get/set + `is_window_visible`,
            `register/remove_create_ui_component_callback`.

## Migrated this pass (2026-07-02)

- [x] **PyInventory** -> `src/GW/item/inventory_bindings.cpp`. Bag snapshots
      (dict-based, replaces SafeBag/SafeItem), hovered item, validated salvage
      (uses the Item::IsSalvageKit/IsSalvagable fold), salvage-window
      auto-accept. Item operations intentionally NOT duplicated - they live in
      PyItem.
- [x] **PyPathing** -> `GW/pathing` module (`pathing.h`, `pathing.cpp`,
      `pathing_patterns.cpp`, `pathing_bindings.cpp`, `offsets/pathing.json`).
      PathPlanner (plan/compute_immediate/status) over the lazily resolved
      engine FindPath; `Context::PathPoint` added to `GW/context/pathing.h`.
- [x] **PyUIManager (mapped surface)** -> `src/GW/ui/ui_bindings.cpp`.
      Frame tree/geometry/state, preferences, UI messages (legacy 16-word
      packed SendUIMessage preserved), enc-str helpers, widget create + typed
      widget families (button/checkbox/dropdown/slider/editable/progress/tabs/
      scrollable/text labels). Legacy UIFrame snapshot class replaced by a
      `get_frame_snapshot(frame_id)` dict (documented deviation).
- [x] **Combat events VERIFIED** - legacy PyCombatEvents (which shipped
      force-disabled: its Initialize returned before registering hooks) is
      superseded by `listeners/agent_events` (PyAgentEvents): same 7 packets,
      identical 33 event-type values, same naming normalization; reworked to a
      dumb-capture ring buffer (EFFECT_RENEWED derivation moved to Python).
## Closed by decision (2026-07-02)

- [x] **Host UI** - legacy `Py4GW_UI.cpp` is DEPRECATED; the new
      `src/imgui/{console_host_ui,console_ui,compact_ui,test_ui}.cpp` stack
      replaces it.
- [x] **PyPointers** (`PyPointers.h`) - RELOCATED to shared memory (`Pointers_SHMemStruct`). 11/12 legacy pointer getters matched; `GetAreaInfoPtr` (standalone dev tool) is the only gap. Superseded natively by `GW::Context::Get*`.
- [x] **PyScanner** (`PyScanner.h`) - MIGRATED 2026-07-03 as a parity port in
      `src/base/scanner_bindings.cpp` over `base/scanner` (decision reversed:
      the legacy Python library's `NativeSymbol`/`NativeFunction` layer and
      several contexts scan from Python, so the binding is a hard dependency
      for the Python-side migration). Native code keeps using the offsets JSON
      pattern system; PyScanner is the Python-facing escape hatch. Deviations
      documented in `python-migration-system-overview.md` section 2.1.
- [x] **Ini_handler.h** - superseded by SettingsManager/PySettings.

## Migrated (file-level complete)

- [x] py_agent -> GW/agent (PyAgent)
- [x] py_agent_tag_color -> GW/agent_recolor (PyAgentTagColor -> PyAgentRecolor)
- [x] py_camera -> GW/camera (PyCamera)
- [x] py_dialog + py_dialog_catalog -> GW/dialog, merged (PyDialog; PyDialogCatalog retired)
- [x] py_effects -> GW/effects (PyEffects)
- [x] py_imgui -> src/imgui (PyImGui, registrar split)
- [x] py_items -> GW/item (PyItem); ItemExtension folded into Context::Item
- [x] py_map -> GW/map (PyMap, incl. raycast bridge)
- [x] py_merchant -> GW/merchant (PyMerchant)
- [x] py_name_obfuscator -> GW/name_obfuscator (PyNameObfuscator)
- [x] py_packet_sniffer -> GW/packet_sniffer (PyPacketSniffer)
- [x] py_party -> GW/party (PyParty)
- [x] py_pinghandler -> GW/ping (PyPing)
- [x] py_player -> GW/player (PyPlayer)
- [x] py_quest -> GW/quest (PyQuest)
- [x] py_skillbar -> GW/skillbar (PySkillbar)
- [x] py_trading -> GW/trade (PyTrading -> PyTrade)
- [x] py_overlay + py_2d_renderer -> overlay module (PyOverlay; Py2DRenderer -> PyDXOverlay)
- [x] TextureManager + ArenaNetFileParser + AtexAsm + GwDatTextureManager + GwDatXentax
      -> GW/textures (texture_manager, arenanet_file_parser, arenanet_texture,
      gw_dat_reader, gw_dat_unpack; PyTexture)
- [x] SharedMemory -> GW/shared_memory (+ core Py4GW module surface)
- [x] VirtualInput -> include/virtual_input (PyKeystroke, PyMouse)
- [x] Timer.h -> base/timer.h
- [x] Resources (DX task queue) -> system (EnqueueDxTask/DxUpdate)
- [x] WindowCfg.h (borderless window) -> GW/ui ui_methods + system
- [x] FontManager -> src/imgui/font_manager.cpp
- [x] Logger/Globals/dllmain/Py4GW core -> base/ + src/Py4GW.cpp (re-architected)

## Not applicable

- Headers.h, StdAfx.h (umbrella/PCH), Py4GW_UI.cpp.bak_codex (backup),
  IconsFontAwesome5.h (present verbatim).

## Script-facing renames to track in the proxy layer

- Py2DRenderer -> PyDXOverlay (class DXOverlay)
- PyAgentTagColor -> PyAgentRecolor
- PyTrading -> PyTrade
- PyDialogCatalog -> merged into PyDialog (read_dialog_* statics)
- PyOverlay Point2D/Point3D -> Vec2f/Vec3f

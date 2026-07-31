# GWToolbox feature inventory (port candidates)

Full sweep of `C:\Users\Apo\GWToolboxpp-new\GWToolboxdll` against what exists today in
`Py4GW_Reforged_Native` (native) and `Py4GW_Reforged` (Python widgets), so the whole universe of
portable features is visible in one place.

## How this was built, and what it is not

Derived from three sources: the `Name()` string of every `ToolboxModule` / `ToolboxWidget` /
`ToolboxWindow`, the `ImGui::Checkbox` labels inside the big settings modules, and the directory
layout. **Implementations were not read end to end.** So:

- **Size estimates are rough** - they come from file size and apparent dependency surface, not from
  reading the code. Treat S/M/L as "which pile does this go in", not as a schedule.
- **"Partial" means a same-named thing exists on our side, not that it reaches parity.** Every
  Partial row needs a real comparison before it is trusted.
- Anything under `GWToolboxdll/Unused/` is excluded - it is dead code in their tree.

Size legend: **S** = one listener / one hook, a few hours. **M** = a module plus its own UI.
**L** = a subsystem with data tables, persistence, or an external dependency.

## Already covered (do not re-port)

**Native listeners** (14): `merchant`, `agent_events`, `skill_list_filter`,
`signet_of_capture_limit`, `faction_warning`, `cinematic_skip`, `auto_return_on_defeat`,
`disable_gold_confirmation`, `remove_cast_bar_minimum`, `auto_cancel_ua`, `auto_open_locked_chest`,
`faction_donate_skip_name`, `keep_current_quest`, `skip_campaign_prompt`.

**Native GW modules** covering GWToolbox equivalents: `chat_commands` (ChatCommands), `dialog`
(DialogModule), `name_obfuscator` (Obfuscator), `packet_sniffer` (PacketLogger backend), `pathing`
(Pathfinding backend), `textures` (GwDatModule), `friend_list`, `party`, `trade`, `quest`, `camera`,
`native_ui`, `world_render` + `overlay` + particle system (LootBeacons-class rendering).

**Python widgets** with a GWToolbox counterpart: `Pycons.py` (Pcons), `Travel.py` (Travel),
`Vanquish Tracker.py` (Vanquish), `Instance Timer.py` (Timer), `Map Overlay.py` (Minimap-class),
`Skillbar +.py` (Skillbar), `Enemy Tracker.py` (EnemyWindow), `Titles.py` (TitleTracker-class),
`Switch Character.py` (Reroll-class), `Disable Camera Smoothing.py` (CameraUnlock-class).

---

## A. Game QoL toggles - listener-shaped, smallest wins

These live inside `Modules/GameSettings.cpp` and are the same shape as the 14 already ported: one
UI-message or packet hook behind an opt-in flag. Highest value per hour of work.

| Feature (GWToolbox label) | Status | Size |
|---|---|---|
| Move items from/to storage with Control+Click | Missing | S |
| Move materials to open storage pane on click | Missing | S |
| Tick is a toggle | Partial - `set_tick_toggle` exists natively, unbound as a listener | S |
| Auto-lock heroes and pets onto your called target | Missing | S |
| Hide in-game store message on character select | Missing | S |
| Auto-set 'Away' after N minutes idle | Missing | S |
| Block full-screen message entering a new area | Missing | S |
| Block full-screen vanquish popup | Missing | S |
| Block full-screen dungeon-chest popup | Missing | S |
| Prevent weapon-spell skin on player weapons | Missing | S |
| Prevent dervish avatar changing character appearance | Missing | S |
| Set GW window title to logged-in character | Missing | S |
| Hide minimize/restore/close in borderless + fullscreen | Missing | S |
| Show XP progress instead of level on the XP bar | Missing | S |
| Show 'You have N Lockpicks' on chest name tags | Missing | S |
| Disable skill descriptions (explorable / outpost) | Missing | S |
| Party poppers / fireworks auto-use | Missing | S |
| Flash window on: zoning, cinematic, trade, name ping | Missing | S each |
| Focus window on: launch, zoning, trade | Missing | S each |
| Notify when friend logs in/out, joins/leaves outpost | Missing | S each |
| Notify when player joins/leaves party or outpost | Missing | S each |
| Toast on: vanquish, boss kill, mission, dungeon, title maxed | Missing | S each |
| Overhead number filters (faction, XP, 0 XP, zero dmg/heal, dmg in/out, heal in/out) | Missing | S each |

**Chat filtering** (`Modules/ChatFilter.cpp`) - a self-contained bundle, ~30 toggles:
item drops (rare/common, self/ally, pick-up), guild announcements, Hall of Heroes winners, favor
announcements, playtime messages, title achievements, faction gain, inventory full, already
identified, not enough adrenaline/energy, skill points, 9 Rings, lunar fortunes, "no one hears you",
salvaging, ashes dropped, plus substring and per-player blocklists and per-channel muting.
**Status: Missing. Size: M** as one unit, S per rule once the harness exists.

**Chat settings** (`Modules/ChatSettings.cpp`): message timestamps (24h, seconds), open web links
from templates, clear chat when hiding the chat window. **Missing, S.** Note `kOpenTemplate`
link-opening already exists natively in `ui.cpp` - only the timestamp work is new.

---

## B. Overlays and widgets

| GWToolbox widget | What it does | Status | Size |
|---|---|---|---|
| Bonds | Grid of maintained enchantments on party | Missing | M |
| Health | Target/self health with thresholds | Missing | S |
| Distance | Range to target in game units | Missing | S |
| Clock | Wall clock / instance time | Missing | S |
| Latency | Ping display | Partial - native `ping` module exists, no widget | S |
| Server Info | Server region/host info | Missing | S |
| Party Damage | Per-party-member damage meter | Missing | M |
| Skill Monitor | Live skill usage per party member | Missing | M |
| Effects Monitor | Party effect/condition grid | Missing | M |
| Alcohol | Drunk level + timer | Partial - `Disable Alcohol Effect.py` is a different feature | S |
| Title Tracker | Live title progress | Partial - `Titles.py` exists, verify | S |
| Favor Tracker / Favor Overlay | Gods' favor state + countdown | Missing | M |
| Bounty Kill Tracker | Luxon/Kurzick bounty counts | Missing | S |
| Exploitable Corpses | Highlights exploitable bodies | Missing | M |
| Inventory Overlay | Inventory as a screen overlay | Partial - `InventoryPlus.py` | M |
| Snaps To Party Window | Docks overlays to the party frame | Missing | S |
| Mission Map / World Map | Map overlays with custom markers | Partial - `Map Overlay.py` | M |
| Vanquish Map Overlay | Vanquish progress on the map | Partial - `Vanquish Tracker.py` | S |
| Minimap renderer suite | Agent/Effect/Range/Symbols/PingsLines/Pmap/GameWorld renderers | Partial - our overlay + `world_render` cover the plumbing; the renderers themselves are not ported | L |

---

## C. Tool windows

| GWToolbox window | What it does | Status | Size |
|---|---|---|---|
| **Armory** | Locally spoofs `NPCEquipment` to preview any armor set + dye combo on your character. ~1.5k lines plus a ~1.8k-line per-profession armor table. | **Missing** | **L** |
| Account Inventory | Inventory across all characters on the account | Partial - `TeamInventoryViewer.py` | M |
| Builds / Hero Builds | Build template manager, load to skillbar | Missing | L |
| Completion | Missions / skills / vanquish / hero / HoM completion tracker, per character and account-wide | Mostly ready - see note below | S native + M Python |
| Daily Quests | ZM / ZB / WB dailies | Partial - `Calendar.py` | M |
| Materials | Material trader helper | Missing | M |
| Inventory Sorting | Auto-sort inventory | Missing | M |
| Drop Tracker | Session drop log | Missing | M |
| Objective Timer | Per-mission objective split timers | Missing | L |
| Party Statistics | Per-member skill usage totals | Missing | M |
| Party Search | Party-search browser | Partial - native calls exist, no window | M |
| Target Info | Detail panel for current target | Missing | S |
| Enemy Window | Enemy list in instance | Partial - `Enemy Tracker.py` | M |
| Door Monitor | Door open/closed state | Missing | S |
| Faction Leaderboard | Luxon/Kurzick standings | Missing | S |
| GW Market | Market price browser (external API) | Missing | M |
| Trade | Trade-chat monitor + search (external service) | Missing | L |
| Price Checker (module) | Item price lookup on hover | Missing | M |
| Hall of Monuments | HoM calculator | Missing | M |
| Guild Wars Preferences | Surfaces GW's own preference set | Missing | M |
| Skill Listing | Full skill table dump | Missing | S |
| String Decoder | Decodes encoded strings - **useful for our own RE work** | Missing | S |
| Notepad | Scratch notes | Missing | S |
| Info | Assorted state/debug readouts | Missing | S |
| Performance | Frame timing breakdown | Partial - `PyProfiler` exists | S |
| Packet Logger | Packet log UI | Partial - native `packet_sniffer`, no window | S |
| Duping | Niche dupe helper | Missing | S |
| Observer suite (5 windows + module) | PvP observer-mode stats | Missing | L |

### Note: Completion window - closer than it looks

Sized `L / Missing` on the first pass; that was wrong in a way worth recording. `CheckProgress`
(`CompletionWindow.cpp:455-545`) reads exactly **eight** arrays off `WorldContext` / `CharContext`,
and **six are already bound to Python** through `PyPlayer` (`player_bindings.cpp:389-396`):

| Source array | Bound? |
|---|---|
| `missions_completed`, `missions_bonus`, `missions_completed_hm`, `missions_bonus_hm` | Yes |
| `unlocked_character_skills` (+ `learnable_character_skills`) | Yes |
| `unlocked_map` (as `unlocked_maps`) | Yes |
| `vanquished_areas` (WorldContext +0x83C) | **No** - declared in `world.h`, bound nowhere |
| `hero_info` -> per-entry `hero_id` (the account's unlocked hero roster) | **No** - `party_bindings.cpp` only exposes heroes in the *current party* |

So the native work is one small binding pass: add `vanquished_areas` and an unlocked-hero-id list to
`PyPlayer`, both alongside arrays that are already copied there.

Everything else is Python-side, and most of it is transcription rather than engineering:

- **Static campaign tables** - `CompletionWindow_Constants.h` (479 lines) plus `Initialize_Prophecies`
  / `_Factions` / `_Nightfall` / `_EotN` / `_Dungeons`. This is the bulk of the 3.3k-line `.cpp` and
  is pure data: missions, vanquish areas, elite skills, hero unlocks, minipets, festival hats.
- **Per-character persistence** - `character_completion.json`, keyed by character name, so the view
  works account-wide while logged into one character.
- **The HoM half is a separate dependency.** `minipets_unlocked` / `festival_hats` /
  `hom_achievements` do not come from game memory at all - they come from
  `HallOfMonumentsModule::AsyncGetAccountAchievements`, an async HTTP fetch against the Hall of
  Monuments calculator. That half can be deferred without blocking the rest.

**Overlap to resolve first:** `src/GW/multibox/manager.cpp:596-601` already copies
`missions_completed` / `_bonus` / `_hm` / `_bonus_hm` and `unlocked_character_skills` into shared
memory per slot - i.e. we already do account-wide completion aggregation by a different mechanism.
Decide whether Completion reads that shared-memory view or keeps its own JSON before building
either.

**Reusable API:** GWToolbox exposes `IsAreaComplete`, `IsAreaUnlocked`, `IsSkillUnlocked`,
`GetCharactersWithoutAreaComplete/AreaUnlocked/SkillUnlocked`. Other features consume these, and they
are directly useful for multibox and account tooling independent of the window itself.

---

## D. Systems and behavior modules

| GWToolbox module | What it does | Status | Size |
|---|---|---|---|
| Hotkeys (window + 13 action types) | Full hotkey system: use item, drop/use buff, equip, flag hero, command pet, send chat, target, toggle, dialog, move, GW key, groups | Missing - only raw keystroke exists | L |
| Inventory Manager | Bulk salvage / identify / stack / destroy workflows | Partial - `SuperItemEater.py`, `item_eater.py` | L |
| Item Filter (ItemDrops) | Filters which drops are shown/announced | Partial - `loot_config.json`, `rarity_filter_data.json` | M |
| Item Tooltip / Item Description Handler | Custom item tooltips and description rewriting | Missing | M |
| Transmo | Model transmogrification (self/party/NPC) | Missing | M |
| Texmod (gMod/uMod) | Texture replacement pipeline | Missing - our `textures` module is the base | L |
| Party Window Module | Adds heroes/henchmen/allies rows to the party frame | Missing | M |
| Hero Panel Position | Persists hero panel layout | Missing | S |
| Quest Module | Quest marker/pathing helper | Partial - native `quest`, `PartyQuestLog.py` | M |
| Cartographer Helper | Map exploration percentage aid | Missing | M |
| Skill Range Rings / Danger Rings | Aggro and skill-range rings | Partial - `Map Overlay.py` draws rings; verify | M |
| Lava Rivers (RiverModule) | Renders lava/river hazards | Missing | M |
| Weather | Force/lock weather | Missing | S |
| Audio Settings | Per-channel audio control | Missing | M |
| Mouse Settings / Mouse Fix | Mouse behavior fixes | Missing | S |
| Keyboard Layout Fix | Non-US keyboard fix | Missing | S |
| FPS Fix / Code Optimiser / Vendor Fix | Perf and client-bug patches | Missing | S each |
| Gamepad | Controller support | Missing | L |
| Login Module | Auto-login / character select automation | Partial - `MultiBoxing.py`, launcher | M |
| Resign Log | Tracks who resigned | Missing | S |
| Party Broadcast | Broadcasts party state | Partial - `Lightbringer Party Broadcast.py` | M |
| Toast Notifications | In-client toast system | Missing - prerequisite for many GameSettings notify toggles | M |
| Hints | Contextual hint popups | Missing | S |
| Text-to-speech | Reads chat aloud | Missing | M |
| Discord / Twitch / Teamspeak 3 / Teamspeak 5 | External integrations | Missing | M each |
| Guild Wars Settings | Reads/writes GW's own registry settings | Missing | M |

---

## E. Out of scope

Toolbox's own infrastructure, with no meaning in our architecture:
`ToolboxSettings`, `ToolboxTheme`, `SettingsWindow`, `MainWindow`, `Updater`, `BackupModule`,
`PluginModule`, `Resources`, `TestHarness`, `CrashHandler` (we have our own), `HintsModule`
scaffolding, `AprilFools` and everything else under `Unused/`.

---

## Suggested first wave

Ordered by value per unit of work, not by size:

1. **The GameSettings toggle backlog (section A).** Same shape as the 14 already done, the harness
   exists, each is hours. This is the densest vein of "little" features in the whole tree.
2. **Toast Notifications.** Unlocks ~15 notify/toast toggles in section A that otherwise have
   nowhere to render.
3. **Chat Filter** as one bundle. Self-contained, high daily-use value, no new native surface.
4. **String Decoder.** Small, and it pays for itself immediately in our own RE work - it is exactly
   the tool that would have shortcut the `skip_campaign_prompt` encoded-string hunt.
5. **Armory.** The one you flagged. Genuinely large - equipment spoofing plus a big static armor
   table - so worth scheduling deliberately rather than slipping it into a QoL batch.
6. **Hotkeys system.** Large but foundational; a lot of other features become one-liners once a
   real hotkey action framework exists.

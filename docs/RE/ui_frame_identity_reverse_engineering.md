# UI Frame Identity - Reverse Engineering and Live Verification

Date: 2026-07-28
Static target: `F:\GW\GW1\Gw.exe` (live client, 2026-07-22 build)
Cross-reference: `/Gw.wasm` (symbols), `/Gw.exe (06-14)` in Ghidra project "third time"
Live target: in-game frame tree, captured via `frame_tree_dump.py`

## 0. What this establishes

1. A frame's `frame_hash_id` is `StrHashI(name, -1)` - a plain table-driven string hash we
   can reimplement. Verified against the live client.
2. The numeric "child offsets" everyone treats as array indices are **key codes** - keyed
   entries in a global hash table. Adding or reordering siblings does **not** renumber them.
3. The name string is **not retained in memory**. Only the hash is. Any hash -> name
   resolution requires a dictionary built by forward-hashing strings from `Gw.exe`.
4. Only ~12% of live frames carry a name at all. The rest are created with `name == NULL`
   and can only ever be addressed by a key-code path.
5. Points 1-3 were confirmed against a live in-game dump, not just static analysis.

---

## 1. Frame identity model

`IFrame::Frame` embeds an `IFrame::CRelation` at **`+0x128`** (matches `GW::ui::Frame::relation`).
Constructor `IFrame::CRelation::CRelation(Frame* parent, unsigned int code, wchar_t const* name)`
@ `ram:8097d2d3`:

```c
this[0] = parent ? (parent + 0x128) : 0;        // +0x00  parent CRelation*
this[1] = code;                                 // +0x04  m_keyCode
this[2] = parent ? (parent + 0x128) : 0;        // +0x08  parent CRelation* (name-table key)
this[3] = name ? StrHashI(name, -1) : 0;        // +0x0C  m_nameHash  <-- frame_hash_id
...
TBaseHashTable<CRelation>::Add(&g_codeTable, this, CHashCode::GetHash(this));   // keys +0x00,+0x04
TBaseHashTable<CRelation>::Add(&g_nameTable, this, CHashName::GetHash(this+2)); // keys +0x08,+0x0C
```

Mapping onto `include/GW/ui/ui.h`:

| our field | CRelation offset | real meaning |
|---|---|---|
| `parent` | +0x00 | parent `CRelation*` |
| `field67_0x124` | +0x04 | **`m_keyCode`** (the `child` argument to `FrameCreate`) |
| `field68_0x128` | +0x08 | parent `CRelation*`, duplicated as the name-table key |
| `frame_hash_id` | +0x0C | **`m_nameHash` = `StrHashI(name, -1)`** |

Two global hash tables index every live frame:

- code table (buckets `ram:005a03b0`, count `005a03b8`, mask `005a03bc`) keyed by `(parent, key_code)`
- name table (buckets `ram:005a03d4`, count `005a03dc`, mask `005a03e0`) keyed by `(parent, name_hash)`

`CRelation::GetChild(code)` @ `ram:8098385c` and `CRelation::GetChildFromNameHash(hash)` @
`ram:80983fda` are O(1) probes into those tables.

### 1.1 Child "offsets" are key codes, not indices

`FrameGetChild(frame_id, code)` @ `ram:809afc7e` - what `g_get_child_frame_id_func` resolves to:

```
GetFrame(frame_id) -> frame
CRelation::GetChild(frame + 0x128, code)   // hash lookup on (parent, code)
return child->frame_id                     // frame + 0xBC
```

It never indexes a child array. The number is `m_keyCode`, a literal baked into each
`FrameCreate` call site. Adding, removing or reordering siblings does not renumber the others.

### 1.2 The name is not stored

The `CRelation` constructor hashes `name` and discards the pointer. No field holds the text.
Consequence: **there is no way to read a frame's name from memory**, and `StrHashI` is one-way
(`a = (a << 3) ^ c` shifts prior state out after ~11 chars), so the hash cannot be inverted.
Forward-hash-and-match against a dictionary is the only method.

---

## 2. The hash function

`frame_hash_id` uses `StrHashI(wchar_t const*, unsigned long)` @ `ram:80020180` - the
**case-insensitive** variant. (`StrHash` @ `ram:80020082` is the case-sensitive twin and is
*not* what frames use.) The second argument is a max character count, not a seed; `-1` means
"to the NUL".

```python
MIX = [  # 16 x uint32, LE, from ram:00130f50
 0x92B9A528, 0x25D4FC88, 0xEDCBEFB8, 0x51063A80, 0x91341C61, 0x0261229D, 0x726F48ED, 0xCE1C088C,
 0x76253EB5, 0x31E3A0DE, 0xA2AAD215, 0xCA7D6D27, 0xA5F98970, 0x0541C365, 0x3C14FF04, 0x5056AF4F]

def str_hash_i(s):
    h, a, b = 0x325D1EAE, 0xE2C15C9D, 0x2170A28A
    for ch in s:
        c = ord(ch) & 0xFFFF
        if 0x61 <= c <= 0x7A:          # a-z -> A-Z
            c -= 0x20
        a = c ^ ((a << 3) & 0xFFFFFFFF)
        b = (MIX[a & 0xF] + b) & 0xFFFFFFFF
        h = ((b + a) & 0xFFFFFFFF) ^ h
    return h
```

`GW::ui::GetHashByLabel` already calls the engine's own copy via `g_create_hash_from_wchar_func`,
so C++ does not need this; Python does, for offline work.

---

## 3. The floating-dialog table

`IUi::Game::DialogShow(parent, EFloatingDialog dialog, ...)` @ `ram:815cdb8c`:

```c
assert(dialog < arrsize(s_floatingDialog));   // 58 entries
desc = &s_floatingDialog[dialog];             // stride 0x24 (9 dwords)
key  = dialog + 0x13;
FrameCreate(parent, desc[3], key, desc[0], param, desc[1]);
//                  flags    ^code  proc          ^name
```

Table located at **VA `0x0094bee8`** in the live `Gw.exe`, 58 entries, **sorted alphabetically
by name**. Every hash it produces matches `frame_aliases.json`. `Salvage` is index 41, hash
`684387150`, key code 60.

The alphabetical ordering is a hazard: adding a dialog that sorts earlier shifts every later
index *and its key code*. Addressing by hash survives that; addressing by key code does not.

---

## 4. What is statically recoverable

`FrameCreate` is at **`0x00630c90`** in the live build (was `0x0062bfc0` in 06-14 - it moves,
so always locate by pattern `33 D2 89 45 08 B9 C8 01 00 00` = `mov ecx, 0x1C8`, then walk to
function start).

A typical call site:

```asm
004e0df4  PUSH 0x9487bc     ; name  -> L"SkillBar"   (UTF-16LE literal)
004e0dfb  PUSH 0x0          ; create_param
004e0dff  PUSH 0x52ed40     ; FrameProc -> widget type
004e0e09  PUSH 0x72         ; key code
004e0e10  PUSH EAX          ; flags
004e0e11  PUSH EDI          ; parent  <-- runtime frame id, NOT static
004e0e12  CALL FrameCreate
```

Recoverable: `(key_code, name, FrameProc, flags)`. **Not** recoverable: the parent, always a
runtime value. So static analysis yields an inventory, not a tree.

### Results on the live build

| metric | value |
|---|---|
| `FrameCreate` call sites | 2,015 |
| sites with a literal name | 505 |
| sites with `name = NULL` (anonymous) | 1,510 |
| sites where the argument decoder gave up | 405 |
| **distinct confirmed frame names** | **427** (370 call-site + 57 dialog table) |
| harvested-string dictionary | 17,138 entries |

Two dictionaries with different confidence:

- **confirmed** (`frame_names_confirmed.json`, 427) - the `name` argument was read at an actual
  creation site. High confidence.
- **harvested** (`frame_names.json`, 17,138) - every identifier-like string in `Gw.exe`,
  forward-hashed. A match is inference, not proof.

Collision measurement on the harvested set: 16,734 candidate strings -> 16,691 distinct hashes,
**43 collisions, all 3-character junk** (`JBX`/`JRH`, `VF8`/`VVH`, ...), **none touching a
confirmed frame name**. The weak-mixing band is short strings; real frame names are almost all
>= 4 chars.

---

## 5. Tooling

Offline (run against a `Gw.exe`, game not required):

| script | purpose |
|---|---|
| `gen_frame_names.py` | harvest strings -> `frame_name_hashes.json` (hash -> name) |
| `dump_framecreate.py` | locate `FrameCreate` by pattern, scrape all call sites -> `framecreate_sites.json` |
| `find_dialog_table.py` | locate and dump `s_floatingDialog[]` -> `floating_dialogs.json` |

In-game (`Py4GW_Reforged` repo root, next to `frame_viewer.py`):

| file | purpose |
|---|---|
| `frame_tree_dump.py` | ImGui window; one button walks the live tree and classifies every hash |
| `frame_names_confirmed.json` | 427 confirmed names |
| `frame_names.json` | 17,138 harvested names |

Outputs: `frame_tree_dump.json` (data), `frame_tree_dump.txt` (readable),
`frame_unknown_hashes.txt` (hashed frames with no dictionary entry).

Note `frame_tree_dump.py` uses `SCRIPT_DIR = os.getcwd()`, matching `frame_viewer.py`. Do not
use `__file__` - scripts are exec'd into a fresh module and it may be undefined.

---

## 6. Live verification

Two captures from the running game.

| metric | normal play | salvage window open |
|---|---|---|
| frames | 580 | 683 |
| leaves | 359 | 439 |
| **hashed (have a name)** | **69 (12%)** | 70 |
| resolved via confirmed dict | 50 | 51 |
| resolved via harvested dict | 19 | 19 |
| **unknown (no dictionary entry)** | **0** | **0** |

**Zero unknowns.** Every hashed frame in the live tree resolved to a name.

Independent corroboration: 18 of the 19 harvested-only names match hashes captured from live
memory in the existing `frame_aliases.json`, and the labels agree semantically 1:1
(`Compass`/Compass, `SkillBar`/Skillbar, `StatHealth`/Health Bar, `Blurb`/Mission Goals, ...).
The 19th, `PartyMission` (`3497061758`), was absent from that file entirely.

So the confirmed/harvested split, while methodologically real, made no practical difference on
this tree.

### Live key codes under `Game -> 6`

`140452905` is `Game`. Its child `6` is anonymous and hosts the HUD and the floating dialogs:

```
11 Bundle          13 Chat            14 Compass         15 DamageMonitor   16 StClock
32 Friends         42 MapWindow       48 PartyMinions    77 Effects         78 District
79 DnStat          82 InputGuide      85 LevelProgress   90 MissionProgress 91 MissionStatus
102 Party          103 PartyMission   107 NpcDialogue1   108 NpcDialogue2   109 Blurb
110 Hint           111 NotifyText     116 SkillBar       117 SkillMonitor   118 SkillUpkeep
119 SkillWarmup    123 StatEnergy     124 StatHealth     125 StartMenuBar   126 Target
127 TaskTracker    128 BtnTrade       129 WeaponBar
```

Codes >= `0x03000001` (50331649) and `0x04000018` (67108888) are **runtime-allocated per agent**,
not compile-time constants. They change every session and must never be written into an alias
file.

---

## 7. Case study: the salvage materials prompt

A repeat breakage. Four places in the Python tree encoded the same dialog with four different
values - patch archaeology, each fixed in isolation and left to rot:

| location | value |
|---|---|
| `frame_aliases.json` | `6,109` |
| `Py4GWCoreLib/Inventory.py` | `[6,113,6]` |
| `Py4GWCoreLib/GlobalCache/InventoryCache.py` | `[6,98,6]` |
| `Examples/Accept Material Salvaging.py` | `[6,98,6]` |

**Static prediction.** Site `0x004eb330`, matching `IUi::Game::OnSalvageBegin` @ `ram:815df1c9`
on two independent signatures - flags `0x00254030` and `TextEncode(0x7b82)`:

```asm
50               push eax            ; create_param
68 f0 b4 87 00   push 0x0087b4f0     ; FrameProc
6a 71            push 0x71           ; key code = 113
68 30 40 25 00   push 0x00254030     ; flags
ff 75 08         push [ebp+8]        ; parent
e8 5b 59 14 00   call FrameCreate
```

**Live confirmation.** Dumping with the prompt open produced a frame that was absent from the
baseline dump:

```
#680   code=113    (anonymous, hash=0)
  #681   code=0      Question Frame
  #682   code=1      Label
  #685   code=6      Yes Button
  #686   code=4      No Button
```

Static and live agree: the path is **`140452905,6,113`**, and the prompt is anonymous
(`hash = 0`), so a code path is the only handle it will ever have.

> Ruled out: sites `0x004e7677` / `0x004e7829` (codes 121/122, proc `0x00526b60`, flags `0x30`)
> sit near a `push 0x29` but are a different dialog family. Do not re-derive these.

---

## 8. Stale entries found, and what was changed

The live dump exposed four `Game -> 6` entries pointing at the wrong frame:

| entry | label in `frame_aliases.json` | actually is, live |
|---|---|---|
| `140452905,6,109` | Salvage Materials Dialog | **`Blurb`** (Mission Goals) |
| `140452905,6,111` | LesserSalvageWindow | **`NotifyText`** (Notifications) |
| `140452905,6,117` | Floating Buttons.Log Out | **`SkillMonitor`** |
| `140452905,6,126` | Floating Button.Options | **`Target`** (Target Display) |

Applied (2026-07-28):

- `Py4GWCoreLib/frame_aliases.json` - 5 keys remapped `109 -> 113`. Backup `.bak` alongside.
- `Py4GWCoreLib/GlobalCache/InventoryCache.py:478` - `[6,98,6]` -> `[6,113,6]`
- `Examples/Accept Material Salvaging.py:14-15` - `[6,98]` -> `[6,113]`, `[6,98,6]` -> `[6,113,6]`
- `Py4GWCoreLib/Inventory.py:401` already correct, untouched.

Not applied - still stale or unreviewed:

- `140452905,6,111` / `,117` / `,126` in `frame_aliases.json`
- `Py4GWCoreLib/UIManager.py:1637-1671` - `LesserSalvage*` / `ExpertSalvageUnidentified*`
  `FrameInfo` entries still on `[6,111]` / `[6,112]`
- `Sources/frenkeyLib/ItemHandling/UIManagerExtensions.py:47-48,253,263`
- `Widgets/Guild Wars/Items & Loot/MerchantRules.py:20340-20343`

Several of those try multiple codes as a fallback chain, which may be deliberate hedging rather
than rot - review before changing.

---

## 9. Limits

- **88% of live frames have no name** (`hash == 0`). Names anchor roots and notable controls;
  interior controls need code paths. A fully name-addressed tree is impossible.
- **The name is not in memory**, so a runtime-only naming scheme cannot work. A `FrameCreate`
  hook would capture names for frames created *while attached*, but the persistent UI is built
  at login - likely before injection.
- **Key codes are compile-time literals**, stable against sibling churn but not against source
  edits. They are the residual fragility.
- **Runtime-allocated codes** (`0x03000001+`, `0x04000018+`) are per-session; never persist them.
- The call-site scraper resolves 1,610 of 2,015 sites. The 405 failures are argument sequences
  the backward push-decoder cannot follow; improving it moves frames from harvested to confirmed.

## 10. Upkeep loop

1. After a patch, re-run the three offline scripts against the new `Gw.exe`.
2. Launch, dump the tree, dump again with the target dialog open.
3. Diff `frame_tree_dump.json` against the previous capture - renames, moved key codes, new frames.
4. The dump already emits alias-format paths, so the alias file can be regenerated from the live
   tree rather than hand-edited.

## 11. Symbol reference

| symbol | address |
|---|---|
| `StrHashI(wchar_t const*, unsigned long)` | `Gw.wasm ram:80020180` |
| `StrHash(wchar_t const*, unsigned long)` | `Gw.wasm ram:80020082` |
| mix table (16 x u32) | `Gw.wasm ram:00130f50` |
| `FrameCreate` | `Gw.wasm ram:809a13ea` / live `Gw.exe 0x00630c90` |
| `IFrame::Frame::Frame(Frame*, uint, uint, wchar_t const*)` | `ram:8099ad3b` |
| `IFrame::CRelation::CRelation(Frame*, uint, wchar_t const*)` | `ram:8097d2d3` |
| `IFrame::CRelation::CHashCode::GetHash() const` | `ram:8097bcaf` |
| `IFrame::CRelation::CHashName::GetHash() const` | `ram:8097bdf3` |
| `IFrame::CRelation::GetChild(uint)` | `ram:8098385c` |
| `IFrame::CRelation::GetChildFromNameHash(uint)` | `ram:80983fda` |
| `FrameGetChild(uint, uint)` | `ram:809afc7e` |
| `IUi::Game::DialogShow(uint, EFloatingDialog, int, void const*)` | `ram:815cdb8c` |
| `IUi::Game::OnSalvageBegin(uint, UiMsgItemUpgradeBegin const&)` | `ram:815df1c9` |
| `s_floatingDialog[]` | live `Gw.exe VA 0x0094bee8` |

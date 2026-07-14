# Item & Gadget Name-Tag Recolor Reverse Engineering

> **Backend note - we are on Reforged.** This is the `Py4GW_Reforged_Native` project. Migrated
> managers live in `src\GW\<module>\` + `include\GW\<module>\`; runtime addresses are resolved from
> `offsets\<module>.json`, never hardcoded. GWCA names are legacy cross-references only. `Gw.exe` /
> `Gw.wasm` addresses describe the actual game and stay valid. RE done WASM-first on `/Gw.wasm`
> (full debug symbols), pinned to `/Gw.exe (06-14)` (the injected build).

> **STATUS.** Gadget name-tag recolor: **RE COMPLETE, hook target EXE-pinned, not yet implemented.**
> Item name-tag recolor: **mechanism fully RE'd, two hook levers identified and EXE-pinned; the
> per-rarity color source (`CNameComposer` EColor switch / `@`-name color table on 06-14) is the one
> remaining pin.** This doc extends `name_tag_color_reverse_engineering.md` (which covers living
> agents - SHIPPED). Read that first: it establishes the `TextData` struct, the `Color4b` ARGB
> convention, and the markup color system this doc builds on.

## Scope

The shipped agent recolor (`GW::agent_recolor`, legacy `AgentTagColor`) only covers **living agents**
(players/NPCs/enemies) because those route through the `GetConsiderColor` resolver. **Items and
gadgets do NOT use that resolver** - they each have their own `GetTextData` and their own coloring
mechanism. This doc reverse-engineers those two paths so they can be recolored natively.

## Executive summary

Guild Wars has **three** name-tag color mechanisms, one per agent view class. All three funnel
through a virtual `GetTextData(TextData* nameTag, TextData* subText)` that fills a 0x14-byte
`TextData` struct, but each fills the color differently:

| View class | Color mechanism | Recolor lever |
|------------|-----------------|---------------|
| `CCharAgent` (living) | `GetConsiderColor` resolver writes `Color4b` | hook the resolver (**SHIPPED**) |
| `CGadgetAgent` | **two plain `Color4b` written inline** in `GetTextData` | **hook `GetTextData`, overwrite the two color words** |
| `CItemAgent` | **ownership** `Color4b` inline + **rarity `<c=@...>` markup** inside the coded name | hook `GetTextData` (base color) and/or rewrite the coded-name markup / patch the `@`-name color table |

**Gadgets are easy** - a single `GetTextData` hook, structurally identical to the char path.
**Items are the hard one** the user cares about most: the rarity color is not a `Color4b` field, it
is `<c=@ItemRare>...` markup embedded in the item's coded name string, resolved at render time by the
markup parser against a named-color table.

---

## 1. The shared `TextData` struct (0x14 bytes, 5 dwords)

All three `GetTextData` overloads fill the same struct. Confirmed identical across the char, gadget,
and item paths on both WASM and EXE 06-14:

| Offset | Field | Meaning |
|--------|-------|---------|
| `+0x00` | `wchar_t* coded_name` | encoded name string; **may contain `<c=@...>` color markup** (items) |
| `+0x04` | `uint32 flag` | 0 or 1 - "has data" / type selector |
| `+0x08` | `Color4b color_full` | **the primary text color** (ARGB `0xAARRGGBB`) |
| `+0x0C` | `Color4b color_dim` | same color, **alpha forced to `0xC0`** via a `*(byte*)(td+0x0F)=0xC0` write (dim/shadow variant) |
| `+0x10` | `uint32 flag2` | 0 |

The `*(td+0x0F) = 0xC0` alpha poke is the tell that proves the byte order: byte 3 of the `+0x0C`
dword is alpha, i.e. the u32 is literally `0xAARRGGBB` with `[B,G,R,A]` little-endian memory layout.
No channel swap. (Same finding as the agent doc SS3.)

**The visible name-tag color is `+0x08`.** `+0x0C` is the dimmed-alpha copy used for the
distance-faded / shadowed render. A recolor must write **both** for a clean result.

---

## 2. Gadget path - RE COMPLETE

### 2.1 `CGadgetAgent::GetTextData` - the hook target

- WASM: `IAgentView::CGadgetAgent::GetTextData` @ `ram:80bb4451`
- **EXE 06-14: `FUN_007f9950`** (`__thiscall`), verified by its call to `GadgetCliGetCodedName` and
  the inline color writes.

EXE 06-14 body (cleaned):

```c
void __thiscall CGadgetAgent__GetTextData(int this, u32* nameTag, u32* subText) {
    if (nameTag != 0) {
        u32 coded = GadgetCliGetCodedName(*(u32*)(this + 0x2C));  // agent id at this+0x2C
        nameTag[3] = 0xFFFFFF00;        // +0x0C
        nameTag[0] = coded;             // +0x00
        nameTag[1] = 0;                 // +0x04
        nameTag[2] = 0xFFFFFF00;        // +0x08  <- visible color
        *((u8*)nameTag + 0x0F) = 0xC0;  // +0x0C alpha -> 0xC0FFFF00
        nameTag[4] = 0;                 // +0x10
    }
    if (subText != 0) *subText = 0;
}
```

`0xFFFFFF00` in ARGB = A=`FF`, R=`FF`, G=`FF`, B=`00` = **opaque yellow** (the default gadget /
signpost / collector name-tag color). `+0x0C` becomes `0xC0FFFF00` (75% alpha).

**The agent id is at `this + 0x2C`** - the same view offset the char resolver detour uses. Consistent
across all agent view classes.

### 2.2 `GadgetCliGetCodedName` - anchor / reads

- WASM: `GadgetCliGetCodedName(unsigned long)` @ `ram:80bb279e`
- **EXE 06-14: `FUN_00839630`** (`__cdecl(agent_id)`), verified by its asserts at
  `GdCliApi.cpp` lines `0x1ad` / `0x1ae` (and array-bounds `0x24b`). No rarity markup - gadgets have
  no rarity; the coded name is a plain encoded string. This function is only needed as a resolution
  anchor; the color is written by `GetTextData`, not here.

### 2.3 Gadget recolor recipe

Hook `FUN_007f9950` (`CGadgetAgent::GetTextData`) with a `__thiscall`-emulated `__fastcall` detour,
run the original, then overwrite the color words on the `nameTag` out-struct:

```c
using GadgetTextDataFn = void(__fastcall*)(void* view, void* edx, uint32_t* nameTag, uint32_t* subText);
GadgetTextDataFn Original;  // MinHook trampoline

void __fastcall Detour_CGadgetAgent_GetTextData(void* view, void* edx, uint32_t* nameTag, uint32_t* subText) {
    Original(view, edx, nameTag, subText);   // let the game fill the default (yellow)
    if (nameTag && view) {
        uint32_t agentId = *(uint32_t*)((uintptr_t)view + 0x2C);
        uint32_t argb;
        if (LookupGadgetRule(agentId, &argb)) {         // per-gadget or "all gadgets"
            nameTag[2] = argb;                          // +0x08 full color
            nameTag[3] = (argb & 0x00FFFFFF) | 0xC0000000;  // +0x0C dim (force alpha 0xC0), OR just argb
        }
    }
}
```

**Resolution (build-portable):** anchor on `GadgetCliGetCodedName` via
`FindAssertion("GdCliApi.cpp", <msg>, 0x1ad)` -> that gives `FUN_00839630`; `CGadgetAgent::GetTextData`
is its caller in the AgentView range (`FUN_007f9950`). Alternatively byte-pattern the small, very
distinctive `GetTextData` prologue (writes `0xFFFFFF00` twice + `mov byte [reg+0x0F], 0xC0`). Validate
before hooking.

> NOTE: `CGadgetAgent::SetColor(uint)` (WASM `ram:80bb6337`) is a **different** thing - it tints the
> gadget's 3D **model texture** (via `ModelGetTexture` / `GrTextureAdjustColors` / `ConstGetColorShift`),
> not the name tag. Useful if you ever want to recolor the gadget object itself, but irrelevant to
> name-tag recolor.

---

## 3. Item path - mechanism RE COMPLETE, color source pin remaining

### 3.1 `CItemAgent::GetTextData`

- WASM: `IAgentView::CItemAgent::GetTextData` @ `ram:80b20fb6`
- **EXE 06-14: `FUN_007fa6a0`** (`__thiscall`), verified by the ownership-color math and the
  `ItemCliGetCodedName(item_id, 0xFF/0xFE, 0)` call.

EXE 06-14 body (cleaned, name-tag branch):

```c
void __thiscall CItemAgent__GetTextData(int this, u32* nameTag, int* subText) {
    // owner id at this+0xC4 ; item id at this+0xC8 (== this+200)
    int owner   = *(int*)(this + 0xC4);
    bool ownerValid = owner && AgentValidate(owner);
    int  reservedToMe = ownerValid && (owner == ManagerGetViewpointAgent());  // -> iVar4 = 1/0
    if (ItemCliValidate(*(u32*)(this + 0xC8)) == 0) { MemZero(nameTag,0x14); MemZero(subText,0x14); return; }

    if (nameTag != 0) {
        nameTag[1] = 1;                                                    // +0x04
        nameTag[0] = ItemCliGetCodedName(*(u32*)(this+0xC8), reservedToMe + 0xFE, 0);  // flag 0xFF mine / 0xFE other
        nameTag[4] = 0;                                                    // +0x10
        int c = (-(uint)(reservedToMe != 0) & 0x7F7F80) - 0x7F7F80;        // 0 if mine, 0xFF808080 if other
        nameTag[2] = c;  // +0x08
        nameTag[3] = c;  // +0x0C
    }
    // subText branch fills description via GetDescription / ItemCliGetCodedName(...,0xFF,0)
}
```

**Key finding (confirms `name_tag_color` SS4):** the two `Color4b` fields carry **ownership only** -
`0x00000000` when the item is reserved to you (transparent -> the label renders in its rarity markup
color), `0xFF808080` gray when reserved to another player. **The rarity color (white/blue/purple/gold/
green) is NOT in these fields.** It lives inside the coded name string as `<c=@ItemRare>...</c>`
markup produced by `ItemCliGetCodedName`.

### 3.2 `ItemCliGetCodedName` - where the markup is produced

- WASM (3-arg): `ItemCliGetCodedName(unsigned int, unsigned int, unsigned int)` @ `ram:80ac1c74`
- **EXE 06-14: `FUN_0083f4f0`** (3-arg form; `FUN_0083f370` is the 2-arg description form).
- Asserts at `ItCliApi.cpp` lines `0x3ec` / `0x3ed` (`"ptr"`, `"ptr->IsDetailHigh()"`).

It looks up the item struct from the item context (`PropGet(EProp 0x10)` -> array at `ctx+0xB8`,
count at `ctx+0xC0`), gathers item fields (EItemType at `item+0x20`, name id at `item+0x34`, quantity
at `item+0x4C`, flags, etc.) and calls **`ItemCommon::CNameComposer::Compose(...)`** (WASM
`ram:80a9d32f`), which builds the encoded name **including the color markup**. `Compose` stores an
`EColor` at composer `+0x30` and emits it through `TextEncodeCat`; the actual rarity->EColor decision
happens inside `Compose`'s `PreProcess` / `ProcessCodes` / `PostProcess` sub-steps (not yet fully
decompiled - the one remaining item pin).

### 3.3 How the rarity color is actually encoded (CORRECTED - it is NOT `<c=@...>` ASCII)

The item **name tag** does not use literal `<c=@ItemRare>` ASCII markup (that form is used in item
**descriptions/tooltips**). The name color is emitted through GW's **encoded-string** system as a
leading **color control word** in the encoded `wchar_t` stream. Chain (all in
`ItemName.cpp` / `CNameComposer`):

- `CNameComposer::StatementInc(EPriority, EColor, ...)` stores the chosen `EColor` at composer
  `+0x30` and `m_colorCurr` at composer `+0x0C`.
- `CNameComposer::FinalizeStatement` (WASM `ram:80a7bbab`) reads `m_colorCurr` (`+0x0C`) and indexes
  the **`s_colorText[]` color-word table** to get the encoded color-open template id, then
  `TextEncode`s it around the statement text. Guarded by asserts
  `"m_colorCurr < arrsize(s_colorText)"` (`ItemName.cpp:0x366`) and
  `"s_colorText[m_colorCurr] != TEXT..."` (`0x367`).
- The rarity->`EColor` decision is spread across `PreProcess` (`ram:80a9bde2`) / `PostProcess`
  (`ram:80a9cc00`) from item properties (EItemType `item+0x20`, flag word `item+0x1C`, rarity/req
  bits in `item+0x14` / `item+0x7`).

**`s_colorText[]` table (WASM `ram:0015ca10`)** - the name-tag color-word template ids, indexed by
`m_colorCurr`:

```
[0]=0x93e [1]=0x93b [2]=0x187ca [3]=0x93c [4]=0x942 [5]=0x940 [6]=0x943
[7]=0x1011c [8]=0x1182f [9]=0x187ca [10]=0x98c [11]=0 [12]=0x98e [13]=0x990 [14]=0x991 [15]=0x992
```

These `0x9xx`-range ids are the **name-tag** color words; the `0xA3B..0xA43` range is the parallel
**description/markdown** set (`<c=@Item*>`). EXE anchor for the table: `ItemName.cpp` string @
`0x00bc48c0`, `m_colorCurr` assert @ `0x00bc4924` - pin `FinalizeStatement` in the EXE via that
xref, then read the table it indexes (WASM->EXE map of the table itself is the one remaining address
to pin if the table-patch lever is chosen).

### 3.4 Tie-in: the Python project already has rarity + encoded-string color

Two existing project facilities make the item lever tractable without re-RE'ing anything:

- **Ground-item rarity is already resolved.** `Item.Rarity.GetRarity(item_id)` ->
  `Item.item_instance(item_id).rarity` (reads the item struct), and item<->agent id mapping exists
  (`agent_id` / `agent_item_id`). A ground item is an item agent, so agent -> item_id -> rarity works
  today. (`Py4GWCoreLib/Item.py`.)
- **The encoded color words are already known.** `Py4GWCoreLib/native_src/internals/encoded_strings.py`
  `GWEncoded` defines the item color control words as encoded bytes -
  `ITEM_RARE = [0x40,0xA,0xA,0x1]` (template `0x0A40`), `ITEM_UNCOMMON` (`0x0A42`),
  `ITEM_UNIQUE` (`0x0A43`), `ITEM_ENHANCE`, `ITEM_DULL`, ... - and `GWStringEncoded.get_rarity_bytes(Rarity)`
  already maps `Rarity -> color bytes`. `string_table.py` (base-`0x0100` encoding) round-trips these,
  and its char table includes `#`, so an arbitrary `<c=#RRGGBB>` color is also encodable.

So the mapping `rarity -> desired color word` is a small table we already own; the native side only
needs to mirror it (or a `#RRGGBB` word for arbitrary colors).

### 3.5 Item recolor levers (revised)

Overwriting the `CItemAgent::GetTextData` `Color4b` words (`+0x08`/`+0x0C`) only sets the **base**
color behind the encoded color word - it will NOT change the rarity-colored name text. The real
options, best first:

1. **Hook `CItemAgent::GetTextData` (`FUN_007fa6a0`) and recolor the encoded name in place
   (RECOMMENDED - scoped to ground labels).** After the original runs, `nameTag[0]` holds the encoded
   name pointer and `this+0xC8` holds the item id. Resolve rarity (or a per-item rule), and if it
   matches, write a **recolored copy** of the encoded string into a rotating static/TLS buffer -
   replace the leading color-open word (an `s_colorText`-range id) with the desired rarity word, or
   inject an encoded `<c=#RRGGBB>` word for arbitrary RGB - then store the buffer pointer back into
   `nameTag[0]`. The game copies it into the tag immediately, so a rotating buffer is safe. This
   confines the recolor to **ground-item name tags** only.
2. **Hook `ItemCliGetCodedName` (`FUN_0083f4f0`) and rewrite the returned encoded string.** Same
   rewrite, but affects **every** coded-name consumer (inventory, chat links, tooltips), not just
   ground labels. Use only if you want the recolor everywhere.
3. **Patch the `s_colorText[]` table (global, per color-category).** Repoint a color index to a
   different existing color-word template - recolors all items in that category with zero per-frame
   cost, but only to another **existing** game color (not arbitrary RGB). Needs the 06-14 table
   address pinned (SS3.3).

---

## 4. EXE 06-14 <-> WASM address map (new pins from this pass)

| Item | Gw.wasm | Gw.exe (06-14) | Status |
|------|---------|----------------|--------|
| `CGadgetAgent::GetTextData` - **gadget hook target** | `ram:80bb4451` | **`FUN_007f9950`** | verified |
| `GadgetCliGetCodedName` - gadget anchor | `ram:80bb279e` | **`FUN_00839630`** | verified (asserts 0x1ad/0x1ae/0x24b) |
| `CGadgetAgent::SetColor` (model tint, not name tag) | `ram:80bb6337` | not pinned | - |
| `CItemAgent::GetTextData` - item base/ownership hook | `ram:80b20fb6` | **`FUN_007fa6a0`** | verified |
| `ItemCliGetCodedName` (3-arg) - item markup source / hook | `ram:80ac1c74` | **`FUN_0083f4f0`** | verified |
| `ItemCliGetCodedName` (2-arg, description) | - | `FUN_0083f370` | verified (sibling) |
| `ItemCommon::CNameComposer::Compose` - rarity->EColor | `ram:80a9d32f` | not pinned | rarity switch not yet decompiled |
| `CCharAgent::GetTextData` (reference) | `ram:80b7faa7` | `FUN_007f0620` | from agent doc |
| `@ItemRare` markup name string | - | `0x00b96ff0` | verified (table relocated on 06-14) |
| `@`-name string-pointer array | - | `~0x00bf4844` | verified (ARGB storage TBD) |

Anchor strings: `P:\Code\Gw\Item\Cli\ItCliApi.cpp` @ EXE `0x00b90754`;
`P:\Code\Gw\Gadget\Cli\GdCliApi.cpp` @ EXE `0x00b90508`.

---

## 5. Native implementation (fully C++, no Python dependency) - IMPLEMENTED

Shipped as a **single unified `GW::agent_recolor::AgentRecolor` class** (one module, one
`PyAgentRecolor` binding, one `PyAgentRecolor.pyi` stub) that owns all three detours and rule stores -
agents, gadgets, and items are one unit, not three modules. Snapshot rule stores, per-category
enable, combined diagnostics, `CrashContextScope`, `offsets/agent_recolor.json` resolution. The
sections below describe each category's mechanism; the class simply installs all three hooks in
`AgentRecolor::Initialize()`.

### 5.1 Gadget recolor - ready to build, zero porting

- Hook `FUN_007f9950` (`CGadgetAgent::GetTextData`), `__thiscall` emulated as `__fastcall`.
- After the original runs, overwrite `nameTag[2]` (`+0x08`) and `nameTag[3]` (`+0x0C`) with the rule
  color; id at `this+0x2C`.
- Resolution: `offsets/gadget_recolor.json` - anchor `GadgetCliGetCodedName` via
  `FindAssertion("GdCliApi.cpp", "result", 0x1ad)` (= `FUN_00839630`), or byte-pattern the
  `GetTextData` prologue (`C7 46 0C 00 FF FF FF ... C7 46 08 00 FF FF FF C6 46 0F C0`).
- Lives as a sibling `GW::gadget_recolor` module (or a second hook in `agent_recolor`).

### 5.2 Item recolor - ready to build, ~zero porting

The native `GW::Context::Item` struct **already has** what Python's `Item.Rarity` provides:
`GetRarity()`, `IsGold()/IsPurple()/IsGreen()/IsBlue()`, the `interaction` field (`+0x28`), and the
encoded `single_item_name`. Crucially `IsBlue()` checks `single_item_name[0] == 0xA3F` - i.e. **the
first `wchar_t` of the encoded item name IS the color control word** (`0x0A3F` = `ITEM_ENHANCE`/blue).
Recoloring a label = replace that leading word.

- Hook `FUN_007fa6a0` (`CItemAgent::GetTextData`). After the original runs, `nameTag[0]` is the
  encoded name pointer; item id is at `this+0xC8`.

> **CRITICAL (crash learned in-client 2026-07-12).** The leading `0x0A3B..0x0A43` wchar is a color
> **template opcode that wraps the name as a parameter** (`0x0A3F 0x010A <name> ...`), NOT a
> standalone tag. **Stripping it** leaves a dangling param and trips `TextParser.cpp:724
> IsParam(data)` -> crash (seen on a white item; its opcode is `ITEM_COMMON`/`ITEM_DULL`). Never strip.
>
> **TRUE arbitrary RGB (implemented).** GW's `CtlTextMl` renders `<c=#RRGGBB>...</c>` markup, and the
> game itself calls `Ui_CreateEncodedText` (`FUN_007c3be0`) *from inside* `CItemAgent::GetTextData`
> (the description branch) - so calling it from our hook is the exact same context (no reentrancy
> risk). Mechanism: resolve the item's plain name (async, `ui::AsyncGetItemName`, markup stripped),
> build the literal `<c=#RRGGBB>name</c>`, encode it via `GW::native_ui::CreateEncodedTextLiteral`
> (Ui_CreateEncodedText style 8 / layout 7 = literal-text wrapper), cache per `(model_id, argb)`, and
> point `nameTag[0]` at it. The game never frees `nameTag[0]` (normally the item's persistent cached
> name), so the cached encoded string is reused safely (bounded leak, one per model x color).
> **Fallback:** until the name/encoder is ready, or when matched with no model (agent_id-only), swap
> the leading opcode to the nearest **palette** color (`ITEM_COMMON` white, `ITEM_DULL` gray,
> `ITEM_ENHANCE` cyan/blue, `ITEM_RARE` gold, `ITEM_RESTRICT` red, `ITEM_UNCOMMON` violet,
> `ITEM_UNIQUE` green) - structure preserved, never strips. Agents/gadgets use a raw `Color4b`.
>
> **VALIDATE IN-CLIENT:** the `Ui_CreateEncodedText` style/layout `(8, 7)` is the window-title literal
> form; confirm it yields a `CtlTextMl`-parseable markup string on the name-tag path. If the label
> shows literal `<c=#..>` text instead of color, adjust the style/layout params (soft failure, not a
> crash).

- Resolve the item (existing item context lookup) -> `GetRarity()`. If a rule matches (per-rarity or
  per-item): copy the encoded string into a rotating static/TLS `wchar_t` buffer, **swap**
  `buffer[0]` to the nearest palette color opcode, store the buffer back into `nameTag[0]`. The game copies it
  into the tag immediately, so the rotating buffer is safe. Scopes recolor to **ground-item labels**.
- **Port needed (tiny):** a `rarity -> color-word` constant table, mirrored from Python
  `GWStringEncoded.get_rarity_bytes` / `GWEncoded.ITEM_*`:

  | Rarity | color word (leading `wchar_t`) | source constant |
  |--------|-------------------------------|-----------------|
  | White/Common | `0x0A3D` (`ITEM_COMMON`) / `0x0A3B` basic | `GWEncoded.ITEM_COMMON` |
  | Blue | `0x0A3F` (`ITEM_ENHANCE`) | `GWEncoded.ITEM_ENHANCE` |
  | Purple/Uncommon | `0x0A42` (`ITEM_UNCOMMON`) | `GWEncoded.ITEM_UNCOMMON` |
  | Gold/Rare | `0x0A40` (`ITEM_RARE`) | `GWEncoded.ITEM_RARE` |
  | Green/Unique | `0x0A43` (`ITEM_UNIQUE`) | `GWEncoded.ITEM_UNIQUE` |
  | Gray/Dull | `0x0A3E` (`ITEM_DULL`) | `GWEncoded.ITEM_DULL` |

- **Arbitrary RGB (optional):** the palette words above are fixed. For an arbitrary `#RRGGBB`, prepend
  an encoded `<c=#RRGGBB>` sequence instead of swapping the single word - that requires porting the
  small string-table number/hex encoder (`GWEncoded._encode_string_table_number`, base-`0x0100`) from
  Python. Do this only if arbitrary color (beyond the 6 rarity palette entries) is required.
- Resolution: `offsets/item_recolor.json` - `CItemAgent::GetTextData` anchored via
  `ItemCliGetCodedName` (`ItCliApi.cpp:0x3ec`) or its ownership-color prologue.

### 5.3 Recolor rule keys - rarity is just one option

The recolor **mechanism** (rewrite `nameTag[0]`'s leading color word, or inject `#RRGGBB`) is identical
no matter what you match on. Only the **rule lookup** changes, and every identity field is already on
the native `GW::Context::Item` struct (resolved from `this+0xC8` in the `CItemAgent::GetTextData`
hook). So the rule store is multi-keyed with a precedence order:

| Match by | Native source (already exists) | Notes |
|----------|-------------------------------|-------|
| **Agent id** (one ground item instance) | `this+0x2C` on the view / `item->agent_id` | most specific |
| **Item id** | `this+0xC8` | specific dropped item |
| **Model id** (item "kind" / robust "by name") | `item->model_id` (`+0x2C`), `item->model_file_id` (`+0x1C`) | numeric, stable - the practical "recolor all Ectos" |
| **Item type** | `item->type` (`+0x20`, `GW::Constants::ItemType`) | recolor all weapons / trophies / materials / etc. |
| **Rarity** | `item->GetRarity()` / `IsGold()` etc. | broadest |
| **Display name (text)** | `ui::AsyncGetItemName(item, wstr)` -> decoded `std::wstring` | substring/exact match; see caveat |

Suggested precedence: agent_id > item_id > model_id > name > type > rarity (first match wins).

**By item type / model id / rarity:** trivial - all are synchronous field reads on the item struct,
same cost as the rarity path. No extra RE, no porting.

**By display name (text):** the game's decoder is already native - `ui::AsyncDecodeStr(enc, &wstr)`
and `AsyncGetItemName(const Item*, std::wstring&)` (`src/GW/ui/ui_methods.cpp`,
`src/GW/item/item_methods.cpp`). **No string-table codec port needed.** One caveat: decoding is
**async** (callback over a frame or two), so you cannot decode synchronously inside the
`GetTextData` hook. Pattern: maintain a `model_id -> decoded_name` cache populated off the hot path
(via `AsyncGetItemName`), evaluate name-rules against that cache to produce a `model_id -> color`
decision, and the hook just does the synchronous `model_id` (or item_id) lookup. This also means a
name rule effectively resolves to a model-id rule once the name is known.

### 5.4 What is NOT needed

The full `string_table.py` decode/encode codec does **not** need porting for palette recolor - the
recolor is a single leading-word overwrite on an already-encoded string. Only the arbitrary-`#RRGGBB`
path needs the small numeric encoder. The `s_colorText[]` table patch (SS3.5 lever 3) is optional and
only if you want a zero-hook global recolor.

## 6. Confidence and open questions

**Verified (static, WASM + EXE 06-14):** all three `GetTextData` bodies and the `TextData` layout; the
gadget inline `0xFFFFFF00` color and `this+0x2C` id; the item ownership-color math and the
`0xFF/0xFE` coded-name flag; `ItemCliGetCodedName` / `GadgetCliGetCodedName` EXE pins; that item
rarity is markup, not a `Color4b` field.

**Open:**
- `CNameComposer::Compose` rarity->`EColor` switch (which item field selects `@ItemRare` vs
  `@ItemCommon` etc.) - decompile `PreProcess`/`ProcessCodes`/`PostProcess`.
- The 06-14 `@`-name **ARGB storage** address (name-pointer array is pinned at `~0x00bf4844`; the
  color array is stale at the old `0x00bef348`).
- In-client validation of both hooks (gadget `GetTextData` override; item markup rewrite).

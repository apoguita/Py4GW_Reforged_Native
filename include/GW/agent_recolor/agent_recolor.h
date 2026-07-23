#pragma once

#include "base/error_handling.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace GW::Context { struct Item; }

// AgentRecolor (legacy AgentTagColor, py_agent_tag_color.h; renamed on request)
// ----------------------------------------------------------------------------
// One class owning ALL overhead name-tag recoloring: living agents, gadgets,
// and ground items. Each category uses a different native mechanism, so the
// class installs three detours and holds three rule stores, but it is a single
// unit exposed as the single PyAgentRecolor module.
//
//  - Living agents (players/NPCs/enemies): detour the color RESOLVER
//    GetConsiderColor (EXE FUN_007f02e0). The game re-reads it on every agent
//    update; the detour overwrites *out for matching agents. id at view+0x2C.
//  - Gadgets (signposts/chests/shrines/collectors): detour
//    CGadgetAgent::GetTextData (EXE FUN_007f9950); overwrite the two Color4b
//    words (TextData +0x08/+0x0C) it writes. id at view+0x2C.
//  - Ground items: detour CItemAgent::GetTextData (EXE FUN_007fa6a0); for a
//    matching item set the base Color4b and strip the encoded name's leading
//    color control word so the base color governs (arbitrary per-item RGB).
//    item id at view+0xC8, agent id at view+0x2C.
//
// Colors are ARGB 0xAARRGGBB (opaque red = 0xFFFF0000).
// RE: docs/RE/name_tag_color_reverse_engineering.md (agents) and
//     docs/RE/item_gadget_recolor_reverse_engineering.md (items/gadgets).
// Scan inputs live in offsets/agent_recolor.json.

namespace GW::agent_recolor {

using Item = GW::Context::Item;

bool Initialize();
void Shutdown();

// GetConsiderColor RESOLVER (EXE FUN_007f02e0), __thiscall emulated as
// __fastcall: `this` in ECX, unused EDX, then stack args (out, flag). RET 8.
using ResolverFn = uint32_t*(__fastcall*)(void* view, void* edx, uint32_t* out, int flag);
// ManagerFindChar (EXE FUN_007fc920), __cdecl: agent_id -> CCharAgent view or null.
using FindCharFn = void*(__cdecl*)(uint32_t agent_id);
// CGadgetAgent/CItemAgent::GetTextData (EXE FUN_007f9950 / FUN_007fa6a0),
// __thiscall emulated as __fastcall. Trailing args are TextData* (0x14 bytes).
using TextDataFn = void(__fastcall*)(void* view, void* edx, uint32_t* name_tag, uint32_t* sub_text);

// Module-owned resolved symbols.
extern ResolverFn g_resolver;
extern ResolverFn g_resolver_original;
extern FindCharFn g_find_char;
extern TextDataFn g_char_get_text_data;
extern TextDataFn g_char_get_text_data_original;
extern TextDataFn g_gadget_get_text_data;
extern TextDataFn g_gadget_get_text_data_original;
extern TextDataFn g_item_get_text_data;
extern TextDataFn g_item_get_text_data_original;

// Resolvers (bodies in agent_recolor_patterns.cpp; inputs in the JSON).
bool ResolveFindCharFunction();
bool ResolveConsiderColorResolver();
bool ResolveCharGetTextData();
bool ResolveGadgetGetTextData();
bool ResolveItemGetTextData();

class AgentRecolor {
public:
    struct Diagnostics {
        bool initialized = false;
        // per-category hook install + enable state
        bool agent_hook_installed = false;
        bool char_tag_hook_installed = false;
        bool gadget_hook_installed = false;
        bool item_hook_installed = false;
        bool agent_enabled = false;
        bool gadget_enabled = false;
        bool item_enabled = false;
        // agent counters
        uint32_t resolver_calls_seen = 0;
        uint32_t agent_rule_hits = 0;
        uint32_t allegiance_rule_hits = 0;
        uint32_t last_agent_id = 0;
        uint32_t last_agent_color = 0;
        // gadget counters
        uint32_t gadget_calls_seen = 0;
        uint32_t gadget_rule_hits = 0;
        uint32_t gadget_all_hits = 0;
        uint32_t last_gadget_id = 0;
        uint32_t last_gadget_color = 0;
        // item counters
        uint32_t item_calls_seen = 0;
        uint32_t item_rule_hits = 0;
        uint32_t last_item_id = 0;
        uint32_t last_item_model = 0;
        uint32_t last_item_color = 0;
        uint32_t item_name_cache_size = 0;
    };

    static AgentRecolor& Instance();

    void Initialize();   // resolve + install all three detours (safe once at DLL init)
    void Terminate();

    // ===== Living agents =====
    void Enable();
    void Disable();
    bool IsEnabled() const;
    bool IsHookInstalled() const;
    void SetAgentColor(uint32_t agent_id, uint32_t argb);
    bool RemoveAgentColor(uint32_t agent_id);
    // Bulk replace: swap the WHOLE per-agent store to exactly `rules`
    // (agent_id, argb). Python computes the full matched set each pass and hands
    // it over in one call; ids not present are dropped. Allegiance rules are a
    // separate store and are left untouched. Simpler than per-id set/remove.
    void SetAgentColors(const std::vector<std::pair<uint32_t, uint32_t>>& rules);
    void SetAllegianceColor(int allegiance, uint32_t argb);   // 1..6
    bool RemoveAllegianceColor(int allegiance);
    void ClearRules();
    std::map<uint32_t, uint32_t> GetAgentRules() const;
    std::map<int, uint32_t> GetAllegianceRules() const;
    uint32_t ReadConsiderColor(uint32_t agent_id);
    uint32_t ApplyAgentOverride(uint32_t agent_id, uint32_t resolved_color);
    // Returns the agent's rule color (with alpha) if enabled and a rule matches;
    // used by the CCharAgent::GetTextData hook to apply fade/hide to the name
    // tag (the resolver alone cannot: the game forces the dim alpha to 0xC0 and
    // cannot blank the tag). Game-thread.
    bool AgentRuleColor(uint32_t agent_id, uint32_t* out_argb);
    bool CharIsHookInstalled() const;

    // ===== Gadgets =====
    void GadgetEnable();
    void GadgetDisable();
    bool GadgetIsEnabled() const;
    bool GadgetIsHookInstalled() const;
    void SetGadgetColor(uint32_t agent_id, uint32_t argb);
    bool RemoveGadgetColor(uint32_t agent_id);
    // Bulk replace of the per-gadget store (see SetAgentColors). The "all
    // gadgets" color is a separate flag and is left untouched.
    void SetGadgetColors(const std::vector<std::pair<uint32_t, uint32_t>>& rules);
    void SetAllGadgetColor(uint32_t argb);
    void ClearAllGadgetColor();
    void GadgetClearRules();
    std::map<uint32_t, uint32_t> GetGadgetRules() const;
    bool HasAllGadgetColor() const;
    uint32_t GetAllGadgetColor() const;
    uint32_t ApplyGadgetOverride(uint32_t agent_id, uint32_t resolved_color);

    // ===== Ground items (rich filter store) =====
    // Precedence: agent_id > item_id > model_id > name > type > rarity.
    void ItemEnable();
    void ItemDisable();
    bool ItemIsEnabled() const;
    bool ItemIsHookInstalled() const;
    void SetItemAgentColor(uint32_t agent_id, uint32_t argb);
    bool RemoveItemAgentColor(uint32_t agent_id);
    void SetItemIdColor(uint32_t item_id, uint32_t argb);
    bool RemoveItemIdColor(uint32_t item_id);
    void SetItemModelColor(uint32_t model_id, uint32_t argb);
    bool RemoveItemModelColor(uint32_t model_id);
    void SetItemTypeColor(int item_type, uint32_t argb);
    bool RemoveItemTypeColor(int item_type);
    void SetItemRarityColor(int rarity, uint32_t argb);   // 0..4
    bool RemoveItemRarityColor(int rarity);
    void SetItemNameColor(const std::wstring& substring, uint32_t argb);  // case-insensitive
    bool RemoveItemNameColor(const std::wstring& substring);
    void ItemClearRules();
    std::map<uint32_t, uint32_t> GetItemAgentRules() const;
    std::map<uint32_t, uint32_t> GetItemIdRules() const;
    std::map<uint32_t, uint32_t> GetItemModelRules() const;
    std::map<int, uint32_t> GetItemTypeRules() const;
    std::map<int, uint32_t> GetItemRarityRules() const;
    std::vector<std::pair<std::wstring, uint32_t>> GetItemNameRules() const;
    void OnItemGetTextData(void* view, uint32_t* name_tag);

    // ===== Shared =====
    // Master hook switch (driven by the per-account System Settings toggle).
    // Toggles the ACTUAL detours via EnableHooks/DisableHooks so an account with
    // the feature off runs no detour code at all (zero overhead), independent of
    // the per-category enable gates and the rule stores. Hooks are installed once
    // at DLL init; this only flips their active state (default: enabled).
    void MasterEnable();
    void MasterDisable();
    bool IsMasterEnabled() const;

    void ClearAllRules();  // all three categories
    Diagnostics GetDiagnostics() const;
    void ResetDiagnostics();

    // Snapshots (copy-on-write; read lock-free from detours).
    struct AgentSnapshot {
        std::map<uint32_t, uint32_t> agent_rules;
        std::map<int, uint32_t> allegiance_rules;
    };
    struct GadgetSnapshot {
        std::map<uint32_t, uint32_t> gadget_rules;
        bool has_all = false;
        uint32_t all_color = 0;
    };
    struct ItemSnapshot {
        std::map<uint32_t, uint32_t> agent_rules;
        std::map<uint32_t, uint32_t> item_rules;
        std::map<uint32_t, uint32_t> model_rules;
        std::map<int, uint32_t> type_rules;
        std::map<int, uint32_t> rarity_rules;
        std::vector<std::pair<std::wstring, uint32_t>> name_rules;  // lowercased
        bool any() const {
            return !agent_rules.empty() || !item_rules.empty() || !model_rules.empty() ||
                   !type_rules.empty() || !rarity_rules.empty() || !name_rules.empty();
        }
    };

    struct NameEntry {
        std::wstring raw;      // decoded name (may contain <c=@..> markup)
        std::wstring lower;    // lowercased raw, for case-insensitive name matching
        std::wstring display;  // markup-stripped proper-case name, for true-color rebuild
        bool requested = false;
    };

    // Recolored item-name strings kept in OUR OWN stable storage (the engine's
    // Ui_CreateEncodedText returns a shared temp buffer recycled by other UI
    // text, so we must copy). Both members are set at most ONCE and never
    // reassigned, so any pointer we hand the game (`.c_str()`) stays valid for
    // the process lifetime - even across the palette->true-color upgrade (we
    // return `truecolor` once ready but keep `palette` alive for any lingering
    // holder, so nothing is ever freed under the game).
    struct ColorEntry {
        std::wstring palette;    // fallback opcode-swap copy (set once)
        std::wstring truecolor;  // arbitrary-RGB <c=#..> form (set once, when ready)
    };

private:
    AgentRecolor() = default;
    AgentRecolor(const AgentRecolor&) = delete;
    AgentRecolor& operator=(const AgentRecolor&) = delete;

    void RebuildAgentSnapshotLocked();
    void RebuildGadgetSnapshotLocked();
    void RebuildItemSnapshotLocked();
    // Returns the resolved name entry (raw/lower/display) for `model_id`, or
    // nullptr if not yet resolved (a decode is requested on first sight).
    const NameEntry* ResolveItemName(uint32_t model_id, const Item* item);
    // Returns a STABLE recolored encoded name for (model_id, argb): the true
    // arbitrary-RGB `<c=#RRGGBB>name</c>` once the name resolves, else a palette
    // opcode-swap copy of `game_name_src`. nullptr if nothing can be built
    // (no model, or source has no color opcode to swap). Game-thread only.
    const wchar_t* GetItemColorString(uint32_t model_id, uint32_t argb, const Item* item,
                                      const wchar_t* game_name_src);

    mutable std::mutex mutex_;

    // agent rules
    std::map<uint32_t, uint32_t> agent_rules_;
    std::map<int, uint32_t> allegiance_rules_;
    std::shared_ptr<const AgentSnapshot> agent_snapshot_;

    // gadget rules
    std::map<uint32_t, uint32_t> gadget_rules_;
    bool gadget_has_all_ = false;
    uint32_t gadget_all_color_ = 0;
    std::shared_ptr<const GadgetSnapshot> gadget_snapshot_;

    // item rules
    std::map<uint32_t, uint32_t> item_agent_rules_;
    std::map<uint32_t, uint32_t> item_id_rules_;
    std::map<uint32_t, uint32_t> item_model_rules_;
    std::map<int, uint32_t> item_type_rules_;
    std::map<int, uint32_t> item_rarity_rules_;
    std::vector<std::pair<std::wstring, uint32_t>> item_name_rules_;  // lowercased
    std::shared_ptr<const ItemSnapshot> item_snapshot_;

    // item name cache (game-thread only)
    std::map<uint32_t, NameEntry> item_name_cache_;
    // (model_id<<32 | argb) -> stable recolored encoded name (game-thread only)
    std::map<uint64_t, ColorEntry> item_color_cache_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> master_enabled_{true};   // hooks active after install; master gate
    std::atomic<bool> agent_enabled_{false};
    std::atomic<bool> gadget_enabled_{false};
    std::atomic<bool> item_enabled_{false};
    std::atomic<bool> agent_hook_installed_{false};
    std::atomic<bool> char_tag_hook_installed_{false};
    std::atomic<bool> gadget_hook_installed_{false};
    std::atomic<bool> item_hook_installed_{false};

    // diagnostics
    std::atomic<uint32_t> diag_resolver_calls_{0};
    std::atomic<uint32_t> diag_agent_hits_{0};
    std::atomic<uint32_t> diag_allegiance_hits_{0};
    std::atomic<uint32_t> diag_last_agent_{0};
    std::atomic<uint32_t> diag_last_agent_color_{0};
    std::atomic<uint32_t> diag_gadget_calls_{0};
    std::atomic<uint32_t> diag_gadget_hits_{0};
    std::atomic<uint32_t> diag_gadget_all_hits_{0};
    std::atomic<uint32_t> diag_last_gadget_{0};
    std::atomic<uint32_t> diag_last_gadget_color_{0};
    std::atomic<uint32_t> diag_item_calls_{0};
    std::atomic<uint32_t> diag_item_hits_{0};
    std::atomic<uint32_t> diag_last_item_{0};
    std::atomic<uint32_t> diag_last_item_model_{0};
    std::atomic<uint32_t> diag_last_item_color_{0};
    std::atomic<uint32_t> diag_item_name_cache_size_{0};
};

}  // namespace GW::agent_recolor

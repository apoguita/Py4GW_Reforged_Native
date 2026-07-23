#include "base/error_handling.h"

#include "GW/agent_recolor/agent_recolor.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "GW/agent/agent.h"
#include "GW/context/agent.h"
#include "GW/item/item.h"
#include "GW/context/item.h"
#include "GW/common/constants/item.h"
#include "GW/native_ui/native_ui.h"
#include "GW/game_thread/game_thread.h"

#include <cwctype>
#include <cwchar>

namespace GW::agent_recolor {

ResolverFn g_resolver = nullptr;
ResolverFn g_resolver_original = nullptr;
FindCharFn g_find_char = nullptr;
TextDataFn g_char_get_text_data = nullptr;
TextDataFn g_char_get_text_data_original = nullptr;
TextDataFn g_gadget_get_text_data = nullptr;
TextDataFn g_gadget_get_text_data_original = nullptr;
TextDataFn g_item_get_text_data = nullptr;
TextDataFn g_item_get_text_data_original = nullptr;
SetNameTagVisibilityFn g_set_nametag_visibility = nullptr;
uint32_t* g_nametag_flags = nullptr;

namespace {
    int ResolveAgentAllegiance(uint32_t agent_id) {
        Context::Agent* agent = GW::agent::GetAgentByID(agent_id);
        if (!agent)
            return 0;
        Context::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living)
            return 0;
        return static_cast<int>(living->allegiance);
    }

    // ---- living-agent resolver detour ----
    uint32_t* __fastcall Detour_GetConsiderColor(void* view, void* edx, uint32_t* out, int flag) {
        uint32_t* result = g_resolver_original ? g_resolver_original(view, edx, out, flag) : out;
        if (out && view) {
            const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
            *out = AgentRecolor::Instance().ApplyAgentOverride(agent_id, *out);
        }
        return result;
    }

    // ---- living-agent (CCharAgent) name-tag detour: applies fade/hide ----
    // The resolver hook already recolored the tag+ring, but the game forces the
    // dim-variant alpha to 0xC0 and cannot blank the tag, so alpha/hide need this
    // post-pass on the name-tag Color4b. Only ruled agents are touched (others
    // keep the game's default distance-dimming).
    void __fastcall Detour_CharGetTextData(void* view, void* edx, uint32_t* name_tag, uint32_t* sub_text) {
        if (g_char_get_text_data_original)
            g_char_get_text_data_original(view, edx, name_tag, sub_text);
        if (name_tag && view) {
            const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
            uint32_t argb = 0;
            if (AgentRecolor::Instance().AgentRuleColor(agent_id, &argb)) {
                if ((argb >> 24) == 0) {
                    for (int i = 0; i < 5; ++i) name_tag[i] = 0;  // alpha 0 == hide
                }
                else {
                    name_tag[2] = argb;  // honor alpha uniformly (near + dim)
                    name_tag[3] = argb;
                }
            }
        }
    }

    // ---- gadget GetTextData detour ----
    void __fastcall Detour_GadgetGetTextData(void* view, void* edx, uint32_t* name_tag, uint32_t* sub_text) {
        if (g_gadget_get_text_data_original)
            g_gadget_get_text_data_original(view, edx, name_tag, sub_text);
        if (name_tag && view) {
            const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
            const uint32_t overridden = AgentRecolor::Instance().ApplyGadgetOverride(agent_id, name_tag[2]);
            if (overridden != name_tag[2]) {
                if ((overridden >> 24) == 0) {
                    // alpha 0 == HIDE: blank the tag (matches the game's no-tag path).
                    for (int i = 0; i < 5; ++i) name_tag[i] = 0;
                }
                else {
                    // Honor the requested alpha uniformly (near + dim) so fades work.
                    name_tag[2] = overridden;
                    name_tag[3] = overridden;
                }
            }
        }
    }

    // ---- item GetTextData detour ----
    void __fastcall Detour_ItemGetTextData(void* view, void* edx, uint32_t* name_tag, uint32_t* sub_text) {
        if (g_item_get_text_data_original)
            g_item_get_text_data_original(view, edx, name_tag, sub_text);
        if (name_tag && view)
            AgentRecolor::Instance().OnItemGetTextData(view, name_tag);
    }

    // An encoded item name begins with a COLOR TEMPLATE opcode in the ITEM_*
    // markdown range (0x0A3B ITEM_BASIC .. 0x0A43 ITEM_UNIQUE). It is NOT a
    // standalone tag - it wraps the name as a parameter (e.g. `0x0A3F 0x010A
    // <name>`). Stripping it leaves a dangling param and trips the game's
    // TextParser `IsParam(data)` assertion (crash). All opcodes in this range
    // share the same param structure, so we RECOLOR by swapping just the opcode
    // wchar to another palette opcode - the string stays well-formed.
    inline bool IsItemColorWord(wchar_t w) { return w >= 0x0A3B && w <= 0x0A43; }

    // Palette: color-template opcode -> its representative ARGB (from the legacy
    // named-color table). GW encodes item-label color as a template, so only
    // these palette colors are reachable via the name string; an arbitrary ARGB
    // is snapped to the nearest one. (Agents/gadgets use a raw Color4b and are
    // NOT limited this way.)
    struct PaletteEntry { wchar_t opcode; uint32_t argb; };
    constexpr PaletteEntry kPalette[] = {
        {0x0A3D, 0xFFFFFFFFu},  // ITEM_COMMON   white
        {0x0A3E, 0xFFA0A0A0u},  // ITEM_DULL     gray
        {0x0A3F, 0xFFA0F5F8u},  // ITEM_ENHANCE  cyan/blue
        {0x0A40, 0xFFFFD24Fu},  // ITEM_RARE     gold
        {0x0A41, 0xFFED0002u},  // ITEM_RESTRICT red
        {0x0A42, 0xFFB38AECu},  // ITEM_UNCOMMON violet/purple
        {0x0A43, 0xFF00FF00u},  // ITEM_UNIQUE   green
    };

    wchar_t NearestPaletteOpcode(uint32_t argb) {
        const int r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
        wchar_t best = kPalette[0].opcode;
        long best_d = 0x7FFFFFFF;
        for (const auto& p : kPalette) {
            const int pr = (p.argb >> 16) & 0xFF, pg = (p.argb >> 8) & 0xFF, pb = p.argb & 0xFF;
            const long d = (long)(r - pr) * (r - pr) + (long)(g - pg) * (g - pg) + (long)(b - pb) * (b - pb);
            if (d < best_d) { best_d = d; best = p.opcode; }
        }
        return best;
    }

    // Copy a null-terminated (encoded) wchar string into an owned std::wstring
    // (capped). The engine's Ui_CreateEncodedText and the item's cached name are
    // both memory we must NOT alias long-term, so we snapshot into stable storage.
    std::wstring CopyEncoded(const wchar_t* src) {
        std::wstring out;
        if (!src)
            return out;
        for (int i = 0; i < 512 && src[i] != 0; ++i)
            out.push_back(src[i]);
        return out;
    }

    std::wstring ToLower(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s)
            out.push_back(static_cast<wchar_t>(std::towlower(c)));
        return out;
    }

    // Strip a leading `<c=...>` open tag and trailing `</c>` close tags from a
    // decoded item name, leaving the bare display text.
    std::wstring StripMarkup(const std::wstring& s) {
        std::wstring r = s;
        if (r.rfind(L"<c=", 0) == 0) {
            const size_t p = r.find(L'>');
            if (p != std::wstring::npos)
                r = r.substr(p + 1);
        }
        const std::wstring close = L"</c>";
        while (r.size() >= close.size() && r.compare(r.size() - close.size(), close.size(), close) == 0)
            r = r.substr(0, r.size() - close.size());
        return r;
    }
}  // namespace

bool Initialize() {
    AgentRecolor::Instance().Initialize();
    return true;
}

void Shutdown() {
    AgentRecolor::Instance().Terminate();
}

AgentRecolor& AgentRecolor::Instance() {
    static AgentRecolor instance;
    return instance;
}

void AgentRecolor::Initialize() {
    if (initialized_.exchange(true))
        return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        RebuildAgentSnapshotLocked();
        RebuildGadgetSnapshotLocked();
        RebuildItemSnapshotLocked();
    }

    // --- living-agent resolver hook ---
    if (ResolveFindCharFunction() && ResolveConsiderColorResolver()) {
        CrashContextScope context("startup", "agent_recolor", "install_resolver_hook");
        if (Logger::AssertHook(
                "GetConsiderColor_Resolver_Func",
                PY4GW::HookBase::CreateHook(
                    reinterpret_cast<void**>(&g_resolver),
                    reinterpret_cast<void*>(&Detour_GetConsiderColor),
                    reinterpret_cast<void**>(&g_resolver_original)),
                "agent_recolor")) {
            PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_resolver));
            agent_hook_installed_.store(true);
        }
    }

    // --- living-agent name-tag hook (fade/hide; the resolver alone can't) ---
    if (ResolveCharGetTextData()) {
        CrashContextScope context("startup", "agent_recolor", "install_char_tag_hook");
        if (Logger::AssertHook(
                "CCharAgent_GetTextData_Func",
                PY4GW::HookBase::CreateHook(
                    reinterpret_cast<void**>(&g_char_get_text_data),
                    reinterpret_cast<void*>(&Detour_CharGetTextData),
                    reinterpret_cast<void**>(&g_char_get_text_data_original)),
                "agent_recolor")) {
            PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_char_get_text_data));
            char_tag_hook_installed_.store(true);
        }
    }

    // --- gadget GetTextData hook ---
    if (ResolveGadgetGetTextData()) {
        CrashContextScope context("startup", "agent_recolor", "install_gadget_hook");
        if (Logger::AssertHook(
                "CGadgetAgent_GetTextData_Func",
                PY4GW::HookBase::CreateHook(
                    reinterpret_cast<void**>(&g_gadget_get_text_data),
                    reinterpret_cast<void*>(&Detour_GadgetGetTextData),
                    reinterpret_cast<void**>(&g_gadget_get_text_data_original)),
                "agent_recolor")) {
            PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_gadget_get_text_data));
            gadget_hook_installed_.store(true);
        }
    }

    // --- item GetTextData hook ---
    if (ResolveItemGetTextData()) {
        CrashContextScope context("startup", "agent_recolor", "install_item_hook");
        if (Logger::AssertHook(
                "CItemAgent_GetTextData_Func",
                PY4GW::HookBase::CreateHook(
                    reinterpret_cast<void**>(&g_item_get_text_data),
                    reinterpret_cast<void*>(&Detour_ItemGetTextData),
                    reinterpret_cast<void**>(&g_item_get_text_data_original)),
                "agent_recolor")) {
            PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_item_get_text_data));
            item_hook_installed_.store(true);
        }
    }

    // --- name-tag refresh symbols (NOT hooked; called to force a re-render) ---
    // Best-effort: recolor works without these (tags refresh on hover/state change);
    // with them, RefreshNameTags() makes a rule change apply immediately.
    ResolveSetNameTagVisibility();
    ResolveNameTagFlags();
}

void AgentRecolor::Terminate() {
    if (!initialized_.exchange(false))
        return;

    CrashContextScope context("shutdown", "agent_recolor", "remove_hooks");
    if (agent_hook_installed_.exchange(false) && g_resolver) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_resolver));
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_resolver));
    }
    if (char_tag_hook_installed_.exchange(false) && g_char_get_text_data) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_char_get_text_data));
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_char_get_text_data));
    }
    if (gadget_hook_installed_.exchange(false) && g_gadget_get_text_data) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_gadget_get_text_data));
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_gadget_get_text_data));
    }
    if (item_hook_installed_.exchange(false) && g_item_get_text_data) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_item_get_text_data));
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_item_get_text_data));
    }
    g_resolver = nullptr;
    g_resolver_original = nullptr;
    g_find_char = nullptr;
    g_char_get_text_data = nullptr;
    g_char_get_text_data_original = nullptr;
    g_gadget_get_text_data = nullptr;
    g_gadget_get_text_data_original = nullptr;
    g_item_get_text_data = nullptr;
    g_item_get_text_data_original = nullptr;
    agent_enabled_.store(false);
    gadget_enabled_.store(false);
    item_enabled_.store(false);

    std::lock_guard<std::mutex> lock(mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    gadget_rules_.clear();
    gadget_has_all_ = false;
    gadget_all_color_ = 0;
    item_agent_rules_.clear();
    item_id_rules_.clear();
    item_model_rules_.clear();
    item_type_rules_.clear();
    item_rarity_rules_.clear();
    item_name_rules_.clear();
    agent_snapshot_.reset();
    gadget_snapshot_.reset();
    item_snapshot_.reset();
    // item_name_cache_ is intentionally not cleared (a decode may still be
    // writing into a cache node; the singleton lives to process end).
}

// ================= Living agents =================

void AgentRecolor::Enable() { agent_enabled_.store(true); }
void AgentRecolor::Disable() { agent_enabled_.store(false); }
bool AgentRecolor::IsEnabled() const { return agent_enabled_.load(); }
bool AgentRecolor::IsHookInstalled() const { return agent_hook_installed_.load(); }

void AgentRecolor::SetAgentColor(uint32_t agent_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    agent_rules_[agent_id] = argb;
    RebuildAgentSnapshotLocked();
}
bool AgentRecolor::RemoveAgentColor(uint32_t agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = agent_rules_.erase(agent_id) != 0;
    if (erased) RebuildAgentSnapshotLocked();
    return erased;
}
void AgentRecolor::SetAgentColors(const std::vector<std::pair<uint32_t, uint32_t>>& rules) {
    std::lock_guard<std::mutex> lock(mutex_);
    agent_rules_.clear();
    for (const auto& r : rules)
        agent_rules_[r.first] = r.second;
    RebuildAgentSnapshotLocked();
}
void AgentRecolor::SetAllegianceColor(int allegiance, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    allegiance_rules_[allegiance] = argb;
    RebuildAgentSnapshotLocked();
}
bool AgentRecolor::RemoveAllegianceColor(int allegiance) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = allegiance_rules_.erase(allegiance) != 0;
    if (erased) RebuildAgentSnapshotLocked();
    return erased;
}
void AgentRecolor::ClearRules() {
    std::lock_guard<std::mutex> lock(mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    RebuildAgentSnapshotLocked();
}
std::map<uint32_t, uint32_t> AgentRecolor::GetAgentRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return agent_rules_;
}
std::map<int, uint32_t> AgentRecolor::GetAllegianceRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return allegiance_rules_;
}

uint32_t AgentRecolor::ReadConsiderColor(uint32_t agent_id) {
    if (!g_find_char || !g_resolver_original)
        return 0;
    void* view = g_find_char(agent_id);
    if (!view)
        return 0;
    uint32_t color = 0;
    g_resolver_original(view, nullptr, &color, 1);  // flag 1 = name-tag text color
    return color;
}

uint32_t AgentRecolor::ApplyAgentOverride(uint32_t agent_id, uint32_t resolved_color) {
    if (!agent_enabled_.load(std::memory_order_relaxed))
        return resolved_color;

    std::shared_ptr<const AgentSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = agent_snapshot_;
    }

    diag_resolver_calls_.fetch_add(1, std::memory_order_relaxed);
    diag_last_agent_.store(agent_id, std::memory_order_relaxed);

    uint32_t out = resolved_color;
    if (snap) {
        const auto agent_it = snap->agent_rules.find(agent_id);
        if (agent_it != snap->agent_rules.end()) {
            out = agent_it->second;
            diag_agent_hits_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (!snap->allegiance_rules.empty()) {
            const int allegiance = ResolveAgentAllegiance(agent_id);
            const auto alleg_it = snap->allegiance_rules.find(allegiance);
            if (alleg_it != snap->allegiance_rules.end()) {
                out = alleg_it->second;
                diag_allegiance_hits_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    diag_last_agent_color_.store(out, std::memory_order_relaxed);
    return out;
}

bool AgentRecolor::AgentRuleColor(uint32_t agent_id, uint32_t* out_argb) {
    if (!agent_enabled_.load(std::memory_order_relaxed))
        return false;
    std::shared_ptr<const AgentSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = agent_snapshot_;
    }
    if (!snap)
        return false;
    const auto agent_it = snap->agent_rules.find(agent_id);
    if (agent_it != snap->agent_rules.end()) {
        *out_argb = agent_it->second;
        return true;
    }
    if (!snap->allegiance_rules.empty()) {
        const int allegiance = ResolveAgentAllegiance(agent_id);
        const auto alleg_it = snap->allegiance_rules.find(allegiance);
        if (alleg_it != snap->allegiance_rules.end()) {
            *out_argb = alleg_it->second;
            return true;
        }
    }
    return false;
}

bool AgentRecolor::CharIsHookInstalled() const { return char_tag_hook_installed_.load(); }

// ================= Gadgets =================

void AgentRecolor::GadgetEnable() { gadget_enabled_.store(true); }
void AgentRecolor::GadgetDisable() { gadget_enabled_.store(false); }
bool AgentRecolor::GadgetIsEnabled() const { return gadget_enabled_.load(); }
bool AgentRecolor::GadgetIsHookInstalled() const { return gadget_hook_installed_.load(); }

void AgentRecolor::SetGadgetColor(uint32_t agent_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    gadget_rules_[agent_id] = argb;
    RebuildGadgetSnapshotLocked();
}
bool AgentRecolor::RemoveGadgetColor(uint32_t agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = gadget_rules_.erase(agent_id) != 0;
    if (erased) RebuildGadgetSnapshotLocked();
    return erased;
}
void AgentRecolor::SetGadgetColors(const std::vector<std::pair<uint32_t, uint32_t>>& rules) {
    std::lock_guard<std::mutex> lock(mutex_);
    gadget_rules_.clear();
    for (const auto& r : rules)
        gadget_rules_[r.first] = r.second;
    RebuildGadgetSnapshotLocked();
}
void AgentRecolor::SetAllGadgetColor(uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    gadget_has_all_ = true;
    gadget_all_color_ = argb;
    RebuildGadgetSnapshotLocked();
}
void AgentRecolor::ClearAllGadgetColor() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (gadget_has_all_) {
        gadget_has_all_ = false;
        gadget_all_color_ = 0;
        RebuildGadgetSnapshotLocked();
    }
}
void AgentRecolor::GadgetClearRules() {
    std::lock_guard<std::mutex> lock(mutex_);
    gadget_rules_.clear();
    gadget_has_all_ = false;
    gadget_all_color_ = 0;
    RebuildGadgetSnapshotLocked();
}
std::map<uint32_t, uint32_t> AgentRecolor::GetGadgetRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return gadget_rules_;
}
bool AgentRecolor::HasAllGadgetColor() const {
    std::lock_guard<std::mutex> lock(mutex_); return gadget_has_all_;
}
uint32_t AgentRecolor::GetAllGadgetColor() const {
    std::lock_guard<std::mutex> lock(mutex_); return gadget_all_color_;
}

uint32_t AgentRecolor::ApplyGadgetOverride(uint32_t agent_id, uint32_t resolved_color) {
    if (!gadget_enabled_.load(std::memory_order_relaxed))
        return resolved_color;

    std::shared_ptr<const GadgetSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = gadget_snapshot_;
    }

    diag_gadget_calls_.fetch_add(1, std::memory_order_relaxed);
    diag_last_gadget_.store(agent_id, std::memory_order_relaxed);

    uint32_t out = resolved_color;
    if (snap) {
        const auto it = snap->gadget_rules.find(agent_id);
        if (it != snap->gadget_rules.end()) {
            out = it->second;
            diag_gadget_hits_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (snap->has_all) {
            out = snap->all_color;
            diag_gadget_all_hits_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    diag_last_gadget_color_.store(out, std::memory_order_relaxed);
    return out;
}

// ================= Ground items =================

void AgentRecolor::ItemEnable() { item_enabled_.store(true); }
void AgentRecolor::ItemDisable() { item_enabled_.store(false); }
bool AgentRecolor::ItemIsEnabled() const { return item_enabled_.load(); }
bool AgentRecolor::ItemIsHookInstalled() const { return item_hook_installed_.load(); }

void AgentRecolor::SetItemAgentColor(uint32_t agent_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    item_agent_rules_[agent_id] = argb;
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemAgentColor(uint32_t agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = item_agent_rules_.erase(agent_id) != 0;
    if (erased) RebuildItemSnapshotLocked();
    return erased;
}
void AgentRecolor::SetItemIdColor(uint32_t item_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    item_id_rules_[item_id] = argb;
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemIdColor(uint32_t item_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = item_id_rules_.erase(item_id) != 0;
    if (erased) RebuildItemSnapshotLocked();
    return erased;
}
void AgentRecolor::SetItemModelColor(uint32_t model_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    item_model_rules_[model_id] = argb;
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemModelColor(uint32_t model_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = item_model_rules_.erase(model_id) != 0;
    if (erased) RebuildItemSnapshotLocked();
    return erased;
}
void AgentRecolor::SetItemTypeColor(int item_type, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    item_type_rules_[item_type] = argb;
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemTypeColor(int item_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = item_type_rules_.erase(item_type) != 0;
    if (erased) RebuildItemSnapshotLocked();
    return erased;
}
void AgentRecolor::SetItemRarityColor(int rarity, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    item_rarity_rules_[rarity] = argb;
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemRarityColor(int rarity) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool erased = item_rarity_rules_.erase(rarity) != 0;
    if (erased) RebuildItemSnapshotLocked();
    return erased;
}
void AgentRecolor::SetItemNameColor(const std::wstring& substring, uint32_t argb) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::wstring key = ToLower(substring);
    for (auto& rule : item_name_rules_) {
        if (rule.first == key) { rule.second = argb; RebuildItemSnapshotLocked(); return; }
    }
    item_name_rules_.emplace_back(key, argb);
    RebuildItemSnapshotLocked();
}
bool AgentRecolor::RemoveItemNameColor(const std::wstring& substring) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::wstring key = ToLower(substring);
    for (auto it = item_name_rules_.begin(); it != item_name_rules_.end(); ++it) {
        if (it->first == key) { item_name_rules_.erase(it); RebuildItemSnapshotLocked(); return true; }
    }
    return false;
}
void AgentRecolor::ItemClearRules() {
    std::lock_guard<std::mutex> lock(mutex_);
    item_agent_rules_.clear();
    item_id_rules_.clear();
    item_model_rules_.clear();
    item_type_rules_.clear();
    item_rarity_rules_.clear();
    item_name_rules_.clear();
    RebuildItemSnapshotLocked();
}
std::map<uint32_t, uint32_t> AgentRecolor::GetItemAgentRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_agent_rules_;
}
std::map<uint32_t, uint32_t> AgentRecolor::GetItemIdRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_id_rules_;
}
std::map<uint32_t, uint32_t> AgentRecolor::GetItemModelRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_model_rules_;
}
std::map<int, uint32_t> AgentRecolor::GetItemTypeRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_type_rules_;
}
std::map<int, uint32_t> AgentRecolor::GetItemRarityRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_rarity_rules_;
}
std::vector<std::pair<std::wstring, uint32_t>> AgentRecolor::GetItemNameRules() const {
    std::lock_guard<std::mutex> lock(mutex_); return item_name_rules_;
}

void AgentRecolor::OnItemGetTextData(void* view, uint32_t* name_tag) {
    if (!item_enabled_.load(std::memory_order_relaxed))
        return;

    std::shared_ptr<const ItemSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = item_snapshot_;
    }
    if (!snap || !snap->any())
        return;

    const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
    const uint32_t item_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0xC8);

    diag_item_calls_.fetch_add(1, std::memory_order_relaxed);
    diag_last_item_.store(item_id, std::memory_order_relaxed);

    bool matched = false;
    uint32_t argb = 0;

    // Precedence: agent_id > item_id > model_id > name > type > rarity.
    if (!snap->agent_rules.empty()) {
        const auto it = snap->agent_rules.find(agent_id);
        if (it != snap->agent_rules.end()) { argb = it->second; matched = true; }
    }

    // Always fetch the item (cheap array lookup) so the true-color path has the
    // model + name even when the match was by agent_id.
    const Item* item = item_id ? GW::item::GetItemById(item_id) : nullptr;

    if (!matched && !snap->item_rules.empty()) {
        const auto it = snap->item_rules.find(item_id);
        if (it != snap->item_rules.end()) { argb = it->second; matched = true; }
    }

    if (item) {
        const uint32_t model_id = item->model_id;
        diag_last_item_model_.store(model_id, std::memory_order_relaxed);

        if (!matched && !snap->model_rules.empty()) {
            const auto it = snap->model_rules.find(model_id);
            if (it != snap->model_rules.end()) { argb = it->second; matched = true; }
        }
        if (!matched && !snap->name_rules.empty()) {
            const NameEntry* ne = ResolveItemName(model_id, item);
            if (ne && !ne->lower.empty()) {
                for (const auto& rule : snap->name_rules) {
                    if (ne->lower.find(rule.first) != std::wstring::npos) { argb = rule.second; matched = true; break; }
                }
            }
        }
        if (!matched && !snap->type_rules.empty()) {
            const auto it = snap->type_rules.find(static_cast<int>(item->type));
            if (it != snap->type_rules.end()) { argb = it->second; matched = true; }
        }
        if (!matched && !snap->rarity_rules.empty()) {
            const auto it = snap->rarity_rules.find(static_cast<int>(item->GetRarity()));
            if (it != snap->rarity_rules.end()) { argb = it->second; matched = true; }
        }
    }

    if (matched) {
        if ((argb >> 24) == 0) {
            // alpha 0 == HIDE (loot-filter): blank the label. Matches the game's
            // own no-tag path (MemZero on an invalid item), so it is safe.
            for (int i = 0; i < 5; ++i) name_tag[i] = 0;
        }
        else {
            const uint32_t model_id = item ? item->model_id : 0;
            const wchar_t* src = reinterpret_cast<const wchar_t*>(name_tag[0]);
            // A STABLE recolored string (true RGB - alpha honored via <c=0x..>
            // markup - once the name resolves, else a palette opcode-swap). Both
            // live in our own storage, never the engine's recycled temp buffers,
            // so the tag can hold the pointer across frames.
            const wchar_t* colored = GetItemColorString(model_id, argb, item, src);
            if (colored) {
                name_tag[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(colored));
            }
            else {
                // No model / no color opcode: best-effort base Color4b (safe - we
                // do NOT touch the string, so the TextParser can't assert). Alpha
                // honored uniformly for fades.
                name_tag[2] = argb;
                name_tag[3] = argb;
            }
        }
        diag_item_hits_.fetch_add(1, std::memory_order_relaxed);
        diag_last_item_color_.store(argb, std::memory_order_relaxed);
    }
}

const AgentRecolor::NameEntry* AgentRecolor::ResolveItemName(uint32_t model_id, const Item* item) {
    if (!model_id)
        return nullptr;
    NameEntry& entry = item_name_cache_[model_id];
    if (!entry.requested) {
        entry.requested = true;
        diag_item_name_cache_size_.store(static_cast<uint32_t>(item_name_cache_.size()), std::memory_order_relaxed);
        GW::item::AsyncGetItemName(item, entry.raw);  // sync if cached, else fills a stable node later
    }
    if (entry.raw.empty())
        return nullptr;  // still pending
    if (entry.lower.empty())
        entry.lower = ToLower(entry.raw);
    if (entry.display.empty())
        entry.display = StripMarkup(entry.raw);
    return &entry;
}

const wchar_t* AgentRecolor::GetItemColorString(uint32_t model_id, uint32_t argb, const Item* item,
                                                const wchar_t* game_name_src) {
    if (!model_id)
        return nullptr;  // no stable key; caller falls back to base color
    const uint64_t key = (static_cast<uint64_t>(model_id) << 32) | argb;
    ColorEntry& e = item_color_cache_[key];
    if (!e.truecolor.empty())
        return e.truecolor.c_str();  // best form already built and stable

    // Try to build the TRUE arbitrary-RGB form. Needs the plain name. The game
    // itself calls Ui_CreateEncodedText inside GetTextData, so this is a safe
    // context; we then SNAPSHOT the result into our own storage because the
    // engine buffer is a shared temp (recycled by other UI text).
    const NameEntry* ne = ResolveItemName(model_id, item);
    if (ne && !ne->display.empty()) {
        // `<c=0xAARRGGBB>` honors the alpha channel (the `#RRGGBB` form forces
        // opaque). alpha 0 is handled earlier as HIDE, so here alpha is 1..255.
        wchar_t prefix[20] = {};
        std::swprintf(prefix, 20, L"<c=0x%08X>", argb);
        const std::wstring literal = std::wstring(prefix) + ne->display + L"</c>";
        const wchar_t* encoded = GW::native_ui::title_hook::CreateEncodedTextLiteral(literal.c_str());
        std::wstring snapshot = CopyEncoded(encoded);
        if (!snapshot.empty()) {
            e.truecolor = std::move(snapshot);  // set once; palette kept alive for lingering holders
            return e.truecolor.c_str();
        }
    }

    // Fallback until the name/encoder is ready: a palette opcode-swap of the
    // game's (persistent) name string, snapshotted once into our storage so it
    // stays valid across frames.
    if (e.palette.empty() && game_name_src && IsItemColorWord(game_name_src[0])) {
        e.palette = CopyEncoded(game_name_src);
        if (!e.palette.empty())
            e.palette[0] = NearestPaletteOpcode(argb);
    }
    return e.palette.empty() ? nullptr : e.palette.c_str();
}

// ================= Shared =================

void AgentRecolor::MasterEnable() {
    if (master_enabled_.exchange(true))
        return;  // already on
    CrashContextScope context("runtime", "agent_recolor", "master_enable_hooks");
    if (agent_hook_installed_.load() && g_resolver)
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_resolver));
    if (char_tag_hook_installed_.load() && g_char_get_text_data)
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_char_get_text_data));
    if (gadget_hook_installed_.load() && g_gadget_get_text_data)
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_gadget_get_text_data));
    if (item_hook_installed_.load() && g_item_get_text_data)
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_item_get_text_data));
}

void AgentRecolor::MasterDisable() {
    if (!master_enabled_.exchange(false))
        return;  // already off
    CrashContextScope context("runtime", "agent_recolor", "master_disable_hooks");
    if (agent_hook_installed_.load() && g_resolver)
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_resolver));
    if (char_tag_hook_installed_.load() && g_char_get_text_data)
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_char_get_text_data));
    if (gadget_hook_installed_.load() && g_gadget_get_text_data)
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_gadget_get_text_data));
    if (item_hook_installed_.load() && g_item_get_text_data)
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_item_get_text_data));
}

bool AgentRecolor::IsMasterEnabled() const { return master_enabled_.load(); }

void AgentRecolor::RefreshNameTags() {
    if (!g_set_nametag_visibility || !g_nametag_flags)
        return;  // symbols not resolved on this build; hover/state-change still refreshes
    // The flash walks the agent-view array and dispatches UI messages, so it MUST run
    // on the game thread (the Python update thread is not phase-locked to render).
    GW::game_thread::Enqueue([] {
        if (!g_set_nametag_visibility || !g_nametag_flags)
            return;
        CrashContextScope context("runtime", "agent_recolor", "refresh_name_tags");
        const uint32_t prev = *g_nametag_flags;
        g_set_nametag_visibility(0);      // hide all tags
        g_set_nametag_visibility(prev);   // restore -> forces a re-render (+ resolver rerun)
    });
}

void AgentRecolor::ClearAllRules() {
    ClearRules();
    GadgetClearRules();
    ItemClearRules();
}

AgentRecolor::Diagnostics AgentRecolor::GetDiagnostics() const {
    Diagnostics d;
    d.initialized = initialized_.load();
    d.agent_hook_installed = agent_hook_installed_.load();
    d.char_tag_hook_installed = char_tag_hook_installed_.load();
    d.gadget_hook_installed = gadget_hook_installed_.load();
    d.item_hook_installed = item_hook_installed_.load();
    d.agent_enabled = agent_enabled_.load();
    d.gadget_enabled = gadget_enabled_.load();
    d.item_enabled = item_enabled_.load();
    d.resolver_calls_seen = diag_resolver_calls_.load();
    d.agent_rule_hits = diag_agent_hits_.load();
    d.allegiance_rule_hits = diag_allegiance_hits_.load();
    d.last_agent_id = diag_last_agent_.load();
    d.last_agent_color = diag_last_agent_color_.load();
    d.gadget_calls_seen = diag_gadget_calls_.load();
    d.gadget_rule_hits = diag_gadget_hits_.load();
    d.gadget_all_hits = diag_gadget_all_hits_.load();
    d.last_gadget_id = diag_last_gadget_.load();
    d.last_gadget_color = diag_last_gadget_color_.load();
    d.item_calls_seen = diag_item_calls_.load();
    d.item_rule_hits = diag_item_hits_.load();
    d.last_item_id = diag_last_item_.load();
    d.last_item_model = diag_last_item_model_.load();
    d.last_item_color = diag_last_item_color_.load();
    d.item_name_cache_size = diag_item_name_cache_size_.load();
    return d;
}

void AgentRecolor::ResetDiagnostics() {
    diag_resolver_calls_.store(0);
    diag_agent_hits_.store(0);
    diag_allegiance_hits_.store(0);
    diag_last_agent_.store(0);
    diag_last_agent_color_.store(0);
    diag_gadget_calls_.store(0);
    diag_gadget_hits_.store(0);
    diag_gadget_all_hits_.store(0);
    diag_last_gadget_.store(0);
    diag_last_gadget_color_.store(0);
    diag_item_calls_.store(0);
    diag_item_hits_.store(0);
    diag_last_item_.store(0);
    diag_last_item_model_.store(0);
    diag_last_item_color_.store(0);
}

void AgentRecolor::RebuildAgentSnapshotLocked() {
    auto s = std::make_shared<AgentSnapshot>();
    s->agent_rules = agent_rules_;
    s->allegiance_rules = allegiance_rules_;
    agent_snapshot_ = s;
}
void AgentRecolor::RebuildGadgetSnapshotLocked() {
    auto s = std::make_shared<GadgetSnapshot>();
    s->gadget_rules = gadget_rules_;
    s->has_all = gadget_has_all_;
    s->all_color = gadget_all_color_;
    gadget_snapshot_ = s;
}
void AgentRecolor::RebuildItemSnapshotLocked() {
    auto s = std::make_shared<ItemSnapshot>();
    s->agent_rules = item_agent_rules_;
    s->item_rules = item_id_rules_;
    s->model_rules = item_model_rules_;
    s->type_rules = item_type_rules_;
    s->rarity_rules = item_rarity_rules_;
    s->name_rules = item_name_rules_;
    item_snapshot_ = s;
}

}  // namespace GW::agent_recolor

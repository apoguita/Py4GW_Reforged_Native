#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/agent/agent.h"
#include "GW/common/constants/constants.h"
#include "GW/context/context.h"
#include "GW/context/item.h"
#include "GW/context/item_context.h"
#include "GW/context/world_context.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace GW::item {

using Item = Context::Item;
using ItemArray = Context::ItemArray;
using Bag = Context::Bag;
using Inventory = Context::Inventory;
using SalvageSessionInfo = Context::SalvageSessionInfo;
using PvPItemUpgradeInfo = Context::PvPItemUpgradeInfo;
using PvPItemInfo = Context::PvPItemInfo;
using CompositeModelInfo = Context::CompositeModelInfo;
using ItemFormula = Context::ItemFormula;
using ItemModifier = Context::ItemModifier;

using PvPItemUpgradeArray = GW::GWArray<Context::PvPItemUpgradeInfo>;
using PvPItemInfoArray = GW::GWArray<Context::PvPItemInfo>;
using CompositeModelInfoArray = GW::GWArray<Context::CompositeModelInfo>;

enum class EquipmentType : uint32_t {
    Cape = 0x0, Helm = 0x2, CostumeBody = 0x4, CostumeHeadpiece = 0x6, Unknown = 0xff
};

enum class EquipmentStatus : uint32_t {
    AlwaysHide, HideInTownsAndOutposts, HideInCombatAreas, AlwaysShow
};

enum ItemClickType : uint32_t {
    ItemClickType_Add = 2,
    ItemClickType_Click = 5,
    ItemClickType_Release = 7,
    ItemClickType_DoubleClick = 8,
    ItemClickType_Move = 9,
    ItemClickType_DragStart = 10,
    ItemClickType_DragStop = 12,
};

struct ItemClickParam {
    uint32_t unk0;
    uint32_t slot;
    uint32_t type;
};

struct MoveItemUIMessage {
    uint32_t item_id;
    uint32_t quantity;
    uint32_t bag_index;
    uint32_t slot;
};

using ItemClickFn = void(__fastcall*)(uint32_t* bag_id, void* edx, ItemClickParam* param);
using DoActionFn = void(__cdecl*)(uint32_t identifier);
using VoidFn = void(__cdecl*)();
using EquipItemFn = void(__cdecl*)(uint32_t item_id, uint32_t agent_id);
using DropItemFn = void(__cdecl*)(uint32_t item_id, uint32_t quantity);
using MoveItemFn = void(__cdecl*)(uint32_t item_id, uint32_t quantity, uint32_t bag_index, uint32_t slot);
using SalvageStartFn = void(__cdecl*)(uint32_t salvage_kit_id, uint32_t salvage_session_id, uint32_t item_id);
using IdentifyItemFn = void(__cdecl*)(uint32_t identification_kit_id, uint32_t item_id);
using PingWeaponSetFn = void(__cdecl*)(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
using ChangeGoldFn = void(__cdecl*)(uint32_t character_gold, uint32_t storage_gold);
using ChangeEquipmentVisibilityFn = void(__cdecl*)(uint32_t equipment_state, uint32_t equip_type);
using GetPvPItemUpgradeInfoNameFn = void(__cdecl*)(uint32_t pvp_item_upgrade_id, uint32_t name_or_description, wchar_t** name_out, wchar_t** description_out);
using ItemClickCallback = PY4GW::HookCallback<uint32_t, uint32_t, Bag*>;

bool Initialize();
void Shutdown();

SalvageSessionInfo* GetSalvageSessionInfo();
ItemArray* GetItemArray();
Inventory* GetInventory();
Bag** GetBagArray();
Bag* GetBag(Constants::Bag bag_id);
Bag* GetBagByIndex(uint32_t bag_index);
Item* GetItemBySlot(const Bag* bag, uint32_t slot);
Item* GetHoveredItem();
Item* GetItemById(uint32_t item_id);
bool UseItem(const Item* item);
bool EquipItem(const Item* item, uint32_t agent_id = 0);
bool DropItem(const Item* item, uint32_t quantity);
bool PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
bool MoveItem(const Item* from, Constants::Bag bag_id, uint32_t slot, uint32_t quantity = 0);
bool MoveItem(const Item* from, const Bag* bag, uint32_t slot, uint32_t quantity = 0);
bool MoveItem(const Item* from, const Item* to, uint32_t quantity = 0);
bool UseItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
uint32_t CountItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
Item* GetItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
Item* GetItemByModelIdAndModifiers(uint32_t model_id, const ItemModifier* modifiers, uint32_t modifiers_len, int bag_start = 1, int bag_end = 4);
uint32_t GetGoldAmountOnCharacter();
uint32_t GetGoldAmountInStorage();
bool DropGold(uint32_t amount = 1);
uint32_t DepositGold(uint32_t amount = 0);
uint32_t WithdrawGold(uint32_t amount = 0);
bool ChangeGold(uint32_t character_gold, uint32_t storage_gold);
bool SalvageStart(uint32_t salvage_kit_id, uint32_t item_id);
bool IdentifyItem(uint32_t identification_kit_id, uint32_t item_id);
bool SalvageSessionCancel();
bool SalvageSessionDone();
bool DestroyItem(uint32_t item_id);
bool SalvageMaterials();
void OpenXunlaiWindow(bool anniversary_pane_unlocked = true);
Constants::StoragePane GetStoragePage();
bool GetIsStorageOpen();
void AsyncGetItemName(const Item* item, std::wstring& name);
void RegisterItemClickCallback(PY4GW::HookEntry* entry, const ItemClickCallback& callback);
void RemoveItemClickCallback(PY4GW::HookEntry* entry);
Constants::MaterialSlot GetMaterialSlot(const Item* item);
uint32_t GetEquipmentVisibilityState();
EquipmentStatus GetEquipmentVisibility(EquipmentType type);
bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus state);
uint32_t GetMaterialStorageStackSize();
const PvPItemUpgradeInfo* GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx);
const PvPItemUpgradeArray& GetPvPItemUpgradesArray();
const PvPItemInfo* GetPvPItemInfo(uint32_t pvp_item_idx);
const PvPItemInfoArray& GetPvPItemInfoArray();
const CompositeModelInfo* GetCompositeModelInfo(uint32_t model_file_id);
const CompositeModelInfoArray& GetCompositeModelInfoArray();
bool GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out);
bool GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out);
const ItemFormula* GetItemFormula(const Item* item);
bool PickUpItem(const Item* item, uint32_t call_target = 0);
bool CanAccessXunlaiChest();
bool IsStorageBag(const Bag* bag);
bool IsStorageItem(const Item* item);
bool CanInteractWithItem(const Item* item);

extern DoActionFn g_use_item_func;
extern DoActionFn g_use_item_original;
extern EquipItemFn g_equip_item_func;
extern DropItemFn g_drop_item_func;
extern MoveItemFn g_move_item_func;
extern MoveItemFn g_move_item_original;
extern ItemClickFn g_item_click_func;
extern ItemClickFn g_item_click_original;
extern ui::UIInteractionCallback g_salvage_popup_uicallback_func;
extern ui::UIInteractionCallback g_salvage_popup_uicallback_original;
extern DoActionFn g_drop_gold_func;
extern VoidFn g_salvage_session_cancel_func;
extern VoidFn g_salvage_session_complete_func;
extern VoidFn g_salvage_materials_func;
extern SalvageStartFn g_salvage_start_func;
extern IdentifyItemFn g_identify_item_func;
extern DoActionFn g_destroy_item_func;
extern ChangeEquipmentVisibilityFn g_change_equipment_visibility_func;
extern ChangeGoldFn g_change_gold_func;
extern ChangeGoldFn g_change_gold_original;
extern DoActionFn g_open_locked_chest_func;
extern PingWeaponSetFn g_ping_weapon_set_func;
extern PingWeaponSetFn g_ping_weapon_set_original;
extern GetPvPItemUpgradeInfoNameFn g_pvp_item_upgrade_name_func;
extern uint32_t* g_storage_open_addr;
extern SalvageSessionInfo* g_salvage_context;
extern PvPItemUpgradeArray g_unlocked_pvp_item_upgrade_array;
extern PvPItemInfoArray g_pvp_item_array;
extern CompositeModelInfoArray* g_composite_model_info_array;
extern ItemFormula* g_item_formulas;
extern uint32_t g_item_formula_count;
extern PY4GW::HookEntry g_ui_message_entry;
extern const std::array<ui::UIMessage, 3> g_ui_messages_to_hook;
extern PY4GW::HookEntry g_on_use_item_entry;
extern PY4GW::HookEntry g_on_move_item_entry;
extern PY4GW::HookEntry g_on_ping_weapon_set_entry;
extern std::unordered_map<PY4GW::HookEntry*, ItemClickCallback> g_item_click_callbacks;
extern std::atomic<bool> g_initialized;

inline bool ResolveStorageOpenAddress() {
    CrashContextScope context("startup", "item", "resolve_storage_open");
    const auto* pattern = PY4GW::Patterns::Get("item.storage_open_ptr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.storage_open_ptr", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("StorageOpen_Ref", address, "item")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogError("Storage open pointer is outside the expected data section.", "item");
        return false;
    }
    g_storage_open_addr = *reinterpret_cast<uint32_t**>(address);
    return Logger::AssertAddress("StorageOpen_Addr", reinterpret_cast<uintptr_t>(g_storage_open_addr), "item");
}

inline bool ResolveItemClickFunction() {
    CrashContextScope context("startup", "item", "resolve_item_click");
    const auto* pattern = PY4GW::Patterns::Get("item.item_click_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.item_click_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("ItemClick_Func", address, "item")) {
        return false;
    }
    g_item_click_func = reinterpret_cast<ItemClickFn>(address);
    return true;
}

inline bool ResolveUseItemFunction() {
    CrashContextScope context("startup", "item", "resolve_use_item");
    const auto* pattern = PY4GW::Patterns::Get("item.use_item_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.use_item_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("UseItem_Callsite", address, "item")) {
        return false;
    }
    g_use_item_func = reinterpret_cast<DoActionFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("UseItem_Func", reinterpret_cast<uintptr_t>(g_use_item_func), "item");
}

inline bool ResolveEquipItemFunction() {
    CrashContextScope context("startup", "item", "resolve_equip_item");
    const auto* pattern = PY4GW::Patterns::Get("item.equip_item_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.equip_item_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("EquipItem_Anchor", address, "item")) {
        return false;
    }
    g_equip_item_func = reinterpret_cast<EquipItemFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("EquipItem_Func", reinterpret_cast<uintptr_t>(g_equip_item_func), "item");
}

inline bool ResolveMoveItemFunction() {
    CrashContextScope context("startup", "item", "resolve_move_item");
    const auto* pattern = PY4GW::Patterns::Get("item.move_item_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.move_item_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("MoveItem_Anchor", address, "item")) {
        return false;
    }
    g_move_item_func = reinterpret_cast<MoveItemFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("MoveItem_Func", reinterpret_cast<uintptr_t>(g_move_item_func), "item");
}

inline bool ResolveDropItemFunction() {
    CrashContextScope context("startup", "item", "resolve_drop_item");
    const auto* pattern = PY4GW::Patterns::Get("item.drop_item_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.drop_item_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DropItem_Callsite", address, "item")) {
        return false;
    }
    g_drop_item_func = reinterpret_cast<DropItemFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("DropItem_Func", reinterpret_cast<uintptr_t>(g_drop_item_func), "item");
}

inline bool ResolveSalvagePopupUICallback() {
    CrashContextScope context("startup", "item", "resolve_salvage_popup");
    const auto* pattern = PY4GW::Patterns::Get("item.salvage_popup_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.salvage_popup_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("SalvagePopup_Anchor", address, "item")) {
        return false;
    }
    g_salvage_popup_uicallback_func = reinterpret_cast<ui::UIInteractionCallback>(
        PY4GW::Scanner::ToFunctionStart(address, 0x200));
    return Logger::AssertAddress("SalvagePopup_UICallback", reinterpret_cast<uintptr_t>(g_salvage_popup_uicallback_func), "item");
}

inline bool ResolveDropGoldAndSalvage() {
    CrashContextScope context("startup", "item", "resolve_drop_gold_and_salvage");
    const auto* pattern = PY4GW::Patterns::Get("item.drop_gold_salvage_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.drop_gold_salvage_assertion", "item");
        return false;
    }
    const uintptr_t assertion_address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("DropGoldSalvage_Anchor", assertion_address, "item")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(assertion_address, PY4GW::ScannerSection::Text)) {
        return false;
    }
    uintptr_t address = PY4GW::Scanner::FindInRange("\xe8", "x", 0, assertion_address + 0xf, assertion_address + 0x64);
    g_drop_gold_func = reinterpret_cast<DoActionFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    address = PY4GW::Scanner::FindInRange("\xe8", "x", 0, assertion_address, assertion_address - 0x64);
    g_salvage_session_cancel_func = reinterpret_cast<VoidFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    address = PY4GW::Scanner::FindInRange("\xe8", "x", 0, address, address - 0x64);
    g_salvage_session_complete_func = reinterpret_cast<VoidFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    address = PY4GW::Scanner::FindInRange("\xe8", "x", 0, address, address - 0x64);
    g_salvage_materials_func = reinterpret_cast<VoidFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    const bool drop_gold_ok = Logger::AssertAddress("DropGold_Func", reinterpret_cast<uintptr_t>(g_drop_gold_func), "item");
    const bool cancel_ok = Logger::AssertAddress("SalvageSessionCancel_Func", reinterpret_cast<uintptr_t>(g_salvage_session_cancel_func), "item");
    const bool complete_ok = Logger::AssertAddress("SalvageSessionComplete_Func", reinterpret_cast<uintptr_t>(g_salvage_session_complete_func), "item");
    const bool materials_ok = Logger::AssertAddress("SalvageMaterials_Func", reinterpret_cast<uintptr_t>(g_salvage_materials_func), "item");
    return drop_gold_ok && cancel_ok && complete_ok && materials_ok;
}

inline bool ResolveSalvageStartFunction() {
    CrashContextScope context("startup", "item", "resolve_salvage_start");
    const auto* pattern = PY4GW::Patterns::Get("item.salvage_start_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.salvage_start_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("SalvageStart_Anchor", address, "item")) {
        return false;
    }
    g_salvage_start_func = reinterpret_cast<SalvageStartFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("SalvageStart_Func", reinterpret_cast<uintptr_t>(g_salvage_start_func), "item");
}

inline bool ResolveIdentifyItemFunction() {
    CrashContextScope context("startup", "item", "resolve_identify_item");
    const auto* pattern = PY4GW::Patterns::Get("item.identify_item_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.identify_item_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("IdentifyItem_Anchor", address, "item")) {
        return false;
    }
    g_identify_item_func = reinterpret_cast<IdentifyItemFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("IdentifyItem_Func", reinterpret_cast<uintptr_t>(g_identify_item_func), "item");
}

inline bool ResolveDestroyItemFunction() {
    CrashContextScope context("startup", "item", "resolve_destroy_item");
    const auto* pattern = PY4GW::Patterns::Get("item.destroy_item_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.destroy_item_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DestroyItem_Callsite", address, "item")) {
        return false;
    }
    g_destroy_item_func = reinterpret_cast<DoActionFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("DestroyItem_Func", reinterpret_cast<uintptr_t>(g_destroy_item_func), "item");
}

inline bool ResolveChangeEquipmentVisibilityFunction() {
    CrashContextScope context("startup", "item", "resolve_change_equipment_visibility");
    const auto* pattern = PY4GW::Patterns::Get("item.change_equipment_visibility_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.change_equipment_visibility_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("ChangeEquipmentVisibility_Callsite", address, "item")) {
        return false;
    }
    g_change_equipment_visibility_func = reinterpret_cast<ChangeEquipmentVisibilityFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("ChangeEquipmentVisibility_Func", reinterpret_cast<uintptr_t>(g_change_equipment_visibility_func), "item");
}

inline bool ResolveChangeGoldFunction() {
    CrashContextScope context("startup", "item", "resolve_change_gold");
    const auto* pattern = PY4GW::Patterns::Get("item.change_gold_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.change_gold_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("ChangeGold_Callsite", address, "item")) {
        return false;
    }
    g_change_gold_func = reinterpret_cast<ChangeGoldFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("ChangeGold_Func", reinterpret_cast<uintptr_t>(g_change_gold_func), "item");
}

inline bool ResolveOpenLockedChestFunction() {
    CrashContextScope context("startup", "item", "resolve_open_locked_chest");
    const auto* pattern = PY4GW::Patterns::Get("item.open_locked_chest_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.open_locked_chest_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("OpenLockedChest_Callsite", address, "item")) {
        return false;
    }
    g_open_locked_chest_func = reinterpret_cast<DoActionFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("OpenLockedChest_Func", reinterpret_cast<uintptr_t>(g_open_locked_chest_func), "item");
}

inline bool ResolvePingWeaponSetFunction() {
    CrashContextScope context("startup", "item", "resolve_ping_weapon_set");
    const auto* pattern = PY4GW::Patterns::Get("item.ping_weapon_set_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.ping_weapon_set_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("PingWeaponSet_Anchor", address, "item")) {
        return false;
    }
    g_ping_weapon_set_func = reinterpret_cast<PingWeaponSetFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("PingWeaponSet_Func", reinterpret_cast<uintptr_t>(g_ping_weapon_set_func), "item");
}

inline bool ResolvePvPItemUpgradeArray() {
    CrashContextScope context("startup", "item", "resolve_pvp_item_upgrade_array");
    const auto* pattern = PY4GW::Patterns::Get("item.pvp_item_upgrade_array_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.pvp_item_upgrade_array_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) return false;
    g_unlocked_pvp_item_upgrade_array.m_buffer = *reinterpret_cast<PvPItemUpgradeInfo**>(address + 0x15);
    g_unlocked_pvp_item_upgrade_array.m_size = *reinterpret_cast<uint32_t*>(address - 0xb);
    const bool buffer_ok = Logger::AssertAddress("PvPItemUpgradeArray_Buffer", reinterpret_cast<uintptr_t>(g_unlocked_pvp_item_upgrade_array.m_buffer), "item");
    return buffer_ok;
}

inline bool ResolvePvPItemArray() {
    CrashContextScope context("startup", "item", "resolve_pvp_item_array");
    const auto* pattern = PY4GW::Patterns::Get("item.pvp_item_array_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.pvp_item_array_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) return false;
    g_pvp_item_array.m_buffer = *reinterpret_cast<PvPItemInfo**>(address + 0x15);
    g_pvp_item_array.m_size = *reinterpret_cast<uint32_t*>(address - 0xb);
    const bool buffer_ok = Logger::AssertAddress("PvPItemArray_Buffer", reinterpret_cast<uintptr_t>(g_pvp_item_array.m_buffer), "item");
    return buffer_ok;
}

inline bool ResolveCompositeModelInfoArray() {
    CrashContextScope context("startup", "item", "resolve_composite_model_info_array");
    const auto* pattern = PY4GW::Patterns::Get("item.composite_model_info_array_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.composite_model_info_array_assertion", "item");
        return false;
    }
    uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) return false;
    address = (*reinterpret_cast<uintptr_t*>(address)) - 8;
    g_composite_model_info_array = reinterpret_cast<CompositeModelInfoArray*>(address);
    return Logger::AssertAddress("CompositeModelInfoArray", reinterpret_cast<uintptr_t>(g_composite_model_info_array), "item");
}

inline bool ResolvePvPItemUpgradeNameFunction() {
    CrashContextScope context("startup", "item", "resolve_pvp_item_upgrade_name");
    const auto* pattern = PY4GW::Patterns::Get("item.pvp_item_upgrade_name_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.pvp_item_upgrade_name_func", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        return false;
    }
    g_pvp_item_upgrade_name_func = reinterpret_cast<GetPvPItemUpgradeInfoNameFn>(address);
    return Logger::AssertAddress("GetPvPItemUpgradeInfoName_Func", reinterpret_cast<uintptr_t>(g_pvp_item_upgrade_name_func), "item");
}

inline bool ResolveItemFormulas() {
    CrashContextScope context("startup", "item", "resolve_item_formulas");
    const auto* pattern = PY4GW::Patterns::Get("item.item_formulas_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: item.item_formulas_assertion", "item");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) return false;
    g_item_formulas = *reinterpret_cast<ItemFormula**>(address + 0x15);
    g_item_formula_count = *reinterpret_cast<uint32_t*>(address - 0xb);
    const bool formulas_ok = Logger::AssertAddress("ItemFormulas", reinterpret_cast<uintptr_t>(g_item_formulas), "item");
    return formulas_ok;
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __fastcall OnItemClick(uint32_t* bag_index, void* edx, ItemClickParam* param);
void __cdecl OnUseItem(uint32_t item_id);
void __cdecl OnMoveItem(uint32_t item_id, uint32_t quantity, uint32_t bag_index, uint32_t slot);
void __cdecl OnPingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
void __cdecl OnChangeGold(uint32_t character_gold, uint32_t storage_gold);
void OnUIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void*);
void OnPingWeaponSet_UIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void*);
void OnSalvagePopup_UICallback(ui::InteractionMessage* message, void* wParam, void* lParam);

}  // namespace GW::item

namespace GW {
namespace Items = item;
}

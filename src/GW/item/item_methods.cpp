#include "base/error_handling.h"

#include "GW/item/item.h"

#include "GW/context/context.h"
#include "GW/context/account_context.h"
#include "GW/context/game_context.h"
#include "GW/context/item_context.h"
#include "GW/map/map.h"
#include "GW/stoc/stoc.h"
#include "GW/ui/ui.h"

#include <algorithm>

namespace GW::item {

static uint32_t GetSalvageSessionId() {
    const auto* w = Context::GetWorldContext();
    return w ? w->salvage_session_id : 0;
}

bool CanAccessXunlaiChest() {
    if (map::GetInstanceType() != Constants::InstanceType::Outpost)
        return false;
    const auto* map_info = map::GetCurrentMapInfo();
    return map_info && map_info->region != Context::Region::Region_Presearing;
}

bool IsStorageBag(const Bag* bag) {
    return bag && bag->bag_type == Context::BagType::Storage;
}

bool IsStorageItem(const Item* item) {
    return item && IsStorageBag(item->bag);
}

bool CanInteractWithItem(const Item* item) {
    return item && !IsStorageItem(item) || CanAccessXunlaiChest();
}

SalvageSessionInfo* GetSalvageSessionInfo() {
    return g_salvage_context;
}

ItemArray* GetItemArray() {
    auto* i = Context::GetItemContext();
    return i && i->item_array.valid() ? &i->item_array : nullptr;
}

Inventory* GetInventory() {
    auto* i = Context::GetItemContext();
    return i ? i->inventory : nullptr;
}

Bag** GetBagArray() {
    auto* i = GetInventory();
    return i ? i->bags : nullptr;
}

Bag* GetBag(Constants::Bag bag_id) {
    if (!(bag_id > Constants::Bag::None && bag_id < Constants::Bag::Max))
        return nullptr;
    Bag** bags = GetBagArray();
    return bags ? bags[static_cast<unsigned>(bag_id)] : nullptr;
}

Bag* GetBagByIndex(uint32_t bag_index) {
    return GetBag(static_cast<Constants::Bag>(bag_index + 1));
}

Item* GetItemBySlot(const Bag* bag, uint32_t slot) {
    if (!bag || slot == 0) return nullptr;
    if (!bag->items.valid()) return nullptr;
    if (slot > bag->items.size()) return nullptr;
    return bag->items[slot - 1];
}

Item* GetHoveredItem() {
    const auto* tooltip = ui::GetCurrentTooltip();
    if (!tooltip) {
        return nullptr;
    }
    if (tooltip->payload_len == 0x8 && tooltip->payload[1] == 0xff) {
        return GetItemById(*reinterpret_cast<uint32_t*>(tooltip->payload));
    }
    if (tooltip->payload_len == 0xC && tooltip->payload[2] == 0xff) {
        const auto* item_ids = reinterpret_cast<uint32_t*>(tooltip->payload);
        return GetItemById(item_ids[0] ? item_ids[0] : item_ids[1]);
    }
    return nullptr;
}

Item* GetItemById(uint32_t item_id) {
    ItemArray* items = item_id ? GetItemArray() : nullptr;
    return items && item_id < items->size() ? items->at(item_id) : nullptr;
}

bool UseItem(const Item* item) {
    if (!(g_use_item_func && item))
        return false;
    if (!CanInteractWithItem(item))
        return false;
    g_use_item_func(item->item_id);
    return true;
}

bool EquipItem(const Item* item, uint32_t agent_id) {
    if (!(item && g_equip_item_func))
        return false;
    if (!CanInteractWithItem(item))
        return false;
    if (!agent_id)
        agent_id = agent::GetControlledCharacterId();
    if (!agent_id)
        return false;
    g_equip_item_func(item->item_id, agent_id);
    return true;
}

bool DropItem(const Item* item, uint32_t quantity) {
    if (!(g_drop_item_func && item))
        return false;
    g_drop_item_func(item->item_id, quantity);
    return true;
}

bool PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id) {
    if (!(g_ping_weapon_set_func && agent_id))
        return false;
    g_ping_weapon_set_func(agent_id, weapon_item_id, offhand_item_id);
    return true;
}

bool PickUpItem(const Item* item, uint32_t call_target) {
    const auto packet = ui::packet::kInteractAgent{item->agent_id, call_target == 1};
    return ui::SendUIMessage(ui::UIMessage::kSendInteractItem, const_cast<ui::packet::kInteractAgent*>(&packet));
}

bool MoveItem(const Item* from, Constants::Bag bag_id, uint32_t slot, uint32_t quantity) {
    return MoveItem(from, GetBag(bag_id), slot, quantity);
}

bool MoveItem(const Item* from, const Bag* bag, uint32_t slot, uint32_t quantity) {
    if (!(g_move_item_func && from && bag)) return false;
    if (bag->items.size() < static_cast<unsigned>(slot)) return false;
    if (quantity <= 0) quantity = from->quantity;
    if (quantity > from->quantity) quantity = from->quantity;
    g_move_item_func(from->item_id, quantity, bag->index, slot);
    return true;
}

bool MoveItem(const Item* from, const Item* to, uint32_t quantity) {
    return MoveItem(from, to->bag, to->slot, quantity);
}

bool UseItemByModelId(uint32_t model_id, int bag_start, int bag_end) {
    return UseItem(GetItemByModelId(model_id, bag_start, bag_end));
}

uint32_t CountItemByModelId(uint32_t model_id, int bag_start, int bag_end) {
    uint32_t itemcount = 0;
    Bag** bags = GetBagArray();
    if (!bags) return 0;

    for (int bagIndex = bag_start; bagIndex <= bag_end; ++bagIndex) {
        Bag* bag = bags[bagIndex];
        if (!(bag && bag->items.valid())) continue;
        for (Item* item : bag->items) {
            if (item && item->model_id == model_id) {
                itemcount += item->quantity;
            }
        }
    }

    return itemcount;
}

Item* GetItemByModelId(uint32_t model_id, int bag_start, int bag_end) {
    Bag** bags = GetBagArray();
    if (!bags) return nullptr;

    for (int bagIndex = bag_start; bagIndex <= bag_end; ++bagIndex) {
        Bag* bag = bags[bagIndex];
        if (!(bag && bag->items.valid())) continue;
        for (Item* item : bag->items) {
            if (item && item->model_id == model_id) {
                return item;
            }
        }
    }

    return nullptr;
}

Item* GetItemByModelIdAndModifiers(uint32_t model_id, const ItemModifier* modifiers, uint32_t modifiers_len, int bag_start, int bag_end) {
    Bag** bags = GetBagArray();
    if (!(bags && modifiers_len && modifiers))
        return nullptr;

    for (int bagIndex = bag_start; bagIndex <= bag_end; ++bagIndex) {
        const Bag* bag = bags[bagIndex];
        if (!bag) continue;
        for (Item* item : bag->items) {
            if (!(item && item->mod_struct_size == modifiers_len && item->model_id == model_id))
                continue;
            if (memcmp(item->mod_struct, modifiers, sizeof(*modifiers) * modifiers_len) == 0)
                return item;
        }
    }

    return nullptr;
}

uint32_t GetGoldAmountOnCharacter() {
    auto* i = GetInventory();
    return i ? i->gold_character : 0;
}

uint32_t GetGoldAmountInStorage() {
    auto* i = GetInventory();
    return i ? i->gold_storage : 0;
}

bool DropGold(uint32_t amount) {
    if (!(g_drop_gold_func && GetGoldAmountOnCharacter() >= amount))
        return false;
    g_drop_gold_func(amount);
    return true;
}

bool ChangeGold(uint32_t character_gold, uint32_t storage_gold) {
    if (!(g_change_gold_func && (GetGoldAmountInStorage() + GetGoldAmountOnCharacter()) == (character_gold + storage_gold)))
        return false;
    g_change_gold_func(character_gold, storage_gold);
    return true;
}

uint32_t DepositGold(uint32_t amount) {
    uint32_t gold_storage = GetGoldAmountInStorage();
    uint32_t gold_character = GetGoldAmountOnCharacter();
    uint32_t will_move = 0;
    if (amount == 0) {
        will_move = std::min(1000000u - gold_storage, gold_character);
    }
    else {
        if (gold_storage + amount > 1000000)
            return 0;
        if (amount > gold_character)
            return 0;
        will_move = amount;
    }
    gold_storage += will_move;
    gold_character -= will_move;
    return ChangeGold(gold_character, gold_storage) ? will_move : 0;
}

uint32_t WithdrawGold(uint32_t amount) {
    uint32_t gold_storage = GetGoldAmountInStorage();
    uint32_t gold_character = GetGoldAmountOnCharacter();
    uint32_t will_move = 0;
    if (amount == 0) {
        will_move = std::min(gold_storage, 100000u - gold_character);
    }
    else {
        if (gold_character + amount > 100000)
            return 0;
        if (amount > gold_storage)
            return 0;
        will_move = amount;
    }
    gold_storage -= will_move;
    gold_character += will_move;
    return ChangeGold(gold_character, gold_storage) ? will_move : 0;
}

bool SalvageStart(uint32_t salvage_kit_id, uint32_t item_id) {
    if (!(g_salvage_start_func && CanInteractWithItem(GetItemById(salvage_kit_id))
        && CanInteractWithItem(GetItemById(item_id)))) {
        return false;
    }
    const auto packet = ui::packet::kPreStartSalvage{item_id, salvage_kit_id};
    ui::SendUIMessage(ui::UIMessage::kPreStartSalvage, const_cast<ui::packet::kPreStartSalvage*>(&packet));
    g_salvage_start_func(salvage_kit_id, GetSalvageSessionId(), item_id);
    return true;
}

bool IdentifyItem(uint32_t identification_kit_id, uint32_t item_id) {
    if (!(CanInteractWithItem(GetItemById(identification_kit_id))
        && CanInteractWithItem(GetItemById(item_id)))) {
        return false;
    }
    return g_identify_item_func ? g_identify_item_func(identification_kit_id, item_id), true : false;
}

bool SalvageSessionCancel() {
    if (!g_salvage_context)
        return false;
    auto* btn = ui::GetChildFrame(ui::GetFrameById(g_salvage_context->frame_id), 1);
    return ui::ButtonClick(btn);
}

bool SalvageSessionDone() {
    return g_salvage_session_complete_func ? g_salvage_session_complete_func(), true : false;
}

bool DestroyItem(uint32_t item_id) {
    return g_destroy_item_func ? g_destroy_item_func(item_id), true : false;
}

bool SalvageMaterials() {
    if (!g_salvage_context)
        return false;
    const auto prev_context = *g_salvage_context;
    g_salvage_context->chosen_salvagable = 3;
    g_salvage_context->salvagable_1 = 0;
    g_salvage_context->salvagable_2 = 0;
    g_salvage_context->salvagable_3 = 0;
    auto* btn = ui::GetChildFrame(ui::GetFrameById(g_salvage_context->frame_id), 2);
    bool ok = ui::ButtonClick(btn);
    if (g_salvage_context)
        *g_salvage_context = prev_context;
    return ok;
}

void OpenXunlaiWindow(bool anniversary_pane_unlocked) {
    GW::Packet::StoC::DataWindow pack{};
    pack.agent = 0;
    pack.type = 0;
    pack.data = anniversary_pane_unlocked ? 3 : 1;
    GW::StoC::EmulatePacket(&pack);
}

Constants::StoragePane GetStoragePage() {
    return static_cast<Constants::StoragePane>(ui::GetPreference(ui::NumberPreference::StorageBagPage));
}

bool GetIsStorageOpen() {
    return g_storage_open_addr && *g_storage_open_addr != 0;
}

void AsyncGetItemName(const Item* item, std::wstring& name) {
    if (!item || !item->complete_name_enc) return;
    ui::AsyncDecodeStr(item->complete_name_enc, &name);
}

void RegisterItemClickCallback(
    PY4GW::HookEntry* entry,
    const ItemClickCallback& callback)
{
    g_item_click_callbacks.insert({ entry, callback });
}

void RemoveItemClickCallback(
    PY4GW::HookEntry* entry)
{
    auto it = g_item_click_callbacks.find(entry);
    if (it != g_item_click_callbacks.end())
        g_item_click_callbacks.erase(it);
}

Constants::MaterialSlot GetMaterialSlot(const Item* item) {
    const auto* mod = item ? item->GetModifier(9480) : nullptr;
    if (!mod) return Constants::MaterialSlot::Count;
    const auto slot = static_cast<Constants::MaterialSlot>(mod->arg1());
    if (slot >= Constants::MaterialSlot::BronzeZCoin)
        return Constants::MaterialSlot::Count;
    return slot;
}

uint32_t GetEquipmentVisibilityState() {
    const auto* w = Context::GetWorldContext();
    return w ? w->equipment_status : 0;
}

EquipmentStatus GetEquipmentVisibility(EquipmentType type) {
    return static_cast<EquipmentStatus>((GetEquipmentVisibilityState() >> static_cast<uint32_t>(type)) & 0x3);
}

bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus state) {
    if (GetEquipmentVisibility(type) == state)
        return true;
    if (!g_change_equipment_visibility_func)
        return false;
    g_change_equipment_visibility_func(
        static_cast<uint32_t>(static_cast<uint32_t>(state) << static_cast<uint32_t>(type)),
        static_cast<uint32_t>(0x3 << static_cast<uint32_t>(type)));
    return true;
}

uint32_t GetMaterialStorageStackSize() {
    const auto* g = Context::GetGameContext();
    const auto* a = g ? g->account : nullptr;
    if (!a) return 250;
    for (auto& unlock : a->account_unlocked_counts) {
        if (unlock.id == 0x83)
            return (unlock.unk1 * 250) + 250;
    }
    return 250;
}

const PvPItemUpgradeInfo* GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx) {
    const auto& arr = GetPvPItemUpgradesArray();
    if (pvp_item_upgrade_idx < arr.size()) {
        return &arr[pvp_item_upgrade_idx];
    }
    return nullptr;
}

const PvPItemUpgradeArray& GetPvPItemUpgradesArray() {
    return g_unlocked_pvp_item_upgrade_array;
}

const PvPItemInfo* GetPvPItemInfo(uint32_t pvp_item_idx) {
    const auto& arr = GetPvPItemInfoArray();
    if (pvp_item_idx < arr.size()) {
        return &arr[pvp_item_idx];
    }
    return nullptr;
}

const PvPItemInfoArray& GetPvPItemInfoArray() {
    return g_pvp_item_array;
}

const CompositeModelInfo* GetCompositeModelInfo(uint32_t model_file_id) {
    const auto& arr = GetCompositeModelInfoArray();
    if (model_file_id < arr.size()) {
        return &arr[model_file_id];
    }
    return nullptr;
}

const CompositeModelInfoArray& GetCompositeModelInfoArray() {
    return *g_composite_model_info_array;
}

bool GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out) {
    const auto* info = GetPvPItemUpgrade(pvp_item_upgrade_idx);
    if (!(info && g_pvp_item_upgrade_name_func && out))
        return false;
    *out = nullptr;
    wchar_t* tmp;
    g_pvp_item_upgrade_name_func(pvp_item_upgrade_idx, false, out, &tmp);
    return *out != nullptr;
}

bool GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out) {
    const auto* info = GetPvPItemUpgrade(pvp_item_upgrade_idx);
    if (!(info && g_pvp_item_upgrade_name_func && out))
        return false;
    *out = nullptr;
    wchar_t* tmp;
    g_pvp_item_upgrade_name_func(pvp_item_upgrade_idx, false, &tmp, out);
    return *out != nullptr;
}

const ItemFormula* GetItemFormula(const Item* item) {
    if (!(item && g_item_formulas && item->item_formula < g_item_formula_count))
        return nullptr;
    return &g_item_formulas[item->item_formula];
}

}  // namespace GW::item

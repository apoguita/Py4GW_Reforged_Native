#include "base/py_bindings.h"

#include "GW/game_thread/game_thread.h"
#include "GW/item/item.h"
#include "GW/map/map.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace py = pybind11;

namespace {

// Legacy SafeItemModifier (py_items.h/.cpp), exposed to Python as
// PyItem.ItemModifier. Self-contained value type over a raw 32-bit mod word;
// pure bit math, no GW backend dependency. Used by
// Py4GWCoreLib.item_mods_src.decoded_modifier.
std::string IntToBinaryString(uint32_t value, size_t bit_count) {
    std::string binary_str;
    binary_str.reserve(bit_count);
    for (int i = static_cast<int>(bit_count) - 1; i >= 0; --i) {
        binary_str += (value & (1u << i)) ? '1' : '0';
    }
    return binary_str;
}

class ItemModifier {
public:
    explicit ItemModifier(uint32_t mod_value) : mod_(mod_value) {}

    uint32_t GetIdentifier() const { return mod_ >> 16; }
    uint32_t GetArg1() const { return (mod_ & 0x0000FF00u) >> 8; }
    uint32_t GetArg2() const { return (mod_ & 0x000000FFu); }
    uint32_t GetArg() const { return (mod_ & 0x0000FFFFu); }

    bool IsValid() const { return mod_ != 0; }

    std::string GetModBits() const { return IntToBinaryString(mod_, 32); }
    std::string GetIdentifierBits() const { return IntToBinaryString(GetIdentifier(), 16); }
    std::string GetArg1Bits() const { return IntToBinaryString(GetArg1(), 8); }
    std::string GetArg2Bits() const { return IntToBinaryString(GetArg2(), 8); }
    std::string GetArgBits() const { return IntToBinaryString(GetArg(), 16); }

    std::string ToString() const {
        if (!IsValid()) {
            return "No Modifier";
        }
        return "Modifier ID: " + std::to_string(GetIdentifier()) +
            " (" + GetIdentifierBits() + ")" +
            ", Arg1: " + std::to_string(GetArg1()) +
            " (" + GetArg1Bits() + ")" +
            ", Arg2: " + std::to_string(GetArg2()) +
            " (" + GetArg2Bits() + ")" +
            ", Arg: " + std::to_string(GetArg()) +
            " (" + GetArgBits() + ")";
    }

private:
    uint32_t mod_;
};

// ===== PyItem data class (legacy SafeItem port over GW::Context::Item) =====

const char* ItemTypeName(int type_int) {
    switch (static_cast<GW::Constants::ItemType>(type_int)) {
    case GW::Constants::ItemType::Salvage: return "Salvage";
    case GW::Constants::ItemType::Axe: return "Axe";
    case GW::Constants::ItemType::Bag: return "Bag";
    case GW::Constants::ItemType::Boots: return "Boots";
    case GW::Constants::ItemType::Bow: return "Bow";
    case GW::Constants::ItemType::Bundle: return "Bundle";
    case GW::Constants::ItemType::Chestpiece: return "Chestpiece";
    case GW::Constants::ItemType::Rune_Mod: return "Rune_Mod";
    case GW::Constants::ItemType::Usable: return "Usable";
    case GW::Constants::ItemType::Dye: return "Dye";
    case GW::Constants::ItemType::Materials_Zcoins: return "Materials_Zcoins";
    case GW::Constants::ItemType::Offhand: return "Offhand";
    case GW::Constants::ItemType::Gloves: return "Gloves";
    case GW::Constants::ItemType::Hammer: return "Hammer";
    case GW::Constants::ItemType::Headpiece: return "Headpiece";
    case GW::Constants::ItemType::CC_Shards: return "CC_Shards";
    case GW::Constants::ItemType::Key: return "Key";
    case GW::Constants::ItemType::Leggings: return "Leggings";
    case GW::Constants::ItemType::Gold_Coin: return "Gold_Coin";
    case GW::Constants::ItemType::Quest_Item: return "Quest_Item";
    case GW::Constants::ItemType::Wand: return "Wand";
    case GW::Constants::ItemType::Shield: return "Shield";
    case GW::Constants::ItemType::Staff: return "Staff";
    case GW::Constants::ItemType::Sword: return "Sword";
    case GW::Constants::ItemType::Kit: return "Kit";
    case GW::Constants::ItemType::Trophy: return "Trophy";
    case GW::Constants::ItemType::Scroll: return "Scroll";
    case GW::Constants::ItemType::Daggers: return "Daggers";
    case GW::Constants::ItemType::Present: return "Present";
    case GW::Constants::ItemType::Minipet: return "Minipet";
    case GW::Constants::ItemType::Scythe: return "Scythe";
    case GW::Constants::ItemType::Spear: return "Spear";
    case GW::Constants::ItemType::Storybook: return "Storybook";
    case GW::Constants::ItemType::Costume: return "Costume";
    case GW::Constants::ItemType::Costume_Headpiece: return "Costume_Headpiece";
    case GW::Constants::ItemType::Unknown: return "Unknown";
    default: return "Unknown";
    }
}

class PyItemType {
public:
    int type_int = static_cast<int>(GW::Constants::ItemType::Unknown);
    PyItemType() = default;
    explicit PyItemType(int type_value) : type_int(type_value) {}
    int ToInt() const { return type_int; }
    std::string GetName() const { return ItemTypeName(type_int); }
    bool operator==(const PyItemType& other) const { return type_int == other.type_int; }
    bool operator!=(const PyItemType& other) const { return type_int != other.type_int; }
};

const char* DyeColorName(int color_int) {
    switch (static_cast<GW::Constants::DyeColor>(color_int)) {
    case GW::Constants::DyeColor::None: return "None";
    case GW::Constants::DyeColor::Blue: return "Blue";
    case GW::Constants::DyeColor::Green: return "Green";
    case GW::Constants::DyeColor::Purple: return "Purple";
    case GW::Constants::DyeColor::Red: return "Red";
    case GW::Constants::DyeColor::Yellow: return "Yellow";
    case GW::Constants::DyeColor::Brown: return "Brown";
    case GW::Constants::DyeColor::Orange: return "Orange";
    case GW::Constants::DyeColor::Silver: return "Silver";
    case GW::Constants::DyeColor::Black: return "Black";
    case GW::Constants::DyeColor::Gray: return "Gray";
    case GW::Constants::DyeColor::White: return "White";
    case GW::Constants::DyeColor::Pink: return "Pink";
    default: return "None";
    }
}

class PyDyeColor {
public:
    int color_int = 0;
    PyDyeColor() = default;
    explicit PyDyeColor(int color_value) : color_int(color_value) {}
    int ToInt() const { return color_int; }
    std::string ToString() const { return DyeColorName(color_int); }
    bool operator==(const PyDyeColor& other) const { return color_int == other.color_int; }
    bool operator!=(const PyDyeColor& other) const { return color_int != other.color_int; }
};

class PyDyeInfo {
public:
    uint8_t dye_tint = 0;
    PyDyeColor dye1, dye2, dye3, dye4;
    PyDyeInfo() = default;
    explicit PyDyeInfo(const GW::Context::DyeInfo& info)
        : dye_tint(info.dye_tint),
          dye1(static_cast<int>(info.dye1)), dye2(static_cast<int>(info.dye2)),
          dye3(static_cast<int>(info.dye3)), dye4(static_cast<int>(info.dye4)) {}
    std::string ToString() const {
        return "DyeInfo { dye_tint: " + std::to_string(dye_tint) +
               ", dye1: " + dye1.ToString() + ", dye2: " + dye2.ToString() +
               ", dye3: " + dye3.ToString() + ", dye4: " + dye4.ToString() + " }";
    }
};

bool ItemMapReady() {
    const auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving() &&
           instance_type != GW::Constants::InstanceType::Loading;
}

struct ItemNameData {
    std::string item_name;
    bool name_ready = false;
};
std::unordered_map<uint32_t, ItemNameData> g_item_name_map;

std::string ItemWStringToUtf8(const std::wstring& s) {
    if (s.empty()) return "Error In Name";
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "Error In Name";
    std::string out(static_cast<size_t>(size_needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), size_needed, nullptr, nullptr);
    out.resize(static_cast<size_t>(size_needed) - 1);  // strip terminator WCToMB appends
    return out;
}

std::vector<uint8_t> EncBytesFrom(const wchar_t* enc) {
    if (!enc) return {};
    size_t n = 0;
    while (enc[n] != 0) ++n;
    const size_t byte_count = (n + 1) * sizeof(wchar_t);
    std::vector<uint8_t> out(byte_count);
    std::memcpy(out.data(), enc, byte_count);
    return out;
}

std::vector<uint32_t> ItemCompositeModelIDs(uint32_t model_file_id) {
    const auto* info = GW::item::GetCompositeModelInfo(model_file_id);
    if (info) {
        return std::vector<uint32_t>(std::begin(info->file_ids), std::end(info->file_ids));
    }
    return {};
}

class PyItemData {
public:
    int item_id = 0;
    int agent_id = 0;
    int agent_item_id = 0;  // legacy alias of item->item_id
    std::string name;       // populated via GetName()/RequestName(), not this field
    std::vector<ItemModifier> modifiers;
    bool is_customized = false;
    PyItemType item_type;
    PyDyeInfo dye_info;
    int value = 0;
    uint32_t interaction = 0;
    uint32_t model_id = 0;
    uint32_t model_file_id = 0;
    int item_formula = 0;
    int is_material_salvageable = 0;
    int quantity = 0;
    int equipped = 0;
    int profession = 0;
    int slot = 0;
    bool is_stackable = false;
    bool is_inscribable = false;
    bool is_material = false;
    bool is_zcoin = false;
    GW::Constants::Rarity rarity = GW::Constants::Rarity::White;
    int uses = 0;
    bool is_id_kit = false;
    bool is_salvage_kit = false;
    bool is_tome = false;
    bool is_lesser_kit = false;
    bool is_expert_salvage_kit = false;
    bool is_perfect_salvage_kit = false;
    bool is_weapon = false;
    bool is_armor = false;
    bool is_salvageable = false;
    bool is_inventory_item = false;
    bool is_storage_item = false;
    bool is_rare_material = false;
    bool is_offered_in_trade = false;
    bool is_sparkly = false;
    bool is_identified = false;
    bool is_prefix_upgradable = false;
    bool is_suffix_upgradable = false;
    bool is_usable = false;
    bool is_tradable = false;
    bool is_inscription = false;
    bool is_rarity_blue = false;
    bool is_rarity_purple = false;
    bool is_rarity_green = false;
    bool is_rarity_gold = false;

    explicit PyItemData(int id) : item_id(id) { GetContext(); }

    void GetContext() {
        if (!ItemMapReady()) return;
        GW::Context::Item* item = GW::item::GetItemById(static_cast<uint32_t>(item_id));
        if (!item) return;

        agent_id = static_cast<int>(item->agent_id);
        agent_item_id = static_cast<int>(item->item_id);
        modifiers.clear();
        for (uint32_t i = 0; i < item->mod_struct_size; ++i) {
            modifiers.emplace_back(item->mod_struct[i].mod);
        }
        is_customized = item->customized != nullptr;
        item_type = PyItemType(static_cast<int>(item->type));
        dye_info = PyDyeInfo(item->dye);
        value = item->value;
        interaction = item->interaction;
        model_id = item->model_id;
        model_file_id = item->model_file_id;
        item_formula = item->item_formula;
        is_material_salvageable = item->is_material_salvageable;
        quantity = item->quantity;
        equipped = item->equipped;
        profession = item->profession;
        slot = item->slot;
        is_stackable = item->GetIsStackable();
        is_inscribable = item->GetIsInscribable();
        is_material = item->GetIsMaterial();
        is_zcoin = item->GetIsZcoin();
        rarity = item->GetRarity();
        uses = static_cast<int>(item->GetUses());
        is_id_kit = item->IsIdentificationKit();
        is_salvage_kit = item->IsSalvageKit();
        is_tome = item->IsTome();
        is_lesser_kit = item->IsLesserKit();
        is_expert_salvage_kit = item->IsExpertSalvageKit();
        is_perfect_salvage_kit = item->IsPerfectSalvageKit();
        is_weapon = item->IsWeapon();
        is_armor = item->IsArmor();
        is_salvageable = item->IsSalvagable();
        is_inventory_item = item->IsInventoryItem();
        is_storage_item = item->IsStorageItem();
        is_rare_material = item->IsRareMaterial();
        is_offered_in_trade = item->IsOfferedInTrade();
        is_sparkly = item->IsSparkly();
        is_identified = item->GetIsIdentified();
        is_prefix_upgradable = item->IsPrefixUpgradable();
        is_suffix_upgradable = item->IsSuffixUpgradable();
        // Deviation from legacy: legacy SafeItem::GetContext left these unset
        // (always false); the reforged Item exposes the methods, so populate them.
        is_usable = item->IsUsable();
        is_tradable = item->IsTradable();
        is_inscription = item->IsInscription();
        is_rarity_blue = item->IsBlue();
        is_rarity_purple = item->IsPurple();
        is_rarity_green = item->IsGreen();
        is_rarity_gold = item->IsGold();
    }

    bool IsItemValid(uint32_t id) const { return GW::item::GetItemById(id) != nullptr; }

    void RequestName() {
        if (!item_id) return;
        const uint32_t id = static_cast<uint32_t>(item_id);
        g_item_name_map[id].name_ready = false;
        g_item_name_map[id].item_name = "";
        std::thread([id]() {
            GW::Context::Item* item = GW::item::GetItemById(id);
            if (!item) {
                g_item_name_map[id].item_name = "No Item";
                g_item_name_map[id].name_ready = true;
                return;
            }
            std::wstring temp_name;
            const auto start_time = std::chrono::steady_clock::now();
            GW::game_thread::Enqueue([item, &temp_name]() {
                GW::item::AsyncGetItemName(item, temp_name);
            });
            while (temp_name.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= 1000) {
                    g_item_name_map[id].item_name = "Timeout";
                    g_item_name_map[id].name_ready = true;
                    return;
                }
            }
            g_item_name_map[id].item_name = ItemWStringToUtf8(temp_name);
            g_item_name_map[id].name_ready = true;
        }).detach();
    }

    bool IsItemNameReady() const {
        const auto it = g_item_name_map.find(static_cast<uint32_t>(item_id));
        return it != g_item_name_map.end() && it->second.name_ready;
    }

    std::string GetName() const {
        const auto it = g_item_name_map.find(static_cast<uint32_t>(item_id));
        return it != g_item_name_map.end() ? it->second.item_name : "";
    }

    std::vector<uint8_t> GetInfoString() const {
        GW::Context::Item* item = GW::item::GetItemById(static_cast<uint32_t>(item_id));
        return item ? EncBytesFrom(item->info_string) : std::vector<uint8_t>{};
    }
    std::vector<uint8_t> GetNameEnc() const {
        GW::Context::Item* item = GW::item::GetItemById(static_cast<uint32_t>(item_id));
        return item ? EncBytesFrom(item->name_enc) : std::vector<uint8_t>{};
    }
    std::vector<uint8_t> GetCompleteNameEnc() const {
        GW::Context::Item* item = GW::item::GetItemById(static_cast<uint32_t>(item_id));
        return item ? EncBytesFrom(item->complete_name_enc) : std::vector<uint8_t>{};
    }
    std::vector<uint8_t> GetSingleItemName() const {
        GW::Context::Item* item = GW::item::GetItemById(static_cast<uint32_t>(item_id));
        return item ? EncBytesFrom(item->single_item_name) : std::vector<uint8_t>{};
    }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyItem, m) {
    m.doc() = "Py4GW Item bindings";

    py::class_<ItemModifier>(m, "ItemModifier")
        .def(py::init<uint32_t>())
        .def("GetIdentifier", &ItemModifier::GetIdentifier)
        .def("GetArg1", &ItemModifier::GetArg1)
        .def("GetArg2", &ItemModifier::GetArg2)
        .def("GetArg", &ItemModifier::GetArg)
        .def("IsValid", &ItemModifier::IsValid)
        .def("GetModBits", &ItemModifier::GetModBits)
        .def("ToString", &ItemModifier::ToString);

    py::enum_<GW::Constants::Rarity>(m, "Rarity")
        .value("White", GW::Constants::Rarity::White)
        .value("Blue", GW::Constants::Rarity::Blue)
        .value("Purple", GW::Constants::Rarity::Purple)
        .value("Gold", GW::Constants::Rarity::Gold)
        .value("Green", GW::Constants::Rarity::Green);

    py::class_<PyItemType>(m, "ItemTypeClass")
        .def(py::init<int>())
        .def("ToInt", &PyItemType::ToInt)
        .def("GetName", &PyItemType::GetName)
        .def("__eq__", &PyItemType::operator==)
        .def("__ne__", &PyItemType::operator!=);

    py::class_<PyDyeColor>(m, "DyeColorClass")
        .def(py::init<int>())
        .def("ToInt", &PyDyeColor::ToInt)
        .def("ToString", &PyDyeColor::ToString)
        .def("__eq__", &PyDyeColor::operator==)
        .def("__ne__", &PyDyeColor::operator!=);

    py::class_<PyDyeInfo>(m, "DyeInfo")
        .def(py::init<>())
        .def_readonly("dye_tint", &PyDyeInfo::dye_tint)
        .def_readonly("dye1", &PyDyeInfo::dye1)
        .def_readonly("dye2", &PyDyeInfo::dye2)
        .def_readonly("dye3", &PyDyeInfo::dye3)
        .def_readonly("dye4", &PyDyeInfo::dye4)
        .def("ToString", &PyDyeInfo::ToString);

    py::class_<PyItemData>(m, "PyItem")
        .def(py::init<int>(), py::arg("item_id"))
        .def("GetContext", &PyItemData::GetContext)
        .def("RequestName", &PyItemData::RequestName)
        .def("IsItemNameReady", &PyItemData::IsItemNameReady)
        .def("GetName", &PyItemData::GetName)
        .def("GetInfoString", &PyItemData::GetInfoString)
        .def("GetNameEnc", &PyItemData::GetNameEnc)
        .def("GetCompleteNameEnc", &PyItemData::GetCompleteNameEnc)
        .def("GetSingleItemName", &PyItemData::GetSingleItemName)
        .def("IsItemValid", &PyItemData::IsItemValid)
        .def_static("GetCompositeModelIDs", &ItemCompositeModelIDs)
        .def_readonly("item_id", &PyItemData::item_id)
        .def_readonly("agent_id", &PyItemData::agent_id)
        .def_readonly("agent_item_id", &PyItemData::agent_item_id)
        .def_readonly("name", &PyItemData::name)
        .def_readonly("modifiers", &PyItemData::modifiers)
        .def_readonly("is_customized", &PyItemData::is_customized)
        .def_readonly("item_type", &PyItemData::item_type)
        .def_readonly("dye_info", &PyItemData::dye_info)
        .def_readonly("value", &PyItemData::value)
        .def_readonly("interaction", &PyItemData::interaction)
        .def_readonly("model_id", &PyItemData::model_id)
        .def_readonly("model_file_id", &PyItemData::model_file_id)
        .def_readonly("item_formula", &PyItemData::item_formula)
        .def_readonly("is_material_salvageable", &PyItemData::is_material_salvageable)
        .def_readonly("quantity", &PyItemData::quantity)
        .def_readonly("equipped", &PyItemData::equipped)
        .def_readonly("profession", &PyItemData::profession)
        .def_readonly("slot", &PyItemData::slot)
        .def_readonly("is_stackable", &PyItemData::is_stackable)
        .def_readonly("is_inscribable", &PyItemData::is_inscribable)
        .def_readonly("is_material", &PyItemData::is_material)
        .def_readonly("is_zcoin", &PyItemData::is_zcoin)
        .def_readonly("rarity", &PyItemData::rarity)
        .def_readonly("uses", &PyItemData::uses)
        .def_readonly("is_id_kit", &PyItemData::is_id_kit)
        .def_readonly("is_salvage_kit", &PyItemData::is_salvage_kit)
        .def_readonly("is_tome", &PyItemData::is_tome)
        .def_readonly("is_lesser_kit", &PyItemData::is_lesser_kit)
        .def_readonly("is_expert_salvage_kit", &PyItemData::is_expert_salvage_kit)
        .def_readonly("is_perfect_salvage_kit", &PyItemData::is_perfect_salvage_kit)
        .def_readonly("is_weapon", &PyItemData::is_weapon)
        .def_readonly("is_armor", &PyItemData::is_armor)
        .def_readonly("is_salvageable", &PyItemData::is_salvageable)
        .def_readonly("is_inventory_item", &PyItemData::is_inventory_item)
        .def_readonly("is_storage_item", &PyItemData::is_storage_item)
        .def_readonly("is_rare_material", &PyItemData::is_rare_material)
        .def_readonly("is_offered_in_trade", &PyItemData::is_offered_in_trade)
        .def_readonly("is_sparkly", &PyItemData::is_sparkly)
        .def_readonly("is_identified", &PyItemData::is_identified)
        .def_readonly("is_prefix_upgradable", &PyItemData::is_prefix_upgradable)
        .def_readonly("is_suffix_upgradable", &PyItemData::is_suffix_upgradable)
        .def_readonly("is_usable", &PyItemData::is_usable)
        .def_readonly("is_tradable", &PyItemData::is_tradable)
        .def_readonly("is_inscription", &PyItemData::is_inscription)
        .def_readonly("is_rarity_blue", &PyItemData::is_rarity_blue)
        .def_readonly("is_rarity_purple", &PyItemData::is_rarity_purple)
        .def_readonly("is_rarity_green", &PyItemData::is_rarity_green)
        .def_readonly("is_rarity_gold", &PyItemData::is_rarity_gold);

    m.def("use_item_by_id", [](uint32_t item_id) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::UseItem(item);
    }, py::arg("item_id"));

    m.def("equip_item_by_id", [](uint32_t item_id, uint32_t agent_id) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::EquipItem(item, agent_id);
    }, py::arg("item_id"), py::arg("agent_id") = 0);

    m.def("drop_item_by_id", [](uint32_t item_id, uint32_t quantity) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::DropItem(item, quantity);
    }, py::arg("item_id"), py::arg("quantity"));

    m.def("pick_up_item_by_id", [](uint32_t item_id, uint32_t call_target) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::PickUpItem(item, call_target);
    }, py::arg("item_id"), py::arg("call_target") = 0);

    m.def("move_item", [](uint32_t item_id, int bag_id, uint32_t slot, uint32_t quantity) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::MoveItem(item, static_cast<GW::Constants::Bag>(bag_id), slot, quantity);
    }, py::arg("item_id"), py::arg("bag_id"), py::arg("slot"), py::arg("quantity") = 0);

    m.def("use_item_by_model_id", [](uint32_t model_id, int bag_start, int bag_end) -> bool {
        return GW::item::UseItemByModelId(model_id, bag_start, bag_end);
    }, py::arg("model_id"), py::arg("bag_start") = 1, py::arg("bag_end") = 4);

    m.def("count_item_by_model_id", [](uint32_t model_id, int bag_start, int bag_end) -> uint32_t {
        return GW::item::CountItemByModelId(model_id, bag_start, bag_end);
    }, py::arg("model_id"), py::arg("bag_start") = 1, py::arg("bag_end") = 4);

    m.def("get_gold_amount_on_character", []() -> uint32_t {
        return GW::item::GetGoldAmountOnCharacter();
    });

    m.def("get_gold_amount_in_storage", []() -> uint32_t {
        return GW::item::GetGoldAmountInStorage();
    });

    m.def("drop_gold", [](uint32_t amount) -> bool {
        return GW::item::DropGold(amount);
    }, py::arg("amount") = 1);

    m.def("deposit_gold", [](uint32_t amount) -> uint32_t {
        return GW::item::DepositGold(amount);
    }, py::arg("amount") = 0);

    m.def("withdraw_gold", [](uint32_t amount) -> uint32_t {
        return GW::item::WithdrawGold(amount);
    }, py::arg("amount") = 0);

    m.def("change_gold", [](uint32_t character_gold, uint32_t storage_gold) -> bool {
        return GW::item::ChangeGold(character_gold, storage_gold);
    }, py::arg("character_gold"), py::arg("storage_gold"));

    m.def("salvage_start", [](uint32_t salvage_kit_id, uint32_t item_id) -> bool {
        return GW::item::SalvageStart(salvage_kit_id, item_id);
    }, py::arg("salvage_kit_id"), py::arg("item_id"));

    m.def("identify_item", [](uint32_t identification_kit_id, uint32_t item_id) -> bool {
        return GW::item::IdentifyItem(identification_kit_id, item_id);
    }, py::arg("identification_kit_id"), py::arg("item_id"));

    m.def("salvage_session_cancel", []() -> bool {
        return GW::item::SalvageSessionCancel();
    });

    m.def("salvage_session_done", []() -> bool {
        return GW::item::SalvageSessionDone();
    });

    m.def("destroy_item", [](uint32_t item_id) -> bool {
        return GW::item::DestroyItem(item_id);
    }, py::arg("item_id"));

    m.def("salvage_materials", []() -> bool {
        return GW::item::SalvageMaterials();
    });

    m.def("open_xunlai_window", [](bool anniversary_pane_unlocked) {
        GW::item::OpenXunlaiWindow(anniversary_pane_unlocked);
    }, py::arg("anniversary_pane_unlocked") = true);

    m.def("get_storage_page", []() -> int {
        return static_cast<int>(GW::item::GetStoragePage());
    });

    m.def("get_is_storage_open", []() -> bool {
        return GW::item::GetIsStorageOpen();
    });

    m.def("can_access_xunlai_chest", []() -> bool {
        return GW::item::CanAccessXunlaiChest();
    });

    m.def("get_material_storage_stack_size", []() -> uint32_t {
        return GW::item::GetMaterialStorageStackSize();
    });

    // Static PvP-unlock (item-mod) table: number of entries, and the game-composed
    // encoded name + description for a given unlock index (decode client-side).
    m.def("get_pvp_unlock_count", []() -> uint32_t {
        return GW::item::GetPvpUnlockCount();
    });

    m.def("get_pvp_unlock_name_enc", [](uint32_t unlock_index) -> std::pair<std::vector<uint8_t>, std::vector<uint8_t>> {
        wchar_t* name = nullptr;
        wchar_t* desc = nullptr;
        GW::item::GetPvpUnlockEncodedName(unlock_index, &name, &desc);
        return { EncBytesFrom(name), EncBytesFrom(desc) };
    });
}

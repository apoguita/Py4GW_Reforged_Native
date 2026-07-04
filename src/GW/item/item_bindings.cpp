#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/item/item.h"

#include <cstdint>
#include <string>

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
}

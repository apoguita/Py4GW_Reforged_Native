#include "base/py_bindings.h"

#include "GW/game_thread/game_thread.h"
#include "GW/item/item.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

namespace py = pybind11;

namespace {

bool IsMapReady() {
    const auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving()
        && instance_type != GW::Constants::InstanceType::Loading;
}

// ── Bag class (parity with legacy py_Inventory.h SafeBag) ──
struct Bag {
    int id = 0;
    std::string name;
    int container_item = 0;
    int items_count = 0;
    bool is_inventory_bag = false;
    bool is_storage_bag = false;
    bool is_material_storage = false;

    Bag() = default;
    Bag(int bag_id, const std::string& bag_name) : id(bag_id), name(bag_name) {}

    void GetContext() {
        if (!IsMapReady()) return;
        GW::Context::Bag* bag = GW::item::GetBag(static_cast<GW::Constants::Bag>(id));
        if (!bag) return;
        container_item = bag->container_item;
        items_count = bag->items_count;
        is_inventory_bag = bag->IsInventoryBag();
        is_storage_bag = bag->IsStorageBag();
        is_material_storage = bag->IsMaterialStorage();
    }

    int GetSize() const {
        if (!IsMapReady()) return 0;
        GW::Context::Bag* bag = GW::item::GetBag(static_cast<GW::Constants::Bag>(id));
        return bag ? static_cast<int>(bag->items.size()) : 0;
    }

    int GetItemCount() const { return items_count; }

    py::list GetItems() const {
        py::list result;
        if (!IsMapReady()) return result;
        GW::Context::Bag* bag = GW::item::GetBag(static_cast<GW::Constants::Bag>(id));
        if (!bag) return result;
        for (size_t slot = 0; slot < bag->items.size(); ++slot) {
            GW::Context::Item* item = bag->items[slot];
            if (!item) continue;
            py::dict entry;
            entry["item_id"] = item->item_id;
            entry["slot"] = static_cast<uint32_t>(slot);
            entry["model_id"] = item->model_id;
            entry["quantity"] = item->quantity;
            result.append(entry);
        }
        return result;
    }
};

// ── PyInventory class (parity with legacy py_Inventory.h Inventory) ──
struct PyInventory {
    void OpenXunlaiWindow() {
        GW::game_thread::Enqueue([] {
            GW::item::OpenXunlaiWindow();
        });
    }
    bool GetIsStorageOpen() { return GW::item::GetIsStorageOpen(); }

    void PickUpItem(uint32_t item_id, bool call_target) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (item) GW::game_thread::Enqueue([item, call_target] { GW::item::PickUpItem(item, call_target); });
    }
    void DropItem(uint32_t item_id, uint32_t quantity) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (item) GW::game_thread::Enqueue([item, quantity] { GW::item::DropItem(item, quantity); });
    }
    void EquipItem(uint32_t item_id, uint32_t agent_id) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (item) GW::game_thread::Enqueue([item, agent_id] { GW::item::EquipItem(item, agent_id); });
    }
    void UseItem(uint32_t item_id) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (item) GW::game_thread::Enqueue([item] { GW::item::UseItem(item); });
    }
    void DestroyItem(uint32_t item_id) {
        GW::game_thread::Enqueue([item_id] { GW::item::DestroyItem(item_id); });
    }
    void IdentifyItem(uint32_t kit_id, uint32_t item_id) {
        GW::game_thread::Enqueue([kit_id, item_id] { GW::item::IdentifyItem(kit_id, item_id); });
    }
    uint32_t GetHoveredItemID() {
        GW::Context::Item* item = GW::item::GetHoveredItem();
        return item ? item->item_id : 0;
    }
    uint32_t GetGoldAmount() { return GW::item::GetGoldAmountOnCharacter(); }
    uint32_t GetGoldAmountInStorage() { return GW::item::GetGoldAmountInStorage(); }
    void DepositGold(uint32_t amount) { GW::game_thread::Enqueue([amount] { GW::item::DepositGold(amount); }); }
    void WithdrawGold(uint32_t amount) { GW::game_thread::Enqueue([amount] { GW::item::WithdrawGold(amount); }); }
    void DropGold(uint32_t amount) { GW::game_thread::Enqueue([amount] { GW::item::DropGold(amount); }); }
    void MoveItem(uint32_t item_id, uint32_t bag_id, uint32_t slot, uint32_t quantity) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (item) GW::game_thread::Enqueue([item, bag_id, slot, quantity] {
            GW::item::MoveItem(item, static_cast<GW::Constants::Bag>(bag_id), slot, quantity);
        });
    }
    void Salvage(uint32_t kit_id, uint32_t item_id) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        GW::Context::Item* kit = GW::item::GetItemById(kit_id);
        if (item && kit && kit->IsSalvageKit() && item->IsSalvagable())
            GW::item::SalvageStart(kit->item_id, item->item_id);
    }
    void AcceptSalvageWindow() {
        GW::game_thread::Enqueue([] {
            GW::ui::ButtonClick(GW::ui::GetChildFrame(GW::ui::GetFrameByLabel(L"Game"), { 0x6, 0x62, 0x6 }));
        });
    }
};

// ── dict-based bag snapshot (reforged modern path) ──
py::dict GetBagSnapshot(int bag_id) {
    py::dict out;
    out["id"] = bag_id; out["items_count"] = 0; out["container_item"] = 0;
    out["size"] = 0; out["is_inventory_bag"] = false;
    out["is_storage_bag"] = false; out["is_material_storage"] = false;
    py::list items; out["items"] = items;
    if (!IsMapReady()) return out;
    GW::Context::Bag* bag = GW::item::GetBag(static_cast<GW::Constants::Bag>(bag_id));
    if (!bag) return out;
    out["items_count"] = bag->items_count;
    out["container_item"] = bag->container_item;
    out["size"] = bag->items.size();
    out["is_inventory_bag"] = bag->IsInventoryBag();
    out["is_storage_bag"] = bag->IsStorageBag();
    out["is_material_storage"] = bag->IsMaterialStorage();
    for (size_t slot = 0; slot < bag->items.size(); ++slot) {
        GW::Context::Item* item = bag->items[slot];
        if (!item) continue;
        py::dict entry;
        entry["item_id"] = item->item_id; entry["slot"] = static_cast<uint32_t>(slot);
        entry["model_id"] = item->model_id; entry["quantity"] = item->quantity;
        items.append(entry);
    }
    return out;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyInventory, m) {
    m.doc() = "Py4GW Inventory bindings";

    // ── Bag class (legacy surface) ──
    py::class_<Bag>(m, "Bag")
        .def(py::init<int, const std::string&>(), py::arg("id"), py::arg("name") = "")
        .def("GetItems", &Bag::GetItems)
        .def("GetItemCount", &Bag::GetItemCount)
        .def("GetSize", &Bag::GetSize)
        .def("GetContext", &Bag::GetContext)
        .def_readonly("id", &Bag::id)
        .def_readonly("name", &Bag::name)
        .def_readonly("container_item", &Bag::container_item)
        .def_readonly("items_count", &Bag::items_count)
        .def_readonly("is_inventory_bag", &Bag::is_inventory_bag)
        .def_readonly("is_storage_bag", &Bag::is_storage_bag)
        .def_readonly("is_material_storage", &Bag::is_material_storage);

    // ── PyInventory class (legacy surface) ──
    py::class_<PyInventory>(m, "PyInventory")
        .def(py::init<>())
        .def("OpenXunlaiWindow", &PyInventory::OpenXunlaiWindow)
        .def("GetIsStorageOpen", &PyInventory::GetIsStorageOpen)
        .def("PickUpItem", &PyInventory::PickUpItem, py::arg("item_id"), py::arg("call_target") = false)
        .def("DropItem", &PyInventory::DropItem, py::arg("item_id"), py::arg("quantity") = 1)
        .def("EquipItem", &PyInventory::EquipItem, py::arg("item_id"), py::arg("agent_id"))
        .def("UseItem", &PyInventory::UseItem, py::arg("item_id"))
        .def("DestroyItem", &PyInventory::DestroyItem, py::arg("item_id"))
        .def("IdentifyItem", &PyInventory::IdentifyItem, py::arg("id_kit_id"), py::arg("item_id"))
        .def("GetHoveredItemID", &PyInventory::GetHoveredItemID)
        .def("GetGoldAmount", &PyInventory::GetGoldAmount)
        .def("GetGoldAmountInStorage", &PyInventory::GetGoldAmountInStorage)
        .def("DepositGold", &PyInventory::DepositGold, py::arg("amount"))
        .def("WithdrawGold", &PyInventory::WithdrawGold, py::arg("amount"))
        .def("DropGold", &PyInventory::DropGold, py::arg("amount"))
        .def("MoveItem", &PyInventory::MoveItem, py::arg("item_id"), py::arg("bag_id"), py::arg("slot"), py::arg("quantity") = 1)
        .def("Salvage", &PyInventory::Salvage, py::arg("salv_kit_id"), py::arg("item_id"))
        .def("AcceptSalvageWindow", &PyInventory::AcceptSalvageWindow);

    // ── Modern dict-based surface ──
    m.def("get_bag", &GetBagSnapshot, py::arg("bag_id"));

    m.def("get_hovered_item_id", []() -> uint32_t {
        GW::Context::Item* item = GW::item::GetHoveredItem();
        return item ? item->item_id : 0;
    });

    m.def("salvage", [](uint32_t salv_kit_id, uint32_t item_id) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        GW::Context::Item* kit = GW::item::GetItemById(salv_kit_id);
        if (item && kit && kit->IsSalvageKit() && item->IsSalvagable())
            GW::item::SalvageStart(kit->item_id, item->item_id);
    }, py::arg("salv_kit_id"), py::arg("item_id"));

    m.def("accept_salvage_window", []() {
        GW::game_thread::Enqueue([] {
            GW::ui::ButtonClick(GW::ui::GetChildFrame(GW::ui::GetFrameByLabel(L"Game"), { 0x6, 0x62, 0x6 }));
        });
    });
}

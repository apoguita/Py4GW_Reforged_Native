#include "base/py_bindings.h"

#include "GW/merchant/merchant.h"
#include "GW/game_thread/game_thread.h"
#include "GW/item/item.h"
#include "listeners/listeners.h"

#include <cstdint>
#include <vector>

namespace py = pybind11;

namespace {

// Legacy PyMerchant class surface. Mirrors the legacy py_merchant.h API:
// per-NPC-type buy/sell/quote methods delegating to the unified
// GW::merchant::TransactItems / RequestQuote with the correct TransactionType.
struct PyMerchant {
    // NOTE: these mirror legacy include/py_merchant.h EXACTLY (same shape/form): fetch the live
    // GW::item::Item* via GetItemById, use &item->item_id (the live item's id field, not a copy),
    // keep the count var + cost*count, and assign every give/recv field explicitly. The legacy
    // `transaction_complete = false` / `quoted_value = -1` resets are kept too - they live on the
    // listener now, but they must still fire here, at the same points, or the flags latch and
    // Python's wait loops read stale state.

    // --- Trader (buy tab) ---
    bool TraderBuyItem(uint32_t item_id, uint32_t cost) {
        auto* item = GW::item::GetItemById(item_id);
        uint32_t count = 1;
        if (item) {
            PY4GW::listeners::Merchant().ResetTransaction();
            cost = cost * count;
            GW::Context::MerchantTransactionInfo give, recv;
            give.item_count = 0;
            give.item_ids = nullptr;
            give.item_quantities = nullptr;
            recv.item_count = count;
            recv.item_ids = &item->item_id;
            recv.item_quantities = nullptr;
            GW::game_thread::Enqueue([cost, give, recv]() {
                GW::merchant::TransactItems(GW::Constants::TransactionType::TraderBuy, cost, give, 0, recv);
            });
            return true;
        }
        return false;
    }
    bool TraderSellItem(uint32_t item_id, uint32_t price) {
        auto* item = GW::item::GetItemById(item_id);
        uint32_t count = 1;
        if (item) {
            PY4GW::listeners::Merchant().ResetTransaction();
            price = price * count;
            GW::Context::MerchantTransactionInfo give, recv;
            give.item_count = count;
            give.item_ids = &item->item_id;
            give.item_quantities = nullptr;
            recv.item_count = 0;
            recv.item_ids = nullptr;
            recv.item_quantities = nullptr;
            GW::game_thread::Enqueue([price, give, recv]() {
                GW::merchant::TransactItems(GW::Constants::TransactionType::TraderSell, 0, give, price, recv);
            });
            return true;
        }
        return false;
    }
    bool TraderRequestQuote(uint32_t item_id) {
        auto* item = GW::item::GetItemById(item_id);
        if (item) {
            PY4GW::listeners::Merchant().ResetQuote();
            GW::Context::MerchantQuoteInfo give, recv;
            give.unknown = 0;
            give.item_count = 0;
            give.item_ids = nullptr;
            recv.unknown = 0;
            recv.item_count = 1;
            recv.item_ids = &item->item_id;
            GW::game_thread::Enqueue([give, recv]() {
                GW::merchant::RequestQuote(GW::Constants::TransactionType::TraderBuy, give, recv);
            });
            return true;
        }
        return false;
    }
    bool TraderRequestSellQuote(uint32_t item_id) {
        auto* item = GW::item::GetItemById(item_id);
        if (item) {
            PY4GW::listeners::Merchant().ResetQuote();
            GW::Context::MerchantQuoteInfo give, recv;
            recv.unknown = 0;
            recv.item_count = 0;
            recv.item_ids = nullptr;
            give.unknown = 0;
            give.item_count = 1;
            give.item_ids = &item->item_id;
            GW::game_thread::Enqueue([give, recv]() {
                GW::merchant::RequestQuote(GW::Constants::TransactionType::TraderSell, give, recv);
            });
            return true;
        }
        return false;
    }

    // --- Merchant (materials / rune trader) ---
    bool MerchantBuyItem(uint32_t item_id, uint32_t cost) {
        auto* item = GW::item::GetItemById(item_id);
        uint32_t count = 1;
        if (item) {
            PY4GW::listeners::Merchant().ResetTransaction();
            cost = cost * count;
            GW::Context::MerchantTransactionInfo give, recv;
            give.item_count = 0;
            give.item_ids = nullptr;
            give.item_quantities = nullptr;
            recv.item_count = count;
            recv.item_ids = &item->item_id;
            recv.item_quantities = nullptr;
            GW::game_thread::Enqueue([cost, give, recv]() {
                GW::merchant::TransactItems(GW::Constants::TransactionType::MerchantBuy, cost, give, 0, recv);
            });
            return true;
        }
        return false;
    }
    bool MerchantSellItem(uint32_t item_id, uint32_t price) {
        auto* item = GW::item::GetItemById(item_id);
        uint32_t count = 1;
        if (item) {
            PY4GW::listeners::Merchant().ResetTransaction();
            price = price * count;
            GW::Context::MerchantTransactionInfo give, recv;
            give.item_count = count;
            give.item_ids = &item->item_id;
            give.item_quantities = nullptr;
            recv.item_count = 0;
            recv.item_ids = nullptr;
            recv.item_quantities = nullptr;
            GW::game_thread::Enqueue([price, give, recv]() {
                GW::merchant::TransactItems(GW::Constants::TransactionType::MerchantSell, 0, give, price, recv);
            });
            return true;
        }
        return false;
    }

    // --- Crafter --- 1:1 with legacy py_merchant.h:69-96 (no GetItemById guard, no Enqueue,
    // give pointers straight into the vectors, recv_info.item_ids = &item_id).
    bool CrafterBuyItems(uint32_t item_id, uint32_t cost,
                         const std::vector<uint32_t>& give_item_ids,
                         const std::vector<uint32_t>& give_item_quantities) {
        if (give_item_ids.size() != give_item_quantities.size()) {
            return false;
        }
        PY4GW::listeners::Merchant().ResetTransaction();

        uint32_t* item_ids_ptr = give_item_ids.empty() ? nullptr : const_cast<uint32_t*>(give_item_ids.data());
        uint32_t* item_quantities_ptr = give_item_quantities.empty() ? nullptr : const_cast<uint32_t*>(give_item_quantities.data());

        GW::Context::MerchantTransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_item_ids.size());
        give_info.item_ids = item_ids_ptr;
        give_info.item_quantities = item_quantities_ptr;

        GW::Context::MerchantTransactionInfo recv_info;
        recv_info.item_count = 1;
        recv_info.item_ids = &item_id;
        recv_info.item_quantities = nullptr;

        return GW::merchant::TransactItems(GW::Constants::TransactionType::CrafterBuy, cost, give_info, 0, recv_info);
    }

    // --- Collector --- 1:1 with legacy py_merchant.h:98-126.
    bool CollectorBuyItems(uint32_t item_id, uint32_t cost,
                           const std::vector<uint32_t>& give_item_ids,
                           const std::vector<uint32_t>& give_item_quantities) {
        if (give_item_ids.size() != give_item_quantities.size()) {
            return false;
        }

        uint32_t* item_ids_ptr = give_item_ids.empty() ? nullptr : const_cast<uint32_t*>(give_item_ids.data());
        uint32_t* item_quantities_ptr = give_item_quantities.empty() ? nullptr : const_cast<uint32_t*>(give_item_quantities.data());

        PY4GW::listeners::Merchant().ResetTransaction();

        GW::Context::MerchantTransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_item_ids.size());
        give_info.item_ids = item_ids_ptr;
        give_info.item_quantities = item_quantities_ptr;

        GW::Context::MerchantTransactionInfo recv_info;
        recv_info.item_count = 1;
        recv_info.item_ids = &item_id;
        recv_info.item_quantities = nullptr;

        return GW::merchant::TransactItems(GW::Constants::TransactionType::CollectorBuy, cost, give_info, 0, recv_info);
    }

    // --- State getters (delegate to listeners::Merchant singleton) ---
    std::vector<uint32_t> GetTraderItems() const       { return PY4GW::listeners::Merchant().GetMerchantItems(); }
    std::vector<uint32_t> GetTraderItems2() const      { return {}; }  // legacy never populated
    std::vector<uint32_t> GetMerchantItems() const     { return PY4GW::listeners::Merchant().GetMerchantWindowItems(); }
    int                GetQuotedValue() const          { return PY4GW::listeners::Merchant().GetQuotedValue(); }
    uint32_t           GetQuotedItemID() const         { return PY4GW::listeners::Merchant().GetQuotedItemId(); }
    bool               IsTransactionComplete() const   { return PY4GW::listeners::Merchant().IsTransactionComplete(); }
    void               Update()                        { /* legacy no-op - state refreshed by listeners */ }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyMerchant, m) {
    m.doc() = "Py4GW Merchant bindings";

    // -- Legacy PyMerchant class surface (parity with py_merchant.h) --
    py::class_<PyMerchant>(m, "PyMerchant")
        .def(py::init<>())
        .def("trader_buy_item",          &PyMerchant::TraderBuyItem,          py::arg("item_id"), py::arg("cost"), "Buy an item by model ID and quantity")
        .def("trader_sell_item",         &PyMerchant::TraderSellItem,         py::arg("item_id"), py::arg("price"), "Sell an item by model ID and quantity")
        .def("trader_request_quote",     &PyMerchant::TraderRequestQuote,     py::arg("item_id"), "Request a price quote for the item by its ID")
        .def("trader_request_sell_quote",&PyMerchant::TraderRequestSellQuote, py::arg("item_id"), "Request a price quote for the item by its ID")
        .def("merchant_buy_item",        &PyMerchant::MerchantBuyItem,        py::arg("item_id"), py::arg("cost"), "Buy an item by model ID and quantity")
        .def("merchant_sell_item",       &PyMerchant::MerchantSellItem,       py::arg("item_id"), py::arg("price"), "Sell an item by model ID and quantity")
        .def("crafter_buy_item",         &PyMerchant::CrafterBuyItems,        py::arg("item_id"), py::arg("cost"),
             py::arg("give_item_ids"), py::arg("give_item_quantities"), "Buy an item by model ID and quantity")
        .def("collector_buy_item",       &PyMerchant::CollectorBuyItems,      py::arg("item_id"), py::arg("cost"),
             py::arg("give_item_ids"), py::arg("give_item_quantities"), "Buy an item by model ID and quantity")
        .def("get_trader_item_list",     &PyMerchant::GetTraderItems, "Get the list of merchant items queried")
        .def("get_trader_item_list2",    &PyMerchant::GetTraderItems2, "Get the list of merchant items queried")
        .def("get_merchant_item_list",   &PyMerchant::GetMerchantItems, "Get the list of merchant items queried")
        .def("get_quoted_value",         &PyMerchant::GetQuotedValue, "Get the last quoted price for the item")
        .def("get_quoted_item_id",       &PyMerchant::GetQuotedItemID, "Get the last quoted item ID")
        .def("is_transaction_complete",  &PyMerchant::IsTransactionComplete, "Check if the last transaction was completed")
        .def("update",                   &PyMerchant::Update, "Update the Merchant state to be called from Python");

    // -- Modern free-function surface (reforged additions) --
    m.def("transact_items", [](uint32_t type, uint32_t gold_give, const py::list& give_item_ids, const py::list& give_quantities,
                                uint32_t gold_recv, const py::list& recv_item_ids, const py::list& recv_quantities) -> bool {
        auto txn_type = static_cast<GW::Constants::TransactionType>(type);

        size_t give_count = std::min(give_item_ids.size(), give_quantities.size());
        std::vector<uint32_t> g_ids(give_count), g_qties(give_count);
        for (size_t i = 0; i < give_count; ++i) {
            g_ids[i] = give_item_ids[i].cast<uint32_t>();
            g_qties[i] = give_quantities[i].cast<uint32_t>();
        }
        GW::Context::MerchantTransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_count);
        give_info.item_ids = give_count > 0 ? g_ids.data() : nullptr;
        give_info.item_quantities = give_count > 0 ? g_qties.data() : nullptr;

        size_t recv_count = std::min(recv_item_ids.size(), recv_quantities.size());
        std::vector<uint32_t> r_ids(recv_count), r_qties(recv_count);
        for (size_t i = 0; i < recv_count; ++i) {
            r_ids[i] = recv_item_ids[i].cast<uint32_t>();
            r_qties[i] = recv_quantities[i].cast<uint32_t>();
        }
        GW::Context::MerchantTransactionInfo recv_info;
        recv_info.item_count = static_cast<uint32_t>(recv_count);
        recv_info.item_ids = recv_count > 0 ? r_ids.data() : nullptr;
        recv_info.item_quantities = recv_count > 0 ? r_qties.data() : nullptr;

        return GW::merchant::TransactItems(txn_type, gold_give, give_info, gold_recv, recv_info);
    },
    py::arg("type"), py::arg("gold_give") = 0, py::arg("give_item_ids") = py::list(),
    py::arg("give_quantities") = py::list(), py::arg("gold_recv") = 0,
    py::arg("recv_item_ids") = py::list(), py::arg("recv_quantities") = py::list());

    m.def("request_quote", [](uint32_t type, const py::list& give_item_ids, const py::list& recv_item_ids) -> bool {
        auto quote_type = static_cast<GW::Constants::TransactionType>(type);
        size_t give_count = give_item_ids.size();
        std::vector<uint32_t> g_ids(give_count);
        for (size_t i = 0; i < give_count; ++i)
            g_ids[i] = give_item_ids[i].cast<uint32_t>();
        GW::Context::MerchantQuoteInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_count);
        give_info.item_ids = give_count > 0 ? g_ids.data() : nullptr;

        size_t recv_count = recv_item_ids.size();
        std::vector<uint32_t> r_ids(recv_count);
        for (size_t i = 0; i < recv_count; ++i)
            r_ids[i] = recv_item_ids[i].cast<uint32_t>();
        GW::Context::MerchantQuoteInfo recv_info;
        recv_info.item_count = static_cast<uint32_t>(recv_count);
        recv_info.item_ids = recv_count > 0 ? r_ids.data() : nullptr;

        return GW::merchant::RequestQuote(quote_type, give_info, recv_info);
    }, py::arg("type"), py::arg("give_item_ids") = py::list(), py::arg("recv_item_ids") = py::list());
}


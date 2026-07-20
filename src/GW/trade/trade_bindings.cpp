#include "base/py_bindings.h"

#include "GW/trade/trade.h"
#include "GW/game_thread/game_thread.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyTrade, m) {
    m.doc() = "Py4GW Trade bindings";

    m.def("open_trade_window", [](uint32_t agent_id) -> bool {
        GW::game_thread::Enqueue([agent_id]() {
            GW::trade::OpenTradeWindow(agent_id);
        });
        return true;
    }, py::arg("agent_id"));

    m.def("accept_trade", []() -> bool {
        return GW::trade::AcceptTrade();
    });

    m.def("cancel_trade", []() -> bool {
        return GW::trade::CancelTrade();
    });

    m.def("change_offer", []() -> bool {
        return GW::trade::ChangeOffer();
    });

    m.def("submit_offer", [](uint32_t gold) -> bool {
        return GW::trade::SubmitOffer(gold);
    }, py::arg("gold"));

    m.def("remove_item", [](uint32_t slot) -> bool {
        return GW::trade::RemoveItem(slot);
    }, py::arg("slot"));

    m.def("offer_item", [](uint32_t item_id, uint32_t quantity) -> bool {
        return GW::trade::OfferItem(item_id, quantity);
    }, py::arg("item_id"), py::arg("quantity") = 0);

    m.def("is_item_offered", [](uint32_t item_id) -> bool {
        return GW::trade::IsItemOffered(item_id) != nullptr;
    }, py::arg("item_id"));
}

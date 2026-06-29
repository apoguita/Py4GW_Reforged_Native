#include "base/error_handling.h"

#include "GW/trade/trade.h"

#include "GW/context/context.h"
#include "GW/ui/ui.h"

namespace GW::trade {

ui::Frame* GetTradeWindow() {
    return ui::GetFrameByLabel(L"DlgTrade");
}

uint32_t GetTradeState() {
    const auto c = Context::GetTradeContext();
    return c ? c->flags : 0;
}

bool OpenTradeWindow(uint32_t agent_id) {
    return ui::SendUIMessage(ui::UIMessage::kInitiateTrade, (void*)agent_id);
}

bool AcceptTrade() {
    return ui::ButtonClick(ui::GetChildFrame(GetTradeWindow(), 2));
}

bool CancelTrade() {
    const auto parent = GetTradeWindow();
    return ui::ButtonClick(ui::GetChildFrame(parent, 0xd)) || ui::ButtonClick(ui::GetChildFrame(parent, 1));
}

bool ChangeOffer() {
    return ui::ButtonClick(ui::GetChildFrame(GetTradeWindow(), 0));
}

bool SubmitOffer(uint32_t) {
    return ChangeOffer();
}

bool RemoveItem(uint32_t item_id) {
    if (GetTradeState() != Context::TradeContext::TRADE_INITIATED)
        return false;
    const auto frame = ui::GetFrameByLabel(L"CartPlayer");
    struct {
        uint32_t h0000 = 0;
        uint32_t h0004 = 9;
        uint32_t h0008 = 6;
        uint32_t h000c;
    } action;
    action.h000c = item_id;
    return frame && IsItemOffered(item_id) && ui::SendFrameUIMessage(frame, ui::UIMessage::kToggleButtonDown, &action);
}

TradeItem* IsItemOffered(uint32_t item_id) {
    const auto ctx = Context::GetTradeContext();
    if (!ctx)
        return nullptr;
    auto& items = ctx->player.items;
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].item_id != item_id)
            continue;
        return &items[i];
    }
    return nullptr;
}

bool OfferItem(uint32_t item_id, uint32_t quantity) {
    if (GetTradeState() != Context::TradeContext::TRADE_INITIATED)
        return false;
    const auto frame = ui::GetFrameByLabel(L"CartPlayer");
    struct {
        uint32_t h0000 = 0;
        uint32_t h0004 = 2;
        uint32_t h0008 = 6;
        uint32_t* h000c;
    } action;
    uint32_t item_id_and_qty[] = { item_id, quantity };
    action.h000c = item_id_and_qty;

    return frame && !IsItemOffered(item_id) && ui::SendFrameUIMessage(frame, ui::UIMessage::kToggleButtonDown, &action);
}

}  // namespace GW::trade

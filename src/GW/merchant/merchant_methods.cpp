#include "base/error_handling.h"

#include "GW/merchant/merchant.h"

#include "GW/context/context.h"
#include "GW/context/world_context.h"

namespace GW::merchant {

bool TransactItems(TransactionType type, uint32_t gold_give, TransactionInfo give, uint32_t gold_recv, TransactionInfo recv) {
    if (g_transact_item_func) {
        g_transact_item_func(type, gold_give, give, gold_recv, recv);
        return true;
    }
    return false;
}

bool RequestQuote(TransactionType type, QuoteInfo give, QuoteInfo recv) {
    if (g_request_quote_func) {
        g_request_quote_func(type, 0, give, recv);
        return true;
    }
    return false;
}

MerchItemArray* GetMerchantItemsArray() {
    auto* w = Context::GetWorldContext();
    return w && w->merch_items.valid() ? &w->merch_items : nullptr;
}

}  // namespace GW::merchant

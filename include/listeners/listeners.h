#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "base/timer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PY4GW::listeners {

// A named listener over a set of native game (StoC) events whose sole job is to
// be switched on or off. Enable installs the packet callbacks, Disable removes
// them; both are idempotent. Subclasses implement Install/Uninstall for their
// packet set. New listeners are added by declaring another subclass here.
class Listener {
public:
    virtual ~Listener() = default;
    virtual const char* Name() const = 0;

    void Enable();
    void Disable();
    void SetEnabled(bool enabled);
    void Toggle();
    bool IsEnabled() const { return enabled_; }

    // Whether Initialize() enables this listener at startup. Default true;
    // override to false for opt-in listeners that should stay off until a
    // consumer toggles them on (e.g. high-volume capture).
    virtual bool EnabledByDefault() const { return true; }

protected:
    virtual void Install() = 0;
    virtual void Uninstall() = 0;

    bool enabled_ = false;
};

// Merchant listener migrated from the legacy Py4GW::InitializeMerchantCallbacks.
// Populates quoted-price, transaction, and merchant/window item state while
// enabled.
class MerchantListener : public Listener {
public:
    const char* Name() const override { return "merchant"; }

    uint32_t GetQuotedItemId() const { return quoted_item_id_; }
    // Legacy `int quoted_value`: -1 is the "no quote yet" sentinel that Python
    // waits on (Routines.Yield.Merchant._wait_for_quote checks `>= 0`). Must not
    // be unsigned or the sentinel is unrepresentable.
    int GetQuotedValue() const { return quoted_value_; }
    bool IsTransactionComplete() const { return transaction_complete_; }
    const std::vector<uint32_t>& GetMerchantWindowItems() const { return merchant_window_items_; }
    const std::vector<uint32_t>& GetMerchantItems() const { return merch_items_; }

    // Pre-transaction resets. Legacy did these inline at the top of every
    // PyMerchant buy/sell/craft/collect + quote method; without them the flags
    // latch and every subsequent wait short-circuits on stale state.
    void ResetTransaction() { transaction_complete_ = false; }
    void ResetQuote() { quoted_value_ = -1; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    void OnPriceReceived(uint32_t item_id, uint32_t price);
    void OnTransactionComplete();
    void OnNormalMerchantItemsReceived(const uint32_t* item_ids, uint32_t count);
    void OnItemStreamEnd(uint32_t unk1);

    PY4GW::HookEntry quoted_item_price_entry_;
    PY4GW::HookEntry transaction_done_entry_;
    PY4GW::HookEntry item_stream_end_entry_;
    PY4GW::HookEntry window_items_entry_;
    PY4GW::HookEntry window_items_end_entry_;

    uint32_t quoted_item_id_ = 0;
    int quoted_value_ = 0;
    bool transaction_complete_ = false;
    std::vector<uint32_t> merchant_window_items_;
    std::vector<uint32_t> merch_items_;
    PY4GW::Timer reset_merchant_window_item_;
};

// Module lifecycle: registers every listener and enables the defaults. Wired
// into the top-level Py4GW bootstrap after the GW layer (StoC hooks) is ready.
bool Initialize();
void Shutdown();

// Runtime toggle surface, addressed by listener name (see Listener::Name).
// Each returns false when the name is unknown.
std::vector<std::string> GetListenerNames();
bool Enable(const std::string& name);
bool Disable(const std::string& name);
bool Toggle(const std::string& name);
bool SetEnabled(const std::string& name, bool enabled);
bool IsEnabled(const std::string& name);

// Direct accessor for the merchant listener (state reads).
MerchantListener& Merchant();

}  // namespace PY4GW::listeners

#include "base/error_handling.h"

#include "listeners/listeners.h"

#include "listeners/agent_events_listener.h"
#include "listeners/auto_cancel_ua_listener.h"
#include "listeners/auto_open_chest_listener.h"
#include "listeners/auto_return_listener.h"
#include "listeners/cinematic_skip_listener.h"
#include "listeners/faction_donate_listener.h"
#include "listeners/faction_warning_listener.h"
#include "listeners/keep_quest_listener.h"
#include "listeners/memory_patch_listeners.h"
#include "listeners/signet_capture_listener.h"
#include "listeners/skill_filter_listener.h"
#include "GW/common/stoc.h"
#include "GW/context/context.h"
#include "GW/context/item.h"
#include "GW/context/world.h"
#include "GW/stoc/stoc.h"

namespace PY4GW::listeners {

namespace StoCPacket = GW::Packet::StoC;

/* ---------------- Listener toggle base ---------------- */

void Listener::Enable() {
    if (enabled_) {
        return;
    }
    Install();
    enabled_ = true;
}

void Listener::Disable() {
    if (!enabled_) {
        return;
    }
    Uninstall();
    enabled_ = false;
}

void Listener::SetEnabled(bool enabled) {
    if (enabled) {
        Enable();
    } else {
        Disable();
    }
}

void Listener::Toggle() {
    SetEnabled(!enabled_);
}

/* ---------------- MerchantListener ---------------- */

void MerchantListener::OnPriceReceived(uint32_t item_id, uint32_t price) {
    quoted_item_id_ = item_id;
    quoted_value_ = static_cast<int>(price);
}

void MerchantListener::OnTransactionComplete() {
    transaction_complete_ = true;
}

void MerchantListener::OnNormalMerchantItemsReceived(const uint32_t* item_ids, uint32_t count) {
    // Clear the previous list on a fresh stream (throttled like legacy).
    if (reset_merchant_window_item_.hasElapsed(1000)) {
        merchant_window_items_.clear();
        reset_merchant_window_item_.reset();
    }

    for (uint32_t i = 0; i < count; ++i) {
        merchant_window_items_.push_back(item_ids[i]);
    }
}

void MerchantListener::OnItemStreamEnd(uint32_t unk1) {
    if (unk1 != 12) {  // 12 means we're in the "buy" tab
        return;
    }
    // Deviation from legacy: the merchant item array accessor lives on the
    // shared context (GW::Context::GetMerchantItemsArray) in this tree, not on
    // a GW::Merchant helper.
    GW::Context::MerchItemArray* items = GW::Context::GetMerchantItemsArray();
    merch_items_.clear();
    if (items) {
        for (const auto item_id : *items) {
            merch_items_.push_back(item_id);
        }
    }
}

void MerchantListener::Install() {
    GW::StoC::RegisterPacketCallback<StoCPacket::QuotedItemPrice>(
        &quoted_item_price_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::QuotedItemPrice* pak) {
            OnPriceReceived(pak->itemid, pak->price);
        });

    GW::StoC::RegisterPacketCallback<StoCPacket::TransactionDone>(
        &transaction_done_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::TransactionDone*) {
            OnTransactionComplete();
        });

    GW::StoC::RegisterPacketCallback<StoCPacket::ItemStreamEnd>(
        &item_stream_end_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::ItemStreamEnd* pak) {
            OnItemStreamEnd(pak->unk1);
        });

    GW::StoC::RegisterPacketCallback<StoCPacket::WindowItems>(
        &window_items_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::WindowItems* pak) {
            OnNormalMerchantItemsReceived(pak->item_ids, pak->count);
        });

    GW::StoC::RegisterPacketCallback<StoCPacket::WindowItemsEnd>(
        &window_items_end_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::WindowItemsEnd*) {
            // Stream-end hook retained for parity; no state change.
        });

    reset_merchant_window_item_.start();
}

void MerchantListener::Uninstall() {
    GW::StoC::RemoveCallback<StoCPacket::QuotedItemPrice>(&quoted_item_price_entry_);
    GW::StoC::RemoveCallback<StoCPacket::TransactionDone>(&transaction_done_entry_);
    GW::StoC::RemoveCallback<StoCPacket::ItemStreamEnd>(&item_stream_end_entry_);
    GW::StoC::RemoveCallback<StoCPacket::WindowItems>(&window_items_entry_);
    GW::StoC::RemoveCallback<StoCPacket::WindowItemsEnd>(&window_items_end_entry_);
}

/* ---------------- Registry + lifecycle ---------------- */

MerchantListener& Merchant() {
    static MerchantListener instance;
    return instance;
}

namespace {

// Every toggleable listener, in registration order.
std::vector<Listener*>& Registry() {
    static std::vector<Listener*> listeners = {
        &Merchant(),
        &AgentEvents(),
        &SkillListFilter(),
        &SignetOfCaptureLimit(),
        &FactionWarning(),
        &CinematicSkip(),
        &AutoReturnOnDefeat(),
        &DisableGoldConfirmation(),
        &RemoveCastBarMinimum(),
        &AutoCancelUnyieldingAura(),
        &AutoOpenLockedChest(),
        &FactionDonateSkipName(),
        &KeepCurrentQuest(),
    };
    return listeners;
}

Listener* Find(const std::string& name) {
    for (Listener* listener : Registry()) {
        if (name == listener->Name()) {
            return listener;
        }
    }
    return nullptr;
}

}  // namespace

bool Initialize() {
    for (Listener* listener : Registry()) {
        if (listener->EnabledByDefault()) {
            listener->Enable();
        }
    }
    return true;
}

void Shutdown() {
    for (Listener* listener : Registry()) {
        listener->Disable();
    }
}

void Update(float delta_ms) {
    for (Listener* listener : Registry()) {
        if (listener->IsEnabled()) {
            listener->Update(delta_ms);
        }
    }
}

std::vector<std::string> GetListenerNames() {
    std::vector<std::string> names;
    names.reserve(Registry().size());
    for (Listener* listener : Registry()) {
        names.emplace_back(listener->Name());
    }
    return names;
}

bool Enable(const std::string& name) {
    Listener* listener = Find(name);
    if (!listener) {
        return false;
    }
    listener->Enable();
    return true;
}

bool Disable(const std::string& name) {
    Listener* listener = Find(name);
    if (!listener) {
        return false;
    }
    listener->Disable();
    return true;
}

bool Toggle(const std::string& name) {
    Listener* listener = Find(name);
    if (!listener) {
        return false;
    }
    listener->Toggle();
    return true;
}

bool SetEnabled(const std::string& name, bool enabled) {
    Listener* listener = Find(name);
    if (!listener) {
        return false;
    }
    listener->SetEnabled(enabled);
    return true;
}

bool IsEnabled(const std::string& name) {
    Listener* listener = Find(name);
    return listener && listener->IsEnabled();
}

}  // namespace PY4GW::listeners

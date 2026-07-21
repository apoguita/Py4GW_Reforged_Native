#include "base/error_handling.h"

#include "listeners/faction_donate_listener.h"

#include "GW/game_thread/game_thread.h"
#include "GW/player/player.h"
#include "GW/ui/ui.h"

namespace PY4GW::listeners {

namespace {

// Runs on the game thread. Mirrors the legacy frame walk: NPCInteract -> {0,0}
// content frame, child 2 is the "Sign" button, child {4,2} is the name input.
// The button is visible-but-disabled only when the player has enough faction to
// donate; that is the gate for prefilling.
void PrefillFactionDonationName() {
    using namespace GW::ui;
    Frame* npc = GetFrameByLabel(L"NPCInteract");
    if (!npc) {
        return;
    }
    Frame* frame = GetChildFrame(npc, {0, 0});
    if (!frame) {
        return;
    }
    Frame* sign_btn = GetChildFrame(frame, 2);
    if (!(sign_btn && sign_btn->IsVisible() && sign_btn->IsDisabled())) {
        return;  // Sign button hidden/enabled -> not the donation prompt, or not enough faction.
    }
    auto* name_input = static_cast<EditableTextFrame*>(GetChildFrame(frame, {4, 2}));
    if (!name_input) {
        return;
    }
    name_input->SetValue(GW::player::GetPlayerName());
    SetFrameVisible(name_input, false);
    SetFrameDisabled(sign_btn, false);
}

// Positive altitude: run after the game handler so the window frames exist
// (matches the legacy module's post-registration).
constexpr int kPostAltitude = 0x8000;

}  // namespace

void FactionDonateSkipNameListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &vendor_window_entry_,
        GW::ui::UIMessage::kVendorWindow,
        [](PY4GW::HookStatus*, GW::ui::UIMessage, void*, void*) {
            PrefillFactionDonationName();
        },
        kPostAltitude);

    GW::ui::RegisterUIMessageCallback(
        &vendor_trans_complete_entry_,
        GW::ui::UIMessage::kVendorTransComplete,
        [this](PY4GW::HookStatus*, GW::ui::UIMessage, void*, void*) {
            deferred_timer_.reset();
            deferred_timer_.start();
            deferred_pending_ = true;
        },
        kPostAltitude);
}

void FactionDonateSkipNameListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&vendor_window_entry_, GW::ui::UIMessage::kVendorWindow);
    GW::ui::RemoveUIMessageCallback(&vendor_trans_complete_entry_, GW::ui::UIMessage::kVendorTransComplete);
    deferred_pending_ = false;
}

void FactionDonateSkipNameListener::Update(float) {
    if (deferred_pending_ && deferred_timer_.hasElapsed(500)) {
        deferred_pending_ = false;
        // Frame work must run on the game thread.
        GW::game_thread::Enqueue([] { PrefillFactionDonationName(); });
    }
}

FactionDonateSkipNameListener& FactionDonateSkipName() {
    static FactionDonateSkipNameListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

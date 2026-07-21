#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "base/timer.h"
#include "listeners/listeners.h"

// "Skip character name input when donating faction", migrated from the legacy
// GWToolbox GameSettings::SkipCharacterNameEntryForFactionDonation. When the
// faction-donation NPC window is shown (and the player has enough faction to
// donate), the character-name field is pre-filled with the player's name and
// hidden, and the "Sign" button is enabled - so donation is one click.
//
// Two triggers, as in the legacy module:
//   - kVendorWindow:        prefill immediately (already on the game thread).
//   - kVendorTransComplete: after a donation several UI messages arrive in
//     varying order, so defer ~500ms (Update tick) then prefill on the game
//     thread.
// Opt-in: disabled by default.

namespace PY4GW::listeners {

class FactionDonateSkipNameListener : public Listener {
public:
    const char* Name() const override { return "faction_donate_skip_name"; }
    bool EnabledByDefault() const override { return false; }

    void Update(float delta_ms) override;

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry vendor_window_entry_;
    PY4GW::HookEntry vendor_trans_complete_entry_;
    PY4GW::Timer deferred_timer_;
    bool deferred_pending_ = false;
};

FactionDonateSkipNameListener& FactionDonateSkipName();

}  // namespace PY4GW::listeners

#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "base/timer.h"
#include "listeners/listeners.h"

// "Automatically return to outpost on defeat", migrated from the legacy
// GWToolbox GameSettings kPartyDefeated handler. Listens for the party-defeated
// UI message and, if the local player is the party leader, returns the party to
// the outpost.
//
// Note: the reforged GW::party::return_to_outpost() clicks the button on the
// "DlgRedirect" defeat dialog, which only exists once the game has shown that
// dialog. So we register at POST altitude (after the game handler) and try
// inline; if the dialog is not up yet we retry from the Update() tick (on the
// game thread) for a short window. Opt-in: disabled by default.

namespace PY4GW::listeners {

class AutoReturnOnDefeatListener : public Listener {
public:
    const char* Name() const override { return "auto_return_on_defeat"; }
    bool EnabledByDefault() const override { return false; }

    void Update(float delta_ms) override;

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry party_defeated_entry_;
    PY4GW::Timer retry_timer_;
    bool pending_ = false;
    int attempts_ = 0;
};

AutoReturnOnDefeatListener& AutoReturnOnDefeat();

}  // namespace PY4GW::listeners

#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "base/timer.h"
#include "listeners/listeners.h"

// "Automatically return to outpost on defeat", migrated from the legacy
// GWToolbox GameSettings kPartyDefeated handler. Polls the party context's
// defeated flag and, if the local player is the party leader in an explorable
// instance, returns the party to the outpost. The party-defeated UI message is
// retained as an early wake-up, but is not the authoritative trigger.
//
// Note: the reforged GW::party::return_to_outpost() clicks the button on the
// "DlgRedirect" defeat dialog, which only exists once the game has shown that
// dialog. So we register at POST altitude (after the game handler) and retry
// from the Update() tick (on the game thread) until the map leaves the
// explorable state. Opt-in: disabled by default.

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
};

AutoReturnOnDefeatListener& AutoReturnOnDefeat();

}  // namespace PY4GW::listeners

#pragma once

#include "base/error_handling.h"

#include "listeners/listeners.h"

// "Show warning when earned faction reaches ___%", migrated from the legacy
// GWToolbox GameSettings::FactionEarnedCheckAndWarn. Unlike the other listeners
// this is a poll, not a callback: the world-context faction values populate a
// little after an outpost is entered, so the legacy module checked them every
// Update behind a once-per-outpost latch. Ported faithfully via the listener
// Update() tick.
//
// When enabled and standing in a Luxon/Kurzick challenge or elite-mission
// outpost, on first valid read it logs a warning if the earned faction has
// reached the configured percentage of the cap (or if the opposing faction is
// notably ahead). Opt-in: disabled by default.

namespace PY4GW::listeners {

class FactionWarningListener : public Listener {
public:
    const char* Name() const override { return "faction_warning"; }
    bool EnabledByDefault() const override { return false; }

    // Percentage of the faction cap (0-100) at which to warn.
    void SetWarnPercent(int percent);
    int WarnPercent() const { return warn_percent_; }

    void Update(float delta_ms) override;

protected:
    // Poll-only: no packet/UI hooks to install. Reset the latch so a freshly
    // enabled listener re-checks the current outpost.
    void Install() override { faction_checked_ = false; }
    void Uninstall() override {}

private:
    int warn_percent_ = 75;  // Legacy GWToolbox default.
    bool faction_checked_ = false;
};

// Process-wide accessor (config + toggle), mirroring Merchant()/AgentEvents().
FactionWarningListener& FactionWarning();

}  // namespace PY4GW::listeners

#pragma once

#include "base/error_handling.h"

#include "listeners/listeners.h"

// Two MemoryPatcher-based toggles migrated from the legacy GWToolbox
// GameSettings module. Each resolves a scan address (offsets/listeners.json)
// once and NOPs two bytes while enabled; Disable restores the original bytes.
// Both are opt-in (disabled by default).
//
//   - DisableGoldConfirmationListener: removes the "sell gold/green item?"
//     confirmation prompt (settings.disable_gold_selling_confirmation).
//   - RemoveCastBarMinimumListener: removes the 1.5s minimum warmup for the
//     cast bar to appear, so short casts still show it
//     (settings.remove_min_skill_warmup_duration).

namespace PY4GW::listeners {

class DisableGoldConfirmationListener : public Listener {
public:
    const char* Name() const override { return "disable_gold_confirmation"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;
};

class RemoveCastBarMinimumListener : public Listener {
public:
    const char* Name() const override { return "remove_cast_bar_minimum"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;
};

DisableGoldConfirmationListener& DisableGoldConfirmation();
RemoveCastBarMinimumListener& RemoveCastBarMinimum();

}  // namespace PY4GW::listeners

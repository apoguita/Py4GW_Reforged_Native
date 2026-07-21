#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

// "Automatically skip cinematics", migrated from the legacy GWToolbox
// GameSettings::OnCinematic. A StoC CinematicPlay packet listener: when a
// cinematic starts playing and this listener is enabled, it is skipped without
// user input. Opt-in: disabled by default.

namespace PY4GW::listeners {

class CinematicSkipListener : public Listener {
public:
    const char* Name() const override { return "cinematic_skip"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry cinematic_play_entry_;
};

CinematicSkipListener& CinematicSkip();

}  // namespace PY4GW::listeners

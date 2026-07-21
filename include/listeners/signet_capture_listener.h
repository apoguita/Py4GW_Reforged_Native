#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

// "Limit signet of capture to 10 in skills window", migrated from the legacy
// GWToolbox GameSettings::OnUpdateSkillCount. Two StoC packets duplicate a
// skill in the skills window to represent multiple owned copies (the after- and
// pre-map-load variants). When the skill is Signet of Capture and this listener
// is enabled, the reported count is clamped to 10 so the window is not flooded.
//
// A plain StoC packet listener (like MerchantListener). Opt-in: disabled by
// default so the real count is shown until a consumer toggles it on.

namespace PY4GW::listeners {

class SignetOfCaptureLimitListener : public Listener {
public:
    const char* Name() const override { return "signet_of_capture_limit"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry after_map_load_entry_;
    PY4GW::HookEntry pre_map_load_entry_;
};

// Process-wide accessor (toggle), mirroring Merchant()/AgentEvents().
SignetOfCaptureLimitListener& SignetOfCaptureLimit();

}  // namespace PY4GW::listeners

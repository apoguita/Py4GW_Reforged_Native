#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

// "Automatically cancel Unyielding Aura when re-casting", migrated from the
// legacy GWToolbox GameSettings::OnAgentStartCast. Listens for the "agent
// started casting" UI message; when the local cast is Unyielding Aura, drops the
// existing UA buff first so the recast is clean. Opt-in: disabled by default.

namespace PY4GW::listeners {

class AutoCancelUnyieldingAuraListener : public Listener {
public:
    const char* Name() const override { return "auto_cancel_ua"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry cast_entry_;
};

AutoCancelUnyieldingAuraListener& AutoCancelUnyieldingAura();

}  // namespace PY4GW::listeners

#pragma once

#include "base/error_handling.h"

#include "listeners/listeners.h"

#include <cstdint>

// Skill-list window filtering, migrated from the legacy GWToolbox
// GameSettings::OnSkillList_UICallback. A single MinHook function hook on the
// skill-list UI interaction callback (resolved from offsets/listeners.json)
// intercepts the frame message that adds a skill to the tome / skill-trainer /
// skill-capture windows and drops it when a filter says so:
//
//   - hide_known_skills:          skip skills the character already owns
//   - hide_nonelites_on_capture:  in the DlgSkillCapture window, skip non-elites
//
// Both features share the one hook (as in the legacy module), so they live on a
// single listener with two independent config flags. Enable() installs the
// hook, Disable() removes it. Opt-in: disabled by default so game behavior is
// unchanged until a consumer toggles it on.

namespace PY4GW::listeners {

class SkillListFilterListener : public Listener {
public:
    const char* Name() const override { return "skill_list_filter"; }
    bool EnabledByDefault() const override { return false; }

    void SetHideKnownSkills(bool value) { hide_known_skills_ = value; }
    bool HideKnownSkills() const { return hide_known_skills_; }

    void SetHideNonElitesOnCapture(bool value) { hide_nonelites_on_capture_ = value; }
    bool HideNonElitesOnCapture() const { return hide_nonelites_on_capture_; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    bool hide_known_skills_ = false;
    bool hide_nonelites_on_capture_ = false;
};

// Process-wide accessor (config + toggle), mirroring Merchant()/AgentEvents().
SkillListFilterListener& SkillListFilter();

}  // namespace PY4GW::listeners

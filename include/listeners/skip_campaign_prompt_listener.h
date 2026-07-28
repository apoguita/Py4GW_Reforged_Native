#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

// "Skip the another-campaign confirmation prompt", migrated from the legacy
// GWToolbox GameSettings::skip_characters_from_another_campaign_prompt.
//
// Entering a mission with a party member whose character is from a different
// campaign makes the party frame raise a yes/no confirmation. When enabled,
// this listener answers "Yes" for you: on kPartyShowConfirmDialog it matches
// the prompt's encoded string against the another-campaign prompt and clicks
// the confirm button in the party frame.
//
// Registered at positive altitude so it runs after the game handler, i.e. once
// the prompt's frames actually exist. Opt-in: disabled by default.

namespace PY4GW::listeners {

class SkipCampaignPromptListener : public Listener {
public:
    const char* Name() const override { return "skip_campaign_prompt"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    PY4GW::HookEntry confirm_dialog_entry_;
};

SkipCampaignPromptListener& SkipCampaignPrompt();

}  // namespace PY4GW::listeners

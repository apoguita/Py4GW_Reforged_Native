#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

#include "GW/common/constants/quest_ids.h"

// "Keep current quest when accepting a new one", migrated from the legacy
// GWToolbox QuestModule. The game auto-switches the active quest whenever a new
// quest is added; this remembers the quest the player last chose manually and
// restores it when an unattended quest change happens. Opt-in: disabled by
// default.

namespace PY4GW::listeners {

class KeepCurrentQuestListener : public Listener {
public:
    const char* Name() const override { return "keep_current_quest"; }
    bool EnabledByDefault() const override { return false; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    void OnQuestChanged(GW::Constants::QuestID new_quest_id);

    // The quest the player last selected manually (via kSendSetActiveQuest).
    GW::Constants::QuestID player_chosen_quest_id_ = GW::Constants::QuestID::None;

    PY4GW::HookEntry send_set_active_entry_;
    PY4GW::HookEntry quest_added_entry_;
    PY4GW::HookEntry quest_details_entry_;
    PY4GW::HookEntry client_active_entry_;
};

KeepCurrentQuestListener& KeepCurrentQuest();

}  // namespace PY4GW::listeners

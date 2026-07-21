#include "base/error_handling.h"

#include "listeners/keep_quest_listener.h"

#include "GW/context/quest.h"
#include "GW/quest/quest.h"
#include "GW/ui/ui.h"

#include <cstdint>

namespace PY4GW::listeners {

namespace {
// Positive altitude: run after the game handler so the new quest is already
// applied before we decide whether to override it (matches the legacy module's
// post-registration).
constexpr int kPostAltitude = 0x8000;
}  // namespace

void KeepCurrentQuestListener::OnQuestChanged(GW::Constants::QuestID new_quest_id) {
    if (player_chosen_quest_id_ == GW::Constants::QuestID::None) {
        return;
    }
    if (new_quest_id == player_chosen_quest_id_) {
        return;  // Already on the chosen quest (also breaks the restore re-entry).
    }
    if (!GW::quest::GetQuest(player_chosen_quest_id_)) {
        return;  // The player's chosen quest is no longer in the log.
    }
    GW::quest::SetActiveQuestId(player_chosen_quest_id_);
}

void KeepCurrentQuestListener::Install() {
    // Capture the player's manual quest selection (outgoing message; wparam is
    // the quest id passed by value).
    GW::ui::RegisterUIMessageCallback(
        &send_set_active_entry_,
        GW::ui::UIMessage::kSendSetActiveQuest,
        [this](PY4GW::HookStatus*, GW::ui::UIMessage, void* wparam, void*) {
            player_chosen_quest_id_ =
                static_cast<GW::Constants::QuestID>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam)));
        });

    // Restore on unattended quest changes (packet is a pointer to the quest id).
    const auto restore = [this](PY4GW::HookStatus*, GW::ui::UIMessage, void* packet, void*) {
        if (packet) {
            OnQuestChanged(*static_cast<GW::Constants::QuestID*>(packet));
        }
    };
    GW::ui::RegisterUIMessageCallback(&quest_added_entry_, GW::ui::UIMessage::kQuestAdded, restore, kPostAltitude);
    GW::ui::RegisterUIMessageCallback(&quest_details_entry_, GW::ui::UIMessage::kQuestDetailsChanged, restore, kPostAltitude);
    GW::ui::RegisterUIMessageCallback(&client_active_entry_, GW::ui::UIMessage::kClientActiveQuestChanged, restore, kPostAltitude);
}

void KeepCurrentQuestListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&send_set_active_entry_, GW::ui::UIMessage::kSendSetActiveQuest);
    GW::ui::RemoveUIMessageCallback(&quest_added_entry_, GW::ui::UIMessage::kQuestAdded);
    GW::ui::RemoveUIMessageCallback(&quest_details_entry_, GW::ui::UIMessage::kQuestDetailsChanged);
    GW::ui::RemoveUIMessageCallback(&client_active_entry_, GW::ui::UIMessage::kClientActiveQuestChanged);
    player_chosen_quest_id_ = GW::Constants::QuestID::None;
}

KeepCurrentQuestListener& KeepCurrentQuest() {
    static KeepCurrentQuestListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

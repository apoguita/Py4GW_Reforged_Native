#include "base/error_handling.h"

#include "listeners/auto_return_listener.h"

#include "GW/game_thread/game_thread.h"
#include "GW/party/party.h"
#include "GW/ui/ui.h"

namespace PY4GW::listeners {

namespace {
// Positive altitude: run after the game handler so the "DlgRedirect" defeat
// dialog exists when we try to click its return button.
constexpr int kPostAltitude = 0x8000;

// Retry the return for up to ~5s (the defeat dialog can appear a beat late).
constexpr int kMaxAttempts = 10;
constexpr double kRetrySpacingMs = 500.0;
}  // namespace

void AutoReturnOnDefeatListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &party_defeated_entry_,
        GW::ui::UIMessage::kPartyDefeated,
        [this](PY4GW::HookStatus*, GW::ui::UIMessage, void*, void*) {
            if (!GW::party::get_is_leader()) {
                return;  // Only the leader can return the party.
            }
            // UI callbacks run on the game thread; try the click inline first.
            if (GW::party::return_to_outpost()) {
                return;
            }
            // Dialog not up yet - retry from Update().
            pending_ = true;
            attempts_ = 0;
            retry_timer_.reset();
            retry_timer_.start();
        },
        kPostAltitude);
}

void AutoReturnOnDefeatListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&party_defeated_entry_, GW::ui::UIMessage::kPartyDefeated);
    pending_ = false;
}

void AutoReturnOnDefeatListener::Update(float) {
    if (!pending_ || !retry_timer_.hasElapsed(kRetrySpacingMs)) {
        return;
    }
    retry_timer_.reset();
    retry_timer_.start();
    if (++attempts_ >= kMaxAttempts) {
        pending_ = false;  // Give up.
    }
    // Frame click must run on the game thread. Clear pending on success.
    GW::game_thread::Enqueue([this] {
        if (GW::party::return_to_outpost()) {
            pending_ = false;
        }
    });
}

AutoReturnOnDefeatListener& AutoReturnOnDefeat() {
    static AutoReturnOnDefeatListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

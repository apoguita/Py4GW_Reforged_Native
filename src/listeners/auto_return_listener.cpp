#include "base/error_handling.h"

#include "listeners/auto_return_listener.h"

#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/party/party.h"
#include "GW/ui/ui.h"

namespace PY4GW::listeners {

namespace {
// Positive altitude: run after the game handler so the "DlgRedirect" defeat
// dialog exists when we try to click its return button.
constexpr int kPostAltitude = 0x8000;

// The context flag is the authoritative defeat state. The UI message is only
// an early wake-up because manual /resign and some map modes can update the
// party context without delivering kPartyDefeated to this callback.
//
// The map/leader gates and defeated flag are continuously evaluated. The
// dialog frame is not touched until all of those gates are true.
constexpr double kRetrySpacingMs = 500.0;

bool IsEligibleDefeatContext() {
    return GW::map::GetIsMapLoaded()
        && GW::map::GetInstanceType() == GW::Constants::InstanceType::Explorable
        && GW::party::get_is_leader();
}
}  // namespace

void AutoReturnOnDefeatListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &party_defeated_entry_,
        GW::ui::UIMessage::kPartyDefeated,
        [this](PY4GW::HookStatus*, GW::ui::UIMessage, void*, void*) {
            if (!GW::party::get_is_party_defeated() || !IsEligibleDefeatContext()) {
                return;
            }
            // The callback is only a wake-up. Update() also polls the context
            // flag, so this path is not required for manual /resign recovery.
            pending_ = true;
            retry_timer_.reset();
            retry_timer_.start();
        },
        kPostAltitude);
}

void AutoReturnOnDefeatListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&party_defeated_entry_, GW::ui::UIMessage::kPartyDefeated);
    pending_ = false;
    retry_timer_.stop();
}

void AutoReturnOnDefeatListener::Update(float) {
    // Map/leader gates are intentionally checked continuously. This keeps a
    // stale defeat state from arming recovery after a map transition.
    if (!IsEligibleDefeatContext() || !GW::party::get_is_party_defeated()) {
        pending_ = false;
        retry_timer_.stop();
        return;
    }

    if (!pending_) {
        // Both the map gates and the party-defeated state are true. Only now
        // arm the throttled UI recovery path.
        pending_ = true;
        retry_timer_.reset();
        retry_timer_.start();
        return;
    }

    // Only an armed defeat recovery reaches this path. Check the dialog at
    // most once per 500 ms, and keep retrying until the map actually changes.
    if (!retry_timer_.hasElapsed(kRetrySpacingMs)) {
        return;
    }
    retry_timer_.reset();
    retry_timer_.start();

    GW::game_thread::Enqueue([this] {
        if (!IsEligibleDefeatContext() || !GW::party::get_is_party_defeated()) {
            pending_ = false;
            retry_timer_.stop();
            return;
        }
        GW::party::return_to_outpost();
    });
}

AutoReturnOnDefeatListener& AutoReturnOnDefeat() {
    static AutoReturnOnDefeatListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

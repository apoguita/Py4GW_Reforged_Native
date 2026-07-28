#include "base/error_handling.h"

#include "listeners/skip_campaign_prompt_listener.h"

#include "GW/context/ui.h"
#include "GW/ui/ui.h"

#include <cwchar>
#include <vector>

namespace PY4GW::listeners {

namespace {

// Encoded prompt string for "this character is from another campaign", same
// literal as the legacy module.
constexpr wchar_t kAnotherCampaignPrompt[] = L"\x8101\x05d2";

// Positive altitude: run after the game handler so the prompt frames exist
// (matches the legacy module's post-registration).
constexpr int kPostAltitude = 0x8000;

// Frame hash of the window hosting the confirmation prompt, and the child-offset
// path from it down to the prompt's OK button.
constexpr uint32_t kPromptFrameHash = 0xD070ED7E;  // 3497061758
const std::vector<uint32_t> kOkButtonPath = {100, 6};

// Runs on the game thread - UI message callbacks are dispatched there, so the
// frame walk and click are safe to do inline (as in the legacy module).
void ClickConfirmButton() {
    using namespace GW::ui;
    const uint32_t ok_button_id = GetChildFrameID(kPromptFrameHash, kOkButtonPath);
    if (!ok_button_id) {
        return;
    }
    ButtonClick(GetFrameById(ok_button_id));
}

}  // namespace

void SkipCampaignPromptListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &confirm_dialog_entry_,
        GW::ui::UIMessage::kPartyShowConfirmDialog,
        [](PY4GW::HookStatus*, GW::ui::UIMessage, void* wparam, void*) {
            const auto* info = static_cast<GW::Context::PartyShowConfirmDialogInfo*>(wparam);
            if (!info || !info->prompt_enc_str) {
                return;
            }
            if (wcscmp(info->prompt_enc_str, kAnotherCampaignPrompt) != 0) {
                return;  // A different confirm prompt - leave it to the player.
            }
            ClickConfirmButton();
        },
        kPostAltitude);
}

void SkipCampaignPromptListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&confirm_dialog_entry_, GW::ui::UIMessage::kPartyShowConfirmDialog);
}

SkipCampaignPromptListener& SkipCampaignPrompt() {
    static SkipCampaignPromptListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

#include "base/error_handling.h"

#include "listeners/auto_open_chest_listener.h"

#include "GW/agent/agent.h"
#include "GW/context/ui.h"
#include "GW/ui/ui.h"

#include <cwchar>

namespace PY4GW::listeners {

namespace {
// Encoded dialog-button messages (same literals as the legacy module).
constexpr wchar_t kUseKeyMessage[] = L"\x8101\x7F88\x10A\x8101\x13BE\x1";
constexpr wchar_t kUseLockpickMessage[] = L"\x8101\x7f88\x010a\x8101\x730e\x1";
// Positive altitude: run after the game handler, matching the legacy module's
// post-registration of the dialog-button handler.
constexpr int kPostAltitude = 0x8000;
}  // namespace

void AutoOpenLockedChestListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &dialog_button_entry_,
        GW::ui::UIMessage::kDialogButton,
        [](PY4GW::HookStatus*, GW::ui::UIMessage, void* wparam, void*) {
            const auto* info = static_cast<GW::Context::DialogButtonInfo*>(wparam);
            if (!info || !info->message) {
                return;
            }
            const AutoOpenLockedChestListener& self = AutoOpenLockedChest();
            if (self.UseKey() && wcscmp(info->message, kUseKeyMessage) == 0) {
                GW::agent::SendDialog(info->dialog_id);
            }
            else if (self.UseLockpick() && wcscmp(info->message, kUseLockpickMessage) == 0) {
                GW::agent::SendDialog(info->dialog_id);
            }
        },
        kPostAltitude);
}

void AutoOpenLockedChestListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&dialog_button_entry_, GW::ui::UIMessage::kDialogButton);
}

AutoOpenLockedChestListener& AutoOpenLockedChest() {
    static AutoOpenLockedChestListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

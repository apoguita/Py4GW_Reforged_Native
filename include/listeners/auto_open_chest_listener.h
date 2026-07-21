#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"

// "Automatically use key / lockpick when interacting with a locked chest",
// migrated from the legacy GWToolbox GameSettings OnDialogButton. Both features
// watch the same dialog-button UI message and auto-send the matching dialog
// response, so they live on one listener with two independent config flags:
//
//   - use_key:      auto-send the "use key" dialog option
//   - use_lockpick: auto-send the "use lockpick" dialog option
//
// Opt-in: disabled by default.

namespace PY4GW::listeners {

class AutoOpenLockedChestListener : public Listener {
public:
    const char* Name() const override { return "auto_open_locked_chest"; }
    bool EnabledByDefault() const override { return false; }

    void SetUseKey(bool value) { use_key_ = value; }
    bool UseKey() const { return use_key_; }

    void SetUseLockpick(bool value) { use_lockpick_ = value; }
    bool UseLockpick() const { return use_lockpick_; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    bool use_key_ = false;
    bool use_lockpick_ = false;
    PY4GW::HookEntry dialog_button_entry_;
};

AutoOpenLockedChestListener& AutoOpenLockedChest();

}  // namespace PY4GW::listeners

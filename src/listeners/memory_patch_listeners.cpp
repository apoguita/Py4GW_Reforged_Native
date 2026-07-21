#include "base/error_handling.h"

#include "listeners/memory_patch_listeners.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_patcher.h"
#include "base/patterns.h"

#include <cstdint>
#include <string>

namespace PY4GW::listeners {

namespace {

// Two NOPs overwrite the two-byte conditional/instruction the game uses.
constexpr uint8_t kNopPatch[] = {0x90, 0x90};

PY4GW::MemoryPatcher g_gold_confirm_patch = {};
PY4GW::MemoryPatcher g_cast_bar_minimum_patch = {};

bool g_gold_confirm_resolved = false;
bool g_cast_bar_minimum_resolved = false;

// Resolve the scan address once and arm the patcher. Returns whether the patch
// is usable; a failed scan logs and leaves the listener inert (no toggle).
bool EnsurePatch(PY4GW::MemoryPatcher& patcher, bool& resolved, const char* offset_key, const char* label) {
    if (resolved) {
        return patcher.IsValid();
    }
    resolved = true;
    CrashContextScope context("runtime", "listeners", label);
    uintptr_t address = 0;
    if (!PY4GW::Patterns::Resolve(offset_key, &address) || !address) {
        Logger::Instance().LogWarning(std::string("[listeners] ") + label + ": patch address not resolved; toggle inert");
        return false;
    }
    patcher.SetPatch(address, kNopPatch, sizeof(kNopPatch));
    return patcher.IsValid();
}

}  // namespace

void DisableGoldConfirmationListener::Install() {
    if (EnsurePatch(g_gold_confirm_patch, g_gold_confirm_resolved, "listeners.gold_confirm_patch_addr", "disable_gold_confirmation")) {
        g_gold_confirm_patch.TogglePatch(true);
    }
}

void DisableGoldConfirmationListener::Uninstall() {
    if (g_gold_confirm_patch.IsValid()) {
        g_gold_confirm_patch.TogglePatch(false);
    }
}

void RemoveCastBarMinimumListener::Install() {
    if (EnsurePatch(g_cast_bar_minimum_patch, g_cast_bar_minimum_resolved, "listeners.cast_bar_minimum_patch_addr", "remove_cast_bar_minimum")) {
        g_cast_bar_minimum_patch.TogglePatch(true);
    }
}

void RemoveCastBarMinimumListener::Uninstall() {
    if (g_cast_bar_minimum_patch.IsValid()) {
        g_cast_bar_minimum_patch.TogglePatch(false);
    }
}

DisableGoldConfirmationListener& DisableGoldConfirmation() {
    static DisableGoldConfirmationListener instance;
    return instance;
}

RemoveCastBarMinimumListener& RemoveCastBarMinimum() {
    static RemoveCastBarMinimumListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

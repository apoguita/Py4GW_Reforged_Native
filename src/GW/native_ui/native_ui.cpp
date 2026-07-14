#include "base/error_handling.h"

#include "GW/native_ui/native_ui.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/scanner.h"

#include <intrin.h>

#include <MinHook.h>

#include <mutex>
#include <vector>

// Native UI runtime: hook install/teardown and subsystem orchestration. The
// scanner resolver bodies live in native_ui_patterns.cpp; this file owns the
// hooks and lifecycle.
namespace GW::native_ui {

namespace title_hook {

namespace {

using UiSetFrameText_pt = void(__cdecl*)(uint32_t frame, uint32_t text_resource_or_string);
using UiSetFrameEncodedTextResource_pt = void(__cdecl*)(uint32_t frame, uint32_t resource_ptr);
using UiCreateEncodedText_pt = uint32_t(__cdecl*)(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved);

struct PendingTitleOverride {
    uint32_t parent_frame_id = 0;
    uint32_t child_index = 0;
    std::wstring title;
};

std::mutex g_mutex;
std::vector<PendingTitleOverride> g_pending_title_overrides;
std::wstring g_next_created_window_title;
std::wstring g_last_applied_title;
uint32_t g_last_applied_frame_id = 0;
bool g_hook_installed = false;
bool g_expect_next_title_set = false;
bool g_expect_next_title_resource_set = false;

// Derived from the batch-1 g_create_encoded_text_func + the DevText call-site
// walk; the trampolines (_Ret) are the MinHook originals.
UiCreateEncodedText_pt UiCreateEncodedText_Func = nullptr;
UiCreateEncodedText_pt UiCreateEncodedText_Ret = nullptr;
UiSetFrameText_pt UiSetFrameText_Func = nullptr;
UiSetFrameText_pt UiSetFrameText_Ret = nullptr;
UiSetFrameEncodedTextResource_pt UiSetFrameEncodedTextResource_Func = nullptr;
UiSetFrameEncodedTextResource_pt UiSetFrameEncodedTextResource_Ret = nullptr;

uintptr_t g_devtext_title_create_return = 0;
uintptr_t g_devtext_title_set_return = 0;
uintptr_t g_devtext_title_resource_set_return = 0;

// Scan input for the DevText title path. Kept as a code literal (not
// offsets/native_ui.json): it drives a loop over string-use indices plus a
// forward CALL walk, i.e. the call site owns the control flow (scan-translation
// rule). Wide string, per FindNthUseOfString(const wchar_t*).
uintptr_t ResolveDevTextStringUse() {
    for (uint32_t xref_index = 0; xref_index < 8; ++xref_index) {
        uintptr_t use_addr = 0;
        try {
            use_addr = PY4GW::Scanner::FindNthUseOfString(L"DlgDevText", xref_index, 0, PY4GW::ScannerSection::Text);
        } catch (...) {
            use_addr = 0;
        }
        if (use_addr) {
            return use_addr;
        }
    }
    return 0;
}

uintptr_t ResolveRelativeCallTarget(uintptr_t call_addr) {
    const auto opcode = *reinterpret_cast<const uint8_t*>(call_addr);
    if (opcode != 0xE8) {
        return 0;
    }
    const int32_t rel = *reinterpret_cast<const int32_t*>(call_addr + 1);
    return call_addr + 5 + rel;
}

bool ResolveSupportFunctions() {
    // Step 1: Ui_CreateEncodedText via the shared batch-1 resolver.
    if (!UiCreateEncodedText_Func) {
        if (!ResolveCreateEncodedText()) {
            return false;
        }
        UiCreateEncodedText_Func = reinterpret_cast<UiCreateEncodedText_pt>(g_create_encoded_text_func);
    }

    // Step 2: derive Ui_SetFrameText from DevText's call site. The direct byte
    // pattern matches 16 functions; instead, from the "DlgDevText" string use,
    // the FIRST CALL after Ui_CreateEncodedText is Ui_SetFrameText.
    if (!UiSetFrameText_Func || !g_devtext_title_create_return || !g_devtext_title_set_return) {
        const uintptr_t use_addr = ResolveDevTextStringUse();
        if (!use_addr) {
            return false;
        }

        bool found_create = false;
        for (uintptr_t addr = use_addr; addr < use_addr + 0x60; ++addr) {
            const uintptr_t target = ResolveRelativeCallTarget(addr);
            if (!target) {
                continue;
            }
            if (!found_create && target == reinterpret_cast<uintptr_t>(UiCreateEncodedText_Func)) {
                g_devtext_title_create_return = addr + 5;
                found_create = true;
                continue;
            }
            if (found_create && !UiSetFrameText_Func) {
                UiSetFrameText_Func = reinterpret_cast<UiSetFrameText_pt>(target);
                g_devtext_title_set_return = addr + 5;
                break;
            }
        }

        if (!UiSetFrameText_Func || !g_devtext_title_create_return || !g_devtext_title_set_return) {
            return false;
        }
    }

    // Step 3: Ui_SetFrameEncodedTextResource = Ui_SetFrameText + 0x70 (stable
    // across known builds). g_devtext_title_resource_set_return stays 0 - the
    // resource path is not called from DevText's OnCreate.
    if (!UiSetFrameEncodedTextResource_Func) {
        constexpr int ENCODED_TEXT_RESOURCE_OFFSET = 0x70;
        const uintptr_t candidate = reinterpret_cast<uintptr_t>(UiSetFrameText_Func) + ENCODED_TEXT_RESOURCE_OFFSET;
        UiSetFrameEncodedTextResource_Func = reinterpret_cast<UiSetFrameEncodedTextResource_pt>(candidate);
    }

    return UiSetFrameText_Func && UiSetFrameEncodedTextResource_Func && UiCreateEncodedText_Func &&
        g_devtext_title_create_return != 0 && g_devtext_title_set_return != 0;
}

int FindPendingOverrideIndex(uint32_t parent_frame_id, uint32_t child_index) {
    for (size_t i = 0; i < g_pending_title_overrides.size(); ++i) {
        const auto& pending = g_pending_title_overrides[i];
        if (pending.parent_frame_id == parent_frame_id && pending.child_index == child_index) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint32_t __cdecl OnUiCreateEncodedText(uint32_t style_id, uint32_t layout_profile, const wchar_t* wide_text, uint32_t reserved) {
    const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
    std::wstring replacement_title;

    if (return_address == g_devtext_title_create_return) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_pending_title_overrides.empty()) {
            replacement_title = g_pending_title_overrides.front().title;
            g_pending_title_overrides.erase(g_pending_title_overrides.begin());
            g_last_applied_title = replacement_title;
            g_expect_next_title_set = true;
            g_expect_next_title_resource_set = true;
        }
    }

    if (!replacement_title.empty() && UiCreateEncodedText_Ret) {
        return UiCreateEncodedText_Ret(style_id, layout_profile, replacement_title.c_str(), reserved);
    }
    if (UiCreateEncodedText_Ret) {
        return UiCreateEncodedText_Ret(style_id, layout_profile, wide_text, reserved);
    }
    return 0;
}

void __cdecl OnUiSetFrameText(uint32_t frame, uint32_t text_resource_or_string) {
    const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (return_address == g_devtext_title_set_return) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_expect_next_title_set) {
            g_last_applied_frame_id = frame;
            g_expect_next_title_set = false;
        }
    }

    if (UiSetFrameText_Ret) {
        UiSetFrameText_Ret(frame, text_resource_or_string);
    }
}

void __cdecl OnUiSetFrameEncodedTextResource(uint32_t frame, uint32_t resource_ptr) {
    const uintptr_t return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (return_address == g_devtext_title_resource_set_return) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_expect_next_title_resource_set) {
            g_last_applied_frame_id = frame;
            g_expect_next_title_resource_set = false;
            return;
        }
    }

    if (UiSetFrameEncodedTextResource_Ret) {
        UiSetFrameEncodedTextResource_Ret(frame, resource_ptr);
    }
}

bool EnsureInstalled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_hook_installed) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "install_title_hook");
    if (!ResolveSupportFunctions()) {
        return false;
    }

    if (PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiCreateEncodedText_Func),
            reinterpret_cast<void*>(OnUiCreateEncodedText),
            reinterpret_cast<void**>(&UiCreateEncodedText_Ret)) != MH_OK) {
        return false;
    }
    if (PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiSetFrameText_Func),
            reinterpret_cast<void*>(OnUiSetFrameText),
            reinterpret_cast<void**>(&UiSetFrameText_Ret)) != MH_OK) {
        return false;
    }
    if (PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&UiSetFrameEncodedTextResource_Func),
            reinterpret_cast<void*>(OnUiSetFrameEncodedTextResource),
            reinterpret_cast<void**>(&UiSetFrameEncodedTextResource_Ret)) != MH_OK) {
        return false;
    }

    PY4GW::HookBase::EnableHooks(UiCreateEncodedText_Func);
    PY4GW::HookBase::EnableHooks(UiSetFrameText_Func);
    PY4GW::HookBase::EnableHooks(UiSetFrameEncodedTextResource_Func);
    g_hook_installed = true;
    return true;
}

void RemoveHooks() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_hook_installed) {
        return;
    }
    if (UiCreateEncodedText_Func) PY4GW::HookBase::RemoveHook(UiCreateEncodedText_Func);
    if (UiSetFrameText_Func) PY4GW::HookBase::RemoveHook(UiSetFrameText_Func);
    if (UiSetFrameEncodedTextResource_Func) PY4GW::HookBase::RemoveHook(UiSetFrameEncodedTextResource_Func);
    g_hook_installed = false;
}

}  // namespace

const wchar_t* CreateEncodedTextLiteral(const wchar_t* text) {
    if (!text) {
        return nullptr;
    }
    if (!UiCreateEncodedText_Func) {
        if (!ResolveCreateEncodedText()) {
            return nullptr;
        }
        UiCreateEncodedText_Func = reinterpret_cast<UiCreateEncodedText_pt>(g_create_encoded_text_func);
    }
    // Prefer the trampoline (skips the title-override detour); fall back to the
    // resolved entry when the hook is not installed.
    const UiCreateEncodedText_pt fn = UiCreateEncodedText_Ret ? UiCreateEncodedText_Ret : UiCreateEncodedText_Func;
    if (!fn) {
        return nullptr;
    }
    const uint32_t handle = fn(8, 7, text, 0);  // style 8 / layout 7 = literal-text wrapper
    return reinterpret_cast<const wchar_t*>(handle);
}

bool SetNextCreatedWindowTitle(const std::wstring& title) {
    if (title.empty()) {
        return false;
    }
    if (!EnsureInstalled()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_next_created_window_title = title;
    return true;
}

void ClearNextCreatedWindowTitle() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_next_created_window_title.clear();
    g_pending_title_overrides.clear();
    g_expect_next_title_set = false;
    g_expect_next_title_resource_set = false;
}

bool HasNextCreatedWindowTitle() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return !g_next_created_window_title.empty();
}

void ArmNextCreatedWindowTitle(uint32_t parent_frame_id, uint32_t child_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_next_created_window_title.empty()) {
        return;
    }
    g_pending_title_overrides.push_back({ parent_frame_id, child_index, g_next_created_window_title });
    g_next_created_window_title.clear();
}

void CancelArmedWindowTitle(uint32_t parent_frame_id, uint32_t child_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const int pending_index = FindPendingOverrideIndex(parent_frame_id, child_index);
    if (pending_index >= 0) {
        g_pending_title_overrides.erase(g_pending_title_overrides.begin() + pending_index);
    }
}

uint32_t GetLastAppliedFrameId() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_last_applied_frame_id;
}

std::wstring GetLastAppliedTitle() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_last_applied_title;
}

bool IsInstalled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_hook_installed;
}

}  // namespace title_hook

/* ---------------- Module lifecycle ---------------- */

bool Initialize() {
    // Hooks install lazily on first use; nothing to arm at startup yet.
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "native_ui", "shutdown");
    title_hook::RemoveHooks();
}

}  // namespace GW::native_ui

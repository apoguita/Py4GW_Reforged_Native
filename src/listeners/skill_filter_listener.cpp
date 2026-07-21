#include "base/error_handling.h"

#include "listeners/skill_filter_listener.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"

#include "GW/common/constants/skills.h"
#include "GW/context/skill.h"
#include "GW/skillbar/skillbar.h"
#include "GW/ui/ui.h"

namespace PY4GW::listeners {

namespace {

namespace ui = GW::ui;

// Resolved skill-list UI interaction callback + its trampoline. Resolved and
// hooked once on first Install; toggled with Enable/DisableHooks thereafter.
ui::UIInteractionCallback g_skill_list_uicallback_func = nullptr;
ui::UIInteractionCallback g_skill_list_uicallback_original = nullptr;
bool g_hook_created = false;

// Detour: when a filter matches, drop the "add skill to list" frame message by
// NOT forwarding it to the original callback (as the legacy module did).
void __cdecl OnSkillList_UICallback(ui::InteractionMessage* message, void* wParam, void* lParam) {
    PY4GW::HookBase::EnterHook();

    const SkillListFilterListener& self = SkillListFilter();
    if (message->message_id == ui::UIMessage::kFrameMessage_0x47) {
        // Hide skills the character already owns (low bits flag learned skills).
        if (self.HideKnownSkills() && (static_cast<uint32_t*>(wParam)[1] & 0x3) != 0) {
            PY4GW::HookBase::LeaveHook();
            return;
        }
        // In the skill-capture window, hide everything that is not elite.
        if (self.HideNonElitesOnCapture()) {
            ui::Frame* parent = ui::GetFrameByLabel(L"DlgSkillCapture");
            if (parent && ui::BelongsToFrame(parent, ui::GetFrameById(message->frame_id))) {
                const auto skill_id = *static_cast<GW::Constants::SkillID*>(wParam);
                const GW::Context::Skill* skill = GW::skillbar::GetSkillConstantData(skill_id);
                if (skill && !skill->IsElite()) {
                    PY4GW::HookBase::LeaveHook();
                    return;
                }
            }
        }
    }

    if (g_skill_list_uicallback_original) {
        g_skill_list_uicallback_original(message, wParam, lParam);
    }
    PY4GW::HookBase::LeaveHook();
}

// Resolve + create the hook once. Returns whether the hook is available.
bool EnsureHook() {
    if (g_hook_created) {
        return g_skill_list_uicallback_func != nullptr;
    }
    CrashContextScope context("runtime", "listeners", "skill_list_filter_resolve");
    if (!g_skill_list_uicallback_func) {
        PY4GW::Patterns::Resolve("listeners.skill_list_uicallback_func", &g_skill_list_uicallback_func);
    }
    if (!g_skill_list_uicallback_func) {
        Logger::Instance().LogWarning("[listeners] skill_list_filter: skill-list UI callback not resolved; filter inactive");
        g_hook_created = true;  // Don't re-attempt every toggle.
        return false;
    }
    Logger::AssertHook("SkillList_UICallback",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&g_skill_list_uicallback_func),
            reinterpret_cast<void*>(&OnSkillList_UICallback),
            reinterpret_cast<void**>(&g_skill_list_uicallback_original)),
        "listeners");
    g_hook_created = true;
    return true;
}

}  // namespace

void SkillListFilterListener::Install() {
    if (!EnsureHook()) {
        return;
    }
    PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_skill_list_uicallback_func));
}

void SkillListFilterListener::Uninstall() {
    // Guard on a valid target: DisableHooks(nullptr) would disable every hook.
    if (g_skill_list_uicallback_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_skill_list_uicallback_func));
    }
}

SkillListFilterListener& SkillListFilter() {
    static SkillListFilterListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

#include "base/error_handling.h"

#include "listeners/auto_cancel_ua_listener.h"

#include "GW/common/constants/skills.h"
#include "GW/context/skill.h"
#include "GW/context/ui.h"
#include "GW/effects/effects.h"
#include "GW/ui/ui.h"

namespace PY4GW::listeners {

void AutoCancelUnyieldingAuraListener::Install() {
    GW::ui::RegisterUIMessageCallback(
        &cast_entry_,
        GW::ui::UIMessage::kAgentStartCasting,
        [](PY4GW::HookStatus*, GW::ui::UIMessage, void* wparam, void*) {
            const auto* pkt = static_cast<GW::Context::AgentStartCastingPacket*>(wparam);
            if (!pkt || pkt->skill_id != GW::Constants::SkillID::Unyielding_Aura) {
                return;
            }
            GW::Context::BuffArray* buffs = GW::effects::GetAgentBuffs(pkt->agent_id);
            if (!buffs) {
                return;
            }
            for (auto& buff : *buffs) {
                if (buff.skill_id == pkt->skill_id) {
                    GW::effects::DropBuff(buff.buff_id);
                }
            }
        });
}

void AutoCancelUnyieldingAuraListener::Uninstall() {
    GW::ui::RemoveUIMessageCallback(&cast_entry_, GW::ui::UIMessage::kAgentStartCasting);
}

AutoCancelUnyieldingAuraListener& AutoCancelUnyieldingAura() {
    static AutoCancelUnyieldingAuraListener instance;
    return instance;
}

}  // namespace PY4GW::listeners

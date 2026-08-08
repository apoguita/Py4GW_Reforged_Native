#include "base/error_handling.h"

#include "GW/effects/effects.h"

#include "GW/context/context.h"
#include "GW/context/world.h"

#include <atomic>
#include <chrono>
#include <mutex>

namespace GW::effects {

using PostProcessEffectFn = void(__cdecl*)(uint32_t intensity, uint32_t tint);
using DropBuffFn = void(__cdecl*)(uint32_t buff_id);

extern PostProcessEffectFn g_post_process_effect_original;
extern DropBuffFn g_drop_buff_func;
extern std::atomic<uint32_t> g_alcohol_level;
extern std::mutex g_alcohol_timer_mutex;
extern uint64_t g_alcohol_expires_at_ms;

uint32_t GetAlcoholLevel() {
    return g_alcohol_level.load();
}

uint32_t GetAlcoholTimeRemaining() {
    const uint64_t now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    std::lock_guard<std::mutex> lock(g_alcohol_timer_mutex);
    if (g_alcohol_expires_at_ms <= now_ms) {
        g_alcohol_level = 0;
        g_alcohol_expires_at_ms = 0;
        return 0;
    }
    const uint64_t remaining_ms = g_alcohol_expires_at_ms - now_ms;
    return remaining_ms > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(remaining_ms);
}

void GetDrunkAf(uint32_t intensity, uint32_t tint) {
    if (g_post_process_effect_original) {
        g_post_process_effect_original(intensity, tint);
    }
}

Context::AgentEffects* GetAgentEffectsArray(uint32_t agent_id) {
    Context::AgentEffectsArray* agent_effects = Context::GetPartyEffectsArray();
    if (!agent_effects) {
        return nullptr;
    }

    for (auto& agent_effect : *agent_effects) {
        if (agent_effect.agent_id == agent_id) {
            return &agent_effect;
        }
    }
    return nullptr;
}

Context::AgentEffects* GetPlayerEffectsArray() {
    return GetAgentEffectsArray(Context::GetControlledCharacterId());
}

Context::EffectArray* GetAgentEffects(uint32_t agent_id) {
    Context::AgentEffects* effects = GetAgentEffectsArray(agent_id);
    return effects && effects->effects.valid() ? &effects->effects : nullptr;
}

Context::BuffArray* GetAgentBuffs(uint32_t agent_id) {
    Context::AgentEffects* effects = GetAgentEffectsArray(agent_id);
    return effects && effects->buffs.valid() ? &effects->buffs : nullptr;
}

Context::EffectArray* GetPlayerEffects() {
    return GetAgentEffects(Context::GetControlledCharacterId());
}

Context::BuffArray* GetPlayerBuffs() {
    return GetAgentBuffs(Context::GetControlledCharacterId());
}

bool DropBuff(uint32_t buff_id) {
    if (!g_drop_buff_func) {
        return false;
    }

    g_drop_buff_func(buff_id);
    return true;
}

Context::Effect* GetPlayerEffectBySkillId(GW::Constants::SkillID skill_id) {
    Context::EffectArray* effects = GetPlayerEffects();
    if (!effects) {
        return nullptr;
    }

    for (auto& effect : *effects) {
        if (effect.skill_id == skill_id) {
            return &effect;
        }
    }
    return nullptr;
}

Context::Buff* GetPlayerBuffBySkillId(GW::Constants::SkillID skill_id) {
    Context::BuffArray* buffs = GetPlayerBuffs();
    if (!buffs) {
        return nullptr;
    }

    for (auto& buff : *buffs) {
        if (buff.skill_id == skill_id) {
            return &buff;
        }
    }
    return nullptr;
}

}  // namespace GW::effects

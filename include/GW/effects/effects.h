#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include "GW/context/skill.h"

#include <atomic>
#include <cstdint>

namespace GW::effects {

bool Initialize();
void Shutdown();

using PostProcessEffectFn = void(__cdecl*)(uint32_t intensity, uint32_t tint);
using DropBuffFn = void(__cdecl*)(uint32_t buff_id);

uint32_t GetAlcoholLevel();
void GetDrunkAf(uint32_t intensity, uint32_t tint);

Context::AgentEffectsArray* GetPartyEffectsArray();
Context::AgentEffects* GetAgentEffectsArray(uint32_t agent_id);
Context::AgentEffects* GetPlayerEffectsArray();
Context::EffectArray* GetAgentEffects(uint32_t agent_id);
Context::BuffArray* GetAgentBuffs(uint32_t agent_id);
Context::EffectArray* GetPlayerEffects();
Context::BuffArray* GetPlayerBuffs();
bool DropBuff(uint32_t buff_id);
Context::Effect* GetPlayerEffectBySkillId(GW::Constants::SkillID skill_id);
Context::Buff* GetPlayerBuffBySkillId(GW::Constants::SkillID skill_id);

extern PostProcessEffectFn g_post_process_effect_func;
extern PostProcessEffectFn g_post_process_effect_original;
extern DropBuffFn g_drop_buff_func;
extern std::atomic<uint32_t> g_alcohol_level;
extern std::atomic<bool> g_initialized;

inline bool ResolvePostProcessEffect() {
    CrashContextScope context("startup", "effects", "resolve_post_process_effect");
    const auto* pattern = PY4GW::Patterns::Get("effects.post_process_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.post_process_target", "effects");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    g_post_process_effect_func = reinterpret_cast<PostProcessEffectFn>(address);
    return Logger::AssertAddress(
        "PostProcessEffect_Func",
        reinterpret_cast<uintptr_t>(g_post_process_effect_func),
        "effects");
}

inline bool ResolveDropBuff() {
    CrashContextScope context("startup", "effects", "resolve_drop_buff");
    const auto* pattern = PY4GW::Patterns::Get("effects.drop_buff_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.drop_buff_callsite", "effects");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DropBuff_Callsite", callsite, "effects")) {
        return false;
    }

    g_drop_buff_func = reinterpret_cast<DropBuffFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress("DropBuff_Func", reinterpret_cast<uintptr_t>(g_drop_buff_func), "effects");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __cdecl OnPostProcessEffect(uint32_t intensity, uint32_t tint);

}  // namespace GW::effects

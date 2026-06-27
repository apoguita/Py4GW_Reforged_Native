#pragma once

#include "base/error_handling.h"

#include "GW/context/gw_array.h"

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace gw::constants {

enum class SkillID : uint32_t;

}  // namespace gw::constants

namespace gw::context {

struct SkillbarSkill {
    uint32_t adrenaline_a;
    uint32_t adrenaline_b;
    uint32_t recharge;
    gw::constants::SkillID skill_id;
    uint32_t event;

    uint32_t GetRecharge() const;
};

struct SkillbarCast {
    uint16_t h0000;
    gw::constants::SkillID skill_id;
    uint32_t h0004;
};

using SkillbarCastArray = gw::GwArray<SkillbarCast>;

struct Skillbar {
    uint32_t agent_id;
    SkillbarSkill skills[8];
    uint32_t disabled;
    uint32_t h00a8[2];
    uint32_t casting;
    uint32_t h00b4[2];

    bool IsValid() const {
        return agent_id > 0;
    }

    SkillbarSkill* GetSkillById(gw::constants::SkillID skill_id, size_t* slot_out = nullptr);
};

using SkillbarArray = gw::GwArray<Skillbar>;

struct Effect {
    gw::constants::SkillID skill_id;
    uint32_t attribute_level;
    uint32_t effect_id;
    uint32_t agent_id;
    float duration;
    DWORD timestamp;

    DWORD GetTimeElapsed() const;
    DWORD GetTimeRemaining() const;
};

struct Buff {
    gw::constants::SkillID skill_id;
    uint32_t h0004;
    uint32_t buff_id;
    uint32_t target_agent_id;
};

using BuffArray = gw::GwArray<Buff>;
using EffectArray = gw::GwArray<Effect>;

struct AgentEffects {
    uint32_t agent_id;
    BuffArray buffs;
    EffectArray effects;
};

using AgentEffectsArray = gw::GwArray<AgentEffects>;

static_assert(sizeof(SkillbarSkill) == 0x14, "SkillbarSkill size mismatch");
static_assert(sizeof(Skillbar) == 0xBC, "Skillbar size mismatch");
static_assert(sizeof(Effect) == 0x18, "Effect size mismatch");
static_assert(sizeof(Buff) == 0x10, "Buff size mismatch");
static_assert(sizeof(AgentEffects) == 0x24, "AgentEffects size mismatch");

}  // namespace gw::context

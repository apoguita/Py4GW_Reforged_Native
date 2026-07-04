#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <string>

namespace GW::skillbar {

// Skill / skill-type / profession name lookups.
// The tables in skill_names.cpp are GENERATED from the reforged constant enums
// (GW::Constants::SkillID and SkillType in common/constants/skills.h,
// GW::Constants::Profession in common/constants/constants.h) - those enums are the
// authoritative id<->name source, so these lookups do not re-port the legacy
// SkillArray tables. Regenerate skill_names.cpp when the enums change.

// Returns the underscore skill name (e.g. "Energy_Drain") for a skill id, or "".
const char* GetSkillNameByID(uint32_t skill_id);

// Reverse lookup: underscore skill name -> id, or 0 (No_Skill) if unknown.
uint32_t GetSkillIDByName(const std::string& name);

// Returns the skill-type name (e.g. "Hex", "Spell"), or "".
const char* GetSkillTypeNameByID(uint32_t type_id);

// Returns the profession name (e.g. "Dervish"), or "".
const char* GetProfessionNameById(uint32_t profession_id);

}  // namespace GW::skillbar

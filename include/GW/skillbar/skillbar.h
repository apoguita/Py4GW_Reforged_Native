#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/agent/agent.h"
#include "GW/common/constants/constants.h"
#include "GW/context/account_context.h"
#include "GW/context/attribute.h"
#include "GW/context/context.h"
#include "GW/context/game_context.h"
#include "GW/context/party_context.h"
#include "GW/context/skill.h"
#include "GW/context/world_context.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace GW::skillbar {

using SkillbarArray = Context::SkillbarArray;
using Skill = Context::Skill;
using AttributeInfo = Context::AttributeInfo;
using Skillbar = Context::Skillbar;
using SkillbarSkill = Context::SkillbarSkill;
using PartyAttributeArray = Context::PartyAttributeArray;
using PartyAttribute = Context::PartyAttribute;
using ProfessionState = Context::ProfessionState;

struct Attribute {
    Constants::Attribute attribute = Constants::Attribute::None;
    uint32_t points = 0;
};

struct SkillTemplate {
    Constants::Profession primary = Constants::Profession::None;
    Constants::Profession secondary = Constants::Profession::None;
    Constants::SkillID skills[8] = {};
    Attribute attributes[16] = {};
};

using UseSkillCallback = PY4GW::HookCallback<uint32_t, uint32_t, uint32_t, uint32_t>;

using UseSkillFn = void(__cdecl*)(uint32_t, uint32_t, uint32_t, uint32_t);
using LoadSkillsFn = void(__cdecl*)(uint32_t agent_id, uint32_t skill_ids_count, uint32_t* skill_ids);
using LoadAttributesFn = void(__cdecl*)(uint32_t agent_id, uint32_t attribute_count, uint32_t* attribute_ids, uint32_t* attribute_values);
using ChangeSecondaryFn = void(__cdecl*)(uint32_t agent_id, uint32_t profession);

struct OnLoadSkillbar_UIMessage_Packet {
    uint32_t agent_id;
    uint32_t skill_ids[8] = {};
};

bool Initialize();
void Shutdown();

int GetSkillSlot(Constants::SkillID skill_id);
bool UseSkill(uint32_t slot, uint32_t target = 0);
bool PointBlankUseSkill(uint32_t slot);
bool UseSkillByID(uint32_t skill_id, uint32_t target = 0);
Skill* GetSkillConstantData(Constants::SkillID skill_id);
AttributeInfo* GetAttributeConstantData(Constants::Attribute attribute_id);
bool ChangeSecondProfession(Constants::Profession profession, uint32_t hero_index = 0);
SkillbarArray* GetSkillbarArray();
Skillbar* GetPlayerSkillbar();
Skillbar* GetHeroSkillbar(uint32_t hero_index);
Skill* GetHoveredSkill();
SkillTemplate GetSkillTemplate(uint32_t hero_index = 0);
bool GetIsSkillUnlocked(Constants::SkillID skill_id);
bool GetIsSkillLearnt(Constants::SkillID skill_id);
bool DecodeSkillTemplate(SkillTemplate* result, const char* temp);
bool EncodeSkillTemplate(const SkillTemplate& in, char* result, size_t result_len);
bool LoadSkillbar(Constants::SkillID* skills, size_t n_skills, uint32_t hero_index = 0);
bool LoadSkillTemplate(const char* temp);
bool LoadSkillTemplate(const char* temp, uint32_t hero_index);
bool SetAttributes(uint32_t attribute_count, uint32_t* attribute_ids, uint32_t* attribute_values, uint32_t hero_index = 0);
bool SetAttributes(Attribute* attributes, size_t n_attributes, uint32_t hero_index = 0);
void RegisterUseSkillCallback(PY4GW::HookEntry* entry, const UseSkillCallback& callback);
void RemoveUseSkillCallback(PY4GW::HookEntry* entry);

extern Skill* g_skill_array_addr;
extern AttributeInfo* g_attribute_array_addr;
extern uint32_t g_attribute_array_count;
extern UseSkillFn g_use_skill_func;
extern UseSkillFn g_use_skill_original;
extern LoadSkillsFn g_load_skills_func;
extern LoadSkillsFn g_load_skills_original;
extern LoadAttributesFn g_load_attributes_func;
extern ChangeSecondaryFn g_change_secondary_func;
extern std::unordered_map<PY4GW::HookEntry*, UseSkillCallback> g_use_skill_callbacks;
extern PY4GW::HookEntry g_load_skillbar_hook_entry;
extern std::atomic<bool> g_initialized;

constexpr char _g_Base64ToValue[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

constexpr unsigned char _g_Base64Table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline int _WriteBits(int val, char* buff, int count = 6) {
    for (int i = 0; i < count; i++) {
        buff[i] = ((val >> i) & 1);
    }
    return count;
}

inline int _ReadBits(char** str, int n) {
    int val = 0;
    char* s = *str;
    for (int i = 0; i < n; i++)
        val |= (*s++ << i);
    *str = s;
    return val;
}

const ProfessionState* GetAgentProfessionState(uint32_t agent_id);
Constants::Profession GetSkillProfession(Constants::SkillID skill_id);
bool IsPrimaryAttributeRequired(const SkillTemplate& skill_template, Constants::Profession profession);
bool IsProfessionRequired(const SkillTemplate& skill_template, Constants::Profession profession);
Constants::Profession GetAttributeProfession(Constants::Attribute attribute, bool* is_primary_attribute = nullptr);

inline bool ResolveSkillArray() {
    CrashContextScope context("startup", "skillbar", "resolve_skill_array");
    const auto* pattern = PY4GW::Patterns::Get("skillbar.skill_array_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: skillbar.skill_array_ref", "skillbar");
        return false;
    }
    uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find skill_array_ref", "skillbar");
        return false;
    }
    address = PY4GW::Scanner::FunctionFromNearCall(address);
    if (!PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        Logger::Instance().LogError("Skill array pointer scan target not in text section.", "skillbar");
        return false;
    }
    g_skill_array_addr = *reinterpret_cast<Skill**>(address + 0x2B);
    return Logger::AssertAddress("skill_array_addr", reinterpret_cast<uintptr_t>(g_skill_array_addr), "skillbar");
}

inline bool ResolveAttributeArray() {
    CrashContextScope context("startup", "skillbar", "resolve_attribute_array");
    const auto* pattern = PY4GW::Patterns::Get("skillbar.attribute_array_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: skillbar.attribute_array_ref", "skillbar");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find attribute_array_ref", "skillbar");
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address), PY4GW::ScannerSection::RData)) {
        Logger::Instance().LogError("Attribute array pointer is outside the expected rdata section.", "skillbar");
        return false;
    }
    g_attribute_array_addr = *reinterpret_cast<AttributeInfo**>(address);
    return Logger::AssertAddress("attribute_array_addr", reinterpret_cast<uintptr_t>(g_attribute_array_addr), "skillbar");
}

inline bool ResolveUseSkillFunction() {
    CrashContextScope context("startup", "skillbar", "resolve_use_skill");
    const auto* pattern = PY4GW::Patterns::Get("skillbar.use_skill_func_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: skillbar.use_skill_func_callsite", "skillbar");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find use_skill_func_callsite", "skillbar");
        return false;
    }
    g_use_skill_func = reinterpret_cast<UseSkillFn>(PY4GW::Scanner::ToFunctionStart(address, 0x200));
    return Logger::AssertAddress("UseSkill_Func", reinterpret_cast<uintptr_t>(g_use_skill_func), "skillbar");
}

inline bool ResolveTemplatesFunctions() {
    CrashContextScope context("startup", "skillbar", "resolve_templates_functions");
    const auto* pattern = PY4GW::Patterns::Get("skillbar.templates_helpers_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: skillbar.templates_helpers_assertion", "skillbar");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to find templates_helpers_assertion", "skillbar");
        return false;
    }
    g_change_secondary_func = reinterpret_cast<ChangeSecondaryFn>(PY4GW::Scanner::FunctionFromNearCall(address + 0x20));
    if (!Logger::AssertAddress("ChangeSecondary_Func", reinterpret_cast<uintptr_t>(g_change_secondary_func), "skillbar")) {
        return false;
    }
    g_load_attributes_func = reinterpret_cast<LoadAttributesFn>(PY4GW::Scanner::FunctionFromNearCall(address + 0x34));
    if (!Logger::AssertAddress("LoadAttributes_Func", reinterpret_cast<uintptr_t>(g_load_attributes_func), "skillbar")) {
        return false;
    }
    g_load_skills_func = reinterpret_cast<LoadSkillsFn>(PY4GW::Scanner::FunctionFromNearCall(address + 0x40));
    return Logger::AssertAddress("LoadSkills_Func", reinterpret_cast<uintptr_t>(g_load_skills_func), "skillbar");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __cdecl OnUseSkill(uint32_t agent_id, uint32_t slot, uint32_t target, uint32_t call_target);
void OnLoadSkillbar(uint32_t agent_id, uint32_t skill_ids_count, uint32_t* skill_ids);
void OnLoadSkillbarUIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void* lparam);

}  // namespace GW::skillbar

namespace GW {
namespace Skillbar = skillbar;
}

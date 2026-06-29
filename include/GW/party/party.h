#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/agent/agent.h"
#include "GW/chat/chat.h"
#include "GW/common/constants/constants.h"
#include "GW/common/game_pos.h"
#include "GW/context/attribute.h"
#include "GW/context/context.h"
#include "GW/context/hero.h"
#include "GW/context/party.h"
#include "GW/context/party_context.h"
#include "GW/context/world_context.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/player/player.h"
#include "GW/skillbar/skillbar.h"
#include "GW/ui/ui.h"

#include <atomic>
#include <cstdint>

namespace GW::party {

using AgentID = Context::AgentID;

using PartySearchSeekFn = void(__cdecl*)(uint32_t search_type, const wchar_t* advertisement, uint32_t unk);
using PartySearchButtonCallbackFn = void(__fastcall*)(void* context, uint32_t edx, uint32_t* wparam);
using DoActionFn = void(__cdecl*)(uint32_t identifier);
using FlagHeroAgentFn = void(__cdecl*)(uint32_t agent_id, GW::GamePos* pos);
using FlagAllFn = void(__cdecl*)(GW::GamePos* pos);
using SetHeroBehaviorFn = void(__cdecl*)(uint32_t agent_id, Context::HeroBehavior behavior);
using LockPetTargetFn = bool(__cdecl*)(uint32_t pet_agent_id, uint32_t target_id);
using CommandHotKeyDisableAiFn = void(__cdecl*)(uint32_t hero_agent_id, uint32_t zero_based_skill_slot);

bool Initialize();
void Shutdown();

void set_tick_toggle(bool enable);
bool tick(bool flag = true);
Context::Attribute* get_agent_attributes(uint32_t agent_id);
Context::PartySearch* get_party_search(uint32_t party_search_id = 0);
Context::PartyInfo* get_party_info(uint32_t party_id = 0);
uint32_t get_party_size();
uint32_t get_party_player_count();
uint32_t get_party_hero_count();
uint32_t get_party_henchman_count();
bool get_is_party_defeated();
bool set_hard_mode(bool flag);
bool return_to_outpost();
bool get_is_party_in_hard_mode();
bool get_is_hard_mode_unlocked();
bool get_is_party_ticked();
bool get_is_player_ticked(uint32_t player_index = 0xFFFFFFFF);
bool get_is_player_loaded(uint32_t player_index = 0xFFFFFFFF);
bool get_is_party_loaded();
bool get_is_leader();
bool respond_to_party_request(uint32_t party_id, bool accept);
bool leave_party();
bool add_hero(uint32_t heroid);
bool kick_hero(uint32_t heroid);
bool kick_all_heroes();
bool add_henchman(uint32_t agent_id);
bool kick_henchman(uint32_t agent_id);
bool invite_player(uint32_t player_id);
bool invite_player(const wchar_t* player_name);
bool kick_player(uint32_t player_id);
bool flag_hero(uint32_t hero_index, GamePos pos);
bool flag_hero_agent(AgentID agent_id, GamePos pos);
bool unflag_hero(uint32_t hero_index);
bool flag_all(GamePos pos);
bool unflag_all();
bool set_hero_behavior(uint32_t agent_id, Context::HeroBehavior behavior);
bool set_hero_skill_ai_enabled(uint32_t hero_agent_id, uint32_t skill_slot, bool enabled);
bool set_pet_behavior(Context::HeroBehavior behavior, uint32_t lock_target_id = 0);
Context::PetInfo* get_pet_info(uint32_t owner_agent_id = 0);
uint32_t get_hero_agent_id(uint32_t hero_index);
uint32_t get_agent_hero_id(AgentID agent_id);
Context::HeroInfo* get_hero_info(uint32_t hero_id);
bool search_party(uint32_t search_type, const wchar_t* advertisement = nullptr);
bool search_party_cancel();
bool search_party_reply(bool accept);

extern ui::UIInteractionCallback g_tick_button_ui_callback;
extern ui::UIInteractionCallback g_tick_button_ui_callback_original;
extern ui::UIInteractionCallback g_party_player_member_ui_callback;
extern PartySearchSeekFn g_party_search_seek_func;
extern PartySearchButtonCallbackFn g_party_search_button_callback_func;
extern PartySearchButtonCallbackFn g_party_window_button_callback_func;
extern DoActionFn g_set_ready_status_func;
extern DoActionFn g_set_difficulty_func;
extern FlagHeroAgentFn g_flag_hero_agent_func;
extern FlagAllFn g_flag_all_func;
extern SetHeroBehaviorFn g_set_hero_behavior_func;
extern LockPetTargetFn g_lock_pet_target_func;
extern CommandHotKeyDisableAiFn g_command_hot_key_disable_ai_func;
extern bool g_tick_work_as_toggle;
extern std::atomic<bool> g_initialized;

inline bool warn_if_missing_address(const char* name, uintptr_t address) {
    if (address) {
        return true;
    }
    Logger::Instance().LogWarning(std::string(name) + " is null.", "party");
    return false;
}

inline bool resolve_tick_button_ui_callback() {
    CrashContextScope context("startup", "party", "resolve_tick_button_ui_callback");
    const auto* pattern = PY4GW::Patterns::Get("party.tick_button_ui_callback_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.tick_button_ui_callback_ref", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find TickButtonUICallback address", "party");
        return false;
    }
    g_tick_button_ui_callback = reinterpret_cast<ui::UIInteractionCallback>(
        PY4GW::Scanner::ToFunctionStart(address, 0xfff));
    return warn_if_missing_address("TickButtonUICallback", reinterpret_cast<uintptr_t>(g_tick_button_ui_callback));
}

inline bool resolve_set_difficulty_func() {
    CrashContextScope context("startup", "party", "resolve_set_difficulty_func");
    const auto* pattern = PY4GW::Patterns::Get("party.set_difficulty_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.set_difficulty_callsite", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find SetDifficulty_Func callsite", "party");
        return false;
    }
    g_set_difficulty_func = reinterpret_cast<DoActionFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return warn_if_missing_address("SetDifficulty_Func", reinterpret_cast<uintptr_t>(g_set_difficulty_func));
}

inline bool resolve_party_search_seek_func() {
    CrashContextScope context("startup", "party", "resolve_party_search_seek_func");
    const auto* pattern = PY4GW::Patterns::Get("party.party_search_seek_func_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.party_search_seek_func_ref", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find PartySearchSeek_Func address", "party");
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        Logger::Instance().LogError("PartySearchSeek_Func pointer is outside the expected text section.", "party");
        return false;
    }
    g_party_search_seek_func = reinterpret_cast<PartySearchSeekFn>(address);
    return warn_if_missing_address("PartySearchSeek_Func", reinterpret_cast<uintptr_t>(g_party_search_seek_func));
}

inline bool resolve_party_search_button_callback_func() {
    CrashContextScope context("startup", "party", "resolve_party_search_button_callback");
    const auto* pattern = PY4GW::Patterns::Get("party.party_search_button_callback_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.party_search_button_callback_assertion", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to find PartySearchButtonCallback_Func", "party");
        return false;
    }
    g_party_search_button_callback_func = reinterpret_cast<PartySearchButtonCallbackFn>(
        PY4GW::Scanner::ToFunctionStart(address));
    return warn_if_missing_address("PartySearchButtonCallback_Func", reinterpret_cast<uintptr_t>(g_party_search_button_callback_func));
}

inline bool resolve_party_window_button_callback_func() {
    CrashContextScope context("startup", "party", "resolve_party_window_button_callback");
    const auto* pattern = PY4GW::Patterns::Get("party.party_window_button_callback_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.party_window_button_callback_ref", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogError("Failed to find PartyWindowButtonCallback_Func", "party");
        return false;
    }
    g_party_window_button_callback_func = reinterpret_cast<PartySearchButtonCallbackFn>(
        PY4GW::Scanner::ToFunctionStart(address));
    return warn_if_missing_address("PartyWindowButtonCallback_Func", reinterpret_cast<uintptr_t>(g_party_window_button_callback_func));
}

inline bool resolve_party_player_member_ui_callback() {
    CrashContextScope context("startup", "party", "resolve_party_player_member_ui_callback");
    const auto* pattern = PY4GW::Patterns::Get("party.party_player_member_ui_callback_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.party_player_member_ui_callback_assertion", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to find PartyPlayerMember_UICallback", "party");
        return false;
    }
    g_party_player_member_ui_callback = reinterpret_cast<ui::UIInteractionCallback>(
        PY4GW::Scanner::ToFunctionStart(address, 0xfff));
    return warn_if_missing_address("PartyPlayerMember_UICallback", reinterpret_cast<uintptr_t>(g_party_player_member_ui_callback));
}

inline bool resolve_set_ready_status_func() {
    CrashContextScope context("startup", "party", "resolve_set_ready_status_func");
    const auto* pattern = PY4GW::Patterns::Get("party.set_ready_status_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.set_ready_status_assertion", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to find SetReadyStatus_Func", "party");
        return false;
    }
    g_set_ready_status_func = reinterpret_cast<DoActionFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return warn_if_missing_address("SetReadyStatus_Func", reinterpret_cast<uintptr_t>(g_set_ready_status_func));
}

inline bool resolve_flag_functions() {
    CrashContextScope context("startup", "party", "resolve_flag_functions");
    const auto* anchor_pattern = PY4GW::Patterns::Get("party.flag_hero_agent_func_ref");
    const auto* hero_call_pattern = PY4GW::Patterns::Get("party.flag_all_inner_call_ref");
    const auto* all_call_pattern = PY4GW::Patterns::Get("party.flag_all_func_inner_call_ref");
    if (!anchor_pattern || !hero_call_pattern || !all_call_pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.flag_hero_agent_func_ref", "party");
        return false;
    }
    uintptr_t address = PY4GW::Scanner::Find(
        anchor_pattern->pattern.c_str(),
        anchor_pattern->mask.c_str(),
        anchor_pattern->offset,
        anchor_pattern->section);
    if (!address) {
        Logger::Instance().LogWarning("FlagAgent address is null.", "party");
        return false;
    }
    if (PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        address = PY4GW::Scanner::FindInRange(
            hero_call_pattern->pattern.c_str(),
            hero_call_pattern->mask.c_str(),
            hero_call_pattern->offset,
            address,
            address + 0x64);
        if (address) {
            g_flag_hero_agent_func = reinterpret_cast<FlagHeroAgentFn>(
                PY4GW::Scanner::FunctionFromNearCall(address));
            address = PY4GW::Scanner::FindInRange(
                all_call_pattern->pattern.c_str(),
                all_call_pattern->mask.c_str(),
                all_call_pattern->offset,
                address,
                address + 0x64);
            if (address) {
                g_flag_all_func = reinterpret_cast<FlagAllFn>(
                    PY4GW::Scanner::FunctionFromNearCall(address));
            }
        }
    }
    warn_if_missing_address("FlagHeroAgent_Func", reinterpret_cast<uintptr_t>(g_flag_hero_agent_func));
    return warn_if_missing_address("FlagAll_Func", reinterpret_cast<uintptr_t>(g_flag_all_func));
}

inline bool resolve_set_hero_behavior_funcs() {
    CrashContextScope context("startup", "party", "resolve_set_hero_behavior_funcs");
    const auto* pattern = PY4GW::Patterns::Get("party.set_hero_behavior_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.set_hero_behavior_callsite", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogWarning("PetTarget address is null.", "party");
        return false;
    }
    if (PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        g_lock_pet_target_func = reinterpret_cast<LockPetTargetFn>(
            PY4GW::Scanner::FunctionFromNearCall(address));
        g_set_hero_behavior_func = reinterpret_cast<SetHeroBehaviorFn>(
            PY4GW::Scanner::FunctionFromNearCall(address + 0x7));
    }
    warn_if_missing_address("SetHeroBehavior_Func", reinterpret_cast<uintptr_t>(g_set_hero_behavior_func));
    return warn_if_missing_address("LockPetTarget_Func", reinterpret_cast<uintptr_t>(g_lock_pet_target_func));
}

inline bool resolve_command_hotkey_disable_ai_func() {
    CrashContextScope context("startup", "party", "resolve_command_hotkey_disable_ai");
    const auto* pattern = PY4GW::Patterns::Get("party.command_hotkey_disable_ai_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: party.command_hotkey_disable_ai_ref", "party");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address) {
        Logger::Instance().LogWarning("CommandHotKeyDisableAi address is null.", "party");
        return false;
    }
    if (PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        g_command_hot_key_disable_ai_func = reinterpret_cast<CommandHotKeyDisableAiFn>(
            PY4GW::Scanner::ToFunctionStart(address));
    }
    return warn_if_missing_address("CommandHotKeyDisableAi_Func", reinterpret_cast<uintptr_t>(g_command_hot_key_disable_ai_func));
}

bool init();
void enable_hooks();
void disable_hooks();
void exit();

void OnTickButtonUICallback(ui::InteractionMessage* message, void* wParam, void* lParam);

}  // namespace GW::party

namespace GW {
namespace Party = party;
}

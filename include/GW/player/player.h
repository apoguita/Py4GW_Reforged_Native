#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/constants/constants.h"
#include "GW/context/player.h"
#include "GW/context/title.h"
#include "GW/skillbar/skillbar.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace GW::player {

using PlayerNumber = uint32_t;

bool Initialize();
void Shutdown();

using RemoveActiveTitleFn = void(__cdecl*)();
using SetActiveTitleFn = void(__cdecl*)(uint32_t identifier);
using DepositFactionFn = void(__cdecl*)(uint32_t always_0, uint32_t allegiance, uint32_t amount);

bool SetActiveTitle(GW::Constants::TitleID title_id);
bool RemoveActiveTitle();

uint32_t GetPlayerAgentId(uint32_t player_id);
uint32_t GetAmountOfPlayersInInstance();

Context::PlayerArray* GetPlayerArray();
PlayerNumber GetPlayerNumber();

Context::Player* GetPlayerByID(uint32_t player_id = 0);
wchar_t* GetPlayerName(uint32_t player_id = 0);
wchar_t* SetPlayerName(uint32_t player_id, const wchar_t* replace_name);

bool ChangeSecondProfession(GW::Constants::Profession profession, uint32_t hero_index = 0);

Context::Player* GetPlayerByName(const wchar_t* name);

Context::Title* GetTitleTrack(GW::Constants::TitleID title_id);
GW::Constants::TitleID GetActiveTitleId();
Context::Title* GetActiveTitle();
std::vector<int> GetTitleIDs();
Context::TitleClientData* GetTitleData(GW::Constants::TitleID title_id);

bool DepositFaction(uint32_t allegiance);

extern RemoveActiveTitleFn g_remove_active_title_func;
extern SetActiveTitleFn g_set_active_title_func;
extern DepositFactionFn g_deposit_faction_func;
extern Context::TitleClientData* g_title_data;
extern std::atomic<bool> g_initialized;

inline bool ResolveSetActiveTitle() {
    CrashContextScope context("startup", "player", "resolve_set_active_title");
    const auto* anchor_pattern = PY4GW::Patterns::Get("player.set_active_title_anchor");
    const auto* call_pattern = PY4GW::Patterns::Get("player.set_active_title_call");
    if (!anchor_pattern || !call_pattern) {
        Logger::Instance().LogError("Missing or invalid set active title pattern.", "player");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        anchor_pattern->assertion_file.c_str(),
        anchor_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(anchor_pattern->line_number),
        anchor_pattern->offset);
    if (!Logger::AssertAddress("SetActiveTitle_Anchor", address, "player")) {
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        call_pattern->pattern.c_str(),
        call_pattern->mask.c_str(),
        call_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("SetActiveTitle_Callsite", address, "player")) {
        return false;
    }

    g_set_active_title_func = reinterpret_cast<SetActiveTitleFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress(
        "SetActiveTitle_Func",
        reinterpret_cast<uintptr_t>(g_set_active_title_func),
        "player");
}

inline bool ResolveRemoveActiveTitle() {
    CrashContextScope context("startup", "player", "resolve_remove_active_title");
    if (!g_set_active_title_func) {
        Logger::Instance().LogError("Set active title function must resolve before remove active title.", "player");
        return false;
    }

    const auto* pattern = PY4GW::Patterns::Get("player.remove_active_title_signature");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.remove_active_title_signature", "player");
        return false;
    }

    uintptr_t address = reinterpret_cast<uintptr_t>(g_set_active_title_func);
    address = PY4GW::Scanner::FindInRange(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        address + 0x10,
        address + 0xFF);
    g_remove_active_title_func = reinterpret_cast<RemoveActiveTitleFn>(address);
    return Logger::AssertAddress(
        "RemoveActiveTitle_Func",
        reinterpret_cast<uintptr_t>(g_remove_active_title_func),
        "player");
}

inline bool ResolveDepositFaction() {
    CrashContextScope context("startup", "player", "resolve_deposit_faction");
    const auto* pattern = PY4GW::Patterns::Get("player.deposit_faction_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.deposit_faction_callsite", "player");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DepositFaction_Callsite", callsite, "player")) {
        return false;
    }

    g_deposit_faction_func = reinterpret_cast<DepositFactionFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress(
        "DepositFaction_Func",
        reinterpret_cast<uintptr_t>(g_deposit_faction_func),
        "player");
}

inline bool ResolveTitleData() {
    CrashContextScope context("startup", "player", "resolve_title_data");
    const auto* pattern = PY4GW::Patterns::Get("player.title_data_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.title_data_ref", "player");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("TitleData_Ref", address, "player")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("TitleData_Ptr", candidate, "player")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate, PY4GW::ScannerSection::RData)) {
        Logger::Instance().LogError("Title data pointer is outside the expected rdata section.", "player");
        return false;
    }

    g_title_data = reinterpret_cast<Context::TitleClientData*>(candidate);
    return true;
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

}  // namespace GW::player

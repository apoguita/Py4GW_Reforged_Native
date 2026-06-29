#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/constants/constants.h"
#include "GW/context/quest.h"
#include "GW/ui/ui.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace GW::quest {

bool Initialize();
void Shutdown();

using RequestQuestInfoFn = void(__cdecl*)(uint32_t identifier);
using RequestQuestDataFn = void(__cdecl*)(uint32_t identifier, bool update_markers);
using DoActionFn = void(__cdecl*)(uint32_t identifier);

GW::Constants::QuestID GetActiveQuestId();

bool SetActiveQuestId(GW::Constants::QuestID quest_id);
bool SetActiveQuest(Context::Quest* quest);
bool AbandonQuest(Context::Quest* quest);
bool AbandonQuestId(GW::Constants::QuestID quest_id);

Context::Quest* GetActiveQuest();
Context::QuestLog* GetQuestLog();
Context::Quest* GetQuest(GW::Constants::QuestID quest_id);

bool GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len);

bool RequestQuestInfo(const Context::Quest* quest, bool update_markers = false);
bool RequestQuestInfoId(GW::Constants::QuestID quest_id, bool update_markers = false);

void AsyncGetQuestName(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestDescription(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestObjectives(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestLocation(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestNPC(const Context::Quest* quest, std::wstring& res);
void AsyncDecodeAnyEncStr(const wchar_t* str, std::wstring& res);

extern RequestQuestInfoFn g_request_quest_info_func;
extern RequestQuestDataFn g_request_quest_data_func;
extern uintptr_t g_request_quest_data_callsite;
extern DoActionFn g_set_active_quest_func;
extern DoActionFn g_set_active_quest_ret;
extern DoActionFn g_abandon_quest_func;
extern DoActionFn g_abandon_quest_ret;
extern PY4GW::HookEntry g_set_active_quest_hook_entry;
extern PY4GW::HookEntry g_abandon_quest_hook_entry;
extern std::atomic<bool> g_initialized;

inline bool ResolveRequestQuestFunctions() {
    CrashContextScope context("startup", "quest", "resolve_request_quest_functions");
    const auto* anchor_pattern = PY4GW::Patterns::Get("quest.request_anchor");
    const auto* call_pattern = PY4GW::Patterns::Get("quest.request_data_call");
    if (!anchor_pattern || !call_pattern) {
        Logger::Instance().LogError("Missing or invalid quest request pattern.", "quest");
        return false;
    }

    const uintptr_t anchor = PY4GW::Scanner::Find(
        anchor_pattern->pattern.c_str(),
        anchor_pattern->mask.c_str(),
        anchor_pattern->offset,
        anchor_pattern->section);
    if (!Logger::AssertAddress("RequestQuest_Anchor", anchor, "quest")) {
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::FindInRange(
        call_pattern->pattern.c_str(),
        call_pattern->mask.c_str(),
        call_pattern->offset,
        anchor,
        anchor + anchor_pattern->range);
    if (!Logger::AssertAddress("RequestQuestData_Callsite", callsite, "quest")) {
        return false;
    }

    g_request_quest_data_callsite = callsite;
    g_request_quest_data_func = reinterpret_cast<RequestQuestDataFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    if (!Logger::AssertAddress(
            "RequestQuestData_Func",
            reinterpret_cast<uintptr_t>(g_request_quest_data_func),
            "quest")) {
        return false;
    }

    g_request_quest_info_func = reinterpret_cast<RequestQuestInfoFn>(
        PY4GW::Scanner::ToFunctionStart(anchor, 0xFF));
    return Logger::AssertAddress(
        "RequestQuestInfo_Func",
        reinterpret_cast<uintptr_t>(g_request_quest_info_func),
        "quest");
}

inline bool ResolveSetActiveQuestAndAbandon() {
    CrashContextScope context("startup", "quest", "resolve_set_active_quest_and_abandon");

    const auto* abandon_pattern = PY4GW::Patterns::Get("quest.abandon_quest_ref");
    if (!abandon_pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: quest.abandon_quest_ref", "quest");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::Find(
        abandon_pattern->pattern.c_str(),
        abandon_pattern->mask.c_str(),
        abandon_pattern->offset,
        abandon_pattern->section);
    if (!Logger::AssertAddress("AbandonQuest_Callsite", address, "quest")) {
        return false;
    }
    g_abandon_quest_func = reinterpret_cast<DoActionFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    if (!Logger::AssertAddress("AbandonQuest_Func", reinterpret_cast<uintptr_t>(g_abandon_quest_func), "quest")) {
        return false;
    }

    if (!g_request_quest_data_callsite) {
        Logger::Instance().LogError("Request quest data callsite must resolve before set active quest.", "quest");
        return false;
    }

    const auto* sig_pattern = PY4GW::Patterns::Get("quest.set_active_quest_sig");
    if (!sig_pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: quest.set_active_quest_sig", "quest");
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        sig_pattern->pattern.c_str(),
        sig_pattern->mask.c_str(),
        sig_pattern->offset,
        g_request_quest_data_callsite,
        g_request_quest_data_callsite - 0xFF);
    if (!Logger::AssertAddress("SetActiveQuest_Func", address, "quest")) {
        return false;
    }
    g_set_active_quest_func = reinterpret_cast<DoActionFn>(address);
    return true;
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

}  // namespace GW::quest

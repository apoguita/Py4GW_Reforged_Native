#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/game_pos.h"
#include "GW/context/agent.h"
#include "GW/context/npc.h"
#include "GW/context/player.h"
#include "GW/ui/ui.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace GW::agent {

using Agent = Context::Agent;
using AgentArray = Context::AgentArray;
using AgentGadget = Context::AgentGadget;
using AgentID = Context::AgentID;
using AgentItem = Context::AgentItem;
using AgentLiving = Context::AgentLiving;
using MapAgent = Context::MapAgent;
using MapAgentArray = Context::MapAgentArray;
using NPC = Context::NPC;
using NPCArray = Context::NPCArray;
using Player = Context::Player;
using PlayerArray = Context::PlayerArray;

using SendDialogFn = void(__cdecl*)(uint32_t dialog_id);
using ChangeTargetFn = void(__cdecl*)(uint32_t agent_id, uint32_t auto_target_id);
using CallTargetFn = void(__cdecl*)(Context::CallTargetType type, uint32_t agent_id);
using MoveToFn = void(__cdecl*)(float* pos);
using DoWorldActionFn = void(__cdecl*)(Context::WorldActionId action_id, uint32_t agent_id, bool suppress_call_target);

struct ChangeTargetUIMsg {
    uint32_t manual_target_id;
    uint32_t h0008;
    uint32_t auto_target_id;
    uint32_t h0010;
    uint32_t current_target_id;
    uint32_t h0018;
};

struct SendCallTargetPacket {
    Context::CallTargetType call_type;
    uint32_t agent_id;
};

struct SendChangeTargetPacket {
    uint32_t target_id;
    uint32_t auto_target_id;
};

struct SendWorldActionPacket {
    Context::WorldActionId action_id;
    AgentID agent_id;
    bool suppress_call_target;
};

bool Initialize();
void Shutdown();

bool SendDialog(uint32_t dialog_id);
bool GetIsAgentTargettable(const Agent* agent);

uint32_t GetObservingId();
uint32_t GetControlledCharacterId();
uint32_t GetTargetId();
uint32_t GetMouseoverId();

AgentArray* GetAgentArray();
Agent* GetAgentByID(uint32_t id);

inline Agent* GetObservingAgent() {
    return GetAgentByID(GetObservingId());
}

inline Agent* GetTarget() {
    return GetAgentByID(GetTargetId());
}

Agent* GetPlayerByID(uint32_t player_id);
AgentLiving* GetControlledCharacter();
bool IsObserving();
AgentLiving* GetTargetAsAgentLiving();

uint32_t GetAmountOfPlayersInInstance();

MapAgentArray* GetMapAgentArray();
MapAgent* GetMapAgentByID(uint32_t agent_id);

PlayerArray* GetPlayerArray();

NPCArray* GetNPCArray();
NPC* GetNPCByID(uint32_t npc_id);

bool ChangeTarget(const Agent* agent);
bool ChangeTarget(AgentID agent_id);

bool Move(float x, float y, uint32_t zplane = 0);
bool Move(GamePos pos);

bool InteractAgent(const Agent* agent, bool call_target = false);
bool CallTarget(uint32_t agent_id);

wchar_t* GetPlayerNameByLoginNumber(uint32_t login_number);
uint32_t GetAgentIdByLoginNumber(uint32_t login_number);
AgentID GetHeroAgentID(uint32_t hero_index);

wchar_t* GetAgentEncName(const Agent* agent);
wchar_t* GetAgentEncName(uint32_t agent_id);

bool AsyncGetAgentName(const Agent* agent, std::wstring& name);
bool AsyncDecodeStr(const wchar_t* enc_str, std::wstring& res);

extern SendDialogFn g_send_agent_dialog_func;
extern SendDialogFn g_send_agent_dialog_original;
extern SendDialogFn g_send_gadget_dialog_func;
extern SendDialogFn g_send_gadget_dialog_original;
extern ChangeTargetFn g_change_target_func;
extern ChangeTargetFn g_change_target_original;
extern CallTargetFn g_call_target_func;
extern CallTargetFn g_call_target_original;
extern MoveToFn g_move_to_func;
extern DoWorldActionFn g_do_world_action_func;
extern DoWorldActionFn g_do_world_action_original;
extern uintptr_t g_agent_array_addr;
extern uintptr_t g_player_agent_id_addr;
extern uint32_t g_dialog_agent_id;
extern uint32_t g_current_target_id;
extern PY4GW::HookEntry g_ui_message_entry;
extern const std::array<ui::UIMessage, 13> g_ui_messages_to_hook;
extern std::atomic<bool> g_initialized;

inline bool ResolveChangeTargetFunction() {
    CrashContextScope context("startup", "agent", "resolve_change_target");
    const auto* pattern = PY4GW::Patterns::Get("agent.change_target_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.change_target_anchor", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("ChangeTarget_Func", address, "agent")) {
        return false;
    }
    g_change_target_func = reinterpret_cast<ChangeTargetFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("ChangeTarget_FuncStart", reinterpret_cast<uintptr_t>(g_change_target_func), "agent");
}

inline bool ResolveAgentArrayAddress() {
    CrashContextScope context("startup", "agent", "resolve_agent_array");
    const auto* pattern = PY4GW::Patterns::Get("agent.agent_array_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.agent_array_ref", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("AgentArray_Ref", address, "agent")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogError("Agent array pointer is outside the expected data section.", "agent");
        return false;
    }
    g_agent_array_addr = *reinterpret_cast<uintptr_t*>(address);
    return Logger::AssertAddress("AgentArray_Addr", g_agent_array_addr, "agent");
}

inline bool ResolvePlayerAgentIdAddress() {
    CrashContextScope context("startup", "agent", "resolve_player_agent_id");
    const auto* pattern = PY4GW::Patterns::Get("agent.player_agent_id_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.player_agent_id_ref", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("PlayerAgentId_Ref", address, "agent")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogError("Player agent id pointer is outside the expected data section.", "agent");
        return false;
    }
    g_player_agent_id_addr = *reinterpret_cast<uintptr_t*>(address);
    return Logger::AssertAddress("PlayerAgentId_Addr", g_player_agent_id_addr, "agent");
}

inline bool ResolveSendAgentDialogFunction() {
    CrashContextScope context("startup", "agent", "resolve_send_agent_dialog");
    const auto* pattern = PY4GW::Patterns::Get("agent.send_dialog_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.send_dialog_callsite", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("SendDialog_Callsite", address, "agent")) {
        return false;
    }
    g_send_agent_dialog_func = reinterpret_cast<SendDialogFn>(PY4GW::Scanner::FunctionFromNearCall(address + 0x15));
    return Logger::AssertAddress("SendAgentDialog_Func", reinterpret_cast<uintptr_t>(g_send_agent_dialog_func), "agent");
}

inline bool ResolveSendGadgetDialogFunction() {
    CrashContextScope context("startup", "agent", "resolve_send_gadget_dialog");
    const auto* pattern = PY4GW::Patterns::Get("agent.send_dialog_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.send_dialog_callsite", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("SendDialog_Callsite", address, "agent")) {
        return false;
    }
    g_send_gadget_dialog_func = reinterpret_cast<SendDialogFn>(PY4GW::Scanner::FunctionFromNearCall(address + 0x25));
    return Logger::AssertAddress("SendGadgetDialog_Func", reinterpret_cast<uintptr_t>(g_send_gadget_dialog_func), "agent");
}

inline bool ResolveMoveToFunction() {
    CrashContextScope context("startup", "agent", "resolve_move_to");
    const auto* pattern = PY4GW::Patterns::Get("agent.move_to_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.move_to_callsite", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("MoveTo_Callsite", address, "agent")) {
        return false;
    }
    g_move_to_func = reinterpret_cast<MoveToFn>(PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("MoveTo_Func", reinterpret_cast<uintptr_t>(g_move_to_func), "agent");
}

inline bool ResolveDoWorldActionFunction() {
    CrashContextScope context("startup", "agent", "resolve_do_world_action");
    const auto* pattern = PY4GW::Patterns::Get("agent.do_world_action_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.do_world_action_anchor", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("DoWorldAction_Func", address, "agent")) {
        return false;
    }
    g_do_world_action_func = reinterpret_cast<DoWorldActionFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("DoWorldAction_FuncStart", reinterpret_cast<uintptr_t>(g_do_world_action_func), "agent");
}

inline bool ResolveCallTargetFunction() {
    CrashContextScope context("startup", "agent", "resolve_call_target");
    const auto* pattern = PY4GW::Patterns::Get("agent.call_target_sender");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: agent.call_target_sender", "agent");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("CallTarget_Scan", address, "agent")) {
        return false;
    }
    g_call_target_func = reinterpret_cast<CallTargetFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("CallTarget_Func", reinterpret_cast<uintptr_t>(g_call_target_func), "agent");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __cdecl OnSendAgentDialog(uint32_t dialog_id);
void __cdecl OnSendGadgetDialog(uint32_t dialog_id);
void __cdecl OnChangeTarget(uint32_t agent_id, uint32_t auto_target_id);
void __cdecl OnCallTarget(Context::CallTargetType type, uint32_t target_id);
void __cdecl OnDoWorldAction(Context::WorldActionId action_id, uint32_t agent_id, bool suppress_call_target);
void OnUIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void* lparam);

}  // namespace GW::agent

namespace GW {
namespace Agents = agent;
}

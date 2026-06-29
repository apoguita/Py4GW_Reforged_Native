#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/memory_patcher.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/game_thread/game_thread.h"
#include "GW/ui/ui.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <windows.h>

namespace GW::chat {

using Color = uint32_t;

inline constexpr Color MakeColorARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<Color>(a) << 24) | (static_cast<Color>(r) << 16) | (static_cast<Color>(g) << 8) | static_cast<Color>(b);
}
inline constexpr Color MakeColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    return MakeColorARGB(0xff, r, g, b);
}

static constexpr size_t CHAT_LOG_LENGTH = 0x200;

#pragma warning(push)
#pragma warning(disable: 4200)
struct ChatMessage {
    uint32_t channel;
    uint32_t unk1;
    FILETIME timestamp;
    wchar_t message[0];
};
#pragma warning(pop)

struct ChatBuffer {
    uint32_t next;
    uint32_t unk1;
    uint32_t unk2;
    ChatMessage* messages[CHAT_LOG_LENGTH];
};

enum Channel : int {
    CHANNEL_ALLIANCE = 0,
    CHANNEL_ALLIES = 1,
    CHANNEL_GWCA1 = 2,
    CHANNEL_ALL = 3,
    CHANNEL_GWCA2 = 4,
    CHANNEL_MODERATOR = 5,
    CHANNEL_EMOTE = 6,
    CHANNEL_WARNING = 7,
    CHANNEL_GWCA3 = 8,
    CHANNEL_GUILD = 9,
    CHANNEL_GLOBAL = 10,
    CHANNEL_GROUP = 11,
    CHANNEL_TRADE = 12,
    CHANNEL_ADVISORY = 13,
    CHANNEL_WHISPER = 14,
    CHANNEL_COUNT,
    CHANNEL_COMMAND,
    CHANNEL_UNKNOW = -1
};

using GetChannelColorFn = Color*(__cdecl*)(Color* color, Channel chan);
using SendChatFn = void(__cdecl*)(wchar_t* message, uint32_t agent_id);
using RecvWhisperFn = void(__cdecl*)(uint32_t transaction_id, wchar_t* player_name, wchar_t* message);
using StartWhisperFn = void(__fastcall*)(ui::Frame* ctx, uint32_t edx, wchar_t* name);
using AddToChatLogFn = void(__cdecl*)(wchar_t* message, uint32_t channel);
using PrintChatMessageFn = void(__fastcall*)(void* ctx, uint32_t edx, Channel channel, wchar_t* message, FILETIME timestamp, uint32_t is_reprint);
using ChatCommandCallback = void(__cdecl*)(PY4GW::HookStatus*, const wchar_t* cmd, int argc, const wchar_t** argv);

struct GetColorPacket {
    Color* color;
    Channel channel;
};

struct SendChatMessagePacket {
    wchar_t* message;
    uint32_t agent_id;
};

struct RecvWhisperPacket {
    uint32_t transaction_id;
    wchar_t* from;
    wchar_t* message;
};

struct StartWhisperPacket {
    wchar_t* player_name;
};

struct LogChatMessagePacket {
    wchar_t* message;
    Channel channel;
};

struct PrintChatMessagePacket {
    Channel channel;
    wchar_t* message;
    FILETIME timestamp;
    uint32_t is_reprint;
};

bool Initialize();
void Shutdown();

ChatBuffer* GetChatLog();
bool AddToChatLog(wchar_t* message, uint32_t channel);
bool GetIsTyping();

bool SendChat(char channel, const wchar_t* msg);
bool SendChat(char channel, const char* msg);
bool SendChat(const wchar_t* from, const wchar_t* msg);
bool SendChat(const char* from, const char* msg);

void WriteChatF(Channel channel, const wchar_t* format, ...);
void WriteChat(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);
void WriteChatEnc(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);

Color SetSenderColor(Channel chan, Color col);
Color SetMessageColor(Channel chan, Color col);
void GetChannelColors(Channel chan, Color* sender, Color* message);
void GetDefaultColors(Channel chan, Color* sender, Color* message);

Channel GetChannel(char opcode);
Channel GetChannel(wchar_t opcode);

void ToggleTimestamps(bool enable);
void SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds = false);
void SetTimestampsColor(Color color);

void CreateCommand(const wchar_t* cmd, ChatCommandCallback callback);
void DeleteCommand(const wchar_t* cmd);

void SendFakeChat(int channel, std::string message);
void SendFakeChatColored(int channel, std::string message, int r, int g, int b);
std::string FormatChatMessage(const std::string message, int r, int g, int b);

extern GetChannelColorFn g_get_sender_color_func;
extern GetChannelColorFn g_get_sender_color_original;
extern GetChannelColorFn g_get_message_color_func;
extern GetChannelColorFn g_get_message_color_original;
extern SendChatFn g_send_chat_func;
extern SendChatFn g_send_chat_original;
extern RecvWhisperFn g_recv_whisper_func;
extern RecvWhisperFn g_recv_whisper_original;
extern StartWhisperFn g_start_whisper_func;
extern StartWhisperFn g_start_whisper_original;
extern AddToChatLogFn g_add_to_chat_log_func;
extern AddToChatLogFn g_add_to_chat_log_original;
extern PrintChatMessageFn g_print_chat_message_func;
extern PrintChatMessageFn g_print_chat_message_original;
extern ChatBuffer** g_chat_buffer_addr;
extern uint32_t* g_is_typing_frame_id;
extern ui::UIInteractionCallback g_uicallback_chat_log_line_func;
extern ui::UIInteractionCallback g_uicallback_chat_log_line_original;
extern ui::UIInteractionCallback g_uicallback_assign_editable_text_func;
extern ui::UIInteractionCallback g_uicallback_assign_editable_text_original;
extern void* g_chat_window_context;
extern PY4GW::MemoryPatcher g_block_chat_timestamps;
extern std::map<Channel, Color> g_chat_sender_color;
extern std::map<Channel, Color> g_chat_message_color;
extern std::unordered_map<std::wstring, ChatCommandCallback> g_chat_command_hook_entries;
extern std::array<bool, static_cast<size_t>(Channel::CHANNEL_COUNT)> g_channel_that_parse_color_tag;
extern bool g_show_timestamps;
extern bool g_timestamp_24h_format;
extern bool g_timestamp_seconds;
extern Color g_timestamps_color;
extern const wchar_t* g_transient_chat_message;
extern PY4GW::HookEntry g_ui_message_entry;
extern const std::array<ui::UIMessage, 5> g_ui_messages_to_hook;
extern std::atomic<bool> g_initialized;

inline bool ResolveGetSenderColorFunction() {
    CrashContextScope context("startup", "chat", "resolve_get_sender_color");
    const auto* pattern = PY4GW::Patterns::Get("chat.get_sender_color_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.get_sender_color_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("GetSenderColor_Func", address, "chat")) {
        return false;
    }
    g_get_sender_color_func = reinterpret_cast<GetChannelColorFn>(address);
    return true;
}

inline bool ResolveGetMessageColorFunction() {
    CrashContextScope context("startup", "chat", "resolve_get_message_color");
    const auto* pattern = PY4GW::Patterns::Get("chat.get_message_color_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.get_message_color_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("GetMessageColor_Func", address, "chat")) {
        return false;
    }
    g_get_message_color_func = reinterpret_cast<GetChannelColorFn>(address);
    return true;
}

inline bool ResolveSendChatFunction() {
    CrashContextScope context("startup", "chat", "resolve_send_chat");
    const auto* pattern = PY4GW::Patterns::Get("chat.send_chat_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.send_chat_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("SendChat_Func", address, "chat")) {
        return false;
    }
    g_send_chat_func = reinterpret_cast<SendChatFn>(address);
    return true;
}

inline bool ResolveStartWhisperFunction() {
    CrashContextScope context("startup", "chat", "resolve_start_whisper");
    const auto* pattern = PY4GW::Patterns::Get("chat.start_whisper_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.start_whisper_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("StartWhisper_Func", address, "chat")) {
        return false;
    }
    g_start_whisper_func = reinterpret_cast<StartWhisperFn>(address);
    return true;
}

inline bool ResolveAddToChatLogFunction() {
    CrashContextScope context("startup", "chat", "resolve_add_to_chat_log");
    const auto* pattern = PY4GW::Patterns::Get("chat.add_to_chat_log_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.add_to_chat_log_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("AddToChatLog_Func", address, "chat")) {
        return false;
    }
    g_add_to_chat_log_func = reinterpret_cast<AddToChatLogFn>(address);
    return true;
}

inline bool ResolveChatBufferAddress() {
    CrashContextScope context("startup", "chat", "resolve_chat_buffer");
    const auto* pattern = PY4GW::Patterns::Get("chat.chat_buffer_addr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.chat_buffer_addr", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!address || !PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address), PY4GW::ScannerSection::Data)) {
        Logger::Instance().LogWarning("ChatBuffer_Addr not found", "chat");
        return false;
    }
    g_chat_buffer_addr = reinterpret_cast<ChatBuffer**>(*reinterpret_cast<uintptr_t*>(address));
    return Logger::AssertAddress("ChatBuffer_Addr", reinterpret_cast<uintptr_t>(g_chat_buffer_addr), "chat");
}

inline bool ResolveRecvWhisperFunction() {
    CrashContextScope context("startup", "chat", "resolve_recv_whisper");
    const auto* pattern = PY4GW::Patterns::Get("chat.recv_whisper_func");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.recv_whisper_func", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section));
    if (!Logger::AssertAddress("RecvWhisper_Func", address, "chat")) {
        return false;
    }
    g_recv_whisper_func = reinterpret_cast<RecvWhisperFn>(address);
    return true;
}

inline bool ResolvePrintChatMessageFunction() {
    CrashContextScope context("startup", "chat", "resolve_print_chat_message");
    const auto* pattern = PY4GW::Patterns::Get("chat.print_chat_message_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.print_chat_message_anchor", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address) {
        Logger::Instance().LogWarning("PrintChatMessage_Anchor not found", "chat");
        return false;
    }
    g_print_chat_message_func = reinterpret_cast<PrintChatMessageFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("PrintChatMessage_Func", reinterpret_cast<uintptr_t>(g_print_chat_message_func), "chat");
}

inline bool ResolveIsTypingFrameId() {
    CrashContextScope context("startup", "chat", "resolve_is_typing_frame_id");
    const auto* assertion_pattern = PY4GW::Patterns::Get("chat.is_typing_assertion");
    if (!assertion_pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.is_typing_assertion", "chat");
        return false;
    }
    uintptr_t address = PY4GW::Scanner::FindAssertion(
        assertion_pattern->assertion_file.c_str(),
        assertion_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(assertion_pattern->line_number),
        assertion_pattern->offset);
    if (address) {
        const auto* sub_pattern = PY4GW::Patterns::Get("chat.is_typing_frame_id_sub");
        if (sub_pattern) {
            address = PY4GW::Scanner::FindInRange(
                sub_pattern->pattern.c_str(),
                sub_pattern->mask.c_str(),
                sub_pattern->offset,
                address,
                address + 0x40);
        }
    }
    if (!address || !PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogWarning("IsTyping_FrameId not found", "chat");
        return false;
    }
    g_is_typing_frame_id = *reinterpret_cast<uint32_t**>(address);
    return Logger::AssertAddress("IsTyping_FrameId", reinterpret_cast<uintptr_t>(g_is_typing_frame_id), "chat");
}

inline bool ResolveUICallbackAssignEditableText() {
    CrashContextScope context("startup", "chat", "resolve_uicallback_assign_editable_text");
    const auto* pattern = PY4GW::Patterns::Get("chat.uicallback_assign_editable_text");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.uicallback_assign_editable_text", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::ToFunctionStart(
        PY4GW::Scanner::FindUseOfString(pattern->pattern.c_str(), 0));
    if (!Logger::AssertAddress("UICallback_AssignEditableText_Func", address, "chat")) {
        return false;
    }
    g_uicallback_assign_editable_text_func = reinterpret_cast<ui::UIInteractionCallback>(address);
    return true;
}

inline bool ResolveUICallbackChatLogLine() {
    CrashContextScope context("startup", "chat", "resolve_uicallback_chat_log_line");
    const auto* pattern = PY4GW::Patterns::Get("chat.uicallback_chat_log_assertion");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.uicallback_chat_log_assertion", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!address || !PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address), PY4GW::ScannerSection::Text)) {
        Logger::Instance().LogWarning("UICallback_ChatLogLine not found", "chat");
        return false;
    }
    g_uicallback_chat_log_line_func = reinterpret_cast<ui::UIInteractionCallback>(*reinterpret_cast<uintptr_t*>(address));
    return Logger::AssertAddress("UICallback_ChatLogLine_Func", reinterpret_cast<uintptr_t>(g_uicallback_chat_log_line_func), "chat");
}

inline bool ResolveChatTimestampsPatch() {
    CrashContextScope context("startup", "chat", "resolve_chat_timestamps_patch");
    const auto* pattern = PY4GW::Patterns::Get("chat.chat_timestamps_patch");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: chat.chat_timestamps_patch", "chat");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (address) {
        const uint8_t nop_pair[] = { 0x90, 0x90 };
        g_block_chat_timestamps.SetPatch(address, nop_pair, sizeof(nop_pair));
    }
    if (!g_block_chat_timestamps.IsValid()) {
        Logger::Instance().LogError("Failed to patch chat timestamps, address not found.", "chat");
        return false;
    }
    return true;
}

void ForceRedrawChatLog();

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

Color* __cdecl OnGetSenderColor(Color* color, Channel chan);
Color* __cdecl OnGetMessageColor(Color* color, Channel chan);
void __cdecl OnSendChat(wchar_t* message, uint32_t agent_id);
void __cdecl OnRecvWhisper(uint32_t transaction_id, wchar_t* player_name, wchar_t* message);
void __fastcall OnStartWhisper(ui::Frame* ctx, uint32_t edx, wchar_t* name);
void __cdecl OnAddToChatLog(wchar_t* message, uint32_t channel);
void __fastcall OnPrintChatMessage(void* ctx, uint32_t edx, Channel channel, wchar_t* message, FILETIME timestamp, uint32_t is_reprint);
void __cdecl OnUICallback_ChatLogLine(ui::InteractionMessage* message, void* wParam, void* lParam);
void __cdecl OnUICallback_AssignEditableText(ui::InteractionMessage* message, void* wParam, void* lParam);
void OnUIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void* lparam);

}  // namespace GW::chat

namespace GW {
namespace Chat = chat;
}

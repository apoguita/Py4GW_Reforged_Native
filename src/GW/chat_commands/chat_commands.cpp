#include "base/error_handling.h"

#include "GW/chat_commands/chat_commands.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "GW/chat/chat.h"
#include "GW/common/constants/chat.h"
#include "GW/context/ui.h"
#include "GW/ui/ui.h"

#include <cwctype>

namespace py = pybind11;

namespace GW::chat_commands {

namespace {
    PY4GW::HookEntry g_ui_message_entry;
    bool g_installed = false;

    std::wstring ToLower(std::wstring s) {
        for (wchar_t& c : s) c = static_cast<wchar_t>(std::towlower(c));
        return s;
    }

    // Split a command line into (command, args, raw). Plain whitespace tokenisation
    // only — C++ is the lexer, Python is the parser. `raw` is the untouched tail
    // after the command word, the escape hatch for handlers that want to parse
    // quoting / key=value themselves. `line` is the message WITHOUT the leading '/'.
    void Tokenize(const std::wstring& line, std::wstring* command,
                  std::vector<std::wstring>* args, std::wstring* raw) {
        size_t i = 0;
        const size_t n = line.size();
        auto skip_spaces = [&] { while (i < n && std::iswspace(line[i])) ++i; };

        skip_spaces();
        const size_t cmd_begin = i;
        while (i < n && !std::iswspace(line[i])) ++i;
        *command = ToLower(line.substr(cmd_begin, i - cmd_begin));

        skip_spaces();
        *raw = line.substr(i);   // everything after the command word (trimmed left)

        while (i < n) {
            const size_t arg_begin = i;
            while (i < n && !std::iswspace(line[i])) ++i;
            args->push_back(line.substr(arg_begin, i - arg_begin));
            skip_spaces();
        }
    }

    // The kSendChatMessage UI-message callback (game thread). Signature matches
    // ui::UIMessageCallback = HookCallback<UIMessage, void*, void*>.
    void OnSendChatMessage(PY4GW::HookStatus* status, ui::UIMessage, void* wparam, void*) {
        if (!status || status->blocked)
            return;
        const auto* packet = static_cast<ui::packet::kSendChatMessage*>(wparam);
        if (!packet || !packet->message || !packet->message[0])
            return;
        // Only intercept the COMMAND channel ('/'); everything else is real chat and
        // is left for the game (this is what keeps /age, /stuck, /resign, ... working).
        if (GW::chat::GetChannel(packet->message[0]) != GW::chat::CHANNEL_COMMAND)
            return;
        if (ChatCommands::Instance().Dispatch(packet->message))
            status->blocked = true;   // swallow: the game never sees an unknown "/name"
    }
}  // namespace

bool Initialize() {
    if (g_installed)
        return true;
    CrashContextScope context("startup", "chat_commands", "install_ui_hook");
    // NEGATIVE altitude so we run in the PRE-game callback group: the dispatcher only honors
    // status->blocked for altitude<=0 callbacks (it checks blocked *before* forwarding), and the
    // chat module forwards the line to the game from its own altitude 0x1 handler gated on
    // !blocked. So we must run before it and set blocked to "eat" a registered command.
    ui::RegisterUIMessageCallback(&g_ui_message_entry, ui::UIMessage::kSendChatMessage,
                                  &OnSendChatMessage, -0x4000);
    g_installed = true;
    return true;
}

void Shutdown() {
    if (!g_installed)
        return;
    ui::RemoveUIMessageCallback(&g_ui_message_entry);
    g_installed = false;
    ChatCommands::Instance().Clear();
}

ChatCommands& ChatCommands::Instance() {
    static ChatCommands instance;
    return instance;
}

void ChatCommands::Register(const std::wstring& name, py::function fn) {
    // Called from Python => GIL already held. Lock only to guard the map structure.
    std::lock_guard<std::mutex> lock(mutex_);
    commands_[ToLower(name)] = std::move(fn);
}

bool ChatCommands::Unregister(const std::wstring& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.erase(ToLower(name)) != 0;
}

void ChatCommands::Clear() {
    // Destroying py::function values needs the GIL. Clear is reached both from Python
    // (GIL already held) and from Shutdown on the game thread (no GIL), so acquire it
    // here — gil_scoped_acquire is safe to nest when already held. GIL-then-mutex order
    // matches Register/Dispatch, so no deadlock.
    py::gil_scoped_acquire gil;
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.clear();
}

bool ChatCommands::IsRegistered(const std::wstring& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.find(ToLower(name)) != commands_.end();
}

std::vector<std::wstring> ChatCommands::List() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::wstring> out;
    out.reserve(commands_.size());
    for (const auto& entry : commands_)
        out.push_back(entry.first);
    return out;
}

bool ChatCommands::Dispatch(const std::wstring& message) {
    // message includes the leading '/'. Tokenise BEFORE taking the GIL (pure string work).
    std::wstring command, raw;
    std::vector<std::wstring> args;
    Tokenize(message.substr(1), &command, &args, &raw);
    if (command.empty())
        return false;

    // Reentrancy guard: a handler that itself sends "/cmd" would re-enter this hook.
    static thread_local bool in_dispatch = false;
    if (in_dispatch)
        return false;

    // Cross the C++ -> Python boundary from the game thread: acquire the GIL.
    py::gil_scoped_acquire gil;

    py::function fn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = commands_.find(command);
        if (it == commands_.end())
            return false;         // not ours -> caller lets the game handle it
        fn = it->second;          // copy out (GIL held) so we can call outside the lock
    }

    in_dispatch = true;
    try {
        fn(args, raw);            // handler(args: list[str], raw: str)
    }
    catch (const py::error_already_set& e) {
        std::string narrow;       // explicit wchar->char narrowing (command names are ASCII)
        narrow.reserve(command.size());
        for (const wchar_t c : command)
            narrow.push_back(static_cast<char>(c));
        Logger::Instance().LogError("chat command '" + narrow + "' raised: " + e.what(), std::string("chat_commands"));
    }
    catch (...) {
        Logger::Instance().LogError(std::string("chat command handler threw a non-Python exception"),
                                    std::string("chat_commands"));
    }
    in_dispatch = false;
    return true;
}

}  // namespace GW::chat_commands

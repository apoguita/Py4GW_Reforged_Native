#pragma once

#include "base/error_handling.h"

#include "base/py_bindings.h"   // pybind11 (function/gil) via the shared prelude

#include <map>
#include <mutex>
#include <string>
#include <vector>

// ChatCommands — a DYNAMIC, runtime chat-command registry.
// ----------------------------------------------------------------------------
// GWCA/Toolbox register their commands at COMPILE time (a fixed table baked into
// the DLL). We instead follow the PyCallback principle: Python hands us a callable
// at RUNTIME keyed by a command name, and we store it (`pybind11::function`) and
// invoke it when the user types "/<name> args" — exactly like PyCallback stores a
// Task's fn and calls it, only the trigger is a command name and the call carries
// the parsed arguments.
//
// Mechanism: we register a callback on the `kSendChatMessage` UI message (the same
// hook GWCA's ChatMgr uses). When a sent line is on the COMMAND channel ('/'), we
// tokenize it, look the command up in the registry, invoke the Python fn with the
// arguments, and set `status->blocked` so the game never sees "/<name>". Unknown
// commands are left alone, so GW's own slash-commands (/age, /stuck, /resign, ...)
// pass straight through.

namespace GW::chat_commands {

bool Initialize();   // install the kSendChatMessage UI-message callback
void Shutdown();     // remove it and drop the registry

class ChatCommands {
public:
    static ChatCommands& Instance();

    // ── dynamic registry (called from Python; GIL held) ──────────────────────
    // `name` is matched case-insensitively (stored lowercased). Re-registering a
    // name replaces its handler. `fn` is called as fn(args: list[str], raw: str).
    void Register(const std::wstring& name, pybind11::function fn);
    bool Unregister(const std::wstring& name);
    void Clear();
    bool IsRegistered(const std::wstring& name) const;
    std::vector<std::wstring> List() const;

    // ── dispatch (called from the UI hook, on the GAME thread) ───────────────
    // `message` is the full sent line INCLUDING the leading '/'. Returns true if a
    // registered command matched and its handler ran (caller then blocks the game
    // command). Acquires the GIL itself; safe to call with no GIL held.
    bool Dispatch(const std::wstring& message);

private:
    ChatCommands() = default;
    ChatCommands(const ChatCommands&) = delete;
    ChatCommands& operator=(const ChatCommands&) = delete;

    mutable std::mutex mutex_;
    // lowercased command name -> Python callable. Copying/erasing a value needs the
    // GIL, so every mutator/Dispatch holds the GIL before touching this map (and
    // always in GIL-then-mutex order, so it can't deadlock against Register).
    std::map<std::wstring, pybind11::function> commands_;
};

}  // namespace GW::chat_commands

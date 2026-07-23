#include "base/py_bindings.h"

#include "GW/chat_commands/chat_commands.h"

namespace py = pybind11;

using GW::chat_commands::ChatCommands;

PYBIND11_EMBEDDED_MODULE(PyChatCommands, m) {
    m.doc() =
        "Dynamic chat-command registry (runtime, not compile-time). Register a Python callable\n"
        "under a command name; typing '/<name> args' in chat calls it as fn(args: list[str], raw: str)\n"
        "on the game thread, and the '/<name>' is swallowed so the game never errors on it. Same\n"
        "principle as PyCallback (native holds the callable and invokes it) — the trigger is a command\n"
        "name and the call carries the parsed args. Names are case-insensitive. Unknown commands pass\n"
        "through to the game (so /age, /stuck, /resign, ... still work). Aliases = register the same fn\n"
        "under more than one name.";

    m.def("register",
        [](const std::wstring& name, py::function fn) { ChatCommands::Instance().Register(name, std::move(fn)); },
        py::arg("name"), py::arg("fn"),
        "Register (or replace) the handler for '/<name>'. fn(args: list[str], raw: str).");
    m.def("unregister",
        [](const std::wstring& name) { return ChatCommands::Instance().Unregister(name); }, py::arg("name"),
        "Remove a command. Returns True if it existed.");
    m.def("clear", []() { ChatCommands::Instance().Clear(); }, "Drop every registered command.");
    m.def("is_registered",
        [](const std::wstring& name) { return ChatCommands::Instance().IsRegistered(name); }, py::arg("name"));
    m.def("list", []() { return ChatCommands::Instance().List(); },
        "List the registered command names (lowercased).");
}

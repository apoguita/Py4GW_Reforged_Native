#include "base/py_bindings.h"

#include "GW/shared_memory/manager.h"
#include "base/memory_manager.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "system/system.h"

#include <cstdint>
#include <string>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PySystem, m) {
    m.doc() = "Py4GW system services bindings";

    py::enum_<MessageType>(m, "MessageType")
        .value("Info", MessageType::Info)
        .value("Warning", MessageType::Warning)
        .value("Error", MessageType::Error)
        .value("Debug", MessageType::Debug)
        .value("Success", MessageType::Success)
        .value("Performance", MessageType::Performance)
        .value("Notice", MessageType::Notice)
        .value("Hook", MessageType::Hook);

    py::class_<PY4GW::ConsoleMessage>(m, "ConsoleMessage")
        .def_readonly("timestamp", &PY4GW::ConsoleMessage::timestamp)
        .def_readonly("display_timestamp", &PY4GW::ConsoleMessage::display_timestamp)
        .def_readonly("module_name", &PY4GW::ConsoleMessage::module_name)
        .def_readonly("level", &PY4GW::ConsoleMessage::level)
        .def_readonly("message_type", &PY4GW::ConsoleMessage::message_type)
        .def_readonly("message", &PY4GW::ConsoleMessage::message)
        .def("__repr__", [](const PY4GW::ConsoleMessage& entry) {
            return "[" + entry.display_timestamp + "] [" + entry.module_name + "] [" + entry.level + "] " + entry.message;
        });

    m.def("get_tick_count64", &PY4GW::System::GetTickCount64, "Get the frame timestamp tick count as a 64-bit integer");
    m.def("get_shared_memory_name", []() -> std::wstring {
        return std::wstring(GW::shared_memory::RuntimeManager().Name());
    }, "Get the current per-process runtime shared-memory name");
    m.def("get_credits", &PY4GW::System::GetCredits, "Get the credits for the Py4GW library");
    m.def("get_license", &PY4GW::System::GetLicense, "Get the license for the Py4GW library");
    m.def("change_working_directory", &PY4GW::System::ChangeWorkingDirectory, "Change the current working directory", py::arg("path"));
    m.def("request_shutdown_prompt", []() {
        PY4GW::System::Instance().RequestShutdownPrompt();
    }, "Open the Py4GW shutdown confirmation modal");
    m.def("cancel_shutdown_prompt", []() {
        PY4GW::System::Instance().CancelShutdownPrompt();
    }, "Dismiss a pending shutdown confirmation modal");
    m.def("is_shutdown_prompt_pending", []() {
        return PY4GW::System::Instance().IsShutdownPromptPending();
    }, "Check whether the shutdown confirmation modal is pending");
    m.def("in_character_select_screen", &PY4GW::System::InCharacterSelectScreen,
        "Check if the character select screen is ready");
    m.def("has_account_email", []() {
        return PY4GW::System::Instance().HasAccountEmail();
    }, "Check whether the account anchor email has been resolved");
    m.def("get_account_email", []() {
        return PY4GW::System::Instance().GetAccountEmail();
    }, "Get the account email used as the persistence anchor (empty until resolved)");
    m.def("get_settings_directory", []() -> std::string {
        return PY4GW::System::Instance().GetSettingsDirectory().string();
    }, "Get the per-account settings directory (empty until the anchor is resolved)");

    py::module_ console = m.def_submodule("Console", "Authoritative script-facing console: logging, retrieval, and window control");
    // Legacy-shaped surface the Python library calls (Py4GW.Console is retired in favor of this).
    console.attr("MessageType") = m.attr("MessageType");
    console.def("Log", [](const std::string& sender, const std::string& message, MessageType message_type) {
        PY4GW::System::Instance().WriteConsoleMessage(sender, message_type, message);
    }, "Write a message to the console", py::arg("sender"), py::arg("message"), py::arg("message_type") = MessageType::Info);
    console.def("get_projects_path", []() -> std::string {
        return PY4GW::process_manager::GetModuleDirectory().string();
    }, "Get the path where Py4GW.dll is located");
    console.def("get_gw_window_handle", []() -> uintptr_t {
        return reinterpret_cast<uintptr_t>(PY4GW::MemoryManager::GetGWWindowHandle());
    }, "Get the Guild Wars window handle as an integer");
    console.def("write", [](const std::string& module_name, const std::string& message, const std::string& level) {
        PY4GW::System::Instance().WriteConsoleMessage(module_name, level, message);
    }, "Write a message to the console", py::arg("module_name"), py::arg("message"), py::arg("level") = "INFO");
    console.def("write", [](const std::string& module_name, const std::string& message, MessageType message_type) {
        PY4GW::System::Instance().WriteConsoleMessage(module_name, message_type, message);
    }, "Write a message to the console", py::arg("module_name"), py::arg("message"), py::arg("message_type"));
    console.def("get_messages", []() {
        return PY4GW::System::Instance().GetConsoleMessages();
    }, "Get all buffered console messages");
    console.def("get_messages", [](MessageType message_type) {
        return PY4GW::System::Instance().GetConsoleMessages(message_type);
    }, "Get buffered console messages of a given type", py::arg("message_type"));
    console.def("filter_messages", [](const std::string& module_name, const std::string& level, const std::string& contains) {
        return PY4GW::System::Instance().FilterConsoleMessages(module_name, level, contains);
    }, "Filter buffered console messages by module, level, and substring",
        py::arg("module_name") = "", py::arg("level") = "", py::arg("contains") = "");
    console.def("clear_messages", []() {
        PY4GW::System::Instance().ClearConsoleMessages();
    }, "Clear the console message buffer");
    console.def("set_output_to_file", [](bool enabled) {
        PY4GW::System::Instance().SetOutputToFile(enabled);
    }, "Mirror console messages into the injection log file", py::arg("enabled"));
    console.def("get_output_to_file", []() {
        return PY4GW::System::Instance().GetOutputToFile();
    }, "Check whether console messages are mirrored to the log file");
    console.def("set_draw_console", [](bool enabled) {
        PY4GW::System::Instance().SetDrawConsole(enabled);
    }, "Show or hide the full console window", py::arg("enabled"));
    console.def("get_draw_console", []() {
        return PY4GW::System::Instance().GetDrawConsole();
    }, "Check whether the full console window is shown");
    console.def("set_draw_compact_console", [](bool enabled) {
        PY4GW::System::Instance().SetDrawCompactConsole(enabled);
    }, "Show or hide the compact console window", py::arg("enabled"));
    console.def("get_draw_compact_console", []() {
        return PY4GW::System::Instance().GetDrawCompactConsole();
    }, "Check whether the compact console window is shown");
    console.def("toggle_console", []() {
        PY4GW::System::Instance().ToggleConsole();
    }, "Toggle the full console window");
    console.def("toggle_compact_console", []() {
        PY4GW::System::Instance().ToggleCompactConsole();
    }, "Toggle the compact console window");

    py::module_ environment = m.def_submodule("environment", "Process environment queries");
    environment.def("get_gw_window_handle", []() -> uintptr_t {
        return reinterpret_cast<uintptr_t>(PY4GW::MemoryManager::GetGWWindowHandle());
    }, "Get the Guild Wars window handle as an integer");
    environment.def("get_projects_path", []() -> std::string {
        return PY4GW::process_manager::GetModuleDirectory().string();
    }, "Get the path where Py4GW.dll is located");

    py::module_ window = m.def_submodule("window", "Guild Wars window control");
    window.def("resize_window", &PY4GW::WindowCfg::ResizeWindow, "Resize the Guild Wars window",
        py::arg("width"), py::arg("height"));
    window.def("move_window_to", &PY4GW::WindowCfg::MoveWindowTo, "Move the Guild Wars window to (x, y)",
        py::arg("x"), py::arg("y"));
    window.def("set_window_geometry", &PY4GW::WindowCfg::SetWindowGeometry, "Set the Guild Wars window geometry (x, y, width, height)",
        py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"));
    window.def("get_window_rect", &PY4GW::WindowCfg::GetWindowRectFn, "Get the Guild Wars window rectangle (left, top, right, bottom)");
    window.def("get_client_rect", &PY4GW::WindowCfg::GetClientRectFn, "Get the Guild Wars client rectangle (left, top, right, bottom)");
    window.def("set_window_active", &PY4GW::WindowCfg::SetWindowActive, "Set the Guild Wars window as active (focused)");
    window.def("set_window_title", [](const std::wstring& title) {
        PY4GW::WindowCfg::SetWindowTitle(title);
    }, py::arg("title"));
    window.def("is_window_active", &PY4GW::WindowCfg::IsWindowActive, "Check if the Guild Wars window is active (focused)");
    window.def("is_window_minimized", &PY4GW::WindowCfg::IsWindowMinimized, "Check if the Guild Wars window is minimized");
    window.def("is_window_in_background", &PY4GW::WindowCfg::IsWindowInBackground, "Check if the Guild Wars window is in the background");
    window.def("set_borderless", &PY4GW::WindowCfg::SetBorderless, "Enable or disable borderless window mode",
        py::arg("enable"));
    window.def("set_always_on_top", &PY4GW::WindowCfg::SetAlwaysOnTop, "Set or unset always-on-top",
        py::arg("enable"));
    window.def("flash_window", &PY4GW::WindowCfg::Flash_Window, "Flash the Guild Wars taskbar button",
        py::arg("repeat_count") = 1);
    window.def("request_attention", &PY4GW::WindowCfg::RequestAttention, "Keep flashing until the window comes to foreground");
    window.def("get_z_order", &PY4GW::WindowCfg::GetZOrder, "Get the Z-order index of the Guild Wars window");
    window.def("set_z_order", &PY4GW::WindowCfg::SetZOrder, "Set the Z-order of the Guild Wars window relative to another window",
        py::arg("insert_after") = 0);
    window.def("send_window_to_back", &PY4GW::WindowCfg::SendWindowToBack, "Send the Guild Wars window to the bottom of the Z-order stack");
    window.def("bring_window_to_front", &PY4GW::WindowCfg::BringWindowToFront, "Bring the Guild Wars window to the front of the Z-order stack");
    window.def("transparent_click_through", &PY4GW::WindowCfg::TransparentClickThrough, "Make the Guild Wars window click-through",
        py::arg("enable"));
    window.def("adjust_window_opacity", &PY4GW::WindowCfg::AdjustWindowOpacity, "Adjust the Guild Wars window opacity (0-255)",
        py::arg("alpha"));
    window.def("hide_window", &PY4GW::WindowCfg::HideWindow, "Hide the Guild Wars window");
    window.def("show_window", &PY4GW::WindowCfg::ShowWindowAgain, "Show the Guild Wars window if hidden");

    py::module_ script_control = m.def_submodule("script_control", "Python script lifecycle control");
    script_control.def("load", [](const std::string& path) {
        PY4GW::python_runtime::SetSelectedScriptPath(path);
        return PY4GW::python_runtime::LoadSelectedScript();
    }, "Load a Python script from path", py::arg("path"));
    script_control.def("run", &PY4GW::python_runtime::RunScript, "Run the currently loaded script");
    script_control.def("stop", &PY4GW::python_runtime::StopScript, "Stop the currently running script");
    script_control.def("pause", &PY4GW::python_runtime::PauseScript, "Pause the running script");
    script_control.def("resume", &PY4GW::python_runtime::ResumeScript, "Resume the paused script");
    script_control.def("status", &PY4GW::python_runtime::GetScriptStatus, "Get current script status");
    script_control.def("defer_load_and_run", &PY4GW::python_runtime::DeferLoadAndRun, "Stop current if needed, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);
    script_control.def("defer_stop_load_and_run", &PY4GW::python_runtime::DeferStopLoadAndRun, "Force stop, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);
    script_control.def("defer_stop_and_run", &PY4GW::python_runtime::DeferStopAndRun, "Stop current script, then rerun it after delay (ms)",
        py::arg("delay_ms") = 1000);

    py::module_ widget_manager = m.def_submodule("widget_manager", "Always-on widget manager script host");
    widget_manager.def("start", &PY4GW::python_runtime::StartWidgetManager, "Load and run the widget manager script");
    widget_manager.def("stop", &PY4GW::python_runtime::StopWidgetManager, "Stop the widget manager script");
    widget_manager.def("status", &PY4GW::python_runtime::GetWidgetManagerStatus, "Get the widget manager run status");
}

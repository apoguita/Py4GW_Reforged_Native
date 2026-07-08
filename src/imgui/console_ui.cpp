#include "base/error_handling.h"

#include "imgui/console_ui.h"

#include "IconsFontAwesome5.h"
#include "base/logger.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "settings/settings.h"
#include "system/system.h"

#include <imgui.h>
#include <ImGuiFileBrowser.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace PY4GW::imgui::console_ui {

namespace {

constexpr char kScriptBrowserPopup[] = "Select Python Script";
constexpr ImVec2 kScriptBrowserSize = ImVec2(700.0f, 450.0f);

ImGuiTextFilter g_log_filter;
std::vector<std::string> g_command_history;
int g_history_pos = -1;
char g_command_buffer[256] = {};
char g_path_buffer[512] = {};
bool g_auto_scroll = true;
bool g_auto_scroll_loaded = false;

ImVec2 console_pos = ImVec2(5, 30);
ImVec2 console_size = ImVec2(800, 700);
bool console_collapsed = false;

struct ScriptBrowserState {
    imgui_addons::ImGuiFileBrowser browser;
    bool open_requested = false;
};

void ShowTooltipInternal(const char* tooltip_text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltip_text);
        ImGui::EndTooltip();
    }
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory) {
        return 0;
    }

    const int previous_history_pos = g_history_pos;
    if (data->EventKey == ImGuiKey_UpArrow) {
        if (g_history_pos == -1) {
            g_history_pos = static_cast<int>(g_command_history.size()) - 1;
        } else if (g_history_pos > 0) {
            --g_history_pos;
        }
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (g_history_pos != -1 && ++g_history_pos >= static_cast<int>(g_command_history.size())) {
            g_history_pos = -1;
        }
    }

    if (previous_history_pos != g_history_pos) {
        const char* history = (g_history_pos >= 0) ? g_command_history[g_history_pos].c_str() : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, history);
    }
    return 0;
}

ImVec4 MessageTypeColor(MessageType message_type) {
    switch (message_type) {
    case MessageType::Error:
        return {1.0f, 0.0f, 0.0f, 1.0f};
    case MessageType::Warning:
        return {1.0f, 1.0f, 0.0f, 1.0f};
    case MessageType::Success:
        return {0.0f, 1.0f, 0.0f, 1.0f};
    case MessageType::Debug:
    case MessageType::Hook:
        return {0.0f, 1.0f, 1.0f, 1.0f};
    case MessageType::Performance:
        return {1.0f, 0.6f, 0.0f, 1.0f};
    case MessageType::Notice:
        return {0.6f, 1.0f, 0.6f, 1.0f};
    case MessageType::Info:
    default:
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

ScriptBrowserState& ScriptBrowser() {
    static std::unique_ptr<ScriptBrowserState> state;
    if (!state) {
        const auto root = process_manager::GetModuleDirectory();
        state = std::make_unique<ScriptBrowserState>();
        state->browser.SetUseModal(true);
        state->browser.SetCurrentPath((root.empty() ? std::filesystem::path(L"C:\\") : root).string());
    }
    return *state;
}

void RequestOpenScriptBrowser() {
    ScriptBrowser().open_requested = true;
}

void RenderScriptBrowser() {
    auto& state = ScriptBrowser();
    if (state.open_requested) {
        ImGui::OpenPopup(kScriptBrowserPopup);
        state.open_requested = false;
    }

    if (state.browser.showFileDialog(kScriptBrowserPopup, imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, kScriptBrowserSize, ".py")) {
        python_runtime::SetSelectedScriptPath(state.browser.selected_path);
        System::Instance().WriteConsoleMessage("Py4GW", MessageType::Info, "Selected script: " + python_runtime::GetSelectedScriptPath());
    }
}

void SyncPathBuffer() {
    const std::string& path = python_runtime::GetSelectedScriptPath();
    strncpy_s(g_path_buffer, sizeof(g_path_buffer), path.c_str(), _TRUNCATE);
}

void CopyLogToClipboard() {
    const auto entries = System::Instance().GetConsoleMessages();
    std::string text;
    for (const auto& entry : entries) {
        text += "[" + entry.timestamp + "] [" + entry.module_name + "] " + entry.message + "\n";
    }
    ImGui::SetClipboardText(text.c_str());
}

void SaveLogToFile() {
    const std::string save_path = System::SaveFileDialog();
    if (save_path.empty()) {
        return;
    }
    std::ofstream output(save_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        System::Instance().WriteConsoleMessage("Py4GW", MessageType::Error, "Failed to open file for writing: " + save_path);
        return;
    }

    const auto entries = System::Instance().GetConsoleMessages();
    for (const auto& entry : entries) {
        output << "[" << entry.timestamp << "] [" << entry.module_name << "] " << entry.message << "\n";
    }
    System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Console log saved to " + save_path);
}

void RenderControls(bool* show_console, bool* show_compact_console) {
    SyncPathBuffer();

    if (ImGui::BeginTable("ScriptOptionsTable", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (python_runtime::GetScriptState() == python_runtime::ScriptState::Running) {
            ImGui::BeginDisabled();
        }
        if (ImGui::InputText("##Path", g_path_buffer, IM_ARRAYSIZE(g_path_buffer))) {
            python_runtime::SetSelectedScriptPath(g_path_buffer);
        }

        if (python_runtime::GetScriptState() == python_runtime::ScriptState::Running) {
            ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(1);
        if (python_runtime::GetScriptState() == python_runtime::ScriptState::Running) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Open")) {
            RequestOpenScriptBrowser();
        }
        if (python_runtime::GetScriptState() == python_runtime::ScriptState::Running) {
            ImGui::EndDisabled();
        }
        ShowTooltipInternal("Select a Python script");

        ImGui::TableSetColumnIndex(2);
        if (g_path_buffer[0] != '\0') {
            const auto state = python_runtime::GetScriptState();
            if (state == python_runtime::ScriptState::Stopped) {
                if (ImGui::Button(ICON_FA_PLAY "##Load & Run")) {
                    python_runtime::StartSelectedScript();
                }
                ShowTooltipInternal("Load and run script");
            } else if (state == python_runtime::ScriptState::Running) {
                if (ImGui::Button(ICON_FA_PAUSE "##Pause")) {
                    python_runtime::PauseScript();
                }
                ShowTooltipInternal("Pause execution");
            } else if (state == python_runtime::ScriptState::Paused) {
                if (ImGui::Button(ICON_FA_PLAY "##Resume")) {
                    python_runtime::ResumeScript();
                }
                ShowTooltipInternal("Resume execution");
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_STOP "##Reset")) {
                python_runtime::StopScript();
            }
            ShowTooltipInternal("Reset environment");
        }

        ImGui::EndTable();
    }
}

void RenderLog(bool* show_console, bool* show_compact_console) {
    const auto entries = System::Instance().GetConsoleMessages();

    if (ImGui::BeginTable("ConsoleControlsTable", 6, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Clear")) {
            System::Instance().ClearConsoleMessages();
        }
        ShowTooltipInternal("Clear the console output");

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Save Log")) {
            SaveLogToFile();
        }
        ShowTooltipInternal("Save console output to file");

        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("Copy All")) {
            CopyLogToClipboard();
        }
        ShowTooltipInternal("Copy console output to clipboard");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_WINDOW_MAXIMIZE "##MaximizeFULL")) {
            *show_console = false;
            *show_compact_console = true;
            System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Toggled Compact Cosole.");
        }
        if (*show_console) {
            ShowTooltipInternal("Hide Console");
        } else {
            ShowTooltipInternal("Show Compact Console");
        }

        ImGui::TableSetColumnIndex(4);
        auto& ini = SettingsManager::Instance().Open("Py4GW.ini");
        if (!g_auto_scroll_loaded && ini.IsBound()) {
            g_auto_scroll = ini.GetBool("console", "auto_scroll", g_auto_scroll);
            g_auto_scroll_loaded = true;
        }
        if (ImGui::Checkbox("Auto-Scroll", &g_auto_scroll)) {
            ini.SetBool("console", "auto_scroll", g_auto_scroll);
        }
        ShowTooltipInternal("Toggle auto-scrolling of console output");

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("ConsoleFilterTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Filter:");
        ImGui::TableSetColumnIndex(1);
        g_log_filter.Draw("##LogFilter", -FLT_MIN);
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::BeginChild("ConsoleArea", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("Clear")) {
            System::Instance().ClearConsoleMessages();
        }
        ImGui::EndPopup();
    }

    for (const auto& entry : entries) {
        const std::string full_message = "[" + entry.display_timestamp + "] [" + entry.module_name + "] " + entry.message;
        if (!g_log_filter.PassFilter(full_message.c_str())) {
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("[%s]", entry.display_timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.75f, 1.0f, 1.0f));
        ImGui::Text("[%s]", entry.module_name.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, MessageTypeColor(entry.message_type));
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (g_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void RenderCommandInput() {
    ImGui::Separator();
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##CommandInput",
            g_command_buffer,
            sizeof(g_command_buffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            &TextEditCallback)) {
        const std::string command(g_command_buffer);
        if (!command.empty()) {
            python_runtime::ExecuteCommand(command);
            g_command_history.push_back(command);
            g_history_pos = -1;
            strcpy_s(g_command_buffer, sizeof(g_command_buffer), "");
        }
    }
    ImGui::PopItemWidth();
}

}  // namespace

void RenderFullConsole(bool* show_console, bool* show_compact_console) {
    ImGui::SetNextWindowPos(console_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(console_size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(console_collapsed, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Py4GW Console###Py4GWFullConsole", show_console)) {
        ImGui::End();
        return;
    }

    RenderControls(show_console, show_compact_console);
    RenderLog(show_console, show_compact_console);

    const auto state = python_runtime::GetScriptState();
    const double elapsed_time_ms = python_runtime::GetScriptElapsedMilliseconds();
    const int minutes = static_cast<int>(elapsed_time_ms) / 60000;
    const int seconds = (static_cast<int>(elapsed_time_ms) % 60000) / 1000;

    ImGui::Separator();
    ImGui::BeginChild("StatusBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing()), false);
    ImGui::Text("Status: ");
    ImGui::SameLine();
    switch (state) {
    case python_runtime::ScriptState::Running:
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Running");
        break;
    case python_runtime::ScriptState::Paused:
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Paused");
        break;
    case python_runtime::ScriptState::Stopped:
    default:
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Stopped");
        break;
    }

    ImGui::SameLine();
    ImGui::Text(" | Script Time: %02d:%02d", minutes, seconds);
    ImGui::EndChild();

    ImGui::End();
    RenderScriptBrowser();
}

}  // namespace PY4GW::imgui::console_ui

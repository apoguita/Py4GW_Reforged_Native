#include "base/error_handling.h"

#include "imgui/compact_ui.h"

#include "IconsFontAwesome5.h"
#include "base/logger.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "system/system.h"

#include <imgui.h>
#include <ImGuiFileBrowser.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace PY4GW::imgui::compact_ui {

namespace {

constexpr char kScriptBrowserPopup[] = "Select Python Script";
constexpr ImVec2 kScriptBrowserSize = ImVec2(700.0f, 450.0f);

char g_path_buffer[512] = {};
ImVec2 compact_console_pos = ImVec2(5, 30);
bool compact_console_collapsed = false;

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

}  // namespace

void RenderCompactConsole(bool* show_console, bool* show_compact_console) {
    ImGui::SetNextWindowPos(compact_console_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(compact_console_collapsed, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::Begin("Py4GW###Py4GWCompactConsole", show_compact_console, flags)) {
        ImGui::End();
        return;
    }

    SyncPathBuffer();

    if (ImGui::BeginTable("compactPy4GWconsoletable", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("InputColumn");
        ImGui::TableSetupColumn("ButtonColumn");
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::InputText("##compact_script_path", g_path_buffer, IM_ARRAYSIZE(g_path_buffer))) {
            python_runtime::SetSelectedScriptPath(g_path_buffer);
        }
        if (ImGui::IsItemHovered() && g_path_buffer[0] != '\0') {
            ShowTooltipInternal(g_path_buffer);
        }

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Open", ImVec2(30, 30))) {
            RequestOpenScriptBrowser();
        }
        ShowTooltipInternal("Open Python script");
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("compactPy4GWButtonTable", 3)) {
        ImGui::TableNextColumn();
        const auto state = python_runtime::GetScriptState();
        if (state == python_runtime::ScriptState::Stopped) {
            if (ImGui::Button(ICON_FA_PLAY "##Run", ImVec2(30, 30))) {
                python_runtime::StartSelectedScript();
            }
            ShowTooltipInternal("Load and run script");
        } else if (state == python_runtime::ScriptState::Running) {
            if (ImGui::Button(ICON_FA_PAUSE "##Pause", ImVec2(30, 30))) {
                python_runtime::PauseScript();
            }
            ShowTooltipInternal("Pause execution");
        } else if (state == python_runtime::ScriptState::Paused) {
            if (ImGui::Button(ICON_FA_PLAY "##Resume", ImVec2(30, 30))) {
                python_runtime::ResumeScript();
            }
            ShowTooltipInternal("Resume execution");
        }

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_STOP "##Stop", ImVec2(30, 30))) {
            python_runtime::StopScript();
        }
        ShowTooltipInternal("Stop execution");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_WINDOW_MAXIMIZE "##Maximize", ImVec2(30, 30))) {
            const bool next_show_console = !*show_console;
            *show_console = next_show_console;
            *show_compact_console = !next_show_console;
            System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Toggled Full Cosole.");
        }
        if (*show_console) {
            ShowTooltipInternal("Hide Full Console");
        } else {
            ShowTooltipInternal("Show Full Console");
        }

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_STICKY_NOTE "##StickyNote", ImVec2(30, 30))) {
            System::Instance().ClearConsoleMessages();
        }
        ShowTooltipInternal("Clear the console output");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_SAVE "##Save", ImVec2(30, 30))) {
            const std::string save_path = System::SaveFileDialog();
            if (!save_path.empty()) {
                std::ofstream output(save_path, std::ios::out | std::ios::trunc);
                if (output.is_open()) {
                    const auto entries = System::Instance().GetConsoleMessages();
                    for (const auto& entry : entries) {
                        output << "[" << entry.timestamp << "] [" << entry.module_name << "] " << entry.message << "\n";
                    }
                    System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Console log saved to " + save_path);
                } else {
                    System::Instance().WriteConsoleMessage("Py4GW", MessageType::Error, "Failed to open file for writing: " + save_path);
                }
            }
        }
        ShowTooltipInternal("Save console output to file");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_COPY "##Copy", ImVec2(30, 30))) {
            const auto entries = System::Instance().GetConsoleMessages();
            std::string text;
            for (const auto& entry : entries) {
                text += "[" + entry.timestamp + "] [" + entry.module_name + "] " + entry.message + "\n";
            }
            ImGui::SetClipboardText(text.c_str());
        }
        ShowTooltipInternal("Copy console output to clipboard");
        ImGui::EndTable();
    }

    ImGui::Separator();
    const auto state = python_runtime::GetScriptState();
    const ImVec4 status_color = state == python_runtime::ScriptState::Running
        ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
        : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(status_color, "%s", state == python_runtime::ScriptState::Running ? "Running" : "Stopped");
    ImGui::End();
    RenderScriptBrowser();
}

}  // namespace PY4GW::imgui::compact_ui

#include "base/error_handling.h"

#include "imgui/addons/filebrowser_demo.h"

#include <imgui.h>
#include <ImGuiFileBrowser.h>

#include <cstring>
#include <filesystem>
#include <memory>

namespace PY4GW::imgui::addons::filebrowser_demo {

namespace {

constexpr char kDemoBrowserPopup[] = "Addon File Browser";
constexpr ImVec2 kDemoBrowserSize = ImVec2(700.0f, 450.0f);

std::filesystem::path GetSafeInitialDirectory() {
    return std::filesystem::path(L"C:\\");
}

struct DemoBrowserState {
    imgui_addons::ImGuiFileBrowser browser;
    bool open_requested = false;
};

}  // namespace

void Render() {
    static std::unique_ptr<DemoBrowserState> browser;
    static bool init_attempted = false;
    static bool init_failed = false;
    static char selected_path[512] = "No file selected";
    static char error_text[512] = "";

    if (!init_attempted) {
        init_attempted = true;
        try {
            browser = std::make_unique<DemoBrowserState>();
            browser->browser.SetUseModal(false);
            browser->browser.SetCurrentPath(GetSafeInitialDirectory().string());
        } catch (const std::exception& error) {
            init_failed = true;
            strncpy_s(error_text, sizeof(error_text), error.what(), _TRUNCATE);
        } catch (...) {
            init_failed = true;
            strncpy_s(error_text, sizeof(error_text), "unknown file browser initialization error", _TRUNCATE);
        }
    }

    ImGui::TextWrapped("Direct ImGui-Addons file picker. Open the popup and use showFileDialog() each frame.");
    if (init_failed || !browser) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "File browser init failed: %s", error_text);
        return;
    }

    if (ImGui::Button("Open File Browser")) {
        browser->open_requested = true;
    }

    try {
        if (browser->open_requested) {
            ImGui::OpenPopup(kDemoBrowserPopup);
            browser->open_requested = false;
        }
        if (browser->browser.showFileDialog(kDemoBrowserPopup, imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, kDemoBrowserSize, ".txt,.json,.py,.dll")) {
            strncpy_s(selected_path, sizeof(selected_path), browser->browser.selected_path.c_str(), _TRUNCATE);
        }
    } catch (const std::exception& error) {
        strncpy_s(error_text, sizeof(error_text), error.what(), _TRUNCATE);
        init_failed = true;
        browser.reset();
    } catch (...) {
        strncpy_s(error_text, sizeof(error_text), "unknown file browser runtime error", _TRUNCATE);
        init_failed = true;
        browser.reset();
    }

    if (init_failed || !browser) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "File browser runtime failed: %s", error_text);
        return;
    }

    ImGui::Separator();
    ImGui::Text("Selected: %s", selected_path);
}

}  // namespace PY4GW::imgui::addons::filebrowser_demo

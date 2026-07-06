#include "base/error_handling.h"

#include "imgui/test_ui.h"

#include "IconsFontAwesome5.h"
#include "imgui/addons/filebrowser_demo.h"
#include "imgui/addons/hotkey_demo.h"
#include "imgui/addons/imanim_demo.h"
#include "imgui/addons/imgui_club_demo.h"
#include "imgui/addons/markdown_demo.h"
#include "imgui/font_manager.h"

#include <windows.h>

#include <imgui.h>

namespace PY4GW::imgui::test_ui {

namespace {

bool g_show_metrics_window = true;
bool g_show_debug_log_window = false;
bool g_show_about_window = false;
bool g_show_style_editor = false;

void RenderFontLine(const char* label, PY4GW::imgui::FontId id, const char* sample) {
    auto& mgr = PY4GW::imgui::FontManager::Instance();
    ImFont* font = mgr.Get(id);
    ImGui::Text("%s", label);
    if (font) {
        ImGui::PushFont(font, mgr.GetSize(id));
        ImGui::TextUnformatted(sample);
        ImGui::PopFont();
    } else {
        ImGui::TextDisabled("Font unavailable");
    }
}

void RenderFontShowcase() {
    ImGui::TextUnformatted("Font catalog smoke test");
    ImGui::BulletText("Use this tab to verify family selection, size changes, and merged icons.");
    ImGui::Separator();

    RenderFontLine("Regular 14", PY4GW::imgui::FontId::Regular14, "The quick brown fox jumps over the lazy dog 0123456789");
    RenderFontLine("Regular 22", PY4GW::imgui::FontId::Regular22, "The quick brown fox jumps over the lazy dog 0123456789");
    RenderFontLine("Bold 22", PY4GW::imgui::FontId::Bold22, "Bold sample text for menu and heading use.");
    RenderFontLine("Italic 22", PY4GW::imgui::FontId::Italic22, "Italic sample text for emphasis.");
    RenderFontLine("Bold Italic 30", PY4GW::imgui::FontId::BoldItalic30, "Bold italic sample for large emphasis.");

    ImGui::Separator();
    ImGui::TextUnformatted("Font Awesome merge test");
    ImGui::Text("%s  %s  %s  %s  %s",
        ICON_FA_HOME,
        ICON_FA_COG,
        ICON_FA_FOLDER_OPEN,
        ICON_FA_SAVE,
        ICON_FA_SEARCH);
    ImGui::Text("%s Home    %s Settings    %s Folder    %s Save    %s Search",
        ICON_FA_HOME,
        ICON_FA_COG,
        ICON_FA_FOLDER_OPEN,
        ICON_FA_SAVE,
        ICON_FA_SEARCH);

    ImFont* large_font = PY4GW::imgui::FontManager::Instance().Get(PY4GW::imgui::FontId::Regular46);
    if (large_font) {
        ImGui::PushFont(large_font, PY4GW::imgui::FontManager::Instance().GetSize(PY4GW::imgui::FontId::Regular46));
        ImGui::Text("%s %s %s", ICON_FA_STAR, ICON_FA_HEART, ICON_FA_BELL);
        ImGui::PopFont();
    }
}

void RenderDockspaceHost() {
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags dockspace_window_flags = ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        dockspace_window_flags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::SetNextWindowPos(main_viewport->WorkPos);
    ImGui::SetNextWindowSize(main_viewport->WorkSize);
    ImGui::SetNextWindowViewport(main_viewport->ID);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Py4GW Test DockSpace", nullptr, dockspace_window_flags);
    ImGui::PopStyleVar(2);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Metrics/Debugger", nullptr, &g_show_metrics_window);
            ImGui::MenuItem("Debug Log", nullptr, &g_show_debug_log_window);
            ImGui::MenuItem("About", nullptr, &g_show_about_window);
            ImGui::MenuItem("Style Editor", nullptr, &g_show_style_editor);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::DockSpace(ImGui::GetID("Py4GWTestDockSpaceRoot"), ImVec2(0.0f, 0.0f), dockspace_flags);
    ImGui::End();
}

void RenderAddonWindows() {
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowSize(ImVec2(460.0f, 230.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Py4GW Addon Demo", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Docking branch and addon integration smoke test.");
    ImGui::Separator();
    ImGui::BulletText("Dear ImGui: %s", ImGui::GetVersion());
    ImGui::BulletText("Docking enabled: %s", (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) ? "yes" : "no");
    ImGui::BulletText("Viewport ID: 0x%08X", main_viewport->ID);
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Addon Showcase", nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::BeginTabBar("AddonTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            ImGui::TextUnformatted("Addons are loaded on demand.");
            ImGui::BulletText("Open each tab individually to validate that addon.");
            ImGui::BulletText("If one addon faults, the overlay now requests shutdown instead of taking the client down.");
            ImGui::BulletText("Start with Markdown or ImAnim, then move to the heavier integrations.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fonts")) {
            RenderFontShowcase();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("FileBrowser")) {
            PY4GW::imgui::addons::filebrowser_demo::Render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImHotKey")) {
            PY4GW::imgui::addons::hotkey_demo::Render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Markdown")) {
            PY4GW::imgui::addons::markdown_demo::Render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImAnim")) {
            PY4GW::imgui::addons::imanim_demo::Render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("imgui_club")) {
            PY4GW::imgui::addons::imgui_club_demo::Render();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    if (g_show_metrics_window) {
        ImGui::ShowMetricsWindow(&g_show_metrics_window);
    }
    if (g_show_debug_log_window) {
        ImGui::ShowDebugLogWindow(&g_show_debug_log_window);
    }
    if (g_show_about_window) {
        ImGui::ShowAboutWindow(&g_show_about_window);
    }
    if (g_show_style_editor) {
        ImGui::Begin("Py4GW Style Editor", &g_show_style_editor, ImGuiWindowFlags_NoCollapse);
        ImGui::ShowStyleEditor();
        ImGui::End();
    }
}

}  // namespace

void BeginFrame() {
    PY4GW::imgui::addons::imanim_demo::BeginFrame();
}

void Render() {
    RenderDockspaceHost();
    RenderAddonWindows();
}

}  // namespace PY4GW::imgui::test_ui

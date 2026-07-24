#include "base/error_handling.h"

#include "imgui/console_host_ui.h"

#include "settings/settings.h"
#include "system/system.h"
#include "imgui/compact_ui.h"
#include "imgui/console_ui.h"

#include <imgui.h>

namespace PY4GW::imgui::console_host_ui {

namespace {

bool g_settings_applied = false;
bool g_last_output_to_file = false;
bool g_last_show_console = false;
bool g_last_show_compact_console = false;
// Set once the user confirms shutdown from the close prompt. From that point
// visibility is never persisted again: the hidden-hidden state that precedes a
// confirmed shutdown is transient and must not become the next session's
// startup state (that bug hid the console permanently on every injection).
bool g_shutdown_confirmed = false;

// Every console surface the host can draw. Extend this check when new
// surfaces are added (third/fourth console modes) so the default-surface
// fallback below stays correct without touching the policy itself.
bool AnyConsoleSurfaceVisible(const System& system) {
    return system.GetDrawConsole() || system.GetDrawCompactConsole();
}

// Default-surface policy: the FULL console is the default surface. It is ON
// unless some other surface is active. Applied after persisted state loads so
// a session can never start with no visible console; the document is repaired
// in place so a poisoned state does not survive to the next session.
void ApplyDefaultSurfaceFallback(System& system, IniFile& ini) {
    if (AnyConsoleSurfaceVisible(system)) {
        return;
    }
    system.SetDrawConsole(true);
    ini.SetBool("console", "show_full_console", true);
}

// Applies persisted console preferences once the account document binds,
// then mirrors runtime changes back into [console] of Py4GW.ini.
void SyncConsoleSettings(System& system) {
    auto& ini = SettingsManager::Instance().OpenPy4GWIni();

    if (!g_settings_applied) {
        if (!ini.IsBound()) {
            return;
        }
        system.SetOutputToFile(ini.GetBool("console", "output_to_file", system.GetOutputToFile()));
        // Non-default surfaces apply first (extend here as surfaces are
        // added); the full console's missing-key default then follows the
        // default-surface policy: ON unless some other surface is active.
        const bool compact_active = ini.GetBool("console", "show_compact_console", false);
        system.SetDrawCompactConsole(compact_active);
        const bool other_surface_active = compact_active;
        system.SetDrawConsole(ini.GetBool("console", "show_full_console", !other_surface_active));
        ApplyDefaultSurfaceFallback(system, ini);
        g_last_output_to_file = system.GetOutputToFile();
        g_last_show_console = system.GetDrawConsole();
        g_last_show_compact_console = system.GetDrawCompactConsole();
        g_settings_applied = true;
        return;
    }

    const bool output_to_file = system.GetOutputToFile();
    if (output_to_file != g_last_output_to_file) {
        ini.SetBool("console", "output_to_file", output_to_file);
        g_last_output_to_file = output_to_file;
    }

    // Visibility is not persisted while the shutdown prompt is pending or
    // after a shutdown has been confirmed, so an X-close (both hidden ->
    // confirm shutdown) cannot save a console-less startup state.
    if (!system.IsShutdownPromptPending() && !g_shutdown_confirmed) {
        const bool show_console = system.GetDrawConsole();
        if (show_console != g_last_show_console) {
            ini.SetBool("console", "show_full_console", show_console);
            g_last_show_console = show_console;
        }
        const bool show_compact_console = system.GetDrawCompactConsole();
        if (show_compact_console != g_last_show_compact_console) {
            ini.SetBool("console", "show_compact_console", show_compact_console);
            g_last_show_compact_console = show_compact_console;
        }
    }
}

void RenderShutdownConfirmationModal(bool* request_shutdown) {
    auto& system = System::Instance();
    if (system.IsShutdownPromptPending()) {
        ImGui::OpenPopup("Confirm Py4GW Shutdown");
    }

    if (!ImGui::BeginPopupModal("Confirm Py4GW Shutdown", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Closing the console will shut down Py4GW.");
    ImGui::Spacing();

    if (ImGui::Button("Shutdown", ImVec2(120.0f, 0.0f))) {
        if (request_shutdown) {
            *request_shutdown = true;
        }
        g_shutdown_confirmed = true;
        system.CancelShutdownPrompt();
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        system.SetDrawCompactConsole(true);
        system.CancelShutdownPrompt();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

}  // namespace

bool Initialize() {
    return true;
}

void Shutdown() {
}

void BeginFrame() {
}

void Render(bool* request_shutdown) {
    auto& system = System::Instance();
    SyncConsoleSettings(system);

    bool show_console = system.GetDrawConsole();
    bool show_compact_console = system.GetDrawCompactConsole();
    const bool had_visible_console = show_console || show_compact_console;

    if (show_console) {
        console_ui::RenderFullConsole(&show_console, &show_compact_console);
    }
    if (show_compact_console) {
        compact_ui::RenderCompactConsole(&show_console, &show_compact_console);
    }

    system.SetDrawConsole(show_console);
    system.SetDrawCompactConsole(show_compact_console);

    // Only a close performed inside this frame (window X button) prompts for
    // shutdown; consoles hidden through System stay hidden silently. Scripts
    // can raise the same prompt via System::RequestShutdownPrompt.
    const bool has_visible_console = show_console || show_compact_console;
    if (had_visible_console && !has_visible_console) {
        system.RequestShutdownPrompt();
    }

    RenderShutdownConfirmationModal(request_shutdown);
}

}  // namespace PY4GW::imgui::console_host_ui

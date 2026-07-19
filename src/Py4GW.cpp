#include "base/error_handling.h"

#include "Py4GW.h"
#include "GW/GuildWars.h"
#include "GW/context/map.h"
#include "GW/dialog/dialog.h"
#include "GW/map/map.h"
#include "GW/render/render.h"
#include "GW/shared_memory/manager.h"
#include "GW/multibox/manager.h"
#include "GW/textures/gw_dat_reader.h"
#include "GW/textures/texture_manager.h"
#include "callback/callback.h"
#include "listeners/listeners.h"
#include "profiler/profiler.h"
#include "base/CrashHandler.h"
#include "imgui/imgui_manager.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "base/scanner.h"
#include "settings/settings.h"
#include "system/system.h"

#include <imgui.h>

#include <cstdio>
#include <d3d9.h>
#include <mutex>
#include <string>
namespace {

std::mutex g_runtime_mutex;
bool g_running = false;
bool g_shutdown_requested = false;
// One-shot latches for RunAutoexecOnce/RunWidgetManagerOnce; file-scope so
// the (normally disabled) debug probe window can display them.
bool g_autoexec_done = false;
bool g_widget_started = false;

std::string Narrow(std::wstring_view value) {
    std::string out;
    out.reserve(value.size());
    for (const wchar_t ch : value) {
        out.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return out;
}

void StopRunningScriptForShutdown() {
    if (PY4GW::python_runtime::GetScriptState() == PY4GW::python_runtime::ScriptState::Stopped) {
        return;
    }

    Logger::Instance().LogInfo("Stopping running script before shutdown.");
    PY4GW::python_runtime::StopScript();
}

void BeginShutdown() {
    if (g_shutdown_requested) {
        return;
    }

    StopRunningScriptForShutdown();
    g_shutdown_requested = true;
}

// Runs the configured autoexec script once, after the per-account settings
// document has bound (which only happens once a map is loaded).
void RunAutoexecOnce() {
    if (g_autoexec_done) {
        return;
    }

    auto& ini = PY4GW::SettingsManager::Instance().Open("Py4GW.ini", PY4GW::SettingsScope::Root);
    if (!ini.IsBound()) {
        return;
    }
    g_autoexec_done = true;

    const std::string autoexec = ini.GetString("settings", "autoexec_script", "");
    if (autoexec.empty()) {
        return;
    }

    PY4GW::python_runtime::SetSelectedScriptPath(autoexec);
    PY4GW::System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Autoexec script: " + autoexec);
    if (!PY4GW::python_runtime::StartSelectedScript()) {
        PY4GW::System::Instance().WriteConsoleMessage("Py4GW", MessageType::Error, "Autoexec script failed to start.");
    }
}

// Starts the always-on widget manager once, after we reach a valid map.
void RunWidgetManagerOnce() {
    if (g_widget_started) {
        return;
    }
    g_widget_started = true;
    PY4GW::python_runtime::StartWidgetManager();
}

// ============================================================================
// DEBUG PROBE — diagnostic window rendered before every gate that can hide
// the console. Shows the live state of the render gate, the settings/account
// binding, the System console flags, and the script hosts. Disabled by
// default; flip kEnableDebugProbe to true to bring it back when diagnosing
// "console gone / is the DLL alive" situations.
// ============================================================================
constexpr bool kEnableDebugProbe = false;
std::string BuildDebugProbeReport(uint64_t probe_frame) {
    std::string report;
    report.reserve(2048);
    char line[512];
    const auto append = [&report, &line](const char* fmt, auto... args) {
        std::snprintf(line, sizeof(line), fmt, args...);
        report += line;
        report += '\n';
    };

    append("=== PY4GW DEBUG PROBE ===");
    append("probe_frame: %llu  tick: %llu",
        static_cast<unsigned long long>(probe_frame),
        static_cast<unsigned long long>(PY4GW::System::GetTickCount64()));

    // --- Render/update gate ---
    const auto instance_type = GW::map::GetInstanceType();
    const bool map_loading = instance_type == GW::Constants::InstanceType::Loading;
    auto* map_info = GW::map::GetMapInfo();
    const uint32_t flags = map_info ? static_cast<uint32_t>(map_info->flags) : 0u;
    const bool gate_skips = !map_loading && map_info && (flags & 0x40001) != 0;
    append("[render gate]");
    append("instance_type: %d  is_map_loaded: %d  map_id: %u",
        static_cast<int>(instance_type),
        GW::map::GetIsMapLoaded() ? 1 : 0,
        static_cast<unsigned>(GW::map::GetMapID()));
    append("map_info: %p  flags: 0x%08X  flags&0x40001: 0x%X",
        static_cast<const void*>(map_info), flags, flags & 0x40001);
    append("gate_skips_scripts_and_console: %s", gate_skips ? "YES" : "no");

    // --- System console flags ---
    auto& system = PY4GW::System::Instance();
    append("[system console flags]");
    append("draw_console: %d  draw_compact_console: %d",
        system.GetDrawConsole() ? 1 : 0, system.GetDrawCompactConsole() ? 1 : 0);
    append("shutdown_prompt_pending: %d  output_to_file: %d",
        system.IsShutdownPromptPending() ? 1 : 0, system.GetOutputToFile() ? 1 : 0);
    append("both_consoles_hidden: %s",
        (!system.GetDrawConsole() && !system.GetDrawCompactConsole()) ? "YES <- console invisible because of this" : "no");

    // --- Account anchor + settings document ---
    append("[settings]");
    append("account_email: '%s'  has_email: %d  char_select: %d",
        system.GetAccountEmail().c_str(),
        system.HasAccountEmail() ? 1 : 0,
        PY4GW::System::InCharacterSelectScreen() ? 1 : 0);
    append("settings_dir: %s", system.GetSettingsDirectory().string().c_str());

    auto& ini = PY4GW::SettingsManager::Instance().Open("Py4GW.ini", PY4GW::SettingsScope::Root);
    append("Py4GW.ini bound: %d  dirty: %d", ini.IsBound() ? 1 : 0, ini.IsDirty() ? 1 : 0);
    append("[console] show_full_console:    has=%d raw='%s'",
        ini.HasKey("console", "show_full_console") ? 1 : 0,
        ini.GetString("console", "show_full_console", "<missing>").c_str());
    append("[console] show_compact_console: has=%d raw='%s'",
        ini.HasKey("console", "show_compact_console") ? 1 : 0,
        ini.GetString("console", "show_compact_console", "<missing>").c_str());
    append("[console] output_to_file:       has=%d raw='%s'",
        ini.HasKey("console", "output_to_file") ? 1 : 0,
        ini.GetString("console", "output_to_file", "<missing>").c_str());
    append("[settings] autoexec_script:     has=%d raw='%s'",
        ini.HasKey("settings", "autoexec_script") ? 1 : 0,
        ini.GetString("settings", "autoexec_script", "<missing>").c_str());

    // --- Script hosts + one-shots ---
    append("[script hosts]");
    append("autoexec_done: %d  widget_started: %d",
        g_autoexec_done ? 1 : 0, g_widget_started ? 1 : 0);
    append("user_script_state: %d  path: %s",
        static_cast<int>(PY4GW::python_runtime::GetScriptState()),
        PY4GW::python_runtime::GetSelectedScriptPath().c_str());
    append("widget_manager_state: %d  status: %s",
        static_cast<int>(PY4GW::python_runtime::GetWidgetManagerState()),
        PY4GW::python_runtime::GetWidgetManagerStatus().c_str());

    return report;
}

void RenderDebugProbeWindow() {
    static uint64_t s_probe_frame = 0;
    ++s_probe_frame;

    ImGui::SetNextWindowPos(ImVec2(60.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (!ImGui::Begin("PY4GW DEBUG PROBE (temporary)", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const std::string report = BuildDebugProbeReport(s_probe_frame);

    auto& system = PY4GW::System::Instance();
    if (ImGui::Button("Copy diagnostics to clipboard")) {
        ImGui::SetClipboardText(report.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Force compact console ON")) {
        system.SetDrawCompactConsole(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Force full console ON")) {
        system.SetDrawConsole(true);
    }
    ImGui::Separator();

    ImGui::TextUnformatted(report.c_str());

    ImGui::End();
}

void UpdateLoopStep() {
    auto& manager = GW::shared_memory::RuntimeManager();
    if (manager.IsValid()) {
        manager.Update();
    }
    PY4GW::System::Instance().UpdateAccountAnchor();
    PY4GW::SettingsManager::Instance().Update();

    bool is_map_loading = GW::map::GetInstanceType() == GW::Constants::InstanceType::Loading;
    if (!is_map_loading) {
        auto map_info = GW::map::GetMapInfo();
        if (map_info && (map_info->flags & 0x40001) != 0) {
            ::Sleep(10);
            return;
        }
    }

    // Everything Python-side (library bring-up, frame callbacks, script + widget
    // updates) waits until the account-email anchor resolves - i.e. we have
    // reached a valid map/character at least once. UpdateAccountAnchor() above
    // keeps retrying every frame until it latches. C++ infra (map-change poll,
    // texture decode) runs regardless.
    const bool py_ready = PY4GW::System::Instance().HasAccountEmail();

    if (py_ready) {
        RunAutoexecOnce();
        RunWidgetManagerOnce();
    }

    // Resume dialog callbacks once a map transition settles (legacy called
    // Dialog::PollMapChange from the update loop before the GIL).
    GW::dialog::PollMapChange();

    if (py_ready) {
        using PyCallback = PY4GW::PyCallback;
        PyCallback::ExecutePhase(PyCallback::Phase::PreUpdate, PyCallback::Context::Update);
        PyCallback::ExecutePhase(PyCallback::Phase::Data, PyCallback::Context::Update);
        PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Update);
    }

    // Drain queued texture CPU-decode jobs (async DAT texture pipeline).
    GW::textures::GWDatReader::Instance().CpuUpdate();

    if (py_ready) {
        const uint64_t frame_id = PY4GW::System::GetTickCount64();
        PY4GW::Profiler::Start("Update.Script");
        PY4GW::python_runtime::ExecutePythonUpdate();
        PY4GW::Profiler::End(frame_id, "Update.Script");

        PY4GW::Profiler::Start("Update.WidgetManager");
        PY4GW::python_runtime::ExecuteWidgetManagerUpdate();
        PY4GW::Profiler::End(frame_id, "Update.WidgetManager");
    }
    ::Sleep(10);
}

void DrawLoop(IDirect3DDevice9* device) {
    if (!g_running) {
        return;
    }
    PY4GW::System::Instance().UpdateFrameTimestamp();

    // Multi-account ("multibox") buffer: publish this client's AccountData as the
    // FIRST thing in the frame, before any Python draw callback reads it. This
    // runs on the render thread (the game's own thread), so it snapshots
    // consistent game state, and it runs every frame - including gated /
    // map-loading frames below - so the slot keeps heartbeating and gate-zeroing.
    // Update() self-gates internally on the account-email anchor.
    if (auto& multibox_manager = GW::multibox::RuntimeManager(); multibox_manager.IsValid()) {
        multibox_manager.Update();
    }

    bool gate_skips = false;
    bool is_map_loading = GW::map::GetInstanceType() == GW::Constants::InstanceType::Loading;
    if (!is_map_loading) {
        auto map_info = GW::map::GetMapInfo();
        if (map_info && (map_info->flags & 0x40001) != 0) {
            gate_skips = true;
        }
    }

    // With the probe disabled, a gated frame renders nothing (original
    // behavior). With the probe enabled, the frame still begins so the probe
    // stays visible even while scripts + console are gated.
    if (gate_skips && !kEnableDebugProbe) {
        return;
    }

    if (!PY4GW::imgui::BeginFrame(device)) {
        return;
    }

    // Texture pipeline: publish the live device and drain queued GPU-upload
    // jobs so decoded DAT textures become available this frame.
    GW::textures::TextureManager::Instance().SetDevice(device);
    GW::textures::GWDatReader::Instance().DxUpdate(device);

    if (kEnableDebugProbe) {
        RenderDebugProbeWindow();
    }

    if (!gate_skips) {
        // Python-side draw work (frame callbacks, script + widget draw, deferred
        // actions) waits until the account-email anchor resolves - same gate as
        // the update loop, i.e. a valid map/character reached at least once. The
        // console UI + ImGui frame lifecycle below are C++ and stay available.
        if (PY4GW::System::Instance().HasAccountEmail()) {
            using PyCallback = PY4GW::PyCallback;
            const uint64_t frame_id = PY4GW::System::GetTickCount64();

            PyCallback::ExecutePhase(PyCallback::Phase::PreUpdate, PyCallback::Context::Draw);
            PyCallback::ExecutePhase(PyCallback::Phase::Data, PyCallback::Context::Draw);

            PY4GW::Profiler::Start("Draw.Script");
            PY4GW::python_runtime::ExecutePythonDraw();
            PY4GW::Profiler::End(frame_id, "Draw.Script");

            PY4GW::Profiler::Start("Draw.WidgetManager");
            PY4GW::python_runtime::ExecuteWidgetManagerDraw();
            PY4GW::Profiler::End(frame_id, "Draw.WidgetManager");

            PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Draw);
            PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Main);

            PY4GW::python_runtime::ProcessDeferredActions();
        }

        bool request_shutdown = false;
        PY4GW::imgui::RenderConsoleUi(&request_shutdown);
    }
    PY4GW::imgui::EndFrame(device);
}

void OnReset(IDirect3DDevice9*) {
    PY4GW::imgui::InvalidateDeviceObjects();
}

}  // namespace

extern "C" {

bool Py4GW_Initialize() {
    std::scoped_lock guard(g_runtime_mutex);
    if (g_running) {
        return true;
    }

    Logger::Instance().SetLogFile("Py4GW_injection_log.txt");
    g_shutdown_requested = false;
    CrashHandler::SetContext("startup", "bootstrap", "scanner_initialize");
    Logger::Instance().LogInfo("Initializing scanner.");
    if (!PY4GW::Scanner::Initialize()) {
        Logger::Instance().LogError("Scanner initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "bootstrap", "patterns_initialize");
    Logger::Instance().LogInfo("Initializing patterns.");
    if (!PY4GW::Patterns::Initialize()) {
        Logger::Instance().LogError("Pattern initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "python_runtime", "initialize");
    Logger::Instance().LogInfo("Initializing Python runtime.");
    if (!PY4GW::python_runtime::Initialize()) {
        Logger::Instance().LogError("Python runtime initialization failed.");
        return false;
    }


    CrashHandler::SetContext("startup", "gw", "initialize");
    Logger::Instance().LogInfo("Initializing Guild Wars modules.");
    if (!GW::Initialize()) {
        Logger::Instance().LogError("Guild Wars hook initialization failed.");
        PY4GW::python_runtime::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "listeners", "initialize");
    PY4GW::listeners::Initialize();

    auto& manager = GW::shared_memory::RuntimeManager();
    if (!manager.IsValid()) {
        const auto name = GW::shared_memory::Manager::BuildName(
            L"Py4GW_Runtime",
            ::GetCurrentProcessId(),
            GW::render::GetWindowHandle());
        if (!manager.Create(name)) {
            Logger::Instance().LogWarning("Shared memory publisher initialization failed.");
        } else {
            Logger::Instance().LogInfo("Shared memory publisher ready: " + Narrow(name));
        }
    }

    auto& multibox_manager = GW::multibox::RuntimeManager();
    if (!multibox_manager.IsValid()) {
        // Fixed name shared by every boxed client on the machine; first instance
        // creates + zeroes, later instances attach. Matches the Python buffer name.
        if (!multibox_manager.CreateOrOpen(L"Py4GW_Shared_Mem")) {
            Logger::Instance().LogWarning("Multibox shared memory initialization failed.");
        } else {
            Logger::Instance().LogInfo("Multibox shared memory ready (creator: " +
                                       std::string(multibox_manager.IsCreator() ? "yes" : "no") + ").");
        }
    }

    CrashHandler::SetContext("startup", "crash_handler", "initialize");
    Logger::Instance().LogInfo("Initializing crash handler.");
    CrashHandler::Instance().Initialize();
    CrashHandler::SetContext("runtime", "bootstrap", "initialized");

    PY4GW::imgui::SetShutdownCallback(&BeginShutdown);
    GW::render::SetResetCallback(&OnReset);
    GW::render::SetRenderCallback(&DrawLoop);

    g_running = true;
    Logger::Instance().LogInfo("Py4GW initialized.");
    return true;
}

void Py4GW_Shutdown() {
    std::scoped_lock guard(g_runtime_mutex);
    if (!g_running) {
        return;
    }

    // Stop the crash handler from reporting expected teardown-order faults as crashes.
    // It stays installed until CrashHandler::Terminate() runs last (in DllMain), but
    // from here on OnException no-ops so subsystem teardown can't spawn spurious,
    // half-written crash folders. Runs on both shutdown paths (RuntimeThread + detach).
    CrashHandler::NotifyShutdown();

    StopRunningScriptForShutdown();

    CrashHandler::SetContext("shutdown", "settings", "flush");
    PY4GW::SettingsManager::Instance().FlushAll();

    GW::render::SetRenderCallback(nullptr);
    GW::render::SetResetCallback(nullptr);

    CrashHandler::SetContext("shutdown", "imgui", "shutdown");
    Logger::Instance().LogInfo("Shutting down ImGui.");
    PY4GW::imgui::Shutdown();
    CrashHandler::SetContext("shutdown", "listeners", "shutdown");
    PY4GW::listeners::Shutdown();
    CrashHandler::SetContext("shutdown", "gw", "shutdown");
    Logger::Instance().LogInfo("Shutting down Guild Wars modules.");
    GW::Shutdown();
    CrashHandler::SetContext("shutdown", "python_runtime", "shutdown");
    Logger::Instance().LogInfo("Shutting down Python runtime.");
    PY4GW::python_runtime::Shutdown();
    GW::shared_memory::RuntimeManager().Destroy();
    GW::multibox::RuntimeManager().Destroy();
    g_running = false;
    g_shutdown_requested = false;
    CrashHandler::SetContext("shutdown", "bootstrap", "shutdown_complete");
    Logger::Instance().LogInfo("Py4GW shutdown complete.");
}

void Py4GW_RequestShutdown() {
    BeginShutdown();
}

}

namespace PY4GW {

DWORD WINAPI RuntimeThread(LPVOID) {
    if (!Py4GW_Initialize()) {
        return 1;
    }

    while (!g_shutdown_requested) {
        UpdateLoopStep();
    }

    // Let the frame that requested shutdown unwind before tearing down hooks and Python.
    ::Sleep(50);
    Py4GW_Shutdown();
    if (process_manager::GetModuleHandle()) {
        ::FreeLibraryAndExitThread(process_manager::GetModuleHandle(), 0);
    }
    return 0;
}

}


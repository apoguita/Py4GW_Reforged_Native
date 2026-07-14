#include "base/error_handling.h"

#include "imgui/imgui_manager.h"

#include "GW/render/render.h"
#include "base/CrashHandler.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "base/logger.h"
#include "imgui/console_host_ui.h"
#include "imgui/font_manager.h"
#include "system/system.h"

#include <WindowsX.h>
#include <d3d9.h>
#include <hidusage.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <filesystem>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace {

bool g_imgui_initialized = false;
bool g_wndproc_attached = false;
bool g_is_dragging = false;
bool g_is_dragging_imgui = false;
bool g_dragging_initialized = false;
HWND g_game_window = nullptr;
WNDPROC g_original_wndproc = nullptr;
PY4GW::imgui::ShutdownCallback g_shutdown_callback = nullptr;
bool g_account_ini_applied = false;
std::string g_account_ini_path;
constexpr float kDefaultWindowRounding = 5.0f;
constexpr float kDefaultWindowBgAlpha = 0.80f;

// Ini persistence stays disabled until the account anchor is resolved; then
// ImGui reads/writes its layout under <dll_dir>/settings/<email>/imgui.ini.
void ApplyAccountIniPath() {
    if (g_account_ini_applied) {
        return;
    }

    auto& system = PY4GW::System::Instance();
    if (!system.HasAccountEmail()) {
        return;
    }
    const auto settings_dir = system.GetSettingsDirectory();
    if (settings_dir.empty()) {
        return;
    }

    g_account_ini_path = (settings_dir / "imgui.ini").string();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = g_account_ini_path.c_str();

    std::error_code ec;
    if (std::filesystem::exists(g_account_ini_path, ec)) {
        ImGui::LoadIniSettingsFromDisk(g_account_ini_path.c_str());
    }
    g_account_ini_applied = true;
}

bool SetupImGuiFonts() {
    return PY4GW::imgui::FontManager::Instance().Initialize();
}

void ApplyImGuiTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.WindowRounding = kDefaultWindowRounding;
    style.FramePadding = ImVec2(5.0f, 5.0f);
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(10.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 20.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 5.0f;
    style.GrabRounding = 3.0f;

    style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, kDefaultWindowBgAlpha);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.15f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.30f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.80f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.80f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.80f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.30f, 0.30f, 0.50f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.30f, 0.40f, 0.50f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.20f, 0.30f, 0.40f, 0.50f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.30f, 0.30f, 0.50f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.30f, 0.40f, 0.50f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.15f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.30f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.10f, 1.00f, 0.10f, 0.43f);
}

void ApplyViewportStyleTweaks() {
    ImGuiStyle& style = ImGui::GetStyle();
    const bool enabled = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
    style.WindowRounding = enabled ? 0.0f : kDefaultWindowRounding;
    style.Colors[ImGuiCol_WindowBg].w = enabled ? 1.0f : kDefaultWindowBgAlpha;
}

bool RenderConsoleUiSafely(bool* request_shutdown) noexcept {
    __try {
        PY4GW::imgui::console_host_ui::BeginFrame();
        PY4GW::imgui::console_host_ui::Render(request_shutdown);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // The user is closing the game window. Flag shutdown as early as possible - the
    // instant the close message arrives - so the crash handler suppresses the
    // teardown-order faults that follow (GW releasing D3D/other resources our hooks
    // still touch) instead of writing spurious reports. This fires well before
    // ExitProcess/LdrShutdownProcess, which the RtlDllShutdownInProgress gate cannot
    // see yet. Runs before the g_imgui_initialized bail so it always takes effect.
    // WM_CLOSE is the earliest signal; WM_DESTROY is the irreversible one (a WndProc
    // never receives WM_QUIT). GW does not prompt/cancel on close, so latching here is
    // safe - and NotifyShutdown only suppresses reports, it does not stop teardown.
    if (message == WM_CLOSE || message == WM_DESTROY) {
        CrashHandler::NotifyShutdown();
    }

    static bool right_mouse_down = false;
    static bool cached_left = false;
    static bool cached_right = false;
    static float cached_x = -1.0f;
    static float cached_y = -1.0f;

    if (!g_imgui_initialized) {
        return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                                  : ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    ImGuiIO& io = ImGui::GetIO();

    POINT pt;
    ::GetCursorPos(&pt);
    ::ScreenToClient(hwnd, &pt);
    io.MousePos = ImVec2(static_cast<float>(pt.x), static_cast<float>(pt.y));

    switch (message) {
    case WM_LBUTTONDOWN:
        cached_left = true;
        break;
    case WM_LBUTTONUP:
        cached_left = false;
        break;
    case WM_RBUTTONDOWN:
        cached_right = true;
        break;
    case WM_RBUTTONUP:
        cached_right = false;
        break;
    case WM_MOUSEMOVE:
        cached_x = static_cast<float>(GET_X_LPARAM(lparam));
        cached_y = static_cast<float>(GET_Y_LPARAM(lparam));
        break;
    default:
        break;
    }

    if (message == WM_SETTEXT || message == WM_GETTEXT || message == WM_GETTEXTLENGTH) {
        return ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    if (message == WM_RBUTTONUP) {
        right_mouse_down = false;
    }
    if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) {
        right_mouse_down = true;
    }

    if (right_mouse_down) {
        io.MouseDown[0] = cached_left;
        io.MouseDown[1] = cached_right;
        if (cached_x >= 0.0f && cached_y >= 0.0f) {
            io.MousePos = ImVec2(cached_x, cached_y);
        }
        return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                                  : ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    if (message == WM_LBUTTONDOWN && !g_dragging_initialized) {
        if (io.WantCaptureMouse) {
            g_is_dragging = true;
            g_is_dragging_imgui = true;
        } else {
            g_is_dragging = true;
            g_is_dragging_imgui = false;
            return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                                      : ::DefWindowProcW(hwnd, message, wparam, lparam);
        }
        cached_left = g_is_dragging;
    }

    if (message == WM_LBUTTONUP) {
        cached_left = false;
        g_is_dragging = false;
        g_is_dragging_imgui = false;
        g_dragging_initialized = false;
    }

    if (g_is_dragging) {
        cached_left = true;
        g_dragging_initialized = true;
        if (g_is_dragging_imgui) {
            ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
            io.MouseDown[0] = cached_left;
            io.MouseDown[1] = cached_right;
            if (cached_x >= 0.0f && cached_y >= 0.0f) {
                io.MousePos = ImVec2(cached_x, cached_y);
            }
            return TRUE;
        }

        io.MouseDown[0] = cached_left;
        io.MouseDown[1] = cached_right;
        if (cached_x >= 0.0f && cached_y >= 0.0f) {
            io.MousePos = ImVec2(cached_x, cached_y);
        }
        return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                                  : ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);

    io.MouseDown[0] = cached_left;
    io.MouseDown[1] = cached_right;
    if (cached_x >= 0.0f && cached_y >= 0.0f) {
        io.MousePos = ImVec2(cached_x, cached_y);
    }

    if (io.WantCaptureMouse &&
        (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK ||
         message == WM_RBUTTONDBLCLK || message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ||
         message == WM_LBUTTONUP || message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
        return TRUE;
    }

    if (io.WantCaptureKeyboard && io.WantTextInput &&
        (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR)) {
        return TRUE;
    }

    return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                              : ::DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK SafeWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    __try {
        return WndProc(hwnd, message, wparam, lparam);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return g_original_wndproc ? ::CallWindowProcW(g_original_wndproc, hwnd, message, wparam, lparam)
                                  : ::DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool AttachWndProc() {
    if (g_wndproc_attached) {
        return true;
    }

    g_game_window = GW::render::GetWindowHandle();
    if (!g_game_window) {
        return false;
    }

    ::SetLastError(0);
    g_original_wndproc =
        reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(g_game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&SafeWndProc)));
    if (g_original_wndproc == nullptr && ::GetLastError() != 0) {
        return false;
    }

    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = g_game_window;
    ::RegisterRawInputDevices(&rid, 1, sizeof(rid));

    g_wndproc_attached = true;
    return true;
}

void RestoreWndProc() {
    if (g_wndproc_attached && g_game_window && g_original_wndproc) {
        ::SetWindowLongPtrW(g_game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    }
    g_wndproc_attached = false;
    g_original_wndproc = nullptr;
    g_game_window = nullptr;
}

}  // namespace

namespace PY4GW::imgui {

bool Initialize(IDirect3DDevice9* device) {
    if (g_imgui_initialized) {
        return true;
    }

    g_game_window = GW::render::GetWindowHandle();
    if (!g_game_window || !device) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!SetupImGuiFonts()) {
        ImGui::DestroyContext();
        Logger::Instance().LogError("Failed to initialize ImGui font stack.");
        return false;
    }
    ApplyImGuiTheme();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    g_account_ini_applied = false;
    io.LogFilename = nullptr;
    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ApplyViewportStyleTweaks();

    if (!console_host_ui::Initialize()) {
        ImGui::DestroyContext();
        Logger::Instance().LogError("Failed to initialize ImGui console UI.");
        return false;
    }

    ImGui_ImplWin32_Init(g_game_window);
    ImGui_ImplDX9_Init(device);
    ImGui_ImplDX9_CreateDeviceObjects();

    if (!AttachWndProc()) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        console_host_ui::Shutdown();
        ImGui::DestroyContext();
        Logger::Instance().LogError("Failed to attach Guild Wars window procedure.");
        return false;
    }

    g_imgui_initialized = true;
    Logger::Instance().LogInfo("ImGui initialized on Guild Wars render device.");
    return true;
}

void Shutdown() {
    if (!g_imgui_initialized) {
        RestoreWndProc();
        return;
    }

    RestoreWndProc();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    console_host_ui::Shutdown();
    FontManager::Instance().Reset();
    ImGui::DestroyContext();
    g_imgui_initialized = false;
}

bool BeginFrame(IDirect3DDevice9* device) {
    if (!g_imgui_initialized && !Initialize(device)) {
        return false;
    }

    const HRESULT result = device->TestCooperativeLevel();
    if (FAILED(result)) {
        if (g_imgui_initialized) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            g_imgui_initialized = false;
        }
        return false;
    }

    ApplyAccountIniPath();

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return true;
}

bool RenderConsoleUi(bool* request_shutdown) {
    if (!RenderConsoleUiSafely(request_shutdown)) {
        Logger::Instance().LogError("ImGui console UI crashed during render; shutting overlay down.");
        if (g_shutdown_callback) {
            g_shutdown_callback();
        }
        return false;
    }

    if (request_shutdown != nullptr && *request_shutdown && g_shutdown_callback) {
        g_shutdown_callback();
    }
    return true;
}

void EndFrame(IDirect3DDevice9* device) {
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

void InvalidateDeviceObjects() {
    if (g_imgui_initialized) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}

void SetShutdownCallback(ShutdownCallback callback) {
    g_shutdown_callback = callback;
}

bool IsDockingEnabled() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return false;
    }
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0;
}

void SetDockingEnabled(bool enabled) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (enabled) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
    }
}

bool IsMultiViewportEnabled() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return false;
    }
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

void SetMultiViewportEnabled(bool enabled) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (enabled) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    }
    ApplyViewportStyleTweaks();
}

bool HasMultiViewportSupport() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return false;
    }
    const ImGuiBackendFlags flags = ImGui::GetIO().BackendFlags;
    const ImGuiBackendFlags required =
        static_cast<ImGuiBackendFlags>(ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_RendererHasViewports);
    return (flags & required) == required;
}

}  // namespace PY4GW::imgui

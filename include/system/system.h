#pragma once

#include "base/error_handling.h"

#include "base/logger.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <windows.h>

namespace PY4GW {

struct ConsoleMessage {
    std::string timestamp;
    std::string display_timestamp;
    std::string module_name;
    std::string level;
    MessageType message_type;
    std::string message;
};

class System {
public:
    static System& Instance();

    // Console output. Messages are screen-only unless output_to_file is enabled.
    // Named WriteConsoleMessage because WriteConsole is a Windows API macro.
    void WriteConsoleMessage(const std::string& module_name, MessageType message_type, const std::string& message);
    void WriteConsoleMessage(const std::string& module_name, const std::string& level, const std::string& message);
    // Buffer-only append; used by Logger to mirror file log lines onto the console.
    void AppendConsoleMessage(const std::string& module_name, MessageType message_type, const std::string& message);

    std::vector<ConsoleMessage> GetConsoleMessages() const;
    std::vector<ConsoleMessage> GetConsoleMessages(MessageType message_type) const;
    std::vector<ConsoleMessage> FilterConsoleMessages(const std::string& module_name, const std::string& level, const std::string& contains) const;
    void ClearConsoleMessages();

    void SetOutputToFile(bool enabled);
    bool GetOutputToFile() const;

    // Console window visibility. The host render path reads these each frame,
    // so scripts can show/hide the consoles without the UI being mandatory.
    void SetDrawConsole(bool enabled);
    bool GetDrawConsole() const;
    void SetDrawCompactConsole(bool enabled);
    bool GetDrawCompactConsole() const;
    void ToggleConsole();
    void ToggleCompactConsole();

    // Shutdown confirmation prompt. The console host renders the modal while
    // pending, so it can be raised from anywhere (UI close or Python helper).
    void RequestShutdownPrompt();
    void CancelShutdownPrompt();
    bool IsShutdownPromptPending() const;

    // Stamped once per rendered frame so GetTickCount64 is frame-coherent.
    void UpdateFrameTimestamp();

    // Account anchor. Stepped from the update loop after injection: if the
    // client sits in character select, Enter is pushed until a map loads,
    // then the account email becomes the stable anchor for persistence under
    // <dll_dir>/settings/<email>/ (the DLL directory, never the game folder).
    void UpdateAccountAnchor();
    bool HasAccountEmail() const;
    std::string GetAccountEmail() const;
    // Empty path until the account anchor is resolved.
    std::filesystem::path GetSettingsDirectory() const;
    static bool InCharacterSelectScreen();

    // Misc process-level helpers migrated from the legacy Py4GW module.
    // Returns the current frame timestamp when available, else the live tick.
    static uint64_t GetTickCount64();
    static std::string GetCredits();
    static std::string GetLicense();
    static bool ChangeWorkingDirectory(const std::string& new_directory);
    // Native save-file dialog (legacy parity); returns "" when cancelled.
    static std::string SaveFileDialog();

private:
    System() = default;
    ~System() = default;
    System(const System&) = delete;
    System& operator=(const System&) = delete;

    static std::string GetTimestamp(const char* format);
    // Legacy parity: console output echoes into the game chat while the full
    // console window is hidden.
    void MirrorToGameChat(const ConsoleMessage& entry);
    // Shared by both the normal (player_email) and fallback (HWND) anchor
    // paths in UpdateAccountAnchor(): latches account_email_, creates the
    // per-account settings directory, and logs readiness.
    void LatchAccountAnchor(const std::string& email);

    mutable std::mutex console_mutex_;
    std::vector<ConsoleMessage> console_messages_;
    size_t max_console_messages_ = 1000;
    std::atomic<bool> output_to_file_{false};
    // Default-surface policy: the full console is the default surface and
    // starts ON; other surfaces (compact today, more later) start OFF and
    // replace it when the user or persisted settings activate them.
    std::atomic<bool> draw_console_{true};
    std::atomic<bool> draw_compact_console_{false};
    std::atomic<bool> shutdown_prompt_pending_{false};
    std::atomic<uint64_t> frame_timestamp_{0};

    mutable std::mutex account_mutex_;
    std::string account_email_;
    std::atomic<bool> account_email_set_{false};
    uint64_t last_enter_push_tick_ = 0;
    // First tick UpdateAccountAnchor() observed a loaded map while still
    // unanchored; 0 means "not observed yet". Used to grace-period the
    // player_email wait before falling back to a synthetic per-process
    // identity (see UpdateAccountAnchor()).
    uint64_t map_loaded_since_tick_ = 0;
};

// Parity port of the legacy WindowCfg helper (legacy include/WindowCfg.h).
// All operations target the Guild Wars window resolved through MemoryManager.

struct BorderlessState {
    BOOL enabled = FALSE;
    BOOL draggable = TRUE;
    int resize_px = 8;  // border width for resize hit-test
    WNDPROC orig_proc = nullptr;
};

class WindowCfg {
public:
    /* ---------------- Window Geometry ---------------- */
    static void ResizeWindow(int width, int height);
    static void MoveWindowTo(int x, int y);
    static void SetWindowGeometry(int x, int y, int width, int height);

    /* ---------------- Query ---------------- */
    static std::tuple<int, int, int, int> GetWindowRectFn();
    static std::tuple<int, int, int, int> GetClientRectFn();

    /* ---------------- Focus / Z ---------------- */
    static void SetWindowTitle(const std::wstring& title);
    static void SetWindowActive();
    static void SetAlwaysOnTop(bool enable);
    static bool IsWindowFocused();
    static bool IsWindowActive();
    static bool IsWindowMinimized();
    static bool IsWindowInBackground();

    /* ---------------- Borderless helpers ---------------- */
    static BorderlessState* GetState(HWND hwnd);
    static void SetState(HWND hwnd, BorderlessState* state);
    static LRESULT HitTestBorderless(HWND hwnd, BorderlessState* state, LPARAM lparam);
    static LRESULT CALLBACK BorderlessProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static bool EnableTrueBorderless(HWND hwnd, bool enable, bool draggable, int resize_px);
    static void SetBorderless(bool enable);

    /* ---------------- Attention / Visibility ---------------- */
    static void Flash_Window(int repeat_count = 1);
    static void RequestAttention();

    static int GetZOrder();
    static void SetZOrder(int insert_after);
    static void SendWindowToBack();
    static void BringWindowToFront();

    static void TransparentClickThrough(bool enable);
    static void AdjustWindowOpacity(int alpha);
    static void HideWindow();
    static void ShowWindowAgain();
};

}  // namespace PY4GW

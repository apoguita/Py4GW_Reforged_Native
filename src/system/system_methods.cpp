#include "base/error_handling.h"

#include "system/system.h"

#include "base/memory_manager.h"
#include "base/process_manager.h"
#include "GW/chat/chat.h"
#include "GW/context/character.h"
#include "GW/context/context.h"
#include "GW/context/pregame.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"
#include "virtual_input/virtual_input.h"

#include <algorithm>

#include <commdlg.h>

namespace PY4GW {

/* ---------------- System console methods ---------------- */

void System::AppendConsoleMessage(const std::string& module_name, MessageType message_type, const std::string& message) {
    const ConsoleMessage entry{
        GetTimestamp("%Y-%m-%d %H:%M:%S"),
        GetTimestamp("%H:%M:%S"),
        module_name.empty() ? "Py4GW" : module_name,
        Logger::MessageTypeToLevel(message_type),
        message_type,
        message};

    {
        std::lock_guard<std::mutex> lock(console_mutex_);
        console_messages_.push_back(entry);
        if (console_messages_.size() > max_console_messages_) {
            console_messages_.erase(console_messages_.begin(), console_messages_.begin() + (console_messages_.size() - max_console_messages_));
        }
    }

    // Legacy parity: while the full console is hidden, console output is
    // echoed into the game chat. Deviation: gated on a loaded map, because
    // unlike legacy the full console starts hidden and bootstrap messages
    // would otherwise hit the chat path before the game is up. A reentrancy
    // guard stops chat-layer diagnostics from mirroring recursively.
    static thread_local bool mirroring = false;
    if (!mirroring && !draw_console_.load() && GW::map::GetIsMapLoaded()) {
        mirroring = true;
        MirrorToGameChat(entry);
        mirroring = false;
    }
}

void System::MirrorToGameChat(const ConsoleMessage& entry) {
    const std::string timestamp = GW::chat::FormatChatMessage("[" + entry.display_timestamp + "]", 180, 180, 180);
    const std::string module = GW::chat::FormatChatMessage("[" + entry.module_name + "]", 100, 190, 255);

    int r = 255, g = 255, b = 255;
    switch (entry.message_type) {
    case MessageType::Error: r = 255; g = 0; b = 0; break;
    case MessageType::Warning: r = 255; g = 255; b = 0; break;
    case MessageType::Success: r = 0; g = 255; b = 0; break;
    case MessageType::Debug: r = 0; g = 255; b = 255; break;
    case MessageType::Performance: r = 255; g = 153; b = 0; break;
    case MessageType::Notice: r = 153; g = 255; b = 153; break;
    case MessageType::Info:
    default:
        break;
    }

    std::string formatted_message = entry.message;
    std::replace(formatted_message.begin(), formatted_message.end(), '\\', '/');
    const std::string formatted_chunk = GW::chat::FormatChatMessage(formatted_message, r, g, b);

    GW::chat::SendFakeChat(static_cast<int>(GW::chat::Channel::CHANNEL_EMOTE),
        timestamp + " " + module + " " + formatted_chunk);
}

void System::WriteConsoleMessage(const std::string& module_name, MessageType message_type, const std::string& message) {
    AppendConsoleMessage(module_name, message_type, message);
    if (output_to_file_.load()) {
        Logger::Instance().WriteFileLine(module_name.empty() ? "Py4GW" : module_name, Logger::MessageTypeToLevel(message_type), message);
    }
}

void System::WriteConsoleMessage(const std::string& module_name, const std::string& level, const std::string& message) {
    WriteConsoleMessage(module_name, Logger::LevelToMessageType(level), message);
}

std::vector<ConsoleMessage> System::GetConsoleMessages() const {
    std::lock_guard<std::mutex> lock(console_mutex_);
    return console_messages_;
}

std::vector<ConsoleMessage> System::GetConsoleMessages(MessageType message_type) const {
    std::lock_guard<std::mutex> lock(console_mutex_);
    std::vector<ConsoleMessage> result;
    for (const auto& entry : console_messages_) {
        if (entry.message_type == message_type) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<ConsoleMessage> System::FilterConsoleMessages(const std::string& module_name, const std::string& level, const std::string& contains) const {
    std::lock_guard<std::mutex> lock(console_mutex_);
    std::vector<ConsoleMessage> result;
    for (const auto& entry : console_messages_) {
        if (!module_name.empty() && entry.module_name != module_name) {
            continue;
        }
        if (!level.empty() && entry.level != level) {
            continue;
        }
        if (!contains.empty() && entry.message.find(contains) == std::string::npos) {
            continue;
        }
        result.push_back(entry);
    }
    return result;
}

void System::ClearConsoleMessages() {
    std::lock_guard<std::mutex> lock(console_mutex_);
    console_messages_.clear();
}

void System::SetOutputToFile(bool enabled) {
    output_to_file_.store(enabled);
}

bool System::GetOutputToFile() const {
    return output_to_file_.load();
}

/* ---------------- System console window visibility ---------------- */

void System::SetDrawConsole(bool enabled) {
    draw_console_.store(enabled);
}

bool System::GetDrawConsole() const {
    return draw_console_.load();
}

void System::SetDrawCompactConsole(bool enabled) {
    draw_compact_console_.store(enabled);
}

bool System::GetDrawCompactConsole() const {
    return draw_compact_console_.load();
}

void System::ToggleConsole() {
    draw_console_.store(!draw_console_.load());
}

void System::ToggleCompactConsole() {
    draw_compact_console_.store(!draw_compact_console_.load());
}

/* ---------------- System shutdown prompt ---------------- */

void System::RequestShutdownPrompt() {
    shutdown_prompt_pending_.store(true);
}

void System::CancelShutdownPrompt() {
    shutdown_prompt_pending_.store(false);
}

bool System::IsShutdownPromptPending() const {
    return shutdown_prompt_pending_.load();
}

/* ---------------- System account anchor ---------------- */

bool System::InCharacterSelectScreen() {
    const GW::Context::PreGameContext* pregame = GW::Context::GetPreGameContext();
    if (!pregame || !pregame->chars_buffer || !pregame->chars_count) {
        return false;
    }
    uint32_t ui_state = 10;
    GW::ui::SendUIMessage(GW::ui::UIMessage::kCheckUIState, nullptr, &ui_state);
    return ui_state == 2;
}

void System::UpdateAccountAnchor() {
    if (account_email_set_.load()) {
        return;
    }

    if (GW::map::GetIsMapLoaded()) {
        const GW::Context::CharContext* char_context = GW::Context::GetCharContext();
        if (!char_context || !char_context->player_email[0]) {
            return;
        }

        std::string email;
        for (const wchar_t ch : char_context->player_email) {
            if (!ch) {
                break;
            }
            email.push_back(ch > 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
        }
        if (email.empty()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(account_mutex_);
            account_email_ = email;
        }
        account_email_set_.store(true);

        std::error_code ec;
        std::filesystem::create_directories(GetSettingsDirectory(), ec);
        WriteConsoleMessage("Py4GW", MessageType::Notice, "Account anchor ready: " + email);
        return;
    }

    // Not in a map yet: if we sit in character select, push Enter to log the
    // selected character in, retrying until the map starts loading.
    if (InCharacterSelectScreen()) {
        const uint64_t now = ::GetTickCount64();
        if (now - last_enter_push_tick_ >= 2000) {
            last_enter_push_tick_ = now;
            KeyHandler().push_key(VK_RETURN);
        }
    }
}

bool System::HasAccountEmail() const {
    return account_email_set_.load();
}

std::string System::GetAccountEmail() const {
    std::lock_guard<std::mutex> lock(account_mutex_);
    return account_email_;
}

std::filesystem::path System::GetSettingsDirectory() const {
    if (!account_email_set_.load()) {
        return {};
    }
    std::lock_guard<std::mutex> lock(account_mutex_);
    return process_manager::GetModuleDirectory() / "settings" / account_email_;
}

/* ---------------- System misc helpers ---------------- */

void System::UpdateFrameTimestamp() {
    frame_timestamp_.store(::GetTickCount64());
}

uint64_t System::GetTickCount64() {
    const uint64_t frame_timestamp = Instance().frame_timestamp_.load();
    return frame_timestamp ? frame_timestamp : ::GetTickCount64();
}

std::string System::GetCredits() {
    return "Py4GW Reforged, Apoguita - 2024,2026";
}

std::string System::GetLicense() {
    return std::string("MIT License\n\n") + "Copyright " + GetCredits() +
        "\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in\n"
        "all copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n"
        "THE SOFTWARE.\n";
}

bool System::ChangeWorkingDirectory(const std::string& new_directory) {
    const std::wstring wide_directory(new_directory.begin(), new_directory.end());
    return ::SetCurrentDirectoryW(wide_directory.c_str()) != 0;
}

std::string System::SaveFileDialog() {
    OPENFILENAMEA ofn;
    char file[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

/* ---------------- WindowCfg public operations ---------------- */

void WindowCfg::ResizeWindow(int width, int height) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    RECT r;
    GetWindowRect(hwnd, &r);
    MoveWindow(hwnd, r.left, r.top, width, height, TRUE);
}

void WindowCfg::MoveWindowTo(int x, int y) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    RECT r;
    GetWindowRect(hwnd, &r);
    MoveWindow(hwnd, x, y, r.right - r.left, r.bottom - r.top, TRUE);
}

void WindowCfg::SetWindowGeometry(int x, int y, int width, int height) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    RECT r = {x, y, x + width, y + height};
    AdjustWindowRectEx(&r,
        GetWindowLong(hwnd, GWL_STYLE),
        FALSE,
        GetWindowLong(hwnd, GWL_EXSTYLE));

    MoveWindow(hwnd, x - 8, y, r.right - r.left, height + 8, TRUE);
}

std::tuple<int, int, int, int> WindowCfg::GetWindowRectFn() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    RECT r{};
    if (hwnd && GetWindowRect(hwnd, &r)) {
        return {r.left, r.top, r.right, r.bottom};
    }
    return {0, 0, 0, 0};
}

std::tuple<int, int, int, int> WindowCfg::GetClientRectFn() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    RECT r{};
    if (hwnd && GetClientRect(hwnd, &r)) {
        return {r.left, r.top, r.right, r.bottom};
    }
    return {0, 0, 0, 0};
}

void WindowCfg::SetWindowTitle(const std::wstring& title) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (hwnd) {
        SetWindowTextW(hwnd, title.c_str());
    }
}

void WindowCfg::SetWindowActive() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);
}

void WindowCfg::SetAlwaysOnTop(bool enable) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;
    SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool WindowCfg::IsWindowFocused() {
    return MemoryManager::GetGWWindowHandle() == GetFocus();
}

bool WindowCfg::IsWindowActive() {
    return MemoryManager::GetGWWindowHandle() == GetActiveWindow();
}

bool WindowCfg::IsWindowMinimized() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    return hwnd && IsIconic(hwnd);
}

bool WindowCfg::IsWindowInBackground() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    return hwnd && GetActiveWindow() != hwnd;
}

void WindowCfg::SetBorderless(bool enable) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (hwnd) {
        EnableTrueBorderless(hwnd, enable, false, 8);
    }
}

void WindowCfg::Flash_Window(int repeat_count) {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    FLASHWINFO fw{sizeof(fw), hwnd, FLASHW_TRAY, static_cast<UINT>(repeat_count), 0};
    FlashWindowEx(&fw);
}

void WindowCfg::RequestAttention() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    FLASHWINFO fw{sizeof(fw), hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, UINT_MAX, 0};
    FlashWindowEx(&fw);
}

// Get current Z-order index (lower index = closer to top)
int WindowCfg::GetZOrder() {
    const HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return -1;

    int z = 0;
    for (HWND h = hwnd; h != NULL; h = GetWindow(h, GW_HWNDPREV)) {
        z++;
    }
    return z;
}

// Set explicit Z-order relative to another window
void WindowCfg::SetZOrder(int insert_after) {
    const HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    SetWindowPos(
        hwnd,
        reinterpret_cast<HWND>(static_cast<intptr_t>(insert_after)),
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WindowCfg::SendWindowToBack() {
    SetZOrder(static_cast<int>(reinterpret_cast<intptr_t>(HWND_BOTTOM)));
}

void WindowCfg::BringWindowToFront() {
    SetZOrder(static_cast<int>(reinterpret_cast<intptr_t>(HWND_TOP)));
}

// Make the window transparent to mouse clicks (click-through) and/or normal
void WindowCfg::TransparentClickThrough(bool enable) {
    const HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) {
        ex_style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        ex_style &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
    // required to apply
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

// Adjust window opacity [0-255]
void WindowCfg::AdjustWindowOpacity(int alpha) {
    const HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (!hwnd) return;

    LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex_style & WS_EX_LAYERED)) {
        SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
    }

    const BYTE clamped = static_cast<BYTE>(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
    SetLayeredWindowAttributes(hwnd, 0, clamped, LWA_ALPHA);
}

void WindowCfg::HideWindow() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

void WindowCfg::ShowWindowAgain() {
    HWND hwnd = MemoryManager::GetGWWindowHandle();
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
    }
}

}  // namespace PY4GW

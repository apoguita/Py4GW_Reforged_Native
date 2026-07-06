#include "base/error_handling.h"

#include "imgui/font_manager.h"

#include "IconsFontAwesome5.h"
#include "base/logger.h"
#include "base/process_manager.h"

#include <imgui.h>

#include <filesystem>

// ImGui 1.92 font model. Fonts are dynamic/scalable: a single ImFont renders at
// any size via ImGui::PushFont(font, size). So instead of the pre-1.92 scheme
// (the same face baked at 24 fixed sizes, with Font Awesome merged into only one
// of them - which left icons rendering on the default font only), we load ONE
// font per style (Regular/Bold/Italic/BoldItalic), merge Font Awesome into EACH,
// and render every FontId at its designed size. Icons therefore work in every
// style and size.
namespace PY4GW::imgui {

namespace {

const char* StyleFile(int style) {
    switch (style) {
    case 0: return "friz-quadrata-std-medium-5870338ec7ef8.otf";      // Regular
    case 1: return "friz-quadrata-std-bold-587034a220f9f.otf";        // Bold
    case 2: return "friz-quadrata-std-italic-587033b2c95df.otf";      // Italic
    case 3: return "friz-quadrata-std-bold_italic-587033d6d4298.otf"; // BoldItalic
    default: return "friz-quadrata-std-medium-5870338ec7ef8.otf";
    }
}

constexpr float kSizes[6] = {14.0f, 22.0f, 30.0f, 46.0f, 62.0f, 124.0f};

// The designed size the style fonts (and the merged icons) are loaded at. Under
// 1.92 this is the LegacySize / default; actual rendering size is supplied per
// use by ImGui::PushFont(font, size).
constexpr float kDesignSize = 14.0f;

}  // namespace

FontManager& FontManager::Instance() {
    static FontManager instance;
    return instance;
}

int FontManager::StyleOf(FontId id) {
    return static_cast<int>(id) / 6;  // 6 sizes per style, in enum order
}

float FontManager::SizeOf(FontId id) {
    return kSizes[static_cast<int>(id) % 6];
}

bool FontManager::Initialize() {
    if (initialized_) {
        return true;
    }

    if (!ResolveFontDirectory()) {
        Logger::Instance().LogError("ImGui fonts directory was not found.");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    for (int style = 0; style < 4; ++style) {
        style_fonts_[style] = LoadStyleFont(style);
    }

    default_font_ = style_fonts_[0];  // Regular
    if (!default_font_) {
        default_font_ = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
        Logger::Instance().LogError("Failed to load primary UI font; using ImGui default.");
    } else {
        io.FontDefault = default_font_;
    }

    initialized_ = true;
    return default_font_ != nullptr;
}

void FontManager::Reset() {
    for (ImFont*& font : style_fonts_) {
        font = nullptr;
    }
    default_font_ = nullptr;
    initialized_ = false;
}

ImFont* FontManager::Get(FontId id) {
    if (!initialized_ && !Initialize()) {
        return default_font_;
    }
    ImFont* font = style_fonts_[StyleOf(id)];
    return font ? font : default_font_;
}

float FontManager::GetSize(FontId id) const {
    return SizeOf(id);
}

ImFont* FontManager::GetDefaultFont() const {
    return default_font_;
}

ImFont* FontManager::LoadStyleFont(int style) {
    const char* file_name = StyleFile(style);
    const std::string path = BuildFontPath(file_name);
    if (path.empty()) {
        Logger::Instance().LogError(std::string("Font file is missing: ") + file_name);
        return nullptr;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    ImFont* loaded = io.Fonts->AddFontFromFileTTF(path.c_str(), kDesignSize, &config);
    if (!loaded) {
        Logger::Instance().LogError(std::string("Failed to load font: ") + file_name);
        return nullptr;
    }

    // Merge Font Awesome into THIS style font so icons render in every style/size.
    MergeFontAwesomeIntoLast(kDesignSize);
    return loaded;
}

bool FontManager::MergeFontAwesomeIntoLast(float size) {
    const std::string regular_path = BuildFontPath("Font Awesome 6 Free-Regular-400.otf");
    const std::string solid_path = BuildFontPath("Font Awesome 6 Free-Solid-900.otf");
    if (regular_path.empty() || solid_path.empty()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.MergeMode = true;  // merge into the just-added style font
    config.PixelSnapH = true;
    // Merged at the SAME designed size as the style font, so both scale together
    // under PushFont(font, size) and icons track text size. (Merging at a
    // *different* explicit size over the style font asserts in 1.92 - CHANGELOG #9361.)

    static const ImWchar icon_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    bool ok = true;
    if (!io.Fonts->AddFontFromFileTTF(regular_path.c_str(), size, &config, icon_ranges)) {
        Logger::Instance().LogError("Failed to load Font Awesome regular font.");
        ok = false;
    }
    if (!io.Fonts->AddFontFromFileTTF(solid_path.c_str(), size, &config, icon_ranges)) {
        Logger::Instance().LogError("Failed to load Font Awesome solid font.");
        ok = false;
    }
    return ok;
}

bool FontManager::ResolveFontDirectory() {
    if (!font_dir_.empty() && std::filesystem::exists(font_dir_)) {
        return true;
    }

    std::filesystem::path directory = PY4GW::process_manager::GetModuleDirectory() / "fonts";
    if (std::filesystem::exists(directory)) {
        font_dir_ = directory.string();
        return true;
    }

    directory = PY4GW::process_manager::GetProcessDirectory() / "fonts";
    if (std::filesystem::exists(directory)) {
        font_dir_ = directory.string();
        return true;
    }

    font_dir_.clear();
    return false;
}

std::string FontManager::BuildFontPath(const char* file_name) const {
    if (font_dir_.empty()) {
        return {};
    }

    const std::filesystem::path path = std::filesystem::path(font_dir_) / file_name;
    if (!std::filesystem::exists(path)) {
        return {};
    }
    return path.string();
}

}  // namespace PY4GW::imgui

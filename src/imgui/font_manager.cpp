#include "base/error_handling.h"

#include "imgui/font_manager.h"

#include "IconsFontAwesome5.h"
#include "base/logger.h"
#include "base/process_manager.h"

#include <imgui.h>

#include <filesystem>

namespace PY4GW::imgui {

namespace {

struct FontDefinition {
    const char* file_name;
    float size;
};

FontDefinition GetFontDefinition(FontId id) {
    switch (id) {
    case FontId::Regular14: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 14.0f };
    case FontId::Regular22: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 22.0f };
    case FontId::Regular30: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 30.0f };
    case FontId::Regular46: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 46.0f };
    case FontId::Regular62: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 62.0f };
    case FontId::Regular124: return { "friz-quadrata-std-medium-5870338ec7ef8.otf", 124.0f };
    case FontId::Bold14: return { "friz-quadrata-std-bold-587034a220f9f.otf", 14.0f };
    case FontId::Bold22: return { "friz-quadrata-std-bold-587034a220f9f.otf", 22.0f };
    case FontId::Bold30: return { "friz-quadrata-std-bold-587034a220f9f.otf", 30.0f };
    case FontId::Bold46: return { "friz-quadrata-std-bold-587034a220f9f.otf", 46.0f };
    case FontId::Bold62: return { "friz-quadrata-std-bold-587034a220f9f.otf", 62.0f };
    case FontId::Bold124: return { "friz-quadrata-std-bold-587034a220f9f.otf", 124.0f };
    case FontId::Italic14: return { "friz-quadrata-std-italic-587033b2c95df.otf", 14.0f };
    case FontId::Italic22: return { "friz-quadrata-std-italic-587033b2c95df.otf", 22.0f };
    case FontId::Italic30: return { "friz-quadrata-std-italic-587033b2c95df.otf", 30.0f };
    case FontId::Italic46: return { "friz-quadrata-std-italic-587033b2c95df.otf", 46.0f };
    case FontId::Italic62: return { "friz-quadrata-std-italic-587033b2c95df.otf", 62.0f };
    case FontId::Italic124: return { "friz-quadrata-std-italic-587033b2c95df.otf", 124.0f };
    case FontId::BoldItalic14: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 14.0f };
    case FontId::BoldItalic22: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 22.0f };
    case FontId::BoldItalic30: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 30.0f };
    case FontId::BoldItalic46: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 46.0f };
    case FontId::BoldItalic62: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 62.0f };
    case FontId::BoldItalic124: return { "friz-quadrata-std-bold_italic-587033d6d4298.otf", 124.0f };
    case FontId::Count: break;
    }
    PY4GW_UNREACHABLE("unknown FontId");
}

}  // namespace

FontManager& FontManager::Instance() {
    static FontManager instance;
    return instance;
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

    default_font_ = LoadFont(FontId::Regular14);
    if (!default_font_) {
        default_font_ = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
        Logger::Instance().LogError("Failed to load primary UI font; using ImGui default.");
    } else {
        io.FontDefault = default_font_;
    }

    if (!MergeFontAwesome()) {
        Logger::Instance().LogError("Failed to merge Font Awesome fonts into the ImGui atlas.");
    }

    for (int i = 0; i < static_cast<int>(FontId::Count); ++i) {
        if (i == static_cast<int>(FontId::Regular14)) {
            fonts_[i] = default_font_;
            continue;
        }
        fonts_[i] = LoadFont(static_cast<FontId>(i));
    }

    initialized_ = true;
    return default_font_ != nullptr;
}

void FontManager::Reset() {
    for (ImFont*& font : fonts_) {
        font = nullptr;
    }
    default_font_ = nullptr;
    initialized_ = false;
}

ImFont* FontManager::Get(FontId id) {
    if (!initialized_ && !Initialize()) {
        return default_font_;
    }
    return fonts_[static_cast<int>(id)] ? fonts_[static_cast<int>(id)] : default_font_;
}

ImFont* FontManager::GetDefaultFont() const {
    return default_font_;
}

ImFont* FontManager::LoadFont(FontId id) {
    const FontDefinition definition = GetFontDefinition(id);
    const std::string path = BuildFontPath(definition.file_name);
    if (path.empty()) {
        Logger::Instance().LogError(std::string("Font file is missing: ") + definition.file_name);
        return nullptr;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    ImFont* loaded = io.Fonts->AddFontFromFileTTF(path.c_str(), definition.size, &config);
    if (!loaded) {
        Logger::Instance().LogError(std::string("Failed to load font: ") + definition.file_name);
    }
    return loaded;
}

bool FontManager::MergeFontAwesome() {
    const std::string regular_path = BuildFontPath("Font Awesome 6 Free-Regular-400.otf");
    const std::string solid_path = BuildFontPath("Font Awesome 6 Free-Solid-900.otf");

    if (regular_path.empty() || solid_path.empty()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.GlyphOffset = ImVec2(0.0f, 5.0f);

    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    bool ok = true;
    if (!io.Fonts->AddFontFromFileTTF(regular_path.c_str(), 20.0f, &config, icon_ranges)) {
        Logger::Instance().LogError("Failed to load Font Awesome regular font.");
        ok = false;
    }
    if (!io.Fonts->AddFontFromFileTTF(solid_path.c_str(), 20.0f, &config, icon_ranges)) {
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

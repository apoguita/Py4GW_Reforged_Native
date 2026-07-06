#pragma once

#include "base/error_handling.h"

#include <string>

struct ImFont;

namespace PY4GW::imgui {

// The 24 legacy font ids (4 styles x 6 sizes). Kept for the Python push_font(idx)
// API surface, but under ImGui 1.92 these are NOT 24 separate baked fonts: each
// resolves to one of 4 dynamic style fonts rendered at an explicit size via
// ImGui::PushFont(font, size). See font_manager.cpp.
enum class FontId : int {
    Regular14,
    Regular22,
    Regular30,
    Regular46,
    Regular62,
    Regular124,
    Bold14,
    Bold22,
    Bold30,
    Bold46,
    Bold62,
    Bold124,
    Italic14,
    Italic22,
    Italic30,
    Italic46,
    Italic62,
    Italic124,
    BoldItalic14,
    BoldItalic22,
    BoldItalic30,
    BoldItalic46,
    BoldItalic62,
    BoldItalic124,
    Count
};

// ImGui 1.92 font model: fonts are dynamic/scalable. We load ONE font per style
// (Regular/Bold/Italic/BoldItalic), merge Font Awesome into EACH once, and render
// any FontId at its designed pixel size with ImGui::PushFont(Get(id), GetSize(id)).
// This replaces the pre-1.92 "same font baked at 24 fixed sizes, icons merged into
// only one" scheme, under which Font Awesome icons rendered on the default font only.
class FontManager {
public:
    static FontManager& Instance();

    bool Initialize();
    void Reset();

    // Dynamic style font backing this id (Regular/Bold/Italic/BoldItalic). Render
    // it at GetSize(id) via ImGui::PushFont(font, size).
    ImFont* Get(FontId id);
    // Designed pixel size for this id (14/22/30/46/62/124).
    float GetSize(FontId id) const;
    ImFont* GetDefaultFont() const;

    static int StyleOf(FontId id);   // 0=Regular,1=Bold,2=Italic,3=BoldItalic
    static float SizeOf(FontId id);

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    // Load one style font and merge Font Awesome (regular + solid) into it.
    ImFont* LoadStyleFont(int style);
    bool MergeFontAwesomeIntoLast(float size);
    bool ResolveFontDirectory();
    std::string BuildFontPath(const char* file_name) const;

    std::string font_dir_;
    ImFont* style_fonts_[4] = {};    // Regular, Bold, Italic, BoldItalic
    ImFont* default_font_ = nullptr;
    bool initialized_ = false;
};

}  // namespace PY4GW::imgui

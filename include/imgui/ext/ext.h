#pragma once

#include "base/error_handling.h"

#include "base/py_bindings.h"

#include <cstdint>
#include <string>

// PyImGui.Ext -- composite widgets built in C++ on top of core ImGui.
//
// These are higher-level than the 1:1 core bindings: they pull in GW::textures and
// fold several ImGui calls into one crossing. So they live in their OWN module and
// namespace (PY4GW::ext), encapsulated in the Ext class -- deliberately NOT mixed
// into the imgui binding domains (PY4GW::imgui_bindings). Every future niche
// extension gets the same isolation instead of piling into a shared namespace.

namespace PY4GW::ext {

namespace py = pybind11;

// Composite ImGui widgets. Stateless static methods; Python reaches them as
// PyImGui.Ext.<name>. The helpers stay private so the implementation surface is
// neither exposed nor shared with other code.
class Ext {
public:
    // Label-less button with a cached texture drawn over its rect (the ImageButton
    // look). `label` must be a unique "##id". Returns true on click. The texture is
    // resolved through the TextureManager cache (a fast keyed lookup, no per-call load).
    static bool ImageButton(const std::string& label, const std::string& texture_path, float width,
                            float height, bool disabled);

    // One launch-bar tile in a single crossing: position the cursor, draw the face
    // (a textured button when `texture_path` is set, else a label button), attach an
    // optional tooltip, and stamp an optional active overlay (filled + outlined rect).
    // Returns true on click. `overlay_fill`/`overlay_outline` are packed ImU32 colors;
    // pass 0 to skip either.
    static bool IconTile(const std::string& label, float x, float y, float width, float height,
                         const std::string& texture_path, bool disabled, const std::string& tooltip,
                         std::uint32_t overlay_fill, std::uint32_t overlay_outline);

private:
    // Draw a cached texture over the LAST item's rect (call right after the button).
    static void DrawItemTexture(const std::string& texture_path, float width, float height, bool disabled);

    // Shared button-with-texture core behind ImageButton and IconTile's textured face.
    static bool ImageButtonImpl(const std::string& label, const std::string& texture_path, float width,
                                float height, bool disabled);
};

// Register the PyImGui.Ext submodule (and its nested LaunchBar submodule) onto the
// parent PyImGui module. Called from the imgui_bindings.cpp assembly step.
void register_ext(py::module_& parent);

}  // namespace PY4GW::ext

#include "base/error_handling.h"

#include "imgui/bindings.h"
#include "imgui/addons/hotkey_demo.h"
#include "imgui/addons/markdown_demo.h"

#include <imgui.h>
#include <ImGuiFileBrowser.h>
#include <imgui_memory_editor.h>
#include <im_anim.h>
#include <TextEditor.h>

#include <pybind11/stl.h>

#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// imgui_markdown.h and imHotKey.h are not included here on purpose: the former
// has non-inline functions, the latter needs legacy key-state shims. Both are
// reached through bridges in their *_demo.cpp translation units.

namespace PY4GW::imgui_bindings {

namespace {

void register_filebrowser(py::module_& parent) {
    py::module_ m = parent.def_submodule("filebrowser", "ImGui-Addons file picker.");

    py::enum_<imgui_addons::ImGuiFileBrowser::DialogMode>(m, "DialogMode")
        .value("SELECT", imgui_addons::ImGuiFileBrowser::DialogMode::SELECT)
        .value("OPEN", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)
        .value("SAVE", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE)
        .export_values();

    py::class_<imgui_addons::ImGuiFileBrowser>(m, "FileBrowser")
        .def(py::init<>())
        .def("show_file_dialog", [](imgui_addons::ImGuiFileBrowser& b,
                                     const std::string& label,
                                     imgui_addons::ImGuiFileBrowser::DialogMode mode,
                                     const std::pair<float, float>& size,
                                     const std::string& valid_types) {
                 return b.showFileDialog(label, mode, ImVec2(size.first, size.second), valid_types);
             },
             py::arg("label"),
             py::arg("mode"),
             py::arg("size") = std::make_pair(0.0f, 0.0f),
             py::arg("valid_types") = "*.*")
        .def("set_current_path", &imgui_addons::ImGuiFileBrowser::SetCurrentPath, py::arg("path"))
        .def("get_current_path", &imgui_addons::ImGuiFileBrowser::GetCurrentPath)
        .def("set_use_modal", &imgui_addons::ImGuiFileBrowser::SetUseModal, py::arg("modal"))
        .def_readwrite("selected_fn", &imgui_addons::ImGuiFileBrowser::selected_fn)
        .def_readwrite("selected_path", &imgui_addons::ImGuiFileBrowser::selected_path)
        .def_readwrite("ext", &imgui_addons::ImGuiFileBrowser::ext);
}

void register_hotkey(py::module_& parent) {
    py::module_ m = parent.def_submodule("hotkey", "Chorded-shortcut editor (ImHotKey).");

    struct PyHotKey {
        std::string name;
        std::string lib;
        unsigned int keys = 0;
    };

    py::class_<PyHotKey>(m, "HotKey")
        .def(py::init([](std::string name, std::string lib, unsigned int keys) {
                 return PyHotKey{std::move(name), std::move(lib), keys};
             }), py::arg("name"), py::arg("lib") = "", py::arg("keys") = 0)
        .def_readwrite("name", &PyHotKey::name)
        .def_readwrite("lib", &PyHotKey::lib)
        .def_readwrite("keys", &PyHotKey::keys)
        .def("__repr__", [](const PyHotKey& h) { return "<HotKey '" + h.name + "' keys=" + std::to_string(h.keys) + ">"; });

    m.def("edit", [](py::list hotkeys, const std::string& popup_label) {
              const int count = static_cast<int>(py::len(hotkeys));
              if (count <= 0) return;
              std::vector<PyHotKey*> objs;
              std::vector<const char*> names, libs;
              std::vector<unsigned int> keys;
              objs.reserve(count); names.reserve(count); libs.reserve(count); keys.reserve(count);
              for (auto item : hotkeys) {
                  PyHotKey& h = item.cast<PyHotKey&>();
                  objs.push_back(&h);
                  names.push_back(h.name.c_str());
                  libs.push_back(h.lib.c_str());
                  keys.push_back(h.keys);
              }
              PY4GW::imgui::addons::hotkey_demo::EditKeys(names.data(), libs.data(), keys.data(), count, popup_label.c_str());
              for (int i = 0; i < count; ++i) objs[i]->keys = keys[i];
          }, py::arg("hotkeys"), py::arg("popup_label"));

    m.def("key_lib", [](unsigned int keys) {
              char buf[256] = {};
              PY4GW::imgui::addons::hotkey_demo::KeyLib(keys, buf, sizeof(buf));
              return std::string(buf);
          }, py::arg("keys"), "Human-readable label for a chord bitmask.");
}

void register_markdown(py::module_& parent) {
    py::module_ m = parent.def_submodule("markdown", "GitHub-style markdown renderer (imgui_markdown).");
    m.def("render", [](const std::string& text) { PY4GW::imgui::addons::markdown_demo::RenderText(text.c_str(), text.size()); },
          py::arg("text"), "Render markdown text; links open in the default browser.");
}

void register_memory_editor(py::module_& parent) {
    py::module_ m = parent.def_submodule("memory_editor", "Hex memory viewer/editor (imgui_club).");

    py::class_<MemoryEditor>(m, "MemoryEditor")
        .def(py::init<>())
        .def_readwrite("read_only", &MemoryEditor::ReadOnly)
        .def_readwrite("open", &MemoryEditor::Open)
        .def("draw_contents", [](MemoryEditor& ed, py::buffer buf, size_t base_addr) {
                 py::buffer_info info = buf.request(!ed.ReadOnly);
                 ed.DrawContents(info.ptr, static_cast<size_t>(info.size * info.itemsize), base_addr);
             }, py::arg("data"), py::arg("base_addr") = 0,
             "Draw a hex view of a bytes/bytearray buffer (set read_only=False + pass a bytearray to edit).")
        .def("draw_window", [](MemoryEditor& ed, const std::string& title, py::buffer buf, size_t base_addr) {
                 py::buffer_info info = buf.request(!ed.ReadOnly);
                 ed.DrawWindow(title.c_str(), info.ptr, static_cast<size_t>(info.size * info.itemsize), base_addr);
             }, py::arg("title"), py::arg("data"), py::arg("base_addr") = 0);
}

void register_anim(py::module_& parent) {
    py::module_ m = parent.def_submodule("anim", "Tweening/easing helpers (ImAnim).");

    py::enum_<iam_ease_type>(m, "Ease")
        .value("Linear", iam_ease_linear)
        .value("InCubic", iam_ease_in_cubic)
        .value("OutCubic", iam_ease_out_cubic)
        .value("InOutCubic", iam_ease_in_out_cubic)
        .export_values();

    py::enum_<iam_policy>(m, "Policy")
        .value("Crossfade", iam_policy_crossfade)
        .value("Cut", iam_policy_cut)
        .value("Queue", iam_policy_queue)
        .export_values();

    m.def("update_begin_frame", []() { iam_update_begin_frame(); });
    m.def("gc", [](unsigned int max_age_frames) { iam_gc(max_age_frames); }, py::arg("max_age_frames") = 600);
    m.def("set_global_time_scale", [](float s) { iam_set_global_time_scale(s); }, py::arg("scale"));
    m.def("get_global_time_scale", []() { return iam_get_global_time_scale(); });

    m.def("tween_float", [](ImGuiID id, ImGuiID channel_id, float target, float dur, int ease, int policy, float dt, float init_value) {
              return iam_tween_float(id, channel_id, target, dur, iam_ease_preset(ease), policy, dt, init_value);
          }, py::arg("id"), py::arg("channel_id"), py::arg("target"), py::arg("duration"),
             py::arg("ease") = iam_ease_out_cubic, py::arg("policy") = iam_policy_crossfade,
             py::arg("dt") = 0.0f, py::arg("init_value") = 0.0f);

    m.def("oscillate", [](ImGuiID id, float amplitude, float frequency, int wave_type, float phase, float dt) {
              return iam_oscillate(id, amplitude, frequency, wave_type, phase, dt);
          }, py::arg("id"), py::arg("amplitude"), py::arg("frequency"),
             py::arg("wave_type") = 0, py::arg("phase") = 0.0f, py::arg("dt") = 0.0f);
}

// Map a lowercase language name to one of TextEditor's built-in language
// definitions. "none"/unknown clears highlighting (nullptr).
const TextEditor::Language* ResolveLanguage(const std::string& name) {
    static const std::unordered_map<std::string, const TextEditor::Language* (*)()> table = {
        {"c", &TextEditor::Language::C},
        {"cpp", &TextEditor::Language::Cpp},
        {"cs", &TextEditor::Language::Cs},
        {"angelscript", &TextEditor::Language::AngelScript},
        {"lua", &TextEditor::Language::Lua},
        {"python", &TextEditor::Language::Python},
        {"glsl", &TextEditor::Language::Glsl},
        {"hlsl", &TextEditor::Language::Hlsl},
        {"json", &TextEditor::Language::Json},
        {"markdown", &TextEditor::Language::Markdown},
        {"sql", &TextEditor::Language::Sql},
    };
    auto it = table.find(name);
    return it == table.end() ? nullptr : it->second();
}

void register_text_editor(py::module_& parent) {
    py::module_ m = parent.def_submodule("text_editor", "Syntax-highlighting code editor (ImGuiColorTextEdit).");

    py::class_<TextEditor>(m, "TextEditor")
        .def(py::init<>())

        // render
        .def("render", [](TextEditor& e, const std::string& title, std::pair<float, float> size, bool border) {
                 e.Render(title.c_str(), ImVec2(size.first, size.second), border);
             }, py::arg("title"), py::arg("size") = std::make_pair(0.0f, 0.0f), py::arg("border") = false,
             "Draw the editor. Size (0,0) fills the available region.")
        .def("set_focus", &TextEditor::SetFocus)

        // text access (UTF-8)
        .def("set_text", [](TextEditor& e, const std::string& t) { e.SetText(t); }, py::arg("text"))
        .def("get_text", &TextEditor::GetText)
        .def("clear_text", &TextEditor::ClearText)
        .def("is_empty", &TextEditor::IsEmpty)
        .def("get_line_count", &TextEditor::GetLineCount)
        .def("get_line_text", &TextEditor::GetLineText, py::arg("line"))

        // language / syntax highlighting
        .def("set_language", [](TextEditor& e, const std::string& name) {
                 std::string lower;
                 lower.reserve(name.size());
                 for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                 e.SetLanguage(ResolveLanguage(lower));
             }, py::arg("name"),
             "Select syntax highlighting: c, cpp, cs, angelscript, lua, python, glsl, hlsl, json, markdown, sql, or none.")
        .def("get_language_name", &TextEditor::GetLanguageName)
        .def("has_language", &TextEditor::HasLanguage)

        // color palette
        .def("set_dark_palette", [](TextEditor& e) { e.SetPalette(TextEditor::GetDarkPalette()); })
        .def("set_light_palette", [](TextEditor& e) { e.SetPalette(TextEditor::GetLightPalette()); })

        // editor options
        .def("set_read_only_enabled", &TextEditor::SetReadOnlyEnabled, py::arg("value"))
        .def("is_read_only_enabled", &TextEditor::IsReadOnlyEnabled)
        .def("set_show_line_numbers_enabled", &TextEditor::SetShowLineNumbersEnabled, py::arg("value"))
        .def("is_show_line_numbers_enabled", &TextEditor::IsShowLineNumbersEnabled)
        .def("set_show_whitespaces_enabled", &TextEditor::SetShowWhitespacesEnabled, py::arg("value"))
        .def("is_show_whitespaces_enabled", &TextEditor::IsShowWhitespacesEnabled)
        .def("set_auto_indent_enabled", &TextEditor::SetAutoIndentEnabled, py::arg("value"))
        .def("is_auto_indent_enabled", &TextEditor::IsAutoIndentEnabled)
        .def("set_tab_size", &TextEditor::SetTabSize, py::arg("value"))
        .def("get_tab_size", &TextEditor::GetTabSize)

        // clipboard / history
        .def("cut", &TextEditor::Cut)
        .def("copy", &TextEditor::Copy)
        .def("paste", &TextEditor::Paste)
        .def("undo", &TextEditor::Undo)
        .def("redo", &TextEditor::Redo)
        .def("can_undo", &TextEditor::CanUndo)
        .def("can_redo", &TextEditor::CanRedo)

        // cursors / selection (zero-based)
        .def("set_cursor", &TextEditor::SetCursor, py::arg("line"), py::arg("column"))
        .def("get_cursor_position", [](const TextEditor& e) {
                 TextEditor::CursorPosition p = e.GetCurrentCursorPosition();
                 return std::make_pair(p.line, p.column);
             }, "Returns (line, column) of the current cursor.")
        .def("select_all", &TextEditor::SelectAll)
        .def("select_line", &TextEditor::SelectLine, py::arg("line"))
        .def("clear_cursors", &TextEditor::ClearCursors)

        // scrolling
        .def("scroll_to_line", [](TextEditor& e, int line, int alignment) {
                 e.ScrollToLine(line, static_cast<TextEditor::Scroll>(alignment));
             }, py::arg("line"), py::arg("alignment") = 0,
             "alignment: 0=top, 1=middle, 2=bottom.")

        // find / replace
        .def("select_first_occurrence_of", [](TextEditor& e, const std::string& t, bool cs, bool ww) {
                 e.SelectFirstOccurrenceOf(t, cs, ww);
             }, py::arg("text"), py::arg("case_sensitive") = true, py::arg("whole_word") = false)
        .def("select_next_occurrence_of", [](TextEditor& e, const std::string& t, bool cs, bool ww) {
                 e.SelectNextOccurrenceOf(t, cs, ww);
             }, py::arg("text"), py::arg("case_sensitive") = true, py::arg("whole_word") = false)
        .def("select_all_occurrences_of", [](TextEditor& e, const std::string& t, bool cs, bool ww) {
                 e.SelectAllOccurrencesOf(t, cs, ww);
             }, py::arg("text"), py::arg("case_sensitive") = true, py::arg("whole_word") = false)
        .def("open_find_replace_window", &TextEditor::OpenFindReplaceWindow)
        .def("close_find_replace_window", &TextEditor::CloseFindReplaceWindow);
}

}  // namespace

void register_addons(py::module_& m) {
    register_filebrowser(m);
    register_hotkey(m);
    register_markdown(m);
    register_memory_editor(m);
    register_anim(m);
    register_text_editor(m);
}

}  // namespace PY4GW::imgui_bindings

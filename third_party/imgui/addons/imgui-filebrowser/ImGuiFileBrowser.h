#ifndef IMGUIFILEBROWSER_H
#define IMGUIFILEBROWSER_H

// Clean-room reimplementation of the ImGui-Addons file dialog.
//
// The original implementation was built on a bundled `dirent` shim plus a
// hand-maintained "current_dirlist" breadcrumb that had to be kept in sync with
// the path string by every navigation function. Any path that didn't flow
// through that exact bookkeeping (e.g. SetCurrentPath with an absolute path)
// left the breadcrumb empty and desynced, which both hid the navigation bar and
// made "go up a directory" pop_back() an empty vector -> undefined behaviour and
// a crash. This version keeps a single source of truth (a std::filesystem::path)
// and derives everything (breadcrumb, listing, navigation) from it, so those
// states can never disagree.
//
// The public surface (class name, DialogMode, showFileDialog, SetCurrentPath,
// GetCurrentPath, SetUseModal, selected_fn/selected_path/ext) is unchanged so
// existing call sites and the pybind binding keep compiling.

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgui_addons
{
    class ImGuiFileBrowser
    {
    public:
        ImGuiFileBrowser();
        ~ImGuiFileBrowser();

        enum class DialogMode
        {
            SELECT, // Pick an existing directory.
            OPEN,   // Pick an existing file.
            SAVE    // Pick/type a file name to write.
        };

        /* Call every frame while the popup should be shown. Returns true for the
         * single frame on which the user confirms a selection; read selected_path
         * / selected_fn / ext on that frame. `valid_types` is a comma separated
         * list of extensions (e.g. ".txt,.json") or "*.*" for everything.
         */
        bool showFileDialog(const std::string& label,
                            DialogMode mode,
                            const ImVec2& sz_xy = ImVec2(0, 0),
                            const std::string& valid_types = "*.*");

        void SetCurrentPath(const std::string& path);
        const std::string& GetCurrentPath() const noexcept { return current_path_str; }
        void SetUseModal(bool modal) noexcept { use_modal = modal; }

        // Valid only on a frame showFileDialog() returned true.
        std::string selected_fn;    // final path component (file or dir name)
        std::string selected_path;  // full path (dirs get a trailing '/')
        std::string ext;            // lowercase extension of the selection ("" for dirs)

    private:
        struct Entry
        {
            std::string name;                       // UTF-8 display name
            std::filesystem::path full;             // absolute path
            std::uintmax_t size = 0;
            std::int64_t mtime = 0;                 // unix seconds, 0 = unknown
            bool is_dir = false;
            bool is_hidden = false;
        };

        struct Crumb
        {
            std::string label;
            std::filesystem::path path;             // empty => drives / Computer view
        };

        struct ExtGroup
        {
            std::string label;
            std::vector<std::string> exts;          // lowercase, leading '.'
            bool all_files = false;
        };

        // Navigation (single source of truth is `current_dir`).
        void setDirectory(std::filesystem::path dir);
        void navigateUp();
        void refreshEntries();
        void loadDrives();
        void rebuildBreadcrumb();
        void updatePathString();

        // Filtering / sorting.
        void parseExtFilters(const std::string& valid_types);
        bool passesExtFilter(const std::string& name) const;
        void sortEntries();

        // Rendering regions.
        void renderToolbar();
        void renderFileList();
        void renderFooter();
        void renderErrorPopup();
        void renderNewFolderPopup();
        void renderOverwritePopup();

        // Actions.
        void requestConfirm();
        void commitFile(const std::filesystem::path& target);
        void commitDirectory(const std::filesystem::path& target);
        void closeDialog();
        void setError(std::string title, std::string message);

        // ---- State ----
        DialogMode dialog_mode = DialogMode::OPEN;
        std::filesystem::path current_dir;          // empty => drives view
        std::string current_path_str;               // GetCurrentPath() backing store

        std::vector<Entry> entries;
        std::vector<Crumb> breadcrumb;
        std::vector<ExtGroup> ext_groups;
        int ext_group_idx = 0;
        int selected_index = -1;                    // index into `entries`, or -1

        int sort_col = 0;                           // 0 name, 1 size, 2 date
        bool sort_asc = true;

        ImGuiTextFilter search_filter;
        char input_fn[512] = {0};
        char new_folder_name[256] = {0};

        std::string error_title;
        std::string error_msg;
        bool show_error = false;

        std::filesystem::path pending_overwrite;
        bool want_overwrite = false;
        bool open_new_folder = false;

        // Deferred so we never mutate `entries` while iterating it mid-render.
        std::filesystem::path pending_dir;
        bool has_pending_dir = false;
        bool pending_up = false;
        bool pending_confirm = false;

        bool selection_made = false;                // return value for showFileDialog()
        bool want_close = false;                    // close the dialog at parent-popup scope
        bool use_modal = true;
        bool show_hidden = false;

        ImVec2 min_size = ImVec2(560, 380);
    };
}

#endif // IMGUIFILEBROWSER_H

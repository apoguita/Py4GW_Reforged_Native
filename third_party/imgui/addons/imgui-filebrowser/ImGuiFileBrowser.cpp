#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "ImGuiFileBrowser.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace imgui_addons
{
namespace
{

// ---- UTF-8 <-> path conversion --------------------------------------------
// std::filesystem::path stores wchar_t on Windows; convert through UTF-8 so
// non-ASCII names round-trip correctly to/from ImGui and Python.
#ifdef _WIN32
std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty())
        return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string PathToUtf8(const fs::path& p) { return WideToUtf8(p.wstring()); }
fs::path Utf8ToPath(const std::string& s) { return fs::path(Utf8ToWide(s)); }
#else
std::string PathToUtf8(const fs::path& p) { return p.string(); }
fs::path Utf8ToPath(const std::string& s) { return fs::path(s); }
#endif

std::string ToLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int ICaseCompare(const std::string& a, const std::string& b)
{
    const size_t n = (a.size() < b.size()) ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i)
    {
        int ca = std::tolower(static_cast<unsigned char>(a[i]));
        int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }
    if (a.size() != b.size())
        return a.size() < b.size() ? -1 : 1;
    return 0;
}

// Extension including the leading dot, lowercased. "" if none.
std::string ExtensionOf(const std::string& name)
{
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
        return std::string();
    return ToLower(name.substr(dot));
}

std::string HumanSize(std::uintmax_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    char buf[32];
    if (bytes < 1024)
    {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return buf;
    }
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 5)
    {
        v /= 1024.0;
        ++u;
    }
    std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
    return buf;
}

std::int64_t ToUnixSeconds(fs::file_time_type t)
{
    // C++17 has no portable clock_cast; rebase onto system_clock via "now" on
    // both clocks. Good to a few ms, which is plenty for a mtime column.
    using namespace std::chrono;
    const auto sys = time_point_cast<system_clock::duration>(t - fs::file_time_type::clock::now() + system_clock::now());
    return static_cast<std::int64_t>(system_clock::to_time_t(sys));
}

std::string FormatTime(std::int64_t unix_seconds)
{
    if (unix_seconds == 0)
        return std::string();
    std::time_t tt = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
#ifdef _WIN32
    if (::localtime_s(&tm, &tt) != 0)
        return std::string();
#else
    if (::localtime_r(&tt, &tm) == nullptr)
        return std::string();
#endif
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d  %H:%M", &tm) == 0)
        return std::string();
    return buf;
}

// Strip trailing separators (except from a bare drive root like "C:\") so
// parent_path() behaves and breadcrumbs stay clean.
fs::path NormalizeDir(fs::path p)
{
    if (p.empty())
        return p;
    p = p.lexically_normal();
#ifdef _WIN32
    std::wstring w = p.wstring();
    while (w.size() > 1 && (w.back() == L'\\' || w.back() == L'/') &&
           !(w.size() == 3 && w[1] == L':'))
        w.pop_back();
    return fs::path(w);
#else
    std::string s = p.string();
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();
    return fs::path(s);
#endif
}

constexpr float kButtonWidth = 92.0f;
const ImVec4 kDirColor = ImVec4(0.882f, 0.745f, 0.078f, 1.0f); // yellow

} // namespace

// ---------------------------------------------------------------------------

ImGuiFileBrowser::ImGuiFileBrowser()
{
    std::error_code ec;
    current_dir = NormalizeDir(fs::current_path(ec));
    if (ec)
        current_dir.clear();
    updatePathString();
}

ImGuiFileBrowser::~ImGuiFileBrowser() = default;

void ImGuiFileBrowser::SetCurrentPath(const std::string& path)
{
    fs::path p = Utf8ToPath(path);
    if (!p.empty())
    {
        std::error_code ec;
        if (fs::exists(p, ec) && !fs::is_directory(p, ec))
            p = p.parent_path();
    }
    setDirectory(NormalizeDir(p));
}

void ImGuiFileBrowser::updatePathString()
{
    if (current_dir.empty())
    {
        current_path_str.clear();
        return;
    }
    current_path_str = PathToUtf8(current_dir);
    std::replace(current_path_str.begin(), current_path_str.end(), '\\', '/');
    if (!current_path_str.empty() && current_path_str.back() != '/')
        current_path_str += '/';
}

// ---- Navigation -----------------------------------------------------------

void ImGuiFileBrowser::setDirectory(fs::path dir)
{
    current_dir = std::move(dir);
    selected_index = -1;
    input_fn[0] = '\0';
    show_error = false;
    updatePathString();
    rebuildBreadcrumb();
    refreshEntries();
}

void ImGuiFileBrowser::navigateUp()
{
    if (current_dir.empty())
        return; // already at the drives / root listing

    fs::path parent = current_dir.parent_path();
    if (parent.empty() || parent == current_dir)
        setDirectory(fs::path()); // drive root -> drives view
    else
        setDirectory(NormalizeDir(parent));
}

void ImGuiFileBrowser::loadDrives()
{
#ifdef _WIN32
    const DWORD cap = 512;
    wchar_t buffer[cap];
    DWORD len = ::GetLogicalDriveStringsW(cap - 1, buffer);
    if (len == 0 || len >= cap)
    {
        setError("Error", "Could not enumerate drives.");
        return;
    }
    for (const wchar_t* d = buffer; *d != L'\0'; d += std::wcslen(d) + 1)
    {
        Entry e;
        e.full = fs::path(d);
        e.name = WideToUtf8(d);
        while (!e.name.empty() && (e.name.back() == '\\' || e.name.back() == '/'))
            e.name.pop_back(); // show "C:" rather than "C:\"
        e.is_dir = true;
        entries.push_back(std::move(e));
    }
#else
    setDirectory(fs::path("/"));
#endif
}

void ImGuiFileBrowser::refreshEntries()
{
    entries.clear();
    selected_index = -1;

    if (current_dir.empty())
    {
        loadDrives();
        sortEntries();
        return;
    }

    std::error_code ec;
    fs::directory_iterator it(current_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec)
    {
        setError("Error", "Cannot open directory:\n" + ec.message());
        return;
    }

    const fs::directory_iterator end;
    for (; it != end; it.increment(ec))
    {
        if (ec)
            break;
        const fs::directory_entry& de = *it;

        Entry e;
        e.full = de.path();
        e.name = PathToUtf8(de.path().filename());
        if (e.name.empty())
            continue;

        std::error_code ec2;
        e.is_dir = de.is_directory(ec2);

#ifdef _WIN32
        DWORD attrs = ::GetFileAttributesW(de.path().c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES)
        {
            if (attrs & FILE_ATTRIBUTE_SYSTEM)
                continue; // hide OS internals ($Recycle.Bin, pagefile, ...)
            e.is_hidden = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
        }
#else
        e.is_hidden = e.name[0] == '.';
#endif

        if (!e.is_dir)
        {
            e.size = de.file_size(ec2);
            if (ec2)
                e.size = 0;
        }
        std::error_code ec3;
        e.mtime = ToUnixSeconds(de.last_write_time(ec3));
        if (ec3)
            e.mtime = 0;

        entries.push_back(std::move(e));
    }

    sortEntries();
}

void ImGuiFileBrowser::rebuildBreadcrumb()
{
    breadcrumb.clear();
#ifdef _WIN32
    breadcrumb.push_back({"Computer", fs::path()});
    if (current_dir.empty())
        return;

    if (current_dir.has_root_name())
    {
        fs::path root = current_dir.root_path();
        breadcrumb.push_back({PathToUtf8(current_dir.root_name()), NormalizeDir(root)});
        fs::path accum = root;
        for (const fs::path& part : current_dir.relative_path())
        {
            accum /= part;
            breadcrumb.push_back({PathToUtf8(part), accum});
        }
    }
    else
    {
        // UNC or otherwise unusual; fall back to a single crumb.
        breadcrumb.push_back({current_path_str, current_dir});
    }
#else
    breadcrumb.push_back({"/", fs::path("/")});
    if (current_dir.empty())
        return;
    fs::path accum("/");
    for (const fs::path& part : current_dir.relative_path())
    {
        accum /= part;
        breadcrumb.push_back({PathToUtf8(part), accum});
    }
#endif
}

// ---- Filtering / sorting --------------------------------------------------

void ImGuiFileBrowser::parseExtFilters(const std::string& valid_types)
{
    ext_groups.clear();
    ext_group_idx = 0;

    if (dialog_mode == DialogMode::SELECT)
        return; // directory picker ignores extensions

    std::vector<std::string> specifics;
    bool all_files = false;

    std::istringstream iss(ToLower(valid_types));
    std::string tok;
    while (std::getline(iss, tok, ','))
    {
        // trim
        size_t a = tok.find_first_not_of(" \t");
        size_t b = tok.find_last_not_of(" \t");
        if (a == std::string::npos)
            continue;
        tok = tok.substr(a, b - a + 1);

        if (tok == "*.*" || tok == "*" || tok == ".*")
        {
            all_files = true;
            continue;
        }
        if (!tok.empty() && tok.front() == '*')
            tok.erase(tok.begin()); // "*.txt" -> ".txt"
        if (!tok.empty() && tok.front() != '.')
            tok.insert(tok.begin(), '.');
        if (!tok.empty())
            specifics.push_back(tok);
    }

    if (specifics.size() > 1)
    {
        ExtGroup g;
        g.label = "All Supported";
        g.exts = specifics;
        ext_groups.push_back(std::move(g));
    }
    for (const std::string& e : specifics)
    {
        ExtGroup g;
        g.label = "*" + e;
        g.exts.push_back(e);
        ext_groups.push_back(std::move(g));
    }
    if (all_files || specifics.empty())
    {
        ExtGroup g;
        g.label = "All Files (*.*)";
        g.all_files = true;
        ext_groups.push_back(std::move(g));
    }
}

bool ImGuiFileBrowser::passesExtFilter(const std::string& name) const
{
    if (ext_groups.empty() || ext_group_idx < 0 || ext_group_idx >= static_cast<int>(ext_groups.size()))
        return true;
    const ExtGroup& g = ext_groups[ext_group_idx];
    if (g.all_files)
        return true;
    const std::string e = ExtensionOf(name);
    for (const std::string& x : g.exts)
        if (e == x)
            return true;
    return false;
}

void ImGuiFileBrowser::sortEntries()
{
    const int col = sort_col;
    const bool asc = sort_asc;
    std::stable_sort(entries.begin(), entries.end(), [col, asc](const Entry& a, const Entry& b) {
        if (a.is_dir != b.is_dir)
            return a.is_dir; // directories always above files
        int c = 0;
        switch (col)
        {
        case 1: c = (a.size < b.size) ? -1 : (a.size > b.size ? 1 : 0); break;
        case 2: c = (a.mtime < b.mtime) ? -1 : (a.mtime > b.mtime ? 1 : 0); break;
        default: c = ICaseCompare(a.name, b.name); break;
        }
        if (c == 0)
            c = ICaseCompare(a.name, b.name);
        return asc ? c < 0 : c > 0;
    });
    selected_index = -1;
}

// ---- Rendering ------------------------------------------------------------

bool ImGuiFileBrowser::showFileDialog(const std::string& label, DialogMode mode, const ImVec2& sz_xy, const std::string& valid_types)
{
    dialog_mode = mode;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(min_size, io.DisplaySize);
    ImGui::SetNextWindowSize(ImVec2((sz_xy.x > min_size.x) ? sz_xy.x : min_size.x,
                                    (sz_xy.y > min_size.y) ? sz_xy.y : min_size.y),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    const bool open = use_modal ? ImGui::BeginPopupModal(label.c_str(), nullptr, flags)
                                : ImGui::BeginPopup(label.c_str(), flags);
    if (!open)
        return false;

    if (ImGui::IsWindowAppearing())
    {
        parseExtFilters(valid_types);
        selected_fn.clear();
        selected_path.clear();
        ext.clear();
        search_filter.Clear();
        want_close = false;
        pending_confirm = false;
        has_pending_dir = false;
        pending_up = false;
        want_overwrite = false;
        open_new_folder = false;
        setDirectory(current_dir); // fresh listing + breadcrumb, reset selection
    }

    selection_made = false;

    renderToolbar();
    renderFileList();

    // Apply navigation deferred out of the list loop so `entries` is never
    // mutated while being iterated.
    if (has_pending_dir)
    {
        setDirectory(NormalizeDir(pending_dir));
        has_pending_dir = false;
    }
    if (pending_up)
    {
        navigateUp();
        pending_up = false;
    }

    renderFooter();

    if (pending_confirm)
    {
        pending_confirm = false;
        requestConfirm();
    }

    renderNewFolderPopup();
    renderOverwritePopup();
    renderErrorPopup();

    // Close at parent-popup scope. commitFile()/closeDialog() may be reached from
    // inside a nested modal (e.g. the overwrite prompt), where CloseCurrentPopup
    // would target that child instead of this dialog; deferring the close to here
    // guarantees it hits the dialog itself.
    if (want_close)
    {
        ImGui::CloseCurrentPopup();
        want_close = false;
    }

    ImGui::EndPopup();

    const bool result = selection_made;
    selection_made = false;
    return result;
}

void ImGuiFileBrowser::renderToolbar()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::BeginDisabled(current_dir.empty());
    if (ImGui::Button("Up"))
        pending_up = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        refreshEntries();
    ImGui::SameLine();
    ImGui::BeginDisabled(current_dir.empty());
    if (ImGui::Button("New Folder"))
    {
        new_folder_name[0] = '\0';
        open_new_folder = true;
    }
    ImGui::EndDisabled();

    // Search box, right-aligned on the same row.
    const float search_w = 200.0f;
    ImGui::SameLine();
    float to_right = ImGui::GetContentRegionAvail().x - search_w;
    if (to_right > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + to_right);
    ImGui::SetNextItemWidth(search_w);
    search_filter.Draw("##fb_search");
    if (!search_filter.IsActive() && ImGui::IsItemHovered())
        ImGui::SetTooltip("Filter by name (space separated; -term excludes)");

    // Breadcrumb, on its own horizontally-scrolling row.
    const float crumb_h = ImGui::GetFrameHeightWithSpacing() + style.WindowPadding.y * 2.0f;
    ImGui::BeginChild("##fb_breadcrumb", ImVec2(0, crumb_h),
                      ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    for (size_t i = 0; i < breadcrumb.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const bool is_last = (i + 1 == breadcrumb.size());
        if (is_last)
            ImGui::PushStyleColor(ImGuiCol_Text, kDirColor);
        if (ImGui::Button(breadcrumb[i].label.c_str()) && !is_last)
        {
            pending_dir = breadcrumb[i].path;
            has_pending_dir = true;
        }
        if (is_last)
            ImGui::PopStyleColor();
        if (!is_last)
        {
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(">");
            ImGui::SameLine(0, 0);
        }
        ImGui::PopID();
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
}

void ImGuiFileBrowser::renderFileList()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Reserve space for the two footer rows (name+ext, checkbox+buttons).
    const float footer_h = ImGui::GetFrameHeightWithSpacing() * 2.0f + style.ItemSpacing.y;
    float list_h = ImGui::GetContentRegionAvail().y - footer_h;
    if (list_h < 80.0f)
        list_h = 80.0f;

    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##fb_files", 3, table_flags, ImVec2(0, list_h)))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f, 1);
    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f, 2);
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
    {
        if (specs->SpecsDirty && specs->SpecsCount > 0)
        {
            sort_col = specs->Specs[0].ColumnIndex;
            sort_asc = specs->Specs[0].SortDirection != ImGuiSortDirection_Descending;
            sortEntries();
            specs->SpecsDirty = false;
        }
    }

    // Synthetic ".." row for going up (always safe; navigateUp is bounds-checked).
    if (!current_dir.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, kDirColor);
        if (ImGui::Selectable("..", false,
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                pending_up = true;
        }
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("-");
    }

    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        const Entry& e = entries[i];

        if (e.is_hidden && !show_hidden)
            continue;
        if (dialog_mode == DialogMode::SELECT && !e.is_dir)
            continue;
        if (!search_filter.PassFilter(e.name.c_str()))
            continue;
        if (!e.is_dir && !passesExtFilter(e.name))
            continue;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushID(i);
        if (e.is_dir)
            ImGui::PushStyleColor(ImGuiCol_Text, kDirColor);

        const bool selected = (selected_index == i);
        if (ImGui::Selectable(e.name.c_str(), selected,
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
        {
            selected_index = i;
            if (dialog_mode == DialogMode::SELECT ? e.is_dir : !e.is_dir)
            {
                std::strncpy(input_fn, e.name.c_str(), sizeof(input_fn) - 1);
                input_fn[sizeof(input_fn) - 1] = '\0';
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (e.is_dir)
                {
                    pending_dir = e.full;
                    has_pending_dir = true;
                }
                else if (dialog_mode != DialogMode::SELECT)
                {
                    pending_confirm = true;
                }
            }
        }

        if (e.is_dir)
            ImGui::PopStyleColor();
        ImGui::PopID();

        ImGui::TableSetColumnIndex(1);
        if (e.is_dir)
            ImGui::TextDisabled("-");
        else
            ImGui::TextUnformatted(HumanSize(e.size).c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(FormatTime(e.mtime).c_str());
    }

    ImGui::EndTable();
}

void ImGuiFileBrowser::renderFooter()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const bool has_ext = (dialog_mode != DialogMode::SELECT) && !ext_groups.empty();

    // Row 1: name / directory input, plus extension filter combo.
    const char* label = (dialog_mode == DialogMode::SELECT) ? "Directory:" : "File name:";
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();

    const float ext_w = has_ext ? 170.0f : 0.0f;
    float input_w = ImGui::GetContentRegionAvail().x - (has_ext ? (ext_w + style.ItemSpacing.x) : 0.0f);
    if (input_w < 80.0f)
        input_w = 80.0f;
    ImGui::SetNextItemWidth(input_w);
    if (ImGui::InputTextWithHint("##fb_name", "Type a name and press Enter...", input_fn, sizeof(input_fn),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
        pending_confirm = true;

    if (has_ext)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ext_w);
        if (ImGui::BeginCombo("##fb_ext", ext_groups[ext_group_idx].label.c_str()))
        {
            for (int i = 0; i < static_cast<int>(ext_groups.size()); ++i)
            {
                const bool sel = (i == ext_group_idx);
                if (ImGui::Selectable(ext_groups[i].label.c_str(), sel))
                    ext_group_idx = i;
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // Row 2: hidden toggle on the left, action buttons on the right.
    ImGui::Checkbox("Show hidden", &show_hidden);
    ImGui::SameLine();

    const char* ok_label = (dialog_mode == DialogMode::SAVE)   ? "Save"
                           : (dialog_mode == DialogMode::SELECT) ? "Select"
                                                                 : "Open";
    const float buttons_w = kButtonWidth * 2.0f + style.ItemSpacing.x;
    float to_right = ImGui::GetContentRegionAvail().x - buttons_w;
    if (to_right > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + to_right);

    if (ImGui::Button(ok_label, ImVec2(kButtonWidth, 0)))
        pending_confirm = true;
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(kButtonWidth, 0)))
        closeDialog();
}

void ImGuiFileBrowser::renderErrorPopup()
{
    if (show_error)
    {
        ImGui::OpenPopup("##fb_error");
        show_error = false;
    }
    ImGui::SetNextWindowSize(ImVec2(320, 0));
    if (ImGui::BeginPopupModal("##fb_error", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "%s", error_title.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", error_msg.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(-FLT_MIN, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ImGuiFileBrowser::renderNewFolderPopup()
{
    if (open_new_folder)
    {
        ImGui::OpenPopup("##fb_newfolder");
        open_new_folder = false;
    }
    ImGui::SetNextWindowSize(ImVec2(340, 0));
    if (ImGui::BeginPopupModal("##fb_newfolder", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::TextUnformatted("Create new folder in:");
        ImGui::TextDisabled("%s", current_path_str.c_str());
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool enter = ImGui::InputTextWithHint("##fb_newfolder_name", "Folder name", new_folder_name,
                                                    sizeof(new_folder_name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();

        const bool create = ImGui::Button("Create", ImVec2(kButtonWidth, 0)) || enter;
        ImGui::SameLine();
        const bool cancel = ImGui::Button("Cancel", ImVec2(kButtonWidth, 0));

        if (create && new_folder_name[0] != '\0')
        {
            std::error_code ec;
            fs::path target = current_dir / Utf8ToPath(new_folder_name);
            if (fs::create_directory(target, ec) && !ec)
            {
                refreshEntries();
                ImGui::CloseCurrentPopup();
            }
            else
            {
                setError("Error", "Could not create folder:\n" + (ec ? ec.message() : std::string("unknown error")));
                ImGui::CloseCurrentPopup();
            }
        }
        else if (cancel)
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ImGuiFileBrowser::renderOverwritePopup()
{
    if (want_overwrite)
    {
        ImGui::OpenPopup("##fb_overwrite");
        want_overwrite = false;
    }
    ImGui::SetNextWindowSize(ImVec2(340, 0));
    if (ImGui::BeginPopupModal("##fb_overwrite", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::TextWrapped("\"%s\" already exists. Replace it?", PathToUtf8(pending_overwrite.filename()).c_str());
        ImGui::Spacing();
        if (ImGui::Button("Replace", ImVec2(kButtonWidth, 0)))
        {
            fs::path target = pending_overwrite;
            ImGui::CloseCurrentPopup();
            commitFile(target);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(kButtonWidth, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---- Actions --------------------------------------------------------------

void ImGuiFileBrowser::requestConfirm()
{
    std::error_code ec;

    if (dialog_mode == DialogMode::SELECT)
    {
        fs::path target = current_dir;
        if (selected_index >= 0 && selected_index < static_cast<int>(entries.size()) && entries[selected_index].is_dir)
            target = entries[selected_index].full;
        else if (input_fn[0] != '\0')
        {
            fs::path typed = Utf8ToPath(input_fn);
            fs::path candidate = typed.is_absolute() ? typed : (current_dir / typed);
            if (fs::is_directory(candidate, ec))
                target = candidate;
        }
        if (target.empty() || !fs::is_directory(target, ec))
        {
            setError("Invalid Directory", "Please choose an existing directory.");
            return;
        }
        commitDirectory(NormalizeDir(target));
        return;
    }

    // OPEN / SAVE need a file name (typed or selected).
    std::string name = input_fn;
    if (name.empty() && selected_index >= 0 && selected_index < static_cast<int>(entries.size()) &&
        !entries[selected_index].is_dir)
        name = entries[selected_index].name;
    if (name.empty())
    {
        setError("No File", "Please enter or select a file name.");
        return;
    }

    fs::path typed = Utf8ToPath(name);
    fs::path target = typed.is_absolute() ? typed : (current_dir / typed);

    // Typing a directory name navigates into it instead of confirming.
    if (fs::is_directory(target, ec))
    {
        pending_dir = target;
        has_pending_dir = true;
        input_fn[0] = '\0';
        return;
    }

    if (dialog_mode == DialogMode::OPEN)
    {
        if (!fs::exists(target, ec))
        {
            setError("Invalid File", "The selected file does not exist.");
            return;
        }
        if (!passesExtFilter(PathToUtf8(target.filename())))
        {
            setError("Invalid File", "The selected file type is not allowed by the current filter.");
            return;
        }
        commitFile(target);
        return;
    }

    // SAVE: append the active filter extension if the user gave none.
    if (target.extension().empty() && !ext_groups.empty() &&
        !ext_groups[ext_group_idx].all_files && !ext_groups[ext_group_idx].exts.empty())
        target += Utf8ToPath(ext_groups[ext_group_idx].exts.front());

    if (fs::exists(target, ec) && !fs::is_directory(target, ec))
    {
        pending_overwrite = target;
        want_overwrite = true;
        return;
    }
    commitFile(target);
}

void ImGuiFileBrowser::commitFile(const fs::path& target)
{
    selected_path = PathToUtf8(target);
    std::replace(selected_path.begin(), selected_path.end(), '\\', '/');
    selected_fn = PathToUtf8(target.filename());
    ext = ToLower(PathToUtf8(target.extension()));
    selection_made = true;
    closeDialog();
}

void ImGuiFileBrowser::commitDirectory(const fs::path& target)
{
    selected_path = PathToUtf8(target);
    std::replace(selected_path.begin(), selected_path.end(), '\\', '/');
    if (!selected_path.empty() && selected_path.back() != '/')
        selected_path += '/';
    selected_fn = PathToUtf8(target.filename());
    if (selected_fn.empty()) // drive root: filename() is empty
        selected_fn = PathToUtf8(target);
    ext.clear();
    selection_made = true;
    closeDialog();
}

void ImGuiFileBrowser::closeDialog()
{
    input_fn[0] = '\0';
    new_folder_name[0] = '\0';
    search_filter.Clear();
    selected_index = -1;
    show_error = false;
    want_overwrite = false;
    open_new_folder = false;
    want_close = true; // actual CloseCurrentPopup() happens at parent-popup scope
}

void ImGuiFileBrowser::setError(std::string title, std::string message)
{
    error_title = std::move(title);
    error_msg = std::move(message);
    show_error = true;
}

} // namespace imgui_addons

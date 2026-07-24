#include "base/error_handling.h"

#include "settings/settings.h"

#include "base/logger.h"
#include "base/process_manager.h"
#include "system/system.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <windows.h>

namespace PY4GW {

namespace {

constexpr uint64_t kAutosaveQuietMs = 2000;   // flush after this much write silence
constexpr uint64_t kAutosaveMaxDirtyMs = 10000;  // never sit dirty longer than this

std::string Trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\r\n");
    const auto end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Sanitize a document name into a safe RELATIVE subpath. Preserves folders
// ("bots/foo/config.ini" stays nested) but blocks traversal: empty, ".", ".."
// and any segment carrying a drive/colon are dropped. Segments join with '/'.
std::string SanitizeRelativePath(const std::string& name) {
    std::string result;
    std::string segment;
    const auto flush = [&]() {
        if (!segment.empty() && segment != "." && segment != ".." &&
            segment.find(':') == std::string::npos) {
            if (!result.empty()) {
                result += '/';
            }
            result += segment;
        }
        segment.clear();
    };
    for (const char c : name) {
        if (c == '/' || c == '\\') {
            flush();
        } else {
            segment += c;
        }
    }
    flush();
    return result;
}

// A safe single path segment for an account-email folder: non-empty, not a
// traversal token, and free of separators/drive markers.
bool IsSafeEmailSegment(const std::string& email) {
    if (email.empty() || email == "." || email == "..") {
        return false;
    }
    for (const char c : email) {
        if (c == '/' || c == '\\' || c == ':') {
            return false;
        }
    }
    return true;
}

}  // namespace

/* ---------------- IniFile internals ---------------- */

IniFile::IniFile(std::string name, SettingsScope scope)
    : name_(std::move(name)), scope_(scope) {}

void IniFile::Bind(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bound_) {
        return;
    }

    // Collect writes staged before the anchor resolved; they are newer than
    // whatever is on disk, so they win after the load.
    std::vector<std::tuple<std::string, std::string, std::string>> staged;
    for (const auto& section : sections_) {
        for (const auto& line : section.lines) {
            if (line.kind == IniLine::Kind::KeyValue) {
                staged.emplace_back(section.name, line.key, line.value);
            }
        }
    }

    path_ = path;
    bound_ = true;

    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        Logger::Instance().LogError("Settings: failed to create directory '" +
                                    path_.parent_path().string() + "' for '" + name_ +
                                    "' - " + ec.message());
    }

    sections_.clear();
    if (!LoadLocked()) {
        // File did not exist on disk: seed a brand-new document from its
        // template (settings/Defaults/<name>.cfg, then default_template.cfg).
        SeedFromTemplateLocked();
    }

    for (const auto& [section, key, value] : staged) {
        SetValueLocked(section, key, value);
    }
    if (!staged.empty()) {
        MarkDirtyLocked();
    }
}

void IniFile::SeedFromTemplateLocked() {
    const std::filesystem::path defaults =
        process_manager::GetModuleDirectory() / "settings" / "Defaults";

    // Template name mirrors the document name with a .cfg extension; fall back
    // to a shared default_template.cfg. Keys in these templates are already
    // lowercase (matching the on-disk key convention), so no case fixup is needed.
    std::filesystem::path specialized = defaults / name_;
    specialized.replace_extension(".cfg");
    const std::filesystem::path fallback = defaults / "default_template.cfg";

    std::error_code ec;
    std::filesystem::path template_path;
    if (std::filesystem::exists(specialized, ec)) {
        template_path = specialized;
    } else if (std::filesystem::exists(fallback, ec)) {
        template_path = fallback;
    } else {
        return;
    }

    std::ifstream file(template_path);
    if (!file.is_open()) {
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    ParseLocked(buffer.str());
    MarkDirtyLocked();
}

void IniFile::AutosaveTick(uint64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!bound_ || !dirty_) {
        return;
    }
    if (now_ms - last_change_tick_ >= kAutosaveQuietMs ||
        now_ms - first_dirty_tick_ >= kAutosaveMaxDirtyMs) {
        SaveLocked();
    }
}

bool IniFile::LoadLocked() {
    std::error_code exists_ec;
    const bool file_exists = std::filesystem::exists(path_, exists_ec);
    std::ifstream file(path_);
    if (!file.is_open()) {
        // A missing file is normal (first run); a file that exists but won't
        // open is a real error worth surfacing rather than swallowing.
        if (file_exists) {
            Logger::Instance().LogError("Settings: '" + path_.string() +
                                        "' exists but could not be opened for reading");
        }
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    ParseLocked(buffer.str());
    return true;
}

void IniFile::ParseLocked(const std::string& content) {
    sections_.clear();
    IniSection* current = &sections_.emplace_back();  // unnamed preamble

    std::istringstream stream(content);
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        if (!raw_line.empty() && raw_line.back() == '\r') {
            raw_line.pop_back();
        }
        const std::string trimmed = Trim(raw_line);

        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            current = &sections_.emplace_back();
            current->name = Trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        IniLine line;
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            line.kind = IniLine::Kind::Comment;
            line.raw = raw_line;
        } else if (const auto delimiter = trimmed.find('='); delimiter != std::string::npos) {
            line.kind = IniLine::Kind::KeyValue;
            line.key = Trim(trimmed.substr(0, delimiter));
            line.value = Trim(trimmed.substr(delimiter + 1));
        } else {
            // Unparseable content is preserved verbatim, never destroyed.
            line.kind = IniLine::Kind::Raw;
            line.raw = raw_line;
        }
        current->lines.push_back(std::move(line));
    }
}

std::string IniFile::SerializeLocked() const {
    std::ostringstream out;
    for (const auto& section : sections_) {
        if (!section.name.empty()) {
            out << "[" << section.name << "]\n";
        } else if (section.lines.empty()) {
            continue;
        }
        for (const auto& line : section.lines) {
            switch (line.kind) {
            case IniLine::Kind::KeyValue:
                out << line.key << " = " << line.value << "\n";
                break;
            case IniLine::Kind::Comment:
            case IniLine::Kind::Raw:
                out << line.raw << "\n";
                break;
            }
        }
    }
    return out.str();
}

bool IniFile::SaveLocked() {
    if (!bound_) {
        return false;
    }

    const std::string content = SerializeLocked();
    const std::filesystem::path temp_path = path_.string() + ".tmp";

    {
        std::ofstream out(temp_path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            Logger::Instance().LogError("Settings save failed (cannot open temp file): " + temp_path.string());
            return false;
        }
        out << content;
        if (!out.good()) {
            Logger::Instance().LogError("Settings save failed (write error): " + temp_path.string());
            return false;
        }
    }

    // Atomic swap: disk always holds either the full old or full new file.
    std::error_code ec;
    std::filesystem::rename(temp_path, path_, ec);
    if (ec) {
        Logger::Instance().LogError("Settings save failed (rename): " + path_.string() + " - " + ec.message());
        return false;
    }

    dirty_ = false;
    first_dirty_tick_ = 0;
    return true;
}

void IniFile::MarkDirtyLocked() {
    const uint64_t now = ::GetTickCount64();
    if (!dirty_) {
        dirty_ = true;
        first_dirty_tick_ = now;
    }
    last_change_tick_ = now;
}

const IniFile::IniSection* IniFile::FindSectionLocked(const std::string& section) const {
    for (const auto& candidate : sections_) {
        if (candidate.name == section) {
            return &candidate;
        }
    }
    return nullptr;
}

IniFile::IniSection& IniFile::FindOrCreateSectionLocked(const std::string& section) {
    for (auto& candidate : sections_) {
        if (candidate.name == section) {
            return candidate;
        }
    }
    auto& created = sections_.emplace_back();
    created.name = section;
    return created;
}

const IniFile::IniLine* IniFile::FindKeyLocked(const std::string& section, const std::string& key) const {
    const IniSection* found = FindSectionLocked(section);
    if (!found) {
        return nullptr;
    }
    for (const auto& line : found->lines) {
        if (line.kind == IniLine::Kind::KeyValue && line.key == key) {
            return &line;
        }
    }
    return nullptr;
}

void IniFile::SetValueLocked(const std::string& section, const std::string& key, const std::string& value) {
    IniSection& target = FindOrCreateSectionLocked(section);
    for (auto& line : target.lines) {
        if (line.kind == IniLine::Kind::KeyValue && line.key == key) {
            line.value = value;
            return;
        }
    }
    IniLine line;
    line.kind = IniLine::Kind::KeyValue;
    line.key = key;
    line.value = value;
    target.lines.push_back(std::move(line));
}

/* ---------------- SettingsManager ---------------- */

SettingsManager& SettingsManager::Instance() {
    static SettingsManager instance;
    return instance;
}

IniFile& SettingsManager::Open(const std::string& name, SettingsScope scope) {
    // Sanitize to a safe relative subpath: folders preserved, no traversal.
    std::string sanitized = SanitizeRelativePath(name);
    if (sanitized.empty()) {
        sanitized = "unnamed.ini";
    }

    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        if (document->name_ == sanitized && document->scope_ == scope) {
            return *document;
        }
    }

    auto& document = documents_.emplace_back(std::unique_ptr<IniFile>(new IniFile(sanitized, scope)));
    if (scope == SettingsScope::Global) {
        // Legacy layout: settings/Global/<name>, shared by every account.
        document->Bind(process_manager::GetModuleDirectory() / "settings" / "Global" / sanitized);
    } else if (scope == SettingsScope::Root) {
        // Root scope is NOT reachable through Open(): there is deliberately no
        // (name, scope) that binds to the bare project root, so a caller can
        // never escape the settings/ jail this way. The single root file
        // (Py4GW.ini) is served only by OpenPy4GWIni(), which hard-codes its
        // name. Reaching here means an internal caller passed Root by mistake;
        // redirect it into the jail (settings/Global) and log, never bind root.
        Logger::Instance().LogError(
            "Settings: Root scope is not openable by name ('" + sanitized +
            "'); use OpenPy4GWIni() for the one root file. Redirected to settings/Global.");
        document->Bind(process_manager::GetModuleDirectory() / "settings" / "Global" / sanitized);
    } else if (scope == SettingsScope::Account && System::Instance().HasAccountEmail()) {
        // Account anchor already resolved: bind (and load from disk) immediately
        // so a read right after Open() sees the file, matching the legacy
        // synchronous handler. If the anchor is not ready yet, Update() binds it
        // once it resolves. Bind() is idempotent, so there is no double-bind.
        document->Bind(System::Instance().GetSettingsDirectory() / sanitized);
    }
    return *document;
}

IniFile& SettingsManager::OpenPy4GWIni() {
    // The one and only document permitted outside the settings/ jail, hard-wired
    // to <module>/Py4GW.ini. The filename is a compile-time constant and there is
    // no parameter to override it, so this accessor cannot be repurposed to write
    // anywhere else - it is the deliberate, unbypassable exception (a cross-process
    // contract with the external launcher, which is not injected and so cannot use
    // the settings/ jail). Everything else must go through Open() (Account/Global).
    static constexpr const char* kRootIniName = "Py4GW.ini";

    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        if (document->scope_ == SettingsScope::Root && document->name_ == kRootIniName) {
            return *document;
        }
    }
    auto& document = documents_.emplace_back(
        std::unique_ptr<IniFile>(new IniFile(kRootIniName, SettingsScope::Root)));
    document->Bind(process_manager::GetModuleDirectory() / kRootIniName);
    return *document;
}

void SettingsManager::Update() {
    const bool anchored = System::Instance().HasAccountEmail();
    const uint64_t now = ::GetTickCount64();

    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        if (anchored && document->scope_ == SettingsScope::Account && !document->IsBound()) {
            document->Bind(System::Instance().GetSettingsDirectory() / document->name_);
        }
        document->AutosaveTick(now);
    }
}

void SettingsManager::FlushAll() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        std::lock_guard<std::mutex> doc_lock(document->mutex_);
        if (document->bound_ && document->dirty_) {
            document->SaveLocked();
        }
    }
}

bool SettingsManager::WriteTriplesToAccount(
    const std::string& name,
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
    const std::string& target_email) {
    if (!IsSafeEmailSegment(target_email)) {
        Logger::Instance().LogError(
            "Settings: copy rejected unsafe target email '" + target_email + "'");
        return false;
    }
    std::string sanitized = SanitizeRelativePath(name);
    if (sanitized.empty()) {
        return false;
    }
    if (triples.empty()) {
        return true;  // nothing to copy is not a failure
    }

    // Load the target account's file (or seed a new one), overlay, atomic save.
    // A local, unregistered document is used so we never touch another account's
    // slot in the registry. Bind()/*Locked are private but SettingsManager is a
    // friend of IniFile.
    const std::filesystem::path target_path =
        process_manager::GetModuleDirectory() / "settings" / target_email / sanitized;

    IniFile target(sanitized, SettingsScope::Account);
    target.Bind(target_path);
    std::lock_guard<std::mutex> lock(target.mutex_);
    for (const auto& [section, key, value] : triples) {
        target.SetValueLocked(section, key, value);
    }
    return target.SaveLocked();
}

bool SettingsManager::CopyKeysToAccount(const std::string& name, const std::string& section,
                                        const std::vector<std::string>& keys,
                                        const std::string& target_email) {
    IniFile& source = Open(SanitizeRelativePath(name), SettingsScope::Account);
    std::vector<std::tuple<std::string, std::string, std::string>> triples;
    triples.reserve(keys.size());
    for (const auto& key : keys) {
        if (source.HasKey(section, key)) {
            triples.emplace_back(section, key, source.GetString(section, key, ""));
        }
    }
    return WriteTriplesToAccount(name, triples, target_email);
}

bool SettingsManager::CopySectionToAccount(const std::string& name, const std::string& section,
                                           const std::string& target_email) {
    IniFile& source = Open(SanitizeRelativePath(name), SettingsScope::Account);
    return CopyKeysToAccount(name, section, source.GetKeys(section), target_email);
}

bool SettingsManager::ApplySectionToAccount(
    const std::string& name, const std::string& section,
    const std::vector<std::pair<std::string, std::string>>& values,
    const std::string& target_email) {
    std::vector<std::tuple<std::string, std::string, std::string>> triples;
    triples.reserve(values.size());
    for (const auto& [key, value] : values) {
        triples.emplace_back(section, key, value);
    }
    return WriteTriplesToAccount(name, triples, target_email);
}

bool SettingsManager::CopyDocumentToAccount(const std::string& name, const std::string& target_email) {
    IniFile& source = Open(SanitizeRelativePath(name), SettingsScope::Account);
    std::vector<std::tuple<std::string, std::string, std::string>> triples;
    // "" is the unnamed preamble section (keys before the first header); GetSections()
    // omits it, so include it explicitly for a genuine whole-file copy.
    std::vector<std::string> sections = source.GetSections();
    sections.insert(sections.begin(), std::string());
    for (const auto& section : sections) {
        for (const auto& key : source.GetKeys(section)) {
            triples.emplace_back(section, key, source.GetString(section, key, ""));
        }
    }
    return WriteTriplesToAccount(name, triples, target_email);
}

}  // namespace PY4GW

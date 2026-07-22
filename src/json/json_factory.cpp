#include "base/error_handling.h"

#include "json/json_factory.h"

#include "base/logger.h"
#include "base/process_manager.h"
#include "system/system.h"

#include <cstddef>
#include <fstream>
#include <functional>
#include <sstream>

#include <windows.h>

namespace PY4GW {

namespace {

using json = nlohmann::json;

constexpr uint64_t kAutosaveQuietMs = 2000;      // flush after this much write silence
constexpr uint64_t kAutosaveMaxDirtyMs = 10000;  // never sit dirty longer than this
constexpr int kIndent = 2;                        // pretty-print indent

// Sanitize a document name into a safe RELATIVE subpath. Preserves folders but
// blocks traversal: empty, ".", ".." and any segment carrying a drive/colon are
// dropped. Segments join with '/'. (Mirrors settings/ exactly.)
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

// A safe single path segment for an account-email folder.
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

// Split an access path ("ui/window/pos/x") into non-empty segments.
std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> out;
    std::string segment;
    for (const char c : path) {
        if (c == '/') {
            if (!segment.empty()) {
                out.push_back(segment);
            }
            segment.clear();
        } else {
            segment += c;
        }
    }
    if (!segment.empty()) {
        out.push_back(segment);
    }
    return out;
}

bool ParseArrayIndex(const std::string& segment, size_t* out) {
    if (segment.empty()) {
        return false;
    }
    size_t value = 0;
    for (const char c : segment) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<size_t>(c - '0');
    }
    *out = value;
    return true;
}

// Set `value` at `segments`, creating intermediate OBJECTs as needed. Numeric
// segments index an existing array in place; otherwise a new object member is
// created (arrays are only grown through Append/SetNode with a list value).
void SetAtPath(json& root, const std::vector<std::string>& segments, json value) {
    if (segments.empty()) {
        root = std::move(value);
        return;
    }
    json* node = &root;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (node->is_array()) {
            size_t index = 0;
            if (ParseArrayIndex(segment, &index) && index < node->size()) {
                node = &(*node)[index];
                continue;
            }
        }
        if (!node->is_object()) {
            *node = json::object();
        }
        node = &(*node)[segment];
    }

    const std::string& last = segments.back();
    if (node->is_array()) {
        size_t index = 0;
        if (ParseArrayIndex(last, &index) && index < node->size()) {
            (*node)[index] = std::move(value);
            return;
        }
    }
    if (!node->is_object()) {
        *node = json::object();
    }
    (*node)[last] = std::move(value);
}

// Delete the node at `segments`; returns true if something was removed.
bool DeleteAtPath(json& root, const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return false;
    }
    json* node = &root;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (node->is_object()) {
            const auto it = node->find(segment);
            if (it == node->end()) {
                return false;
            }
            node = &(*it);
        } else if (node->is_array()) {
            size_t index = 0;
            if (!ParseArrayIndex(segment, &index) || index >= node->size()) {
                return false;
            }
            node = &(*node)[index];
        } else {
            return false;
        }
    }

    const std::string& last = segments.back();
    if (node->is_object()) {
        return node->erase(last) > 0;
    }
    if (node->is_array()) {
        size_t index = 0;
        if (ParseArrayIndex(last, &index) && index < node->size()) {
            node->erase(node->begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
    }
    return false;
}

// A cross-process mutual-exclusion lock: a named Windows mutex keyed by a hash
// of the document's absolute path. Held around the read-modify-write of a shared
// (Global / cross-account) file so multibox writers cannot interleave. Scoped;
// releases on destruction.
//
// Acquisition BLOCKS until the lock is ours - the loser of a race waits for the
// winner to finish its write and release; there is no give-up-and-clobber path.
// This is safe against a peer that dies while holding the lock: Windows marks the
// mutex "abandoned" and hands ownership to the next waiter (WAIT_ABANDONED), so a
// crashed client can never deadlock persistence. The only case that proceeds
// UNLOCKED is CreateMutex itself failing (handle exhaustion / broken system) -
// that is logged, and we fall back to a best-effort write rather than silently
// dropping the document forever.
class CrossProcessLock {
public:
    explicit CrossProcessLock(const std::filesystem::path& path) {
        const size_t hash = std::hash<std::string>{}(path.lexically_normal().string());
        std::ostringstream name;
        name << "Local\\Py4GW_json_" << std::hex << hash;
        handle_ = ::CreateMutexA(nullptr, FALSE, name.str().c_str());
        if (!handle_) {
            Logger::Instance().LogError(
                "Json: could not create cross-process lock for '" + path.string() +
                "' (err " + std::to_string(::GetLastError()) +
                "); writing without cross-process serialization");
            return;
        }
        // Wait indefinitely: the loser must wait until the holder releases. A dead
        // holder yields WAIT_ABANDONED (OS auto-release), so this never deadlocks.
        const DWORD wait = ::WaitForSingleObject(handle_, INFINITE);
        owned_ = (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED);
    }
    ~CrossProcessLock() {
        if (handle_) {
            if (owned_) {
                ::ReleaseMutex(handle_);
            }
            ::CloseHandle(handle_);
        }
    }
    // True when this process holds the lock (false only if CreateMutex failed).
    bool held() const { return owned_; }

    CrossProcessLock(const CrossProcessLock&) = delete;
    CrossProcessLock& operator=(const CrossProcessLock&) = delete;

private:
    HANDLE handle_ = nullptr;
    bool owned_ = false;
};

// Read + parse a JSON file into `out`. Returns false if the file is absent or
// unreadable/unparseable (out is left untouched on parse failure).
bool ReadJsonFile(const std::filesystem::path& path, json* out) {
    std::error_code exists_ec;
    const bool file_exists = std::filesystem::exists(path, exists_ec);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (file_exists) {
            Logger::Instance().LogError("Json: '" + path.string() +
                                        "' exists but could not be opened for reading");
        }
        return false;
    }
    json parsed = json::parse(file, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (parsed.is_discarded()) {
        Logger::Instance().LogError("Json: '" + path.string() + "' is not valid JSON; ignoring on read");
        return false;
    }
    *out = std::move(parsed);
    return true;
}

// Atomic write: temp file in the same directory, then rename over the target.
bool WriteJsonFileAtomic(const std::filesystem::path& path, const json& root) {
    const std::filesystem::path temp_path = path.string() + ".tmp";
    {
        std::ofstream out(temp_path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            Logger::Instance().LogError("Json save failed (cannot open temp file): " + temp_path.string());
            return false;
        }
        out << root.dump(kIndent);
        if (!out.good()) {
            Logger::Instance().LogError("Json save failed (write error): " + temp_path.string());
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        Logger::Instance().LogError("Json save failed (rename): " + path.string() + " - " + ec.message());
        return false;
    }
    return true;
}

}  // namespace

/* ---------------- JsonFile internals ---------------- */

JsonFile::JsonFile(std::string name, JsonScope scope)
    : name_(std::move(name)), scope_(scope) {}

void JsonFile::Bind(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bound_) {
        return;
    }

    // Writes staged before the anchor resolved are newer than disk, so they win
    // after the load. Capture the current journal, reload from disk, replay.
    std::vector<PendingOp> staged = std::move(pending_);
    pending_.clear();

    path_ = path;
    bound_ = true;

    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        Logger::Instance().LogError("Json: failed to create directory '" +
                                    path_.parent_path().string() + "' for '" + name_ +
                                    "' - " + ec.message());
    }

    root_ = json::object();
    if (!LoadLocked()) {
        SeedFromTemplateLocked();
    }

    for (const auto& op : staged) {
        const auto segments = SplitPath(op.path);
        if (op.remove) {
            DeleteAtPath(root_, segments);
        } else {
            SetAtPath(root_, segments, op.value);
        }
    }
    if (!staged.empty()) {
        pending_ = std::move(staged);
        MarkDirtyLocked();
    }
}

void JsonFile::SeedFromTemplateLocked() {
    const std::filesystem::path defaults =
        process_manager::GetModuleDirectory() / "json" / "Defaults";

    std::filesystem::path specialized = defaults / name_;
    specialized.replace_extension(".json");
    const std::filesystem::path fallback = defaults / "default_template.json";

    std::error_code ec;
    std::filesystem::path template_path;
    if (std::filesystem::exists(specialized, ec)) {
        template_path = specialized;
    } else if (std::filesystem::exists(fallback, ec)) {
        template_path = fallback;
    } else {
        return;
    }

    json seeded;
    if (ReadJsonFile(template_path, &seeded)) {
        root_ = std::move(seeded);
        MarkDirtyLocked();
    }
}

void JsonFile::AutosaveTick(uint64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!bound_ || !dirty_) {
        return;
    }
    if (now_ms - last_change_tick_ >= kAutosaveQuietMs ||
        now_ms - first_dirty_tick_ >= kAutosaveMaxDirtyMs) {
        SaveLocked();
    }
}

bool JsonFile::LoadLocked() {
    json loaded;
    if (!ReadJsonFile(path_, &loaded)) {
        return false;
    }
    // A JSON document is addressed as a tree of objects; a non-object root would
    // make path writes ambiguous, so normalize an unexpected scalar/array root
    // by wrapping is avoided - instead keep it only if it is an object.
    if (!loaded.is_object()) {
        Logger::Instance().LogError("Json: '" + path_.string() +
                                    "' root is not an object; starting empty");
        return false;
    }
    root_ = std::move(loaded);
    return true;
}

bool JsonFile::SaveLocked() {
    if (!bound_) {
        return false;
    }
    bool ok = false;
    if (scope_ == JsonScope::Global) {
        ok = WriteMergedGlobalLocked();
    } else {
        ok = WriteJsonFileAtomic(path_, root_);
    }
    if (ok) {
        dirty_ = false;
        first_dirty_tick_ = 0;
        pending_.clear();
    }
    return ok;
}

bool JsonFile::WriteMergedGlobalLocked() {
    // Serialize the read-modify-write of the shared file across processes, then
    // replay only THIS document's journaled ops onto the freshest on-disk tree
    // so a concurrent account's keys are preserved.
    CrossProcessLock lock(path_);

    json disk = json::object();
    json loaded;
    if (ReadJsonFile(path_, &loaded) && loaded.is_object()) {
        disk = std::move(loaded);
    }

    for (const auto& op : pending_) {
        const auto segments = SplitPath(op.path);
        if (op.remove) {
            DeleteAtPath(disk, segments);
        } else {
            SetAtPath(disk, segments, op.value);
        }
    }

    if (!WriteJsonFileAtomic(path_, disk)) {
        return false;
    }
    // Adopt the merged tree so subsequent in-memory reads see peers' values too.
    root_ = std::move(disk);
    return true;
}

void JsonFile::RecordOpLocked(const std::string& path, bool remove, const json& value) {
    PendingOp op;
    op.path = path;
    op.remove = remove;
    op.value = value;
    pending_.push_back(std::move(op));
}

void JsonFile::MarkDirtyLocked() {
    const uint64_t now = ::GetTickCount64();
    if (!dirty_) {
        dirty_ = true;
        first_dirty_tick_ = now;
    }
    last_change_tick_ = now;
}

/* ---------------- JsonFactory ---------------- */

JsonFactory& JsonFactory::Instance() {
    static JsonFactory instance;
    return instance;
}

JsonFile& JsonFactory::Open(const std::string& name, JsonScope scope) {
    std::string sanitized = SanitizeRelativePath(name);
    if (sanitized.empty()) {
        sanitized = "unnamed.json";
    }

    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        if (document->name_ == sanitized && document->scope_ == scope) {
            return *document;
        }
    }

    auto& document = documents_.emplace_back(std::unique_ptr<JsonFile>(new JsonFile(sanitized, scope)));
    const std::filesystem::path root = process_manager::GetModuleDirectory();
    if (scope == JsonScope::Global) {
        document->Bind(root / "json" / "Global" / sanitized);
    } else if (scope == JsonScope::Root) {
        document->Bind(root / sanitized);
    } else if (scope == JsonScope::Account && System::Instance().HasAccountEmail()) {
        document->Bind(root / "json" / System::Instance().GetAccountEmail() / sanitized);
    }
    return *document;
}

void JsonFactory::Update() {
    const bool anchored = System::Instance().HasAccountEmail();
    const uint64_t now = ::GetTickCount64();
    const std::filesystem::path root = process_manager::GetModuleDirectory();
    const std::string email = anchored ? System::Instance().GetAccountEmail() : std::string();

    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        if (anchored && document->scope_ == JsonScope::Account && !document->IsBound()) {
            document->Bind(root / "json" / email / document->name_);
        }
        document->AutosaveTick(now);
    }
}

void JsonFactory::FlushAll() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& document : documents_) {
        std::lock_guard<std::mutex> doc_lock(document->mutex_);
        if (document->bound_ && document->dirty_) {
            document->SaveLocked();
        }
    }
}

bool JsonFactory::MergePatchToAccount(const std::string& name, const json& patch,
                                      const std::string& target_email) {
    if (!IsSafeEmailSegment(target_email)) {
        Logger::Instance().LogError("Json: copy rejected unsafe target email '" + target_email + "'");
        return false;
    }
    std::string sanitized = SanitizeRelativePath(name);
    if (sanitized.empty()) {
        return false;
    }
    if (patch.is_null() || (patch.is_object() && patch.empty())) {
        return true;  // nothing to copy is not a failure
    }

    const std::filesystem::path target_path =
        process_manager::GetModuleDirectory() / "json" / target_email / sanitized;

    std::error_code ec;
    std::filesystem::create_directories(target_path.parent_path(), ec);

    CrossProcessLock lock(target_path);
    json target = json::object();
    json loaded;
    if (ReadJsonFile(target_path, &loaded) && loaded.is_object()) {
        target = std::move(loaded);
    }
    target.merge_patch(patch);  // RFC 7396: source wins, target-only keys kept, null deletes
    return WriteJsonFileAtomic(target_path, target);
}

bool JsonFactory::CopyDocumentToAccount(const std::string& name, const std::string& target_email) {
    JsonFile& source = Open(SanitizeRelativePath(name), JsonScope::Account);
    return MergePatchToAccount(name, source.GetNode(""), target_email);
}

bool JsonFactory::CopyPathToAccount(const std::string& name, const std::string& path,
                                    const std::string& target_email) {
    JsonFile& source = Open(SanitizeRelativePath(name), JsonScope::Account);
    const json subtree = source.GetNode(path);
    if (subtree.is_null()) {
        return true;  // nothing at that path is not a failure
    }
    // Wrap the subtree back under its path so merge_patch lands it in place.
    json patch = json::object();
    SetAtPath(patch, SplitPath(path), subtree);
    return MergePatchToAccount(name, patch, target_email);
}

bool JsonFactory::ApplyToAccount(const std::string& name, const std::string& path,
                                 const json& value, const std::string& target_email) {
    json patch = json::object();
    SetAtPath(patch, SplitPath(path), value);
    return MergePatchToAccount(name, patch, target_email);
}

}  // namespace PY4GW

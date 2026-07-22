#pragma once

// =============================================================================
// Py4GW JSON factory - structured, per-scope configuration documents.
//
//   *** THIS IS A SELF-THROTTLED, SELF-PERSISTING SYSTEM. ***
//   Callers never open, load, or save. Setters mutate memory and mark the
//   document dirty; the factory's debounced autosave pump writes it to disk on
//   its own schedule, and account documents bind/load themselves once the email
//   anchor resolves. Save() (force a write now) and Reload() (force a re-read
//   now) are escape hatches - use them ONLY when the app specifically requires
//   it, never as part of normal flow.
//
// This is the JSON counterpart of the INI settings system (settings/settings.h).
// It is NOT a replacement: settings/ keeps handling flat INI, json/ adds a
// nested/structured backend. See docs/json-factory-design.md for the full
// design. This header declares the two types behind the `PyJson` Python module
// (bound in src/json/json_factory_bindings.cpp):
//
//   * JsonFile     - one JSON document (a single file) with a typed,
//                    path-addressed, never-throwing get/set surface.
//                    Thread-safe.
//   * JsonFactory  - process-wide registry + owner of every JsonFile, the
//                    account-binding step, the debounced autosave pump, and the
//                    cross-account copy operations.
//
// Global-scope documents are shared by every running client (multibox): their
// saves take a cross-process lock and merge a per-document write journal onto
// the current on-disk tree, so concurrent accounts do not clobber each other.
// =============================================================================

#include "base/error_handling.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace PY4GW {

// Where a document's file lives, and when it becomes bound to disk:
//   Scope::Account -> json/<email>/<name>  (staged in memory until the account
//                                           anchor/email resolves, then bound).
//   Scope::Global  -> json/Global/<name>   (bound immediately, shared by every
//                                           account/process; save is locked +
//                                           journal-merged).
//   Scope::Root    -> <name>               (module/project root, shared; bound
//                                           immediately). Reserved for core files.
enum class JsonScope {
    Account,
    Global,
    Root
};

// One JSON document, fully synchronized behind its own mutex. Instances are
// created and owned exclusively by JsonFactory - construct via
// JsonFactory::Open(), never directly (the constructor is private).
//
// Addressing is a slash path that walks the tree: "ui/window/pos/x". A numeric
// segment indexes an array: "waypoints/0/x". Reads never throw - a missing node
// or an unconvertible value returns the caller's default.
class JsonFile {
public:
    // --- Typed leaf reads --------------------------------------------------
    // Never throw. Coerce across JSON scalar types where sensible; a missing
    // node or non-convertible value returns the caller's default.
    std::string GetString(const std::string& path, const std::string& default_value = "") const;
    bool GetBool(const std::string& path, bool default_value = false) const;
    long long GetInt(const std::string& path, long long default_value = 0) const;
    double GetFloat(const std::string& path, double default_value = 0.0) const;

    // --- Typed leaf writes -------------------------------------------------
    // Take effect in memory immediately, journal the op, and mark the document
    // dirty. Intermediate objects along the path are created as needed. The
    // debounced autosave pump persists the change.
    void SetString(const std::string& path, const std::string& value);
    void SetBool(const std::string& path, bool value);
    void SetInt(const std::string& path, long long value);
    void SetFloat(const std::string& path, double value);

    // --- Subtree access ----------------------------------------------------
    // GetNode returns a COPY of the subtree at path (null if absent). SetNode
    // replaces (or creates) the subtree. Append pushes onto an array at path,
    // creating an empty array first if the node is absent.
    nlohmann::json GetNode(const std::string& path) const;
    void SetNode(const std::string& path, const nlohmann::json& value);
    void Append(const std::string& path, const nlohmann::json& value);

    // --- Introspection / structural edits ----------------------------------
    bool Has(const std::string& path) const;
    bool Delete(const std::string& path);                 // true if it existed
    std::vector<std::string> Keys(const std::string& path) const;  // object member names
    size_t Size(const std::string& path) const;           // array length / member count
    bool IsArray(const std::string& path) const;
    bool IsObject(const std::string& path) const;

    // --- Escape hatches ----------------------------------------------------
    bool Save();     // force an immediate write to disk, bypassing the debounce
    bool Reload();   // re-read from disk, DISCARDING unsaved in-memory changes

    // --- Status ------------------------------------------------------------
    bool IsDirty() const;
    bool IsBound() const;
    const std::string& Name() const;
    JsonScope Scope() const;
    std::filesystem::path Path() const;   // absolute path; empty until bound

private:
    friend class JsonFactory;

    // A journaled mutation: a set (remove == false, value is the subtree) or a
    // delete (remove == true). For Global documents these are replayed onto the
    // freshly-read on-disk tree at save time so concurrent writers merge.
    struct PendingOp {
        std::string path;
        bool remove = false;
        nlohmann::json value;
    };

    JsonFile(std::string name, JsonScope scope);
    JsonFile(const JsonFile&) = delete;
    JsonFile& operator=(const JsonFile&) = delete;

    // Attach the document to a concrete path and load it (called once the scope's
    // anchor is known - immediately for Global/Root, after the email for Account).
    void Bind(const std::filesystem::path& path);
    // Stepped by JsonFactory::Update(); saves this document when its dirty
    // debounce window elapses.
    void AutosaveTick(uint64_t now_ms);

    // Locked helpers (caller already holds mutex_):
    bool LoadLocked();                 // read + parse the file into root_
    void SeedFromTemplateLocked();     // seed a brand-new file from json/Defaults/*
    bool SaveLocked();                 // serialize + write, clear dirty/journal
    bool WriteMergedGlobalLocked();    // locked read-merge-write for Global scope
    void RecordOpLocked(const std::string& path, bool remove, const nlohmann::json& value);
    void MarkDirtyLocked();            // set dirty + stamp the debounce ticks

    mutable std::mutex mutex_;
    std::string name_;
    JsonScope scope_;
    std::filesystem::path path_;
    bool bound_ = false;
    bool dirty_ = false;
    uint64_t first_dirty_tick_ = 0;
    uint64_t last_change_tick_ = 0;
    nlohmann::json root_ = nlohmann::json::object();
    std::vector<PendingOp> pending_;   // ops since the last disk sync (Global merge)
};

// Process-wide owner and registry of every JsonFile. Also drives account
// binding, the autosave pump, and cross-account copy operations. Singleton.
class JsonFactory {
public:
    static JsonFactory& Instance();

    // Return the process-wide document for (name, scope); the SAME (name, scope)
    // always yields the SAME JsonFile. `name` is sanitized to a safe relative
    // subpath. This is the one entry point for acquiring a document.
    JsonFile& Open(const std::string& name, JsonScope scope = JsonScope::Account);

    // Step once per frame from the runtime update loop: (1) bind any staged
    // account documents now that the email anchor has resolved; (2) run the
    // debounced autosave pump over every dirty bound document.
    void Update();

    // Save every dirty bound document immediately. Wired into shutdown.
    void FlushAll();

    // --- Cross-account copy ------------------------------------------------
    // Push config from THIS account's (name, Account) document into ANOTHER
    // account's file on disk (json/<target_email>/<name>). Overlay is a deep
    // merge-patch (source wins, target-only keys retained, null deletes). The
    // target write takes the cross-process lock. Returns false on a rejected
    // email / save failure; copying an empty subtree is a success.
    bool CopyDocumentToAccount(const std::string& name, const std::string& target_email);
    bool CopyPathToAccount(const std::string& name, const std::string& path,
                           const std::string& target_email);
    // Merge a caller-supplied subtree at `path` into the target account's file.
    bool ApplyToAccount(const std::string& name, const std::string& path,
                        const nlohmann::json& value, const std::string& target_email);

private:
    // Shared writer: deep-merge `patch` (a full-document patch) onto
    // json/<target_email>/<name> under the cross-process lock, then save.
    bool MergePatchToAccount(const std::string& name, const nlohmann::json& patch,
                             const std::string& target_email);

    JsonFactory() = default;
    ~JsonFactory() = default;
    JsonFactory(const JsonFactory&) = delete;
    JsonFactory& operator=(const JsonFactory&) = delete;

    std::mutex registry_mutex_;
    std::vector<std::unique_ptr<JsonFile>> documents_;
};

}  // namespace PY4GW

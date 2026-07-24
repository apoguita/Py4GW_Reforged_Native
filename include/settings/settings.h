#pragma once

// =============================================================================
// Py4GW settings — INI-backed, per-scope configuration documents.
//
//   *** THIS IS A SELF-THROTTLED, SELF-PERSISTING SYSTEM. ***
//   Callers never open, load, or save. Setters mutate memory and mark the
//   document dirty; the manager's debounced autosave pump writes it to disk on
//   its own schedule, and account documents bind/load themselves once the email
//   anchor resolves. Save() (force a write now) and Reload() (force a re-read
//   now) are escape hatches — use them ONLY when the app specifically requires
//   it (e.g. guaranteeing bytes on disk before a hard exit, or picking up an
//   external edit), never as part of normal flow.
//
// See docs/settings-ini-design.md for the full design. This header declares the
// two types behind the `PySettings` Python module (bound in
// src/settings/settings_bindings.cpp):
//
//   * IniFile         — one INI document (a single file) with a typed, never-
//                       throwing get/set surface. Thread-safe.
//   * SettingsManager — process-wide registry + owner of every IniFile, the
//                       account-binding step, and the debounced autosave pump.
//
// Design in one paragraph: a document is addressed by a (name, scope) pair; the
// manager returns the SAME IniFile for the same pair for the life of the
// process. Callers never open/close/flush explicitly — setters mutate memory and
// mark the document dirty, and the manager's autosave pump (stepped from the
// runtime loop) writes it back on a debounce. Account-scoped documents stage in
// memory until the logged-in email resolves, then bind to disk automatically.
// =============================================================================

#include "base/error_handling.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace PY4GW {

// Where a document's file lives, and when it becomes bound to disk:
//   Scope::Account -> settings/<email>/<name>  (staged in memory until the
//                                               account anchor/email resolves,
//                                               then bound). Per-account prefs.
//   Scope::Global  -> settings/Global/<name>   (bound immediately, shared by
//                                               every account on the machine).
//   Scope::Root    -> <module>/Py4GW.ini       (the SINGLE file allowed outside
//                                               the settings/ jail). INTERNAL
//                                               ONLY: not selectable by name and
//                                               not exposed to Python's scope
//                                               argument. Reachable solely via
//                                               OpenPy4GWIni(), which hard-codes
//                                               the filename, so no (name, scope)
//                                               can ever bind to the bare root.
enum class SettingsScope {
    Account,
    Global,
    Root
};

// One INI document: an ordered list of sections, each an ordered list of lines
// (key/value, comment, or raw) so that round-tripping preserves formatting and
// comments. All public methods are thread-safe (guarded by mutex_). Instances
// are created and owned exclusively by SettingsManager — construct via
// SettingsManager::Open(), never directly (the constructor is private).
class IniFile {
public:
    // --- Typed reads -------------------------------------------------------
    // Never throw. A missing key, missing section, or text that will not convert
    // returns the caller's default. Reads succeed even before the document binds
    // to disk (they see in-memory / freshly-parsed state).
    std::string GetString(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    bool GetBool(const std::string& section, const std::string& key, bool default_value = false) const;
    long long GetInt(const std::string& section, const std::string& key, long long default_value = 0) const;
    double GetFloat(const std::string& section, const std::string& key, double default_value = 0.0) const;

    // --- Typed writes ------------------------------------------------------
    // Take effect in memory immediately and mark the document dirty. They do NOT
    // touch disk directly; the manager's debounced autosave pump persists the
    // change (≈2 s after the last write, or within ≈10 s, and on shutdown).
    // Values are serialized to their INI string form.
    void SetString(const std::string& section, const std::string& key, const std::string& value);
    void SetBool(const std::string& section, const std::string& key, bool value);
    void SetInt(const std::string& section, const std::string& key, long long value);
    void SetFloat(const std::string& section, const std::string& key, double value);

    // --- Introspection / structural edits ----------------------------------
    bool HasKey(const std::string& section, const std::string& key) const;
    std::vector<std::string> GetSections() const;
    std::vector<std::string> GetKeys(const std::string& section) const;
    bool DeleteKey(const std::string& section, const std::string& key);      // true if it existed
    bool DeleteSection(const std::string& section);                          // true if it existed

    // --- Escape hatches ----------------------------------------------------
    // Normal flow never calls these — the autosave pump handles persistence.
    bool Save();     // force an immediate write to disk, bypassing the debounce
    bool Reload();   // re-read from disk, DISCARDING unsaved in-memory changes

    // --- Status ------------------------------------------------------------
    bool IsDirty() const;              // has unsaved in-memory changes
    bool IsBound() const;              // attached to a concrete file on disk yet
    const std::string& Name() const;   // the document's (name) identifier
    SettingsScope Scope() const;
    std::filesystem::path Path() const; // absolute path; empty until bound

private:
    friend class SettingsManager;

    // One physical line of the file. Keeping comment/raw lines lets Save()
    // round-trip the file without dropping the user's formatting or comments.
    struct IniLine {
        enum class Kind { KeyValue, Comment, Raw };
        Kind kind = Kind::Raw;
        std::string key;
        std::string value;
        std::string raw;
    };
    // A `[section]` and the lines under it. An empty name is the preamble that
    // precedes the first `[header]`.
    struct IniSection {
        std::string name;  // empty name = preamble before the first header
        std::vector<IniLine> lines;
    };

    // Constructed only by SettingsManager::Open(). Non-copyable: a document is a
    // unique, shared resource keyed by (name, scope).
    IniFile(std::string name, SettingsScope scope);
    IniFile(const IniFile&) = delete;
    IniFile& operator=(const IniFile&) = delete;

    // Attach the document to a concrete path and load it (called once the scope's
    // anchor is known — immediately for Global/Root, after the email for Account).
    void Bind(const std::filesystem::path& path);
    // Stepped by SettingsManager::Update(); saves this document when its dirty
    // debounce window elapses (write-quiet or max-dirty; see settings.cpp).
    void AutosaveTick(uint64_t now_ms);

    // Locked helpers (caller already holds mutex_):
    bool LoadLocked();                 // read + parse the file into sections_
    void SeedFromTemplateLocked();     // seed a brand-new file from settings/Defaults/*.cfg
    bool SaveLocked();                 // serialize + write, clear dirty
    std::string SerializeLocked() const;
    void ParseLocked(const std::string& content);
    void MarkDirtyLocked();            // set dirty + stamp the debounce ticks

    const IniSection* FindSectionLocked(const std::string& section) const;
    IniSection& FindOrCreateSectionLocked(const std::string& section);
    const IniLine* FindKeyLocked(const std::string& section, const std::string& key) const;
    void SetValueLocked(const std::string& section, const std::string& key, const std::string& value);

    mutable std::mutex mutex_;         // guards every field below (all public methods lock)
    std::string name_;
    SettingsScope scope_;
    std::filesystem::path path_;
    bool bound_ = false;
    bool dirty_ = false;
    uint64_t first_dirty_tick_ = 0;    // when the current dirty streak started (max-dirty cap)
    uint64_t last_change_tick_ = 0;    // when the last write happened (write-quiet debounce)
    std::vector<IniSection> sections_;
};

// Process-wide owner and registry of every IniFile. Also drives account binding,
// the autosave pump, and cross-account copy operations. Singleton (Instance()).
class SettingsManager {
public:
    static SettingsManager& Instance();

    // Return the process-wide document for (name, scope); the SAME (name, scope)
    // always yields the SAME IniFile. `name` is sanitized to a safe relative
    // subpath under the scope's jail. Only Account and Global are openable by
    // name - both resolve strictly under settings/. Passing Root here is a
    // misuse: it is redirected into settings/Global and logged (see settings.cpp),
    // never bound to the project root. This is the entry point Python's
    // PySettings.settings() calls straight through to.
    IniFile& Open(const std::string& name, SettingsScope scope = SettingsScope::Account);

    // The ONE document permitted outside the settings/ jail: <module>/Py4GW.ini.
    // Hard-coded filename, no parameters - the only route to the project root, and
    // therefore unbypassable. Used by the few internal callers that must share the
    // launcher's root INI (autoexec, console, version). Python reaches it through
    // PySettings.py4gw_ini(); it is intentionally the sole exception.
    IniFile& OpenPy4GWIni();

    // Step once per frame from the runtime update loop. Two jobs: (1) bind any
    // account documents that were staged, now that the email anchor has resolved;
    // (2) run the debounced autosave pump over every dirty bound document.
    void Update();

    // Save every dirty bound document immediately. Wired into shutdown so nothing
    // in-flight is lost when the client exits.
    void FlushAll();

    // --- Cross-account copy ------------------------------------------------
    // Copy config from THIS account's (name, Account) document into ANOTHER
    // account's document on disk (settings/<target_email>/<name>). The caller
    // reads its own live document; the values are OVERLAID onto the target's file
    // (existing target keys not present in the source are left untouched). The
    // target account's running client sees them on its next reload
    // (message-triggered or throttled) — this is a disk write, not a live
    // cross-process mutation. Returns false on a rejected email / save failure;
    // copying zero matching keys is a success. `keys` are matched verbatim (the
    // Python wrapper lowercases them to match on-disk key casing).
    //   ...Document -> the whole file (every section + key)
    //   ...Section  -> one section (all its keys)
    //   ...Keys     -> a named subset of keys within one section
    bool CopyDocumentToAccount(const std::string& name, const std::string& target_email);
    bool CopySectionToAccount(const std::string& name, const std::string& section,
                              const std::string& target_email);
    bool CopyKeysToAccount(const std::string& name, const std::string& section,
                           const std::vector<std::string>& keys,
                           const std::string& target_email);

    // Like Copy*, but the values are SUPPLIED by the caller (an explicit
    // key->value mapping - e.g. a saved profile or transformed settings) rather
    // than read from the caller's own document. Overlaid onto the target's
    // section in settings/<target_email>/<name>.
    bool ApplySectionToAccount(const std::string& name, const std::string& section,
                               const std::vector<std::pair<std::string, std::string>>& values,
                               const std::string& target_email);

private:
    // Shared writer: overlay (section, key, value) triples read from the caller's
    // own document onto settings/<target_email>/<name>, then save.
    bool WriteTriplesToAccount(
        const std::string& name,
        const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
        const std::string& target_email);

    SettingsManager() = default;
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    std::mutex registry_mutex_;                          // guards documents_
    std::vector<std::unique_ptr<IniFile>> documents_;    // owns every IniFile
};

}  // namespace PY4GW

#include "base/error_handling.h"

#include "settings/settings.h"

#include <cstdlib>
#include <sstream>

namespace PY4GW {

namespace {

std::string ToLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

std::string FormatFloat(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

}  // namespace

std::string IniFile::GetString(const std::string& section, const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const IniLine* line = FindKeyLocked(section, key);
    return line ? line->value : default_value;
}

bool IniFile::GetBool(const std::string& section, const std::string& key, bool default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const IniLine* line = FindKeyLocked(section, key);
    if (!line) {
        return default_value;
    }
    const std::string lowered = ToLower(line->value);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return default_value;
}

long long IniFile::GetInt(const std::string& section, const std::string& key, long long default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const IniLine* line = FindKeyLocked(section, key);
    if (!line || line->value.empty()) {
        return default_value;
    }
    char* end = nullptr;
    const long long parsed = std::strtoll(line->value.c_str(), &end, 0);
    return (end && *end == '\0') ? parsed : default_value;
}

double IniFile::GetFloat(const std::string& section, const std::string& key, double default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const IniLine* line = FindKeyLocked(section, key);
    if (!line || line->value.empty()) {
        return default_value;
    }
    char* end = nullptr;
    const double parsed = std::strtod(line->value.c_str(), &end);
    return (end && *end == '\0') ? parsed : default_value;
}

void IniFile::SetString(const std::string& section, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    SetValueLocked(section, key, value);
    MarkDirtyLocked();
}

void IniFile::SetBool(const std::string& section, const std::string& key, bool value) {
    SetString(section, key, value ? "true" : "false");
}

void IniFile::SetInt(const std::string& section, const std::string& key, long long value) {
    SetString(section, key, std::to_string(value));
}

void IniFile::SetFloat(const std::string& section, const std::string& key, double value) {
    SetString(section, key, FormatFloat(value));
}

bool IniFile::HasKey(const std::string& section, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return FindKeyLocked(section, key) != nullptr;
}

std::vector<std::string> IniFile::GetSections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& section : sections_) {
        if (!section.name.empty()) {
            names.push_back(section.name);
        }
    }
    return names;
}

std::vector<std::string> IniFile::GetKeys(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    if (const IniSection* found = FindSectionLocked(section)) {
        for (const auto& line : found->lines) {
            if (line.kind == IniLine::Kind::KeyValue) {
                keys.push_back(line.key);
            }
        }
    }
    return keys;
}

bool IniFile::DeleteKey(const std::string& section, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& candidate : sections_) {
        if (candidate.name != section) {
            continue;
        }
        for (auto it = candidate.lines.begin(); it != candidate.lines.end(); ++it) {
            if (it->kind == IniLine::Kind::KeyValue && it->key == key) {
                candidate.lines.erase(it);
                MarkDirtyLocked();
                return true;
            }
        }
    }
    return false;
}

bool IniFile::DeleteSection(const std::string& section) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sections_.begin(); it != sections_.end(); ++it) {
        if (!it->name.empty() && it->name == section) {
            sections_.erase(it);
            MarkDirtyLocked();
            return true;
        }
    }
    return false;
}

bool IniFile::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    return SaveLocked();
}

bool IniFile::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!bound_) {
        return false;
    }
    // Autosave owns persistence for local changes. Do not replace a dirty
    // in-memory document with disk contents before the next autosave tick.
    if (dirty_) {
        return false;
    }
    sections_.clear();
    const bool loaded = LoadLocked();
    dirty_ = false;
    first_dirty_tick_ = 0;
    return loaded;
}

bool IniFile::IsDirty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dirty_;
}

bool IniFile::IsBound() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bound_;
}

const std::string& IniFile::Name() const {
    return name_;
}

SettingsScope IniFile::Scope() const {
    return scope_;
}

std::filesystem::path IniFile::Path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

}  // namespace PY4GW

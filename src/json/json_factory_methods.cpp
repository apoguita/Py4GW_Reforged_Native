#include "base/error_handling.h"

#include "json/json_factory.h"

#include <cstdlib>
#include <sstream>

namespace PY4GW {

namespace {

using json = nlohmann::json;

std::string ToLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

// Split an access path into non-empty segments (duplicated intentionally from
// json_factory.cpp's anonymous namespace; both are tiny and TU-local).
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

const json* TraverseConst(const json& root, const std::vector<std::string>& segments) {
    const json* node = &root;
    for (const auto& segment : segments) {
        if (node->is_object()) {
            const auto it = node->find(segment);
            if (it == node->end()) {
                return nullptr;
            }
            node = &(*it);
        } else if (node->is_array()) {
            size_t index = 0;
            if (!ParseArrayIndex(segment, &index) || index >= node->size()) {
                return nullptr;
            }
            node = &(*node)[index];
        } else {
            return nullptr;
        }
    }
    return node;
}

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

std::string FormatFloat(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

}  // namespace

std::string JsonFile::GetString(const std::string& path, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    if (!node) {
        return default_value;
    }
    if (node->is_string()) {
        return node->get<std::string>();
    }
    if (node->is_boolean()) {
        return node->get<bool>() ? "true" : "false";
    }
    if (node->is_number_integer()) {
        return std::to_string(node->get<long long>());
    }
    if (node->is_number_unsigned()) {
        return std::to_string(node->get<unsigned long long>());
    }
    if (node->is_number_float()) {
        return FormatFloat(node->get<double>());
    }
    return default_value;
}

bool JsonFile::GetBool(const std::string& path, bool default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    if (!node) {
        return default_value;
    }
    if (node->is_boolean()) {
        return node->get<bool>();
    }
    if (node->is_number()) {
        return node->get<double>() != 0.0;
    }
    if (node->is_string()) {
        const std::string lowered = ToLower(node->get<std::string>());
        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
            return true;
        }
        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
            return false;
        }
    }
    return default_value;
}

long long JsonFile::GetInt(const std::string& path, long long default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    if (!node) {
        return default_value;
    }
    if (node->is_number_integer() || node->is_number_unsigned()) {
        return node->get<long long>();
    }
    if (node->is_number_float()) {
        return static_cast<long long>(node->get<double>());
    }
    if (node->is_boolean()) {
        return node->get<bool>() ? 1 : 0;
    }
    if (node->is_string()) {
        const std::string text = node->get<std::string>();
        if (text.empty()) {
            return default_value;
        }
        char* end = nullptr;
        const long long parsed = std::strtoll(text.c_str(), &end, 0);
        return (end && *end == '\0') ? parsed : default_value;
    }
    return default_value;
}

double JsonFile::GetFloat(const std::string& path, double default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    if (!node) {
        return default_value;
    }
    if (node->is_number()) {
        return node->get<double>();
    }
    if (node->is_boolean()) {
        return node->get<bool>() ? 1.0 : 0.0;
    }
    if (node->is_string()) {
        const std::string text = node->get<std::string>();
        if (text.empty()) {
            return default_value;
        }
        char* end = nullptr;
        const double parsed = std::strtod(text.c_str(), &end);
        return (end && *end == '\0') ? parsed : default_value;
    }
    return default_value;
}

void JsonFile::SetString(const std::string& path, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const json node = value;
    SetAtPath(root_, SplitPath(path), node);
    RecordOpLocked(path, false, node);
    MarkDirtyLocked();
}

void JsonFile::SetBool(const std::string& path, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const json node = value;
    SetAtPath(root_, SplitPath(path), node);
    RecordOpLocked(path, false, node);
    MarkDirtyLocked();
}

void JsonFile::SetInt(const std::string& path, long long value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const json node = value;
    SetAtPath(root_, SplitPath(path), node);
    RecordOpLocked(path, false, node);
    MarkDirtyLocked();
}

void JsonFile::SetFloat(const std::string& path, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const json node = value;
    SetAtPath(root_, SplitPath(path), node);
    RecordOpLocked(path, false, node);
    MarkDirtyLocked();
}

nlohmann::json JsonFile::GetNode(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    return node ? *node : json(nullptr);
}

void JsonFile::SetNode(const std::string& path, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    SetAtPath(root_, SplitPath(path), value);
    RecordOpLocked(path, false, value);
    MarkDirtyLocked();
}

void JsonFile::Append(const std::string& path, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto segments = SplitPath(path);
    // Read-or-create the array, push, then write the whole array back so the
    // journal records a single deterministic set (safe for Global merge replay).
    const json* existing = TraverseConst(root_, segments);
    json array = (existing && existing->is_array()) ? *existing : json::array();
    array.push_back(value);
    SetAtPath(root_, segments, array);
    RecordOpLocked(path, false, array);
    MarkDirtyLocked();
}

bool JsonFile::Has(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return TraverseConst(root_, SplitPath(path)) != nullptr;
}

bool JsonFile::Delete(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto segments = SplitPath(path);
    if (segments.empty()) {
        return false;
    }
    // DeleteAtPath lives in json_factory.cpp; re-implement the walk locally to
    // keep this TU self-contained, then journal the delete.
    const json* node = TraverseConst(root_, segments);
    if (!node) {
        return false;
    }
    // Perform the delete by rebuilding via SetAtPath is not possible for
    // removal, so walk to the parent and erase.
    json* cursor = &root_;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (cursor->is_object()) {
            cursor = &(*cursor)[segment];
        } else if (cursor->is_array()) {
            size_t index = 0;
            ParseArrayIndex(segment, &index);
            cursor = &(*cursor)[index];
        }
    }
    const std::string& last = segments.back();
    bool removed = false;
    if (cursor->is_object()) {
        removed = cursor->erase(last) > 0;
    } else if (cursor->is_array()) {
        size_t index = 0;
        if (ParseArrayIndex(last, &index) && index < cursor->size()) {
            cursor->erase(cursor->begin() + static_cast<std::ptrdiff_t>(index));
            removed = true;
        }
    }
    if (removed) {
        RecordOpLocked(path, true, json(nullptr));
        MarkDirtyLocked();
    }
    return removed;
}

std::vector<std::string> JsonFile::Keys(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    const json* node = TraverseConst(root_, SplitPath(path));
    if (node && node->is_object()) {
        for (auto it = node->begin(); it != node->end(); ++it) {
            keys.push_back(it.key());
        }
    }
    return keys;
}

size_t JsonFile::Size(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    if (node && (node->is_array() || node->is_object())) {
        return node->size();
    }
    return 0;
}

bool JsonFile::IsArray(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    return node && node->is_array();
}

bool JsonFile::IsObject(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = TraverseConst(root_, SplitPath(path));
    return node && node->is_object();
}

bool JsonFile::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    return SaveLocked();
}

bool JsonFile::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!bound_) {
        return false;
    }
    root_ = json::object();
    const bool loaded = LoadLocked();
    dirty_ = false;
    first_dirty_tick_ = 0;
    pending_.clear();
    return loaded;
}

bool JsonFile::IsDirty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dirty_;
}

bool JsonFile::IsBound() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bound_;
}

const std::string& JsonFile::Name() const {
    return name_;
}

JsonScope JsonFile::Scope() const {
    return scope_;
}

std::filesystem::path JsonFile::Path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

}  // namespace PY4GW

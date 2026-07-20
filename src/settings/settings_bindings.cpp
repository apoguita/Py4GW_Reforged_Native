#include "base/py_bindings.h"

#include "settings/settings.h"
#include "system/system.h"

#include <string>
#include <utility>

namespace py = pybind11;

namespace {

constexpr const char* kDefaultSection = "settings";

PY4GW::SettingsScope ParseScope(const std::string& scope) {
    if (scope == "global") {
        return PY4GW::SettingsScope::Global;
    }
    if (scope == "account") {
        return PY4GW::SettingsScope::Account;
    }
    if (scope == "root") {
        return PY4GW::SettingsScope::Root;
    }
    throw py::value_error("scope must be \"account\", \"global\", or \"root\"");
}

// Flat keys land in the default section; "section/key" addresses one explicitly.
std::pair<std::string, std::string> SplitKey(const std::string& key) {
    const auto separator = key.find('/');
    if (separator == std::string::npos) {
        return {kDefaultSection, key};
    }
    return {key.substr(0, separator), key.substr(separator + 1)};
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PySettings, m) {
    m.doc() = "Py4GW per-account INI settings (see docs/settings-ini-design.md)";

    py::class_<PY4GW::IniFile, std::unique_ptr<PY4GW::IniFile, py::nodelete>> cls(m, "settings");
    cls.def(py::init([](const std::string& name, const std::string& scope) {
        return &PY4GW::SettingsManager::Instance().Open(name, ParseScope(scope));
    }), py::arg("name"), py::arg("scope") = "account",
        "Bind to a named settings document; no open/close/save needed");

    // write: value type selected by overload. bool is registered before int
    // because Python bool is a subclass of int.
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, bool value) {
        const auto [section, k] = SplitKey(key);
        self.SetBool(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, long long value) {
        const auto [section, k] = SplitKey(key);
        self.SetInt(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, double value) {
        const auto [section, k] = SplitKey(key);
        self.SetFloat(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, const std::string& value) {
        const auto [section, k] = SplitKey(key);
        self.SetString(section, k, value);
    }, py::arg("key"), py::arg("value"));

    // read: the second argument is either a default value (fallback + type)
    // or a Python type token (bool/int/float/str) whose zero value is the
    // fallback.
    cls.def("read", [](PY4GW::IniFile& self, const std::string& key, py::object default_or_type) -> py::object {
        const auto [section, k] = SplitKey(key);
        PyObject* ptr = default_or_type.ptr();

        if (PyType_Check(ptr)) {
            if (ptr == reinterpret_cast<PyObject*>(&PyBool_Type)) {
                return py::bool_(self.GetBool(section, k, false));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyLong_Type)) {
                return py::int_(self.GetInt(section, k, 0));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyFloat_Type)) {
                return py::float_(self.GetFloat(section, k, 0.0));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyUnicode_Type)) {
                return py::str(self.GetString(section, k, ""));
            }
            throw py::type_error("read(): unsupported type token; use bool, int, float, or str");
        }

        if (py::isinstance<py::bool_>(default_or_type)) {
            return py::bool_(self.GetBool(section, k, default_or_type.cast<bool>()));
        }
        if (py::isinstance<py::int_>(default_or_type)) {
            return py::int_(self.GetInt(section, k, default_or_type.cast<long long>()));
        }
        if (py::isinstance<py::float_>(default_or_type)) {
            return py::float_(self.GetFloat(section, k, default_or_type.cast<double>()));
        }
        if (py::isinstance<py::str>(default_or_type)) {
            return py::str(self.GetString(section, k, default_or_type.cast<std::string>()));
        }
        throw py::type_error("read(): default must be bool, int, float, or str, or one of those types");
    }, py::arg("key"), py::arg("default") = py::str(""));

    // Escape hatches; never required in normal flow.
    cls.def("save", &PY4GW::IniFile::Save, "Force an immediate save");
    cls.def("reload", &PY4GW::IniFile::Reload, "Re-read from disk, discarding unsaved changes");
    cls.def("is_dirty", &PY4GW::IniFile::IsDirty);
    cls.def("is_bound", &PY4GW::IniFile::IsBound, "Whether the document is attached to disk yet");
    cls.def("path", [](const PY4GW::IniFile& self) -> std::string {
        return self.Path().string();
    }, "Absolute on-disk path of this document (empty until bound)");
    cls.def("has_key", [](const PY4GW::IniFile& self, const std::string& key) {
        const auto [section, k] = SplitKey(key);
        return self.HasKey(section, k);
    }, py::arg("key"));
    cls.def("keys", [](const PY4GW::IniFile& self, const std::string& section) {
        return self.GetKeys(section);
    }, py::arg("section") = kDefaultSection);
    cls.def("sections", &PY4GW::IniFile::GetSections);
    cls.def("delete", [](PY4GW::IniFile& self, const std::string& key) {
        const auto [section, k] = SplitKey(key);
        return self.DeleteKey(section, k);
    }, py::arg("key"));
    cls.def("delete_section", [](PY4GW::IniFile& self, const std::string& section) {
        return self.DeleteSection(section);
    }, py::arg("section"));

    // Explicit (section, key) API. Unlike write/read above, these take section
    // and key as SEPARATE arguments and never parse a delimiter, so names may
    // contain '/', '\\', ':' or spaces (e.g. "Widget:Guild Wars\\Triggers/Foo.py").
    // bool is registered before int because Python bool subclasses int.
    cls.def("set", [](PY4GW::IniFile& self, const std::string& section, const std::string& key, bool value) {
        self.SetBool(section, key, value);
    }, py::arg("section"), py::arg("key"), py::arg("value"));
    cls.def("set", [](PY4GW::IniFile& self, const std::string& section, const std::string& key, long long value) {
        self.SetInt(section, key, value);
    }, py::arg("section"), py::arg("key"), py::arg("value"));
    cls.def("set", [](PY4GW::IniFile& self, const std::string& section, const std::string& key, double value) {
        self.SetFloat(section, key, value);
    }, py::arg("section"), py::arg("key"), py::arg("value"));
    cls.def("set", [](PY4GW::IniFile& self, const std::string& section, const std::string& key, const std::string& value) {
        self.SetString(section, key, value);
    }, py::arg("section"), py::arg("key"), py::arg("value"));

    cls.def("get", [](PY4GW::IniFile& self, const std::string& section, const std::string& key,
                      py::object default_or_type) -> py::object {
        PyObject* ptr = default_or_type.ptr();
        if (PyType_Check(ptr)) {
            if (ptr == reinterpret_cast<PyObject*>(&PyBool_Type)) return py::bool_(self.GetBool(section, key, false));
            if (ptr == reinterpret_cast<PyObject*>(&PyLong_Type)) return py::int_(self.GetInt(section, key, 0));
            if (ptr == reinterpret_cast<PyObject*>(&PyFloat_Type)) return py::float_(self.GetFloat(section, key, 0.0));
            if (ptr == reinterpret_cast<PyObject*>(&PyUnicode_Type)) return py::str(self.GetString(section, key, ""));
            throw py::type_error("get(): unsupported type token; use bool, int, float, or str");
        }
        if (py::isinstance<py::bool_>(default_or_type)) return py::bool_(self.GetBool(section, key, default_or_type.cast<bool>()));
        if (py::isinstance<py::int_>(default_or_type)) return py::int_(self.GetInt(section, key, default_or_type.cast<long long>()));
        if (py::isinstance<py::float_>(default_or_type)) return py::float_(self.GetFloat(section, key, default_or_type.cast<double>()));
        if (py::isinstance<py::str>(default_or_type)) return py::str(self.GetString(section, key, default_or_type.cast<std::string>()));
        throw py::type_error("get(): default must be bool, int, float, or str, or one of those types");
    }, py::arg("section"), py::arg("key"), py::arg("default") = py::str(""));

    cls.def("has", [](const PY4GW::IniFile& self, const std::string& section, const std::string& key) {
        return self.HasKey(section, key);
    }, py::arg("section"), py::arg("key"));
    cls.def("remove", [](PY4GW::IniFile& self, const std::string& section, const std::string& key) {
        return self.DeleteKey(section, key);
    }, py::arg("section"), py::arg("key"));
    cls.def("items", [](const PY4GW::IniFile& self, const std::string& section) {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& k : self.GetKeys(section)) {
            out.emplace_back(k, self.GetString(section, k, ""));
        }
        return out;
    }, py::arg("section"), "Return (key, value) string pairs for a section");

    // Copy this account's config into ANOTHER account's document on disk. Three
    // granularities: whole file, one section, or a named subset of keys. The
    // target's running client applies the change on its next reload.
    m.def("copy_document_to_account", [](const std::string& name, const std::string& target_email) {
        return PY4GW::SettingsManager::Instance().CopyDocumentToAccount(name, target_email);
    }, py::arg("name"), py::arg("target_email"),
        "Copy an entire document (all sections) into another account's file on disk");
    m.def("copy_section_to_account", [](const std::string& name, const std::string& section,
                                        const std::string& target_email) {
        return PY4GW::SettingsManager::Instance().CopySectionToAccount(name, section, target_email);
    }, py::arg("name"), py::arg("section"), py::arg("target_email"),
        "Copy one whole section into another account's file on disk");
    m.def("copy_keys_to_account", [](const std::string& name, const std::string& section,
                                     const std::vector<std::string>& keys, const std::string& target_email) {
        return PY4GW::SettingsManager::Instance().CopyKeysToAccount(name, section, keys, target_email);
    }, py::arg("name"), py::arg("section"), py::arg("keys"), py::arg("target_email"),
        "Copy a named subset of a section's keys into another account's file on disk");
    m.def("apply_section_to_account", [](const std::string& name, const std::string& section,
                                         const std::vector<std::pair<std::string, std::string>>& values,
                                         const std::string& target_email) {
        return PY4GW::SettingsManager::Instance().ApplySectionToAccount(name, section, values, target_email);
    }, py::arg("name"), py::arg("section"), py::arg("values"), py::arg("target_email"),
        "Overlay a caller-supplied key/value mapping into another account's section on disk");

    m.def("is_anchored", []() {
        return PY4GW::System::Instance().HasAccountEmail();
    }, "Whether account-scoped documents are bound to disk yet");
    m.def("get_settings_directory", []() -> std::string {
        return PY4GW::System::Instance().GetSettingsDirectory().string();
    }, "Per-account settings directory (empty until the anchor resolves)");
}

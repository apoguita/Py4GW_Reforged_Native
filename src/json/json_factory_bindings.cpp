#include "base/py_bindings.h"

#include "json/json_factory.h"
#include "base/process_manager.h"
#include "system/system.h"

#include <nlohmann/json.hpp>

#include <string>

namespace py = pybind11;

namespace {

using json = nlohmann::json;

PY4GW::JsonScope ParseScope(const std::string& scope) {
    if (scope == "global") {
        return PY4GW::JsonScope::Global;
    }
    if (scope == "account") {
        return PY4GW::JsonScope::Account;
    }
    // No "root" for JSON: every document lives under json/. The only root-level
    // exception in the whole system is Py4GW.ini, which is INI-only.
    throw py::value_error("scope must be \"account\" or \"global\"");
}

// Recursively convert an nlohmann::json value to a native Python object.
py::object JsonToPy(const json& value) {
    switch (value.type()) {
    case json::value_t::null:
        return py::none();
    case json::value_t::boolean:
        return py::bool_(value.get<bool>());
    case json::value_t::number_integer:
        return py::int_(value.get<long long>());
    case json::value_t::number_unsigned:
        return py::int_(value.get<unsigned long long>());
    case json::value_t::number_float:
        return py::float_(value.get<double>());
    case json::value_t::string:
        return py::str(value.get<std::string>());
    case json::value_t::array: {
        py::list out;
        for (const auto& item : value) {
            out.append(JsonToPy(item));
        }
        return out;
    }
    case json::value_t::object: {
        py::dict out;
        for (auto it = value.begin(); it != value.end(); ++it) {
            out[py::str(it.key())] = JsonToPy(it.value());
        }
        return out;
    }
    default:
        return py::none();
    }
}

// Recursively convert a native Python object to an nlohmann::json value. bool is
// checked before int because Python bool subclasses int.
json PyToJson(py::handle obj) {
    if (obj.is_none()) {
        return json(nullptr);
    }
    if (py::isinstance<py::bool_>(obj)) {
        return json(obj.cast<bool>());
    }
    if (py::isinstance<py::int_>(obj)) {
        return json(obj.cast<long long>());
    }
    if (py::isinstance<py::float_>(obj)) {
        return json(obj.cast<double>());
    }
    if (py::isinstance<py::str>(obj)) {
        return json(obj.cast<std::string>());
    }
    if (py::isinstance<py::bytes>(obj)) {
        return json(obj.cast<std::string>());
    }
    if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
        json out = json::array();
        for (const auto& item : obj) {
            out.push_back(PyToJson(item));
        }
        return out;
    }
    if (py::isinstance<py::dict>(obj)) {
        json out = json::object();
        for (const auto& item : obj.cast<py::dict>()) {
            out[py::str(item.first).cast<std::string>()] = PyToJson(item.second);
        }
        return out;
    }
    throw py::type_error("set_json(): unsupported value type; use bool/int/float/str/list/dict/None");
}

// Typed leaf read: the argument is either a default value (fallback + type) or a
// Python type token (bool/int/float/str) whose zero value is the fallback.
py::object TypedGet(const PY4GW::JsonFile& self, const std::string& path, py::object default_or_type) {
    PyObject* ptr = default_or_type.ptr();
    if (PyType_Check(ptr)) {
        if (ptr == reinterpret_cast<PyObject*>(&PyBool_Type)) return py::bool_(self.GetBool(path, false));
        if (ptr == reinterpret_cast<PyObject*>(&PyLong_Type)) return py::int_(self.GetInt(path, 0));
        if (ptr == reinterpret_cast<PyObject*>(&PyFloat_Type)) return py::float_(self.GetFloat(path, 0.0));
        if (ptr == reinterpret_cast<PyObject*>(&PyUnicode_Type)) return py::str(self.GetString(path, ""));
        throw py::type_error("get(): unsupported type token; use bool, int, float, or str");
    }
    if (py::isinstance<py::bool_>(default_or_type)) return py::bool_(self.GetBool(path, default_or_type.cast<bool>()));
    if (py::isinstance<py::int_>(default_or_type)) return py::int_(self.GetInt(path, default_or_type.cast<long long>()));
    if (py::isinstance<py::float_>(default_or_type)) return py::float_(self.GetFloat(path, default_or_type.cast<double>()));
    if (py::isinstance<py::str>(default_or_type)) return py::str(self.GetString(path, default_or_type.cast<std::string>()));
    throw py::type_error("get(): default must be bool, int, float, or str, or one of those types");
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyJson, m) {
    m.doc() = "Py4GW per-account structured JSON documents (see docs/json-factory-design.md)";

    py::class_<PY4GW::JsonFile, std::unique_ptr<PY4GW::JsonFile, py::nodelete>> cls(m, "JsonFile");
    cls.def(py::init([](const std::string& name, const std::string& scope) {
        return &PY4GW::JsonFactory::Instance().Open(name, ParseScope(scope));
    }), py::arg("name"), py::arg("scope") = "account",
        "Bind to a named JSON document; no open/close/save needed");

    // Typed leaf write: value type selected by overload. bool before int.
    cls.def("set", [](PY4GW::JsonFile& self, const std::string& path, bool value) {
        self.SetBool(path, value);
    }, py::arg("path"), py::arg("value"));
    cls.def("set", [](PY4GW::JsonFile& self, const std::string& path, long long value) {
        self.SetInt(path, value);
    }, py::arg("path"), py::arg("value"));
    cls.def("set", [](PY4GW::JsonFile& self, const std::string& path, double value) {
        self.SetFloat(path, value);
    }, py::arg("path"), py::arg("value"));
    cls.def("set", [](PY4GW::JsonFile& self, const std::string& path, const std::string& value) {
        self.SetString(path, value);
    }, py::arg("path"), py::arg("value"));

    cls.def("get", &TypedGet, py::arg("path"), py::arg("default") = py::str(""),
        "Typed leaf read; default value declares the type, or pass a type token (bool/int/float/str)");

    // Native subtree access (dict/list/scalars).
    cls.def("set_json", [](PY4GW::JsonFile& self, const std::string& path, py::object value) {
        self.SetNode(path, PyToJson(value));
    }, py::arg("path"), py::arg("value"), "Replace the subtree at path with a Python dict/list/scalar");
    cls.def("get_json", [](const PY4GW::JsonFile& self, const std::string& path) -> py::object {
        return JsonToPy(self.GetNode(path));
    }, py::arg("path") = std::string(), "Return the subtree at path as a native Python object (None if absent)");
    cls.def("append", [](PY4GW::JsonFile& self, const std::string& path, py::object value) {
        self.Append(path, PyToJson(value));
    }, py::arg("path"), py::arg("value"), "Push a value onto the array at path (creating it if absent)");

    // Structure / introspection.
    cls.def("has", &PY4GW::JsonFile::Has, py::arg("path"));
    cls.def("delete", &PY4GW::JsonFile::Delete, py::arg("path"));
    cls.def("keys", &PY4GW::JsonFile::Keys, py::arg("path") = std::string(),
        "Object member names at a path");
    cls.def("size", [](const PY4GW::JsonFile& self, const std::string& path) {
        return self.Size(path);
    }, py::arg("path") = std::string(), "Array length / object member count at a path");
    cls.def("is_array", &PY4GW::JsonFile::IsArray, py::arg("path") = std::string());
    cls.def("is_object", &PY4GW::JsonFile::IsObject, py::arg("path") = std::string());

    // Escape hatches; never required in normal flow.
    cls.def("save", &PY4GW::JsonFile::Save, "Force an immediate save");
    cls.def("reload", &PY4GW::JsonFile::Reload, "Re-read from disk, discarding unsaved changes");
    cls.def("is_dirty", &PY4GW::JsonFile::IsDirty);
    cls.def("is_bound", &PY4GW::JsonFile::IsBound, "Whether the document is attached to disk yet");
    cls.def("path", [](const PY4GW::JsonFile& self) -> std::string {
        return self.Path().string();
    }, "Absolute on-disk path of this document (empty until bound)");

    // Cross-account copy: push this account's config into ANOTHER account's file
    // on disk (deep merge; the target sees it on its next reload).
    m.def("copy_document_to_account", [](const std::string& name, const std::string& target_email) {
        return PY4GW::JsonFactory::Instance().CopyDocumentToAccount(name, target_email);
    }, py::arg("name"), py::arg("target_email"),
        "Merge an entire document into another account's file on disk");
    m.def("copy_path_to_account", [](const std::string& name, const std::string& path,
                                     const std::string& target_email) {
        return PY4GW::JsonFactory::Instance().CopyPathToAccount(name, path, target_email);
    }, py::arg("name"), py::arg("path"), py::arg("target_email"),
        "Merge one subtree into another account's file on disk");
    m.def("apply_to_account", [](const std::string& name, const std::string& path,
                                 py::object value, const std::string& target_email) {
        return PY4GW::JsonFactory::Instance().ApplyToAccount(name, path, PyToJson(value), target_email);
    }, py::arg("name"), py::arg("path"), py::arg("value"), py::arg("target_email"),
        "Merge a caller-supplied subtree at path into another account's file on disk");

    m.def("is_anchored", []() {
        return PY4GW::System::Instance().HasAccountEmail();
    }, "Whether account-scoped documents are bound to disk yet");
    m.def("get_json_directory", []() -> std::string {
        if (!PY4GW::System::Instance().HasAccountEmail()) {
            return "";
        }
        return (PY4GW::process_manager::GetModuleDirectory() / "json" /
                PY4GW::System::Instance().GetAccountEmail()).string();
    }, "Per-account JSON directory (empty until the anchor resolves)");
}

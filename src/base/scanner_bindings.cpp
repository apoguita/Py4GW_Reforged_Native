#include "base/error_handling.h"

#include "base/logger.h"
#include "base/scanner.h"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <windows.h>

#include <cstdint>
#include <string>

namespace py = pybind11;

namespace {

// Python-facing parity surface for the legacy PyScanner module: static
// methods over PY4GW::Scanner. Sections are raw uint8_t indices matching
// PY4GW::ScannerSection (0 = .text, 1 = .rdata, 2 = .data), as in legacy.
class PyScanner {
public:
    static void Initialize(const std::string& module_name = "") {
        HMODULE module = module_name.empty() ? nullptr : ::GetModuleHandleA(module_name.c_str());
        PY4GW::Scanner::Initialize(module);
    }

    static uintptr_t Find(py::bytes pattern, const std::string& mask,
        int offset, uint8_t section) {
        const std::string pat = pattern;
        return PY4GW::Scanner::Find(pat.c_str(), mask.c_str(), offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindAssertion(const std::string& assertion_file,
        const std::string& assertion_msg,
        uint32_t line_number = 0,
        int offset = 0) {
        return PY4GW::Scanner::FindAssertion(assertion_file.c_str(),
            assertion_msg.c_str(),
            line_number,
            offset);
    }

    static uintptr_t FindInRange(py::bytes pattern, const std::string& mask,
        int offset, uintptr_t start, uintptr_t end) {
        const std::string pat = pattern;
        return PY4GW::Scanner::FindInRange(pat.c_str(), mask.c_str(), offset, start, end);
    }

    static py::tuple GetSectionAddressRange(uint8_t section) {
        const auto range = PY4GW::Scanner::GetSectionRange(
            static_cast<PY4GW::ScannerSection>(section));
        return py::make_tuple(range.start, range.end);
    }

    static uintptr_t FunctionFromNearCall(uintptr_t call_addr,
        bool check_valid_ptr = true) {
        return PY4GW::Scanner::FunctionFromNearCall(call_addr, check_valid_ptr);
    }

    static bool IsValidPtr(uintptr_t addr, uint8_t section) {
        return PY4GW::Scanner::IsValidPtr(addr,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t ToFunctionStart(uintptr_t addr,
        uint32_t scan_range = 0xFF) {
        return PY4GW::Scanner::ToFunctionStart(addr, scan_range);
    }

    static uintptr_t FindNthUseOfAddress(uintptr_t address, size_t nth,
        int offset, uint8_t section) {
        return PY4GW::Scanner::FindNthUseOfAddress(address, nth, offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindUseOfAddress(uintptr_t address, int offset,
        uint8_t section) {
        return PY4GW::Scanner::FindUseOfAddress(address, offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindUseOfStringA(const std::string& str, int offset,
        uint8_t section) {
        return PY4GW::Scanner::FindUseOfString(str.c_str(), offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindUseOfStringW(const std::wstring& str, int offset,
        uint8_t section) {
        return PY4GW::Scanner::FindUseOfString(str.c_str(), offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindNthUseOfStringA(const std::string& str, size_t nth,
        int offset, uint8_t section) {
        return PY4GW::Scanner::FindNthUseOfString(str.c_str(), nth, offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static uintptr_t FindNthUseOfStringW(const std::wstring& str, size_t nth,
        int offset, uint8_t section) {
        return PY4GW::Scanner::FindNthUseOfString(str.c_str(), nth, offset,
            static_cast<PY4GW::ScannerSection>(section));
    }

    static py::dict GetScanStatus() {
        py::dict result;
        py::dict scans;
        for (const auto& [name, addr] : Logger::GetScanResults()) {
            scans[py::cast(name)] = addr;
        }
        py::dict hooks;
        for (const auto& [name, status] : Logger::GetHookResults()) {
            hooks[py::cast(name)] = status;
        }
        result["scans"] = scans;
        result["hooks"] = hooks;
        return result;
    }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyScanner, m)
{
    py::class_<PyScanner>(m, "PyScanner")
        // Init
        .def_static("Initialize", &PyScanner::Initialize,
            py::arg("module_name") = "")

        // Pattern scanning
        .def_static("Find", &PyScanner::Find)
        .def_static("FindInRange", &PyScanner::FindInRange)

        // Function resolution
        .def_static("FunctionFromNearCall", &PyScanner::FunctionFromNearCall)
        .def_static("ToFunctionStart", &PyScanner::ToFunctionStart)

        // Pointer validation
        .def_static("IsValidPtr", &PyScanner::IsValidPtr)

        // Address usage scanning
        .def_static("FindUseOfAddress", &PyScanner::FindUseOfAddress)
        .def_static("FindNthUseOfAddress", &PyScanner::FindNthUseOfAddress)

        // String usage scanning
        .def_static("FindUseOfStringA", &PyScanner::FindUseOfStringA)
        .def_static("FindUseOfStringW", &PyScanner::FindUseOfStringW)
        .def_static("FindNthUseOfStringA", &PyScanner::FindNthUseOfStringA)
        .def_static("FindNthUseOfStringW", &PyScanner::FindNthUseOfStringW)

        .def_static("FindAssertion", &PyScanner::FindAssertion,
            py::arg("assertion_file"),
            py::arg("assertion_msg"),
            py::arg("line_number") = 0,
            py::arg("offset") = 0)

        .def_static("GetSectionAddressRange", &PyScanner::GetSectionAddressRange,
            py::arg("section"))

        .def_static("GetScanStatus", &PyScanner::GetScanStatus);
}

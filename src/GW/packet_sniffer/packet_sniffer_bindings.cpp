#include "base/py_bindings.h"

#include "GW/packet_sniffer/packet_sniffer.h"

#include <cstdio>
#include <string>

namespace py = pybind11;

namespace {

// Thin, stateless facade so the Python surface keeps the legacy shape
// (PacketSniffer.instance().initialize(), get_stoc_logs(), etc.). All state
// lives in the GW::packet_sniffer module; this only forwards.
struct PacketSnifferFacade {
    static PacketSnifferFacade& Instance() {
        static PacketSnifferFacade instance;
        return instance;
    }

    bool Initialize() { return GW::packet_sniffer::Initialize(); }
    bool InitializeStoC() { return GW::packet_sniffer::InitializeStoC(); }
    bool InitializeCToS() { return GW::packet_sniffer::InitializeCToS(); }

    void Terminate() { GW::packet_sniffer::Terminate(); }
    void TerminateStoC() { GW::packet_sniffer::TerminateStoC(); }
    void TerminateCToS() { GW::packet_sniffer::TerminateCToS(); }

    std::vector<GW::packet_sniffer::PacketLogEntry> GetLogs() {
        return GW::packet_sniffer::GetLogs();
    }
    std::vector<GW::packet_sniffer::PacketLogEntry> GetStoCLogs() {
        return GW::packet_sniffer::GetLogsByDirection(GW::packet_sniffer::PacketDirection::StoC);
    }
    std::vector<GW::packet_sniffer::PacketLogEntry> GetCToSLogs() {
        return GW::packet_sniffer::GetLogsByDirection(GW::packet_sniffer::PacketDirection::CToS);
    }

    void ClearLogs() { GW::packet_sniffer::ClearLogs(); }
    void ClearStoCLogs() { GW::packet_sniffer::ClearLogsByDirection(GW::packet_sniffer::PacketDirection::StoC); }
    void ClearCToSLogs() { GW::packet_sniffer::ClearLogsByDirection(GW::packet_sniffer::PacketDirection::CToS); }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyPacketSniffer, m) {
    using GW::packet_sniffer::PacketDirection;
    using GW::packet_sniffer::PacketLogEntry;

    m.doc() = R"doc(
Unified packet sniffer for Py4GW.

Captures raw StoC and CToS packets through a single PacketSniffer facade.
)doc";

    py::enum_<PacketDirection>(m, "PacketDirection")
        .value("StoC", PacketDirection::StoC)
        .value("CToS", PacketDirection::CToS)
        .export_values();

    py::class_<PacketLogEntry>(m, "PacketLogEntry")
        .def(py::init<>())
        .def_readonly("tick", &PacketLogEntry::tick)
        .def_readonly("direction", &PacketLogEntry::direction)
        .def_readonly("header", &PacketLogEntry::header)
        .def_readonly("size", &PacketLogEntry::size)
        .def_readonly("data", &PacketLogEntry::data)
        .def("__repr__", [](const PacketLogEntry& entry) {
            const char* direction = entry.direction == PacketDirection::StoC ? "StoC" : "CToS";
            char buf[16];
            snprintf(buf, sizeof(buf), "%04X", entry.header);
            return "<Packet direction=" + std::string(direction) +
                " header=0x" + std::string(buf) +
                " size=" + std::to_string(entry.size) + ">";
        });

    py::class_<PacketSnifferFacade>(m, "PacketSniffer")
        .def_static("instance", &PacketSnifferFacade::Instance, py::return_value_policy::reference)
        .def("initialize", &PacketSnifferFacade::Initialize)
        .def("initialize_stoc", &PacketSnifferFacade::InitializeStoC)
        .def("initialize_ctos", &PacketSnifferFacade::InitializeCToS)
        .def("terminate", &PacketSnifferFacade::Terminate)
        .def("terminate_stoc", &PacketSnifferFacade::TerminateStoC)
        .def("terminate_ctos", &PacketSnifferFacade::TerminateCToS)
        .def("get_logs", &PacketSnifferFacade::GetLogs)
        .def("get_stoc_logs", &PacketSnifferFacade::GetStoCLogs)
        .def("get_ctos_logs", &PacketSnifferFacade::GetCToSLogs)
        .def("clear_logs", &PacketSnifferFacade::ClearLogs)
        .def("clear_stoc_logs", &PacketSnifferFacade::ClearStoCLogs)
        .def("clear_ctos_logs", &PacketSnifferFacade::ClearCToSLogs);
}

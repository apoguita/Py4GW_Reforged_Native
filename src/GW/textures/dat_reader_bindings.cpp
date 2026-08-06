#include "base/py_bindings.h"

#include "GW/textures/gw_dat_reader.h"

#include <vector>

namespace py = pybind11;

namespace {

py::object ReadDatFileByHash(const std::wstring& file_hash) {
    std::vector<uint8_t> bytes;
    GW::textures::GWDatReader::Instance().EnsureHooks();
    if (!GW::textures::GWDatReader::ReadDatFile(file_hash.c_str(), &bytes, 1)) {
        return py::none();
    }
    return py::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

py::object ReadDatFileById(uint32_t file_id, uint32_t stream_id) {
    std::vector<uint8_t> bytes;
    GW::textures::GWDatReader::Instance().EnsureHooks();
    if (!GW::textures::GWDatReader::ReadDatFileById(file_id, &bytes, stream_id)) {
        return py::none();
    }
    return py::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyDatReader, m) {
    m.doc() = "Game-thread GW.dat archive access backed by the native GWDatReader.";

    m.def(
        "read_file_by_hash",
        &ReadDatFileByHash,
        py::arg("file_hash"),
        "Read a decompressed GW.dat entry by encoded file hash, or return None on failure.");

    m.def(
        "read_file_by_id",
        &ReadDatFileById,
        py::arg("file_id"),
        py::arg("stream_id") = 1,
        "Read a decompressed GW.dat entry by sequential file ID, or return None on failure.");
}

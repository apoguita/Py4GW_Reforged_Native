#include "base/py_bindings.h"

#include "GW/textures/gw_dat_reader.h"
#include "GW/textures/texture_manager.h"

#include <string>

#include <windows.h>

namespace py = pybind11;

namespace {

std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyTexture, m) {
    m.doc() = "Texture access: load from file (via WIC) or from the GW.dat archive. "
              "Returns an ImGui texture handle (the D3D9 texture pointer as an int, "
              "0 when not ready), usable directly with PyImGui.image().";

    m.def("get_file_texture",
        [](const std::string& path) {
            auto* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(path));
            return reinterpret_cast<uint64_t>(tex);
        },
        py::arg("path"),
        "Load a texture from a file path (PNG/JPG/BMP/... via WIC). Cached.");

    m.def("get_dat_texture",
        [](const std::string& key) {
            auto* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(key));
            return reinterpret_cast<uint64_t>(tex);
        },
        py::arg("key"),
        "Load a texture by cache key. 'gwdat://<file_id>' routes to the GW.dat reader.");

    m.def("get_texture_by_file_id",
        [](uint32_t file_id) {
            auto* tex = GW::textures::GWDatReader::Instance().GetTextureByFileId(file_id);
            return reinterpret_cast<uint64_t>(tex);
        },
        py::arg("file_id"),
        "Load a texture directly from the GW.dat archive by file id. Decodes "
        "asynchronously; returns 0 until the upload completes (call again next frame).");

    m.def("get_colored_model_texture",
        [](uint32_t model_file_id, uint32_t dye_tint, uint32_t dye1, uint32_t dye2, uint32_t dye3, uint32_t dye4) {
            auto* tex = GW::textures::GWDatReader::Instance().GetColoredModelTexture(
                model_file_id,
                static_cast<uint8_t>(dye_tint),
                static_cast<uint8_t>(dye1),
                static_cast<uint8_t>(dye2),
                static_cast<uint8_t>(dye3),
                static_cast<uint8_t>(dye4));
            return reinterpret_cast<uint64_t>(tex);
        },
        py::arg("model_file_id"), py::arg("dye_tint") = 0,
        py::arg("dye1") = 0, py::arg("dye2") = 0, py::arg("dye3") = 0, py::arg("dye4") = 0,
        "Load a dyed/colored model texture from the GW.dat archive (base icon + dye "
        "mask blended with the client dye colors). Async, like get_texture_by_file_id.");

    m.def("cleanup_old_textures",
        [](int timeout_seconds) {
            GW::textures::TextureManager::Instance().CleanupOldTextures(timeout_seconds);
        },
        py::arg("timeout_seconds") = 30,
        "Release textures unused for longer than timeout_seconds.");
}

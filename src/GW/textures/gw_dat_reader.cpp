#include "base/error_handling.h"

#include "GW/textures/gw_dat_reader.h"

#include "base/CrashHandler.h"
#include "GW/textures/arenanet_file_parser.h"
#include "GW/textures/arenanet_texture.h"
#include "GW/textures/gw_dat_unpack.h"
#include "GW/common/constants/constants.h"
#include "GW/context/context.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

#include <windows.h>

namespace GW::textures {

namespace {

struct RGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

union DXT1Color {
    struct {
        unsigned b1 : 5, g1 : 6, r1 : 5;
        unsigned b2 : 5, g2 : 6, r2 : 5;
    };
    struct {
        unsigned short c1, c2;
    };
};

uint8_t Expand5To8(unsigned value) {
    return static_cast<uint8_t>((value << 3) | (value >> 2));
}

uint8_t Expand6To8(unsigned value) {
    return static_cast<uint8_t>((value << 2) | (value >> 4));
}

/* ---------------- Structured-exception-guarded game calls ---------------- */

bool CopyBytesNoFault(const void* src, void* dst, size_t size) {
    if (!src || !dst || !size) {
        return false;
    }
    __try {
        memcpy(dst, src, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool AtexDecompressNoFault(unsigned int* input, unsigned int size, unsigned int image_format, SImageDescriptor descriptor, unsigned int* output) {
    __try {
        AtexDecompress(input, size, image_format, descriptor, output);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

RecObj* OpenFileByFileIdNoFault(uint32_t archive, uint32_t file_id, uint32_t stream_id, uint32_t flags, uint32_t* error_out) {
    __try {
        return g_open_file_by_file_id_func ? g_open_file_by_file_id_func(archive, file_id, stream_id, flags, error_out) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

RecObj* FileHashToRecObjNoFault(const wchar_t* file_hash, int unk1_1, int unk2_0) {
    __try {
        return g_file_hash_to_rec_obj_func ? g_file_hash_to_rec_obj_func(file_hash, unk1_1, unk2_0) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

uint8_t* ReadFileBufferNoFault(RecObj* rec, int* size_out) {
    __try {
        return g_read_file_buffer_func ? g_read_file_buffer_func(rec, size_out) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool FreeFileBufferNoFault(RecObj* rec, uint8_t* bytes) {
    __try {
        if (g_free_file_buffer_func) {
            g_free_file_buffer_func(rec, bytes);
            return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return false;
}

bool CloseRecObjNoFault(RecObj* rec) {
    __try {
        if (g_close_rec_obj_func) {
            g_close_rec_obj_func(rec);
            return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return false;
}

bool ReadDatRecord(RecObj* rec, std::vector<uint8_t>* bytes_out) {
    if (!rec || !bytes_out) {
        return false;
    }

    int size = 0;
    const auto bytes = ReadFileBufferNoFault(rec, &size);
    if (!bytes || size <= 0) {
        CloseRecObjNoFault(rec);
        return false;
    }

    bytes_out->resize(static_cast<size_t>(size));
    if (!CopyBytesNoFault(bytes, bytes_out->data(), static_cast<size_t>(size))) {
        bytes_out->clear();
        FreeFileBufferNoFault(rec, bytes);
        CloseRecObjNoFault(rec);
        return false;
    }

    FreeFileBufferNoFault(rec, bytes);
    CloseRecObjNoFault(rec);
    return !bytes_out->empty();
}

/* ---------------- DXT decoders (software) ---------------- */

std::vector<RGBA> DecodeDXT1(const uint8_t* data, int width, int height) {
    const auto* d = reinterpret_cast<const uint32_t*>(data);
    std::vector<RGBA> image(static_cast<size_t>(width) * height);

    int p = 0;
    for (int y = 0; y < height / 4; ++y) {
        for (int x = 0; x < width / 4; ++x, ++p) {
            const DXT1Color color = *reinterpret_cast<const DXT1Color*>(&d[p * 2]);
            uint32_t block = d[p * 2 + 1];
            RGBA table[4]{};
            table[0] = { Expand5To8(color.r1), Expand6To8(color.g1), Expand5To8(color.b1), 255 };
            table[1] = { Expand5To8(color.r2), Expand6To8(color.g2), Expand5To8(color.b2), 255 };

            if (color.c1 > color.c2) {
                table[2] = {
                    static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                    static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                    static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                    255 };
                table[3] = {
                    static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                    static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                    static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                    255 };
            }
            else {
                table[2] = {
                    static_cast<uint8_t>((table[0].r + table[1].r) / 2),
                    static_cast<uint8_t>((table[0].g + table[1].g) / 2),
                    static_cast<uint8_t>((table[0].b + table[1].b) / 2),
                    255 };
                table[3] = { 0, 0, 0, 0 };
            }

            for (int by = 0; by < 4; ++by) {
                for (int bx = 0; bx < 4; ++bx) {
                    image[x * 4 + bx + (y * 4 + by) * width] = table[block & 3];
                    block >>= 2;
                }
            }
        }
    }
    return image;
}

std::vector<RGBA> DecodeDXT3(const uint8_t* data, int width, int height) {
    std::vector<RGBA> image(static_cast<size_t>(width) * height);

    size_t pos = 0;
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            uint64_t alpha_bits = 0;
            for (int i = 0; i < 8; ++i) {
                alpha_bits |= static_cast<uint64_t>(data[pos + i]) << (i * 8);
            }
            const uint16_t c0 = static_cast<uint16_t>(data[pos + 8] | (static_cast<uint16_t>(data[pos + 9]) << 8));
            const uint16_t c1 = static_cast<uint16_t>(data[pos + 10] | (static_cast<uint16_t>(data[pos + 11]) << 8));
            uint32_t block = static_cast<uint32_t>(data[pos + 12])
                | (static_cast<uint32_t>(data[pos + 13]) << 8)
                | (static_cast<uint32_t>(data[pos + 14]) << 16)
                | (static_cast<uint32_t>(data[pos + 15]) << 24);

            RGBA table[4]{};
            table[0] = { Expand5To8((c0 >> 11) & 0x1f), Expand6To8((c0 >> 5) & 0x3f), Expand5To8(c0 & 0x1f), 255 };
            table[1] = { Expand5To8((c1 >> 11) & 0x1f), Expand6To8((c1 >> 5) & 0x3f), Expand5To8(c1 & 0x1f), 255 };
            table[2] = {
                static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                255 };
            table[3] = {
                static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                255 };

            for (int by = 0; by < 4; ++by) {
                for (int bx = 0; bx < 4; ++bx) {
                    RGBA pixel = table[block & 3];
                    block >>= 2;
                    pixel.a = static_cast<uint8_t>((alpha_bits & 0xF) * 17);
                    alpha_bits >>= 4;
                    if (x + bx < width && y + by < height) {
                        image[x + bx + (y + by) * width] = pixel;
                    }
                }
            }
            pos += 16;
        }
    }
    return image;
}

std::vector<RGBA> DecodeDXT5(const uint8_t* data, int width, int height, bool premultiply_alpha) {
    std::vector<RGBA> image(static_cast<size_t>(width) * height);

    size_t pos = 0;
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            uint8_t alpha_table[8]{};
            alpha_table[0] = data[pos];
            alpha_table[1] = data[pos + 1];
            if (alpha_table[0] > alpha_table[1]) {
                for (int i = 0; i < 6; ++i) {
                    alpha_table[i + 2] = static_cast<uint8_t>(((6 - i) * alpha_table[0] + (i + 1) * alpha_table[1]) / 7);
                }
            }
            else {
                for (int i = 0; i < 4; ++i) {
                    alpha_table[i + 2] = static_cast<uint8_t>(((4 - i) * alpha_table[0] + (i + 1) * alpha_table[1]) / 5);
                }
                alpha_table[6] = 0;
                alpha_table[7] = 255;
            }
            uint64_t alpha_bits = 0;
            for (int i = 0; i < 6; ++i) {
                alpha_bits |= static_cast<uint64_t>(data[pos + 2 + i]) << (i * 8);
            }
            const uint16_t c0 = static_cast<uint16_t>(data[pos + 8] | (static_cast<uint16_t>(data[pos + 9]) << 8));
            const uint16_t c1 = static_cast<uint16_t>(data[pos + 10] | (static_cast<uint16_t>(data[pos + 11]) << 8));
            uint32_t block = static_cast<uint32_t>(data[pos + 12])
                | (static_cast<uint32_t>(data[pos + 13]) << 8)
                | (static_cast<uint32_t>(data[pos + 14]) << 16)
                | (static_cast<uint32_t>(data[pos + 15]) << 24);

            RGBA table[4]{};
            table[0] = { Expand5To8((c0 >> 11) & 0x1f), Expand6To8((c0 >> 5) & 0x3f), Expand5To8(c0 & 0x1f), 255 };
            table[1] = { Expand5To8((c1 >> 11) & 0x1f), Expand6To8((c1 >> 5) & 0x3f), Expand5To8(c1 & 0x1f), 255 };
            table[2] = {
                static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                255 };
            table[3] = {
                static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                255 };

            for (int by = 0; by < 4; ++by) {
                for (int bx = 0; bx < 4; ++bx) {
                    RGBA pixel = table[block & 3];
                    block >>= 2;
                    pixel.a = alpha_table[alpha_bits & 7];
                    alpha_bits >>= 3;
                    if (premultiply_alpha) {
                        pixel.r = static_cast<uint8_t>((pixel.r * pixel.a) / 255);
                        pixel.g = static_cast<uint8_t>((pixel.g * pixel.a) / 255);
                        pixel.b = static_cast<uint8_t>((pixel.b * pixel.a) / 255);
                    }
                    if (x + bx < width && y + by < height) {
                        image[x + bx + (y + by) * width] = pixel;
                    }
                }
            }
            pos += 16;
        }
    }
    return image;
}

/* ---------------- Decode pipeline ---------------- */

struct DecodedTexture {
    Vec2i dims;
    int levels = 1;
    GR_FORMAT format = GR_FORMAT_UNK;
    std::vector<uint8_t> pixels;  // BGRA
    std::vector<uint8_t> dxt;
};

bool DecodeAtexToBgra(uint8_t* image_bytes, size_t image_size, std::vector<uint8_t>* dst_pixels, Vec2i& dims, int& levels, GR_FORMAT& format, std::vector<uint8_t>* dst_dxt = nullptr) {
    if (!image_bytes || image_size < 12 || !dst_pixels) {
        return false;
    }

    const uint32_t id1 = reinterpret_cast<uint32_t*>(image_bytes)[0];
    const uint32_t id2 = reinterpret_cast<uint32_t*>(image_bytes)[1];
    if (id1 != 'XTTA' && id1 != 'XETA') {
        return false;
    }
    if ((id2 & 0xffffff) != 'TXD') {
        return false;
    }

    const int compression_type = id2 >> 24;
    dims.x = *reinterpret_cast<uint16_t*>(image_bytes + 8);
    dims.y = *reinterpret_cast<uint16_t*>(image_bytes + 10);
    levels = 1;

    if (dims.x <= 0 || dims.y <= 0 || (dims.x % 4) != 0 || (dims.y % 4) != 0) {
        return false;
    }

    uint32_t atex_format = 0;
    bool premultiply_alpha = false;
    switch (compression_type) {
    case '1':
        format = GR_FORMAT_DXT1;
        atex_format = 0x0f;
        break;
    case '2':
    case '3':
    case 'N':
        format = compression_type == 'N' ? GR_FORMAT_DXTN : GR_FORMAT_DXT3;
        atex_format = 0x11;
        break;
    case '4':
    case '5':
    case 'A':
        format = compression_type == 'A' ? GR_FORMAT_DXTA : GR_FORMAT_DXT5;
        atex_format = 0x13;
        break;
    case 'L':
        format = GR_FORMAT_DXTL;
        atex_format = 0x12;
        premultiply_alpha = true;
        break;
    default:
        return false;
    }

    std::vector<uint32_t> dxt_intermediate(static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y), 0);
    SImageDescriptor descriptor{};
    descriptor.xres = dims.x;
    descriptor.yres = dims.y;
    descriptor.Data = image_bytes;
    descriptor.a = static_cast<int>(image_size);
    descriptor.b = 6;
    descriptor.image = reinterpret_cast<unsigned char*>(dxt_intermediate.data());
    descriptor.imageformat = 0x0f;
    descriptor.c = 0;

    if (!AtexDecompressNoFault(
            reinterpret_cast<unsigned int*>(image_bytes),
            static_cast<unsigned int>(image_size),
            atex_format,
            descriptor,
            dxt_intermediate.data())) {
        return false;
    }

    if (dst_dxt) {
        const size_t blocks_x = static_cast<size_t>((dims.x + 3) / 4);
        const size_t blocks_y = static_cast<size_t>((dims.y + 3) / 4);
        const size_t bytes_per_block = format == GR_FORMAT_DXT1 ? 8u : 16u;
        const size_t dxt_size = blocks_x * blocks_y * bytes_per_block;
        const auto* dxt_bytes = reinterpret_cast<const uint8_t*>(dxt_intermediate.data());
        dst_dxt->assign(dxt_bytes, dxt_bytes + dxt_size);
    }

    std::vector<RGBA> rgba;
    switch (format) {
    case GR_FORMAT_DXT1:
        rgba = DecodeDXT1(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y);
        break;
    case GR_FORMAT_DXT2:
    case GR_FORMAT_DXT3:
    case GR_FORMAT_DXTN:
        rgba = DecodeDXT3(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y);
        break;
    case GR_FORMAT_DXT4:
    case GR_FORMAT_DXT5:
    case GR_FORMAT_DXTA:
        rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y, false);
        break;
    case GR_FORMAT_DXTL:
        rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y, premultiply_alpha);
        break;
    default:
        return false;
    }

    if (rgba.size() != static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y)) {
        return false;
    }

    dst_pixels->resize(rgba.size() * 4);
    for (size_t i = 0; i < rgba.size(); ++i) {
        (*dst_pixels)[i * 4 + 0] = rgba[i].b;
        (*dst_pixels)[i * 4 + 1] = rgba[i].g;
        (*dst_pixels)[i * 4 + 2] = rgba[i].r;
        (*dst_pixels)[i * 4 + 3] = rgba[i].a;
    }
    return true;
}

uint32_t OpenImageFromAsset(ArenaNetFileParser::GameAssetFile& asset, std::vector<uint8_t>* dst_pixels, Vec2i& dims, int& levels, GR_FORMAT& format, std::vector<uint8_t>* dst_dxt = nullptr) {
    uint8_t* image_bytes = asset.data.data();
    size_t image_size = asset.data.size();
    if (image_size >= 4 && strncmp(reinterpret_cast<char*>(image_bytes), "ffna", 4) == 0) {
        const auto anet_file = reinterpret_cast<ArenaNetFileParser::ArenaNetFile*>(&asset);
        if (!anet_file->isValid()) {
            return 0;
        }
        const auto chunk = reinterpret_cast<const ArenaNetFileParser::UnknownChunk*>(
            anet_file->FindChunk(ArenaNetFileParser::ChunkType::FA3_InlineTextureDXT3));
        if (!chunk) {
            return 0;
        }
        image_bytes = const_cast<uint8_t*>(chunk->data);
        image_size = chunk->chunk_size;
    }

    if (image_size < 4
        || (strncmp(reinterpret_cast<char*>(image_bytes), "ATEX", 4) != 0
            && strncmp(reinterpret_cast<char*>(image_bytes), "DDS", 3) != 0)) {
        return 0;
    }
    if (strncmp(reinterpret_cast<char*>(image_bytes), "DDS", 3) == 0) {
        return 0;  // DDS payloads unsupported by the custom decoder
    }

    return DecodeAtexToBgra(image_bytes, image_size, dst_pixels, dims, levels, format, dst_dxt) ? 1 : 0;
}

std::shared_ptr<DecodedTexture> DecodeTexture(uint32_t file_id) {
    if (!file_id) {
        return nullptr;
    }
    auto decoded = std::make_shared<DecodedTexture>();
    ArenaNetFileParser::GameAssetFile asset;
    if (!asset.readFromDat(file_id)) {
        return nullptr;
    }
    int levels = 1;
    GR_FORMAT format = GR_FORMAT_UNK;
    if (!OpenImageFromAsset(asset, &decoded->pixels, decoded->dims, levels, format, &decoded->dxt)
        || decoded->pixels.empty() || !decoded->dims.x || !decoded->dims.y) {
        return nullptr;
    }
    decoded->levels = levels;
    decoded->format = format;
    return decoded;
}

IDirect3DTexture9* CreateTexture(IDirect3DDevice9* device, const DecodedTexture& decoded) {
    if (!device || decoded.pixels.empty() || !decoded.dims.x || !decoded.dims.y) {
        return nullptr;
    }

    IDirect3DTexture9* tex = nullptr;
    if (device->CreateTexture(decoded.dims.x, decoded.dims.y, decoded.levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK) {
        return nullptr;
    }

    D3DLOCKED_RECT rect;
    if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK) {
        tex->Release();
        return nullptr;
    }

    const uint8_t* srcdata = decoded.pixels.data();
    for (int y = 0; y < decoded.dims.y; y++) {
        uint8_t* dest_addr = reinterpret_cast<uint8_t*>(rect.pBits) + static_cast<size_t>(y) * rect.Pitch;
        memcpy(dest_addr, srcdata, static_cast<size_t>(decoded.dims.x) * 4);
        srcdata += static_cast<size_t>(decoded.dims.x) * 4;
    }

    tex->UnlockRect(0);
    return tex;
}

/* ---------------- Cache + job queues ---------------- */

struct GwImg {
    uint32_t m_file_id = 0;
    Vec2i m_dims;
    IDirect3DTexture9* m_tex = nullptr;
    std::chrono::steady_clock::time_point m_last_used;
    bool m_evicted = false;

    explicit GwImg(uint32_t file_id)
        : m_file_id(file_id)
        , m_last_used(std::chrono::steady_clock::now()) {
    }

    void Touch() {
        m_last_used = std::chrono::steady_clock::now();
    }
};

std::map<uint32_t, std::shared_ptr<GwImg>> g_textures_by_file_id;
std::recursive_mutex g_textures_mutex;
std::recursive_mutex g_cpu_jobs_mutex;
std::queue<std::function<void()>> g_cpu_jobs;
std::recursive_mutex g_dx_jobs_mutex;
std::queue<std::function<void(IDirect3DDevice9*)>> g_dx_jobs;

void EnqueueCpuTask(const std::function<void()>& task) {
    std::lock_guard<std::recursive_mutex> lock(g_cpu_jobs_mutex);
    g_cpu_jobs.push(task);
}

void EnqueueDxTask(const std::function<void(IDirect3DDevice9*)>& task) {
    std::lock_guard<std::recursive_mutex> lock(g_dx_jobs_mutex);
    g_dx_jobs.push(task);
}

/* ---------------- Colored / dyed model textures ---------------- */

struct DatHeader {
    uint32_t magic = 0;
    uint32_t header_size = 0;
    uint32_t sector_size = 0;
    uint32_t checksum = 0;
    uint64_t mft_offset = 0;
    uint32_t mft_size = 0;
    uint32_t reserved = 0;
};

struct MftCandidate {
    uint32_t index = 0;
    uint64_t data_offset = 0;
    uint32_t size = 0;
    uint16_t compression = 0;
    uint8_t content = 0;
    uint8_t content_type = 0;
    uint32_t unknown = 0;
};

uint32_t ReadLe32(const uint8_t* bytes, size_t size, size_t offset) {
    if (!bytes || offset + 4 > size) {
        return 0;
    }
    return static_cast<uint32_t>(bytes[offset])
        | (static_cast<uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint32_t ReadLe32(const std::vector<uint8_t>& bytes, size_t offset) {
    return ReadLe32(bytes.data(), bytes.size(), offset);
}

std::wstring GetGwDatPath() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (!len || len >= ARRAYSIZE(path)) {
        return L"";
    }
    std::wstring dat_path(path, len);
    const size_t slash = dat_path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L"Gw.dat";
    }
    dat_path.resize(slash + 1);
    dat_path += L"Gw.dat";
    return dat_path;
}

template <typename T>
bool ReadAt(std::ifstream& in, uint64_t offset, T& value) {
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        return false;
    }
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

std::vector<uint8_t> ReadBlob(std::ifstream& in, uint64_t offset, uint32_t size) {
    std::vector<uint8_t> out(size);
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        return {};
    }
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!in) {
        return {};
    }
    return out;
}

bool ReadMftEntry(std::ifstream& in, uint64_t file_size, const DatHeader& header, uint32_t index, MftCandidate& candidate) {
    if (header.mft_size < 24) {
        return false;
    }
    const uint64_t entry_offset = header.mft_offset + static_cast<uint64_t>(index) * 24ull;
    if (entry_offset + 24ull > header.mft_offset + header.mft_size) {
        return false;
    }

    uint8_t entry[24] = {};
    in.seekg(static_cast<std::streamoff>(entry_offset), std::ios::beg);
    in.read(reinterpret_cast<char*>(entry), sizeof(entry));
    if (!in) {
        return false;
    }

    const uint64_t data_offset = static_cast<uint64_t>(ReadLe32(entry, sizeof(entry), 0))
        | (static_cast<uint64_t>(ReadLe32(entry, sizeof(entry), 4)) << 32);
    const uint32_t size = ReadLe32(entry, sizeof(entry), 8);
    if (size && (data_offset < header.header_size || data_offset + size > file_size)) {
        return false;
    }

    candidate = {};
    candidate.index = index;
    candidate.data_offset = data_offset;
    candidate.size = size;
    candidate.compression = static_cast<uint16_t>(entry[12] | (static_cast<uint16_t>(entry[13]) << 8));
    candidate.content = entry[14];
    candidate.content_type = entry[15];
    candidate.unknown = ReadLe32(entry, sizeof(entry), 16);
    return true;
}

std::vector<uint8_t> DecompressDatBlob(const MftCandidate& candidate, const std::vector<uint8_t>& raw) {
    if (candidate.compression == 0) {
        return raw;
    }
    if (candidate.compression != 8 || raw.empty()) {
        return {};
    }

    unsigned char* output = nullptr;
    int output_size = 0;
    UnpackGWDat(
        const_cast<unsigned char*>(raw.data()),
        static_cast<int>(raw.size()),
        output,
        output_size);

    if (!output || output_size < 0) {
        delete[] output;
        return {};
    }

    std::vector<uint8_t> decoded(output, output + output_size);
    delete[] output;
    return decoded;
}

std::vector<uint8_t> ReadDecodedMftBytes(std::ifstream& in, const MftCandidate& candidate) {
    auto bytes = ReadBlob(in, candidate.data_offset, candidate.size);
    return DecompressDatBlob(candidate, bytes);
}

std::vector<uint32_t> ResolveFileIdToMftIndices(std::ifstream& in, uint64_t file_size, const DatHeader& header, uint32_t file_id) {
    MftCandidate hash_entry;
    if (!ReadMftEntry(in, file_size, header, 2, hash_entry)
        || hash_entry.compression != 0
        || hash_entry.size < 8
        || hash_entry.data_offset + hash_entry.size > file_size) {
        return {};
    }

    const auto hash_table = ReadBlob(in, hash_entry.data_offset, hash_entry.size);
    std::vector<uint32_t> indices;
    for (size_t pos = 0; pos + 8 <= hash_table.size(); pos += 8) {
        const uint32_t file_number = ReadLe32(hash_table, pos);
        const uint32_t mft_index = ReadLe32(hash_table, pos + 4);
        if (file_number == file_id) {
            indices.push_back(mft_index);
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

void FileIdToTypedFileHash(uint32_t file_id, uint8_t subtype, wchar_t* file_hash) {
    ArenaNetFileParser::FileIdToFileHash(file_id, file_hash);
    if (subtype) {
        file_hash[2] = static_cast<wchar_t>(subtype + 0x100);
        file_hash[3] = 0;
    }
}

std::shared_ptr<DecodedTexture> DecodeTypedTexture(uint32_t file_id, uint8_t subtype) {
    if (!file_id || !subtype) {
        return nullptr;
    }
    wchar_t file_hash[4] = {};
    FileIdToTypedFileHash(file_id, subtype, file_hash);

    ArenaNetFileParser::GameAssetFile asset;
    if (!asset.readFromDat(file_hash)) {
        return nullptr;
    }
    auto decoded = std::make_shared<DecodedTexture>();
    int levels = 1;
    GR_FORMAT format = GR_FORMAT_UNK;
    if (!OpenImageFromAsset(asset, &decoded->pixels, decoded->dims, levels, format, &decoded->dxt)
        || decoded->pixels.empty() || !decoded->dims.x || !decoded->dims.y) {
        return nullptr;
    }
    decoded->levels = levels;
    decoded->format = format;
    return decoded;
}

bool DecodeTextureFromBytes(std::vector<uint8_t> bytes, DecodedTexture& decoded) {
    if (bytes.empty()) {
        return false;
    }
    ArenaNetFileParser::GameAssetFile asset;
    if (!asset.parse(bytes)) {
        return false;
    }
    int levels = 0;
    GR_FORMAT format = GR_FORMAT_UNK;
    std::vector<uint8_t> pixels;
    Vec2i dims;
    std::vector<uint8_t> dxt;
    if (!OpenImageFromAsset(asset, &pixels, dims, levels, format, &dxt)
        || pixels.empty() || dims.x <= 0 || dims.y <= 0) {
        return false;
    }
    decoded.dims = dims;
    decoded.levels = levels;
    decoded.format = format;
    decoded.pixels = std::move(pixels);
    decoded.dxt = std::move(dxt);
    return true;
}

bool ResolveLinkedIconTexturesFromDat(uint32_t model_file_id, std::shared_ptr<DecodedTexture>& base, std::shared_ptr<DecodedTexture>& mask) {
    const std::wstring dat_path = GetGwDatPath();
    if (dat_path.empty()) {
        return false;
    }

    std::ifstream in(dat_path, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    const uint64_t file_size = static_cast<uint64_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    DatHeader header;
    if (!ReadAt(in, 0, header)
        || header.magic != 0x1A4E4133
        || header.mft_offset >= file_size
        || header.mft_size == 0) {
        return false;
    }

    const auto indices = ResolveFileIdToMftIndices(in, file_size, header, model_file_id);
    if (indices.empty()) {
        return false;
    }

    MftCandidate current;
    if (!ReadMftEntry(in, file_size, header, indices.front(), current)) {
        return false;
    }

    std::vector<uint32_t> seen;
    for (int depth = 0; depth < 16; ++depth) {
        const uint32_t next_index = current.unknown;
        if (!next_index || std::find(seen.begin(), seen.end(), next_index) != seen.end()) {
            break;
        }
        seen.push_back(next_index);

        MftCandidate next;
        if (!ReadMftEntry(in, file_size, header, next_index, next)) {
            break;
        }

        auto bytes = ReadDecodedMftBytes(in, next);
        DecodedTexture decoded;
        const bool is_texture = DecodeTextureFromBytes(std::move(bytes), decoded);

        if (is_texture && next.content == 1 && next.content_type == 12 && decoded.dims.x == 64 && decoded.dims.y == 64) {
            mask = std::make_shared<DecodedTexture>(std::move(decoded));
        }
        else if (is_texture && next.content == 1 && next.content_type == 1 && decoded.dims.x == 64 && decoded.dims.y == 64) {
            base = std::make_shared<DecodedTexture>(std::move(decoded));
            break;
        }
        current = next;
    }
    return base != nullptr;
}

struct ColoredModelTextureKey {
    uint32_t revision = 0;
    uint32_t model_file_id = 0;
    uint8_t dye_tint = 0;
    uint8_t dye1 = 0;
    uint8_t dye2 = 0;
    uint8_t dye3 = 0;
    uint8_t dye4 = 0;

    bool operator<(const ColoredModelTextureKey& other) const {
        if (revision != other.revision) return revision < other.revision;
        if (model_file_id != other.model_file_id) return model_file_id < other.model_file_id;
        if (dye_tint != other.dye_tint) return dye_tint < other.dye_tint;
        if (dye1 != other.dye1) return dye1 < other.dye1;
        if (dye2 != other.dye2) return dye2 < other.dye2;
        if (dye3 != other.dye3) return dye3 < other.dye3;
        return dye4 < other.dye4;
    }
};

std::map<ColoredModelTextureKey, std::shared_ptr<GwImg>> g_textures_by_colored_model;

uint8_t ClampByte(double value) {
    if (value <= 0.0) return 0;
    if (value >= 255.0) return 255;
    return static_cast<uint8_t>(std::lround(value));
}

const uint8_t* FindLoadedDyeTableBase() {
    static const uint8_t* cached = nullptr;
    if (cached) {
        return cached;
    }

    const auto module = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    if (!module) {
        return nullptr;
    }

    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage == 0) {
        return nullptr;
    }

    static constexpr uint8_t tint7_prefix[] = {
        0x80, 0x40, 0x00, 0x40, 0x80,
        0x80, 0x40, 0x00, 0x40, 0x80,
        0x80, 0x4a, 0x88, 0x1b, 0x2f,
    };
    const size_t image_size = nt->OptionalHeader.SizeOfImage;
    for (size_t i = 0; i + sizeof(tint7_prefix) <= image_size; ++i) {
        if (std::memcmp(module + i, tint7_prefix, sizeof(tint7_prefix)) == 0) {
            cached = module + i - 7u * 14u * 5u;
            return cached;
        }
    }
    return nullptr;
}

bool ReadClientDyeRecord(uint8_t dye_color, uint8_t dye_tint, uint8_t record[5]) {
    if (dye_color > 0x0d) {
        return false;
    }
    const uint32_t tint = dye_tint <= 0x31 ? dye_tint : 1;
    const uint8_t* table = FindLoadedDyeTableBase();
    if (!table) {
        return false;
    }
    std::memcpy(record, table + (static_cast<uint32_t>(dye_color) + tint * 14u) * 5u, 5);
    return true;
}

void HslToRgbClient(double h, double s, double l, double& r, double& g, double& b) {
    auto hue_to_rgb = [](double p, double q, double t) {
        if (t < 0.0) t += 1.0;
        if (t > 1.0) t -= 1.0;
        if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
        if (t < 0.5) return q;
        if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        return p;
    };
    if (s <= 0.0) {
        r = g = b = l;
        return;
    }
    const double q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
    const double p = 2.0 * l - q;
    r = hue_to_rgb(p, q, h + 1.0 / 3.0);
    g = hue_to_rgb(p, q, h);
    b = hue_to_rgb(p, q, h - 1.0 / 3.0);
}

void RgbToHslClient(double r, double g, double b, double& h, double& s, double& l) {
    const double min_v = std::min({ r, g, b });
    const double max_v = std::max({ r, g, b });
    const double diff = max_v - min_v;
    l = 0.5 * (min_v + max_v);
    if (diff == 0.0) {
        h = 0.0;
        s = 0.0;
        return;
    }

    const double denom = (l >= 0.5) ? (2.0 - max_v - min_v) : (max_v + min_v);
    s = (denom == 0.0) ? 0.0 : diff / denom;
    if (r == max_v) {
        h = (((max_v - b) / 6.0 + diff * 0.5) / diff)
            - (((max_v - g) / 6.0 + diff * 0.5) / diff);
    }
    else if (g == max_v) {
        h = ((((max_v - r) / 6.0 + diff * 0.5) / diff) + 0.3333329856)
            - (((max_v - b) / 6.0 + diff * 0.5) / diff);
    }
    else {
        h = ((((max_v - g) / 6.0 + diff * 0.5) / diff) + 0.6666659713)
            - (((max_v - r) / 6.0 + diff * 0.5) / diff);
    }
    while (h < 0.0) h += 1.0;
    while (h > 1.0) h -= 1.0;
}

void ClientDyeRecordToRgb(const uint8_t record[5], uint8_t rgb[3]) {
    const double l = static_cast<double>(record[4]) / 510.0 + 0.25;
    const double s = std::min(static_cast<double>(record[3]) * (1.0 / 64.0), 1.0);
    const double h = static_cast<double>(record[2]) / 255.0;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    HslToRgbClient(h, s, l, r, g, b);
    rgb[0] = ClampByte(r * 255.0);
    rgb[1] = ClampByte(g * 255.0);
    rgb[2] = ClampByte(b * 255.0);
}

bool BuildClientDyeRecord(uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4, uint8_t out_record[5]) {
    const uint8_t dyes[4] = { dye1, dye2, dye3, dye4 };
    std::vector<std::array<uint8_t, 5>> records;
    for (uint8_t dye_color : dyes) {
        if (dye_color != 0 && dye_color < 14) {
            std::array<uint8_t, 5> record{};
            if (!ReadClientDyeRecord(dye_color, dye_tint, record.data())) {
                return false;
            }
            records.push_back(record);
        }
    }
    if (records.empty()) {
        return false;
    }
    if (records.size() == 1) {
        std::memcpy(out_record, records.front().data(), 5);
        return true;
    }

    uint32_t sum0 = 0;
    uint32_t sum1 = 0;
    uint32_t complement_sum[3] = {};
    for (const auto& record : records) {
        sum0 += record[0];
        sum1 += record[1];
        uint8_t rgb[3] = {};
        ClientDyeRecordToRgb(record.data(), rgb);
        for (size_t c = 0; c < 3; ++c) {
            complement_sum[c] += 255u - rgb[c];
        }
    }

    const uint32_t count = static_cast<uint32_t>(records.size());
    double mixed[3] = {};
    for (size_t c = 0; c < 3; ++c) {
        mixed[c] = static_cast<double>(255u - (complement_sum[c] / count)) / 255.0;
    }

    double h = 0.0;
    double s = 0.0;
    double l = 0.0;
    RgbToHslClient(mixed[0], mixed[1], mixed[2], h, s, l);

    out_record[0] = static_cast<uint8_t>(sum0 / count);
    out_record[1] = ClampByte(std::lround(9.600000381469727) + static_cast<double>(sum1 / count));
    out_record[2] = ClampByte(h * 255.0);
    out_record[3] = ClampByte((s + 0.10000000149011612) * 255.0 * 0.25);
    out_record[4] = ClampByte(l * 255.0);
    return true;
}

int32_t ClientFixed16_16(double value) {
    const double scaled = static_cast<double>(static_cast<float>(value)) * 65536.0;
    if (scaled >= static_cast<double>(std::numeric_limits<int32_t>::max())) {
        return std::numeric_limits<int32_t>::max();
    }
    if (scaled <= static_cast<double>(std::numeric_limits<int32_t>::min())) {
        return std::numeric_limits<int32_t>::min();
    }
    return static_cast<int32_t>(scaled);
}

void MultiplyClientMatrix(const double lhs[12], double rhs[12]) {
    double out[12] = {};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out[r * 4 + c] =
                lhs[r * 4 + 0] * rhs[0 * 4 + c] +
                lhs[r * 4 + 1] * rhs[1 * 4 + c] +
                lhs[r * 4 + 2] * rhs[2 * 4 + c];
        }
        out[r * 4 + 3] =
            lhs[r * 4 + 0] * rhs[3] +
            lhs[r * 4 + 1] * rhs[7] +
            lhs[r * 4 + 2] * rhs[11] +
            lhs[r * 4 + 3];
    }
    std::memcpy(rhs, out, sizeof(out));
}

bool BuildClientFixedDyeMatrix(uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4, int32_t fixed_matrix[12]) {
    uint8_t record[5] = {};
    if (!BuildClientDyeRecord(dye_tint, dye1, dye2, dye3, dye4, record)) {
        return false;
    }

    const double brightness = (static_cast<double>(record[0]) - 128.0) / 255.0;
    const double contrast = static_cast<double>(record[1] << 2) * (1.0 / 256.0);
    const double hue = static_cast<double>(record[2] * 2u) * 3.1415927410125732 * (1.0 / 256.0);
    const double saturation = static_cast<double>(record[3] << 2) * (1.0 / 256.0);
    const double value = static_cast<double>(record[4] * 2u) * (1.0 / 256.0);

    double matrix[12] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
    };

    if (brightness != 0.0 || contrast != 1.0) {
        const double offset = ((brightness + brightness + 1.0) - contrast) * 128.0;
        matrix[0] = contrast;
        matrix[5] = contrast;
        matrix[10] = contrast;
        matrix[3] = offset;
        matrix[7] = offset;
        matrix[11] = offset;
    }

    if (hue != 0.0 || saturation != 1.0 || value != 1.0) {
        double basis[12] = {
            0.7071070075,  0.0,           -0.7071070075, 0.0,
           -0.4082479775,  0.8164970279, -0.4082479775, 0.0,
            0.5773500204,  0.5773500204,  0.5773500204, 0.0,
        };

        const double c = std::cos(hue) * saturation;
        const double s = std::sin(hue) * saturation;
        double rotate[12] = {
            c,   -s,  0.0,   0.0,
            s,    c,  0.0,   0.0,
            0.0,  0.0, value, 0.0,
        };

        double inverse[12] = {
            0.7071070075,  -0.4082479775, 0.5773500204, 0.0,
            0.0,            0.8164970279, 0.5773500204, 0.0,
           -0.7071070075,  -0.4082479775, 0.5773500204, 0.0,
        };

        MultiplyClientMatrix(rotate, basis);
        MultiplyClientMatrix(inverse, basis);
        MultiplyClientMatrix(basis, matrix);
    }

    for (size_t i = 0; i < 12; ++i) {
        fixed_matrix[i] = ClientFixed16_16(matrix[i]);
    }
    return true;
}

uint8_t ClientAlphaTable(uint32_t alpha_nibble, uint32_t value) {
    return static_cast<uint8_t>((alpha_nibble * value + 8u) / 15u);
}

uint8_t ClientClampQ16ToU8(int64_t value) {
    if (value <= 0) {
        return 0;
    }
    if (value > 0x00ffffffll) {
        return 255;
    }
    return static_cast<uint8_t>((static_cast<uint64_t>(value) >> 16) & 0xffu);
}

uint16_t ClientQuantizeRgb565(uint8_t r, uint8_t g, uint8_t b) {
    const uint16_t r5 = static_cast<uint16_t>((static_cast<uint32_t>(r) * 31u + 127u) / 255u);
    const uint16_t g6 = static_cast<uint16_t>((static_cast<uint32_t>(g) * 63u + 127u) / 255u);
    const uint16_t b5 = static_cast<uint16_t>((static_cast<uint32_t>(b) * 31u + 127u) / 255u);
    return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

uint16_t ClientTransformRgb565(uint16_t color, const int32_t fixed_matrix[12]) {
    const int r = Expand5To8((color >> 11) & 0x1f);
    const int g = Expand6To8((color >> 5) & 0x3f);
    const int b = Expand5To8(color & 0x1f);
    const int64_t v0 = r;
    const int64_t v1 = g;
    const int64_t v2 = b;
    const uint8_t out_r = ClientClampQ16ToU8(
        static_cast<int64_t>(fixed_matrix[0]) * v0
        + static_cast<int64_t>(fixed_matrix[1]) * v1
        + static_cast<int64_t>(fixed_matrix[2]) * v2
        + fixed_matrix[3]);
    const uint8_t out_g = ClientClampQ16ToU8(
        static_cast<int64_t>(fixed_matrix[4]) * v0
        + static_cast<int64_t>(fixed_matrix[5]) * v1
        + static_cast<int64_t>(fixed_matrix[6]) * v2
        + fixed_matrix[7]);
    const uint8_t out_b = ClientClampQ16ToU8(
        static_cast<int64_t>(fixed_matrix[8]) * v0
        + static_cast<int64_t>(fixed_matrix[9]) * v1
        + static_cast<int64_t>(fixed_matrix[10]) * v2
        + fixed_matrix[11]);
    return ClientQuantizeRgb565(out_r, out_g, out_b);
}

std::vector<uint8_t> ApplyClientDxt3EndpointColorOp(const std::vector<uint8_t>& dxt, int width, int height, const int32_t fixed_matrix[12]) {
    std::vector<uint8_t> out = dxt;
    if (width <= 0 || height <= 0) {
        return out;
    }

    const size_t block_count = static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4);
    if (out.size() < block_count * 16u) {
        return out;
    }

    for (size_t block = 0; block < block_count; ++block) {
        uint8_t* ptr = out.data() + block * 16u;
        const uint16_t old_c0 = static_cast<uint16_t>(ptr[8] | (static_cast<uint16_t>(ptr[9]) << 8));
        const uint16_t old_c1 = static_cast<uint16_t>(ptr[10] | (static_cast<uint16_t>(ptr[11]) << 8));
        uint16_t new_c0 = ClientTransformRgb565(old_c0, fixed_matrix);
        uint16_t new_c1 = ClientTransformRgb565(old_c1, fixed_matrix);
        const uint16_t first_transformed = new_c0;
        const uint16_t second_transformed = new_c1;
        if (new_c0 < new_c1) {
            std::swap(new_c0, new_c1);
        }

        ptr[8] = static_cast<uint8_t>(new_c0 & 0xffu);
        ptr[9] = static_cast<uint8_t>(new_c0 >> 8);
        ptr[10] = static_cast<uint8_t>(new_c1 & 0xffu);
        ptr[11] = static_cast<uint8_t>(new_c1 >> 8);

        if (new_c0 != new_c1 && first_transformed < second_transformed) {
            uint32_t selectors = static_cast<uint32_t>(ptr[12])
                | (static_cast<uint32_t>(ptr[13]) << 8)
                | (static_cast<uint32_t>(ptr[14]) << 16)
                | (static_cast<uint32_t>(ptr[15]) << 24);
            selectors ^= 0x55555555u;
            ptr[12] = static_cast<uint8_t>(selectors & 0xffu);
            ptr[13] = static_cast<uint8_t>((selectors >> 8) & 0xffu);
            ptr[14] = static_cast<uint8_t>((selectors >> 16) & 0xffu);
            ptr[15] = static_cast<uint8_t>((selectors >> 24) & 0xffu);
        }
    }
    return out;
}

std::vector<uint8_t> DecodeDxt3ToBgra(const std::vector<uint8_t>& dxt, int width, int height) {
    const auto rgba = DecodeDXT3(dxt.data(), width, height);
    std::vector<uint8_t> bgra(rgba.size() * 4);
    for (size_t i = 0; i < rgba.size(); ++i) {
        bgra[i * 4 + 0] = rgba[i].b;
        bgra[i * 4 + 1] = rgba[i].g;
        bgra[i * 4 + 2] = rgba[i].r;
        bgra[i * 4 + 3] = rgba[i].a;
    }
    return bgra;
}

bool ApplyClientDyeModel(DecodedTexture& base, const DecodedTexture& mask, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4) {
    if (base.pixels.empty()
        || mask.pixels.empty()
        || base.dims.x <= 0
        || base.dims.y <= 0
        || base.pixels.size() % 4 != 0) {
        return false;
    }

    int32_t fixed_matrix[12] = {};
    if (!BuildClientFixedDyeMatrix(dye_tint, dye1, dye2, dye3, dye4, fixed_matrix)) {
        return false;
    }

    std::vector<uint8_t> endpoint_dyed_pixels;
    if (!base.dxt.empty()
        && (base.format == GR_FORMAT_DXT2 || base.format == GR_FORMAT_DXT3 || base.format == GR_FORMAT_DXTN)) {
        const auto endpoint_dxt = ApplyClientDxt3EndpointColorOp(base.dxt, base.dims.x, base.dims.y, fixed_matrix);
        endpoint_dyed_pixels = DecodeDxt3ToBgra(endpoint_dxt, base.dims.x, base.dims.y);
        if (endpoint_dyed_pixels.size() != base.pixels.size()) {
            endpoint_dyed_pixels.clear();
        }
    }

    for (int y = 0; y < base.dims.y; ++y) {
        for (int x = 0; x < base.dims.x; ++x) {
            const int mx = std::min(mask.dims.x - 1, std::max(0, x * mask.dims.x / std::max(1, base.dims.x)));
            const int my = std::min(mask.dims.y - 1, std::max(0, y * mask.dims.y / std::max(1, base.dims.y)));
            const size_t base_offset = (static_cast<size_t>(y) * base.dims.x + x) * 4;
            const size_t mask_offset = (static_cast<size_t>(my) * mask.dims.x + mx) * 4;
            if (base_offset + 3 >= base.pixels.size() || mask_offset + 3 >= mask.pixels.size()) {
                continue;
            }

            const uint32_t coverage = mask.pixels[mask_offset + 3];
            if (!coverage) {
                continue;
            }

            uint8_t dyed_r = 0;
            uint8_t dyed_g = 0;
            uint8_t dyed_b = 0;
            if (!endpoint_dyed_pixels.empty()) {
                dyed_b = endpoint_dyed_pixels[base_offset + 0];
                dyed_g = endpoint_dyed_pixels[base_offset + 1];
                dyed_r = endpoint_dyed_pixels[base_offset + 2];
            }
            else {
                const int64_t r = base.pixels[base_offset + 2];
                const int64_t g = base.pixels[base_offset + 1];
                const int64_t b = base.pixels[base_offset + 0];
                dyed_r = ClientClampQ16ToU8(
                    static_cast<int64_t>(fixed_matrix[0]) * r
                    + static_cast<int64_t>(fixed_matrix[1]) * g
                    + static_cast<int64_t>(fixed_matrix[2]) * b
                    + fixed_matrix[3]);
                dyed_g = ClientClampQ16ToU8(
                    static_cast<int64_t>(fixed_matrix[4]) * r
                    + static_cast<int64_t>(fixed_matrix[5]) * g
                    + static_cast<int64_t>(fixed_matrix[6]) * b
                    + fixed_matrix[7]);
                dyed_b = ClientClampQ16ToU8(
                    static_cast<int64_t>(fixed_matrix[8]) * r
                    + static_cast<int64_t>(fixed_matrix[9]) * g
                    + static_cast<int64_t>(fixed_matrix[10]) * b
                    + fixed_matrix[11]);
            }

            const uint32_t alpha = (coverage * 15u + 128u) / 255u;
            const uint32_t inv_alpha = 15u - alpha;
            base.pixels[base_offset + 2] = static_cast<uint8_t>(std::min<uint32_t>(
                255u,
                static_cast<uint32_t>(ClientAlphaTable(inv_alpha, base.pixels[base_offset + 2]))
                    + static_cast<uint32_t>(ClientAlphaTable(alpha, dyed_r))));
            base.pixels[base_offset + 1] = static_cast<uint8_t>(std::min<uint32_t>(
                255u,
                static_cast<uint32_t>(ClientAlphaTable(inv_alpha, base.pixels[base_offset + 1]))
                    + static_cast<uint32_t>(ClientAlphaTable(alpha, dyed_g))));
            base.pixels[base_offset + 0] = static_cast<uint8_t>(std::min<uint32_t>(
                255u,
                static_cast<uint32_t>(ClientAlphaTable(inv_alpha, base.pixels[base_offset + 0]))
                    + static_cast<uint32_t>(ClientAlphaTable(alpha, dyed_b))));
        }
    }
    return true;
}

bool IsDatTextureLoadSafe(IDirect3DDevice9* device) {
    if (!device || device->TestCooperativeLevel() != D3D_OK) {
        return false;
    }
    if (GW::Context::GetPreGameContext()) {
        return false;
    }
    if (!GW::map::GetIsMapLoaded()) {
        return false;
    }
    if (GW::map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return false;
    }
    if (GW::map::GetIsInCinematic()) {
        return false;
    }
    if (!GW::ui::GetIsUIDrawn()) {
        return false;
    }
    return true;
}

}  // namespace

/* ---------------- Lifecycle ---------------- */

bool Initialize() {
    CrashContextScope context("startup", "textures", "resolve_dat_functions");
    // Resolution is idempotent; do it eagerly so first-use has no stall and
    // crash attribution covers it. Non-fatal: if the game changed and a symbol
    // fails to resolve, textures simply won't load.
    GWDatReader::Instance().EnsureHooks();
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "textures", "shutdown");
    {
        std::lock_guard<std::recursive_mutex> lock(g_textures_mutex);
        for (auto& [id, img] : g_textures_by_file_id) {
            if (img && img->m_tex) {
                img->m_tex->Release();
                img->m_tex = nullptr;
            }
            if (img) {
                img->m_evicted = true;
            }
        }
        g_textures_by_file_id.clear();
        for (auto& [key, img] : g_textures_by_colored_model) {
            if (img && img->m_tex) {
                img->m_tex->Release();
                img->m_tex = nullptr;
            }
            if (img) {
                img->m_evicted = true;
            }
        }
        g_textures_by_colored_model.clear();
    }
    {
        std::lock_guard<std::recursive_mutex> lock(g_cpu_jobs_mutex);
        std::queue<std::function<void()>>().swap(g_cpu_jobs);
    }
    {
        std::lock_guard<std::recursive_mutex> lock(g_dx_jobs_mutex);
        std::queue<std::function<void(IDirect3DDevice9*)>>().swap(g_dx_jobs);
    }
}

/* ---------------- GWDatReader ---------------- */

GWDatReader& GWDatReader::Instance() {
    static GWDatReader instance;
    return instance;
}

GWDatReader::~GWDatReader() {
    std::lock_guard<std::recursive_mutex> lock(g_textures_mutex);
    g_textures_by_file_id.clear();
}

void GWDatReader::SetDevice(IDirect3DDevice9* device) {
    d3d_device_ = device;
}

void GWDatReader::CpuUpdate() {
    while (true) {
        std::function<void()> task;
        {
            std::lock_guard<std::recursive_mutex> lock(g_cpu_jobs_mutex);
            if (g_cpu_jobs.empty()) {
                return;
            }
            task = std::move(g_cpu_jobs.front());
            g_cpu_jobs.pop();
        }
        task();
    }
}

void GWDatReader::DxUpdate(IDirect3DDevice9* device) {
    SetDevice(device);
    while (true) {
        std::function<void(IDirect3DDevice9*)> task;
        {
            std::lock_guard<std::recursive_mutex> lock(g_dx_jobs_mutex);
            if (g_dx_jobs.empty()) {
                return;
            }
            task = std::move(g_dx_jobs.front());
            g_dx_jobs.pop();
        }
        task(device);
    }
}

bool GWDatReader::IsDatTextureKey(const std::wstring& texture_key) {
    constexpr std::wstring_view prefix = L"gwdat://";
    return texture_key.size() > prefix.size() && texture_key.rfind(prefix.data(), 0) == 0;
}

uint32_t GWDatReader::ParseFileId(const std::wstring& texture_key) {
    if (!IsDatTextureKey(texture_key)) {
        return 0;
    }
    try {
        return static_cast<uint32_t>(std::stoul(texture_key.substr(8)));
    }
    catch (...) {
        return 0;
    }
}

bool GWDatReader::ReadDatFile(const wchar_t* file_hash, std::vector<uint8_t>* bytes_out, uint32_t stream_id) {
    if (!(file_hash && *file_hash && bytes_out && g_close_rec_obj_func && g_file_hash_to_rec_obj_func
        && g_read_file_buffer_func && g_free_file_buffer_func)) {
        return false;
    }

    const uint32_t file_id = ArenaNetFileParser::FileHashToFileId(file_hash);
    RecObj* rec = nullptr;
    const bool has_subtype = std::char_traits<wchar_t>::length(file_hash) > 2 && file_hash[2] != 0;
    if (file_id && !has_subtype && g_open_file_by_file_id_func) {
        rec = OpenFileByFileIdNoFault(0, file_id, stream_id, 1, 0);
    }
    if (!rec) {
        rec = FileHashToRecObjNoFault(file_hash, 1, 0);
    }
    if (!rec) {
        return false;
    }

    return ReadDatRecord(rec, bytes_out);
}

bool GWDatReader::ReadDatFileById(uint32_t file_id, std::vector<uint8_t>* bytes_out, uint32_t stream_id) {
    if (!(file_id && bytes_out && g_open_file_by_file_id_func && g_read_file_buffer_func
        && g_free_file_buffer_func && g_close_rec_obj_func)) {
        return false;
    }

    return ReadDatRecord(OpenFileByFileIdNoFault(0, file_id, stream_id, 1, 0), bytes_out);
}

bool GWDatReader::EnsureHooks() {
    if (hooks_initialized_) {
        return hooks_ready_;
    }
    hooks_initialized_ = true;

    ResolveDecodeImageFunc();
    ResolveOpenFileByFileIdFunc();
    ResolveFileHashToRecObjFunc();
    ResolveReadFileBufferFunc();
    ResolveFreeFileBufferFunc();
    ResolveCloseRecObjFunc();
    ResolveAllocateImageFunc();
    ResolveDepalletizeFunc();

    // OpenFileByFileId is optional (ReadDatFile falls back to FileHashToRecObj).
    hooks_ready_ = g_file_hash_to_rec_obj_func
        && g_read_file_buffer_func
        && g_decode_image_func
        && g_free_file_buffer_func
        && g_close_rec_obj_func
        && g_allocate_image_func
        && g_depalletize_func;

    return hooks_ready_;
}

IDirect3DTexture9** GWDatReader::LoadTextureFromFileId(uint32_t file_id) {
    std::shared_ptr<GwImg> gwimg_ptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_textures_mutex);
        auto found = g_textures_by_file_id.find(file_id);
        if (found != g_textures_by_file_id.end()) {
            found->second->Touch();
            return &found->second->m_tex;
        }
        gwimg_ptr = std::make_shared<GwImg>(file_id);
        g_textures_by_file_id[file_id] = gwimg_ptr;
    }

    EnqueueCpuTask([gwimg_ptr]() {
        if (gwimg_ptr->m_evicted) {
            return;
        }
        auto decoded = DecodeTexture(gwimg_ptr->m_file_id);
        if (!decoded || gwimg_ptr->m_evicted) {
            return;
        }
        gwimg_ptr->m_dims = decoded->dims;
        EnqueueDxTask([gwimg_ptr, decoded](IDirect3DDevice9* device) {
            if (gwimg_ptr->m_evicted) {
                return;
            }
            gwimg_ptr->m_tex = CreateTexture(device, *decoded);
        });
    });
    return &gwimg_ptr->m_tex;
}

IDirect3DTexture9* GWDatReader::GetTexture(const std::wstring& texture_key) {
    return GetTextureByFileId(ParseFileId(texture_key));
}

IDirect3DTexture9* GWDatReader::GetTextureByFileId(uint32_t file_id) {
    if (!EnsureHooks() || !file_id || !IsDatTextureLoadSafe(d3d_device_)) {
        return nullptr;
    }
    auto texture = LoadTextureFromFileId(file_id);
    return texture ? *texture : nullptr;
}

IDirect3DTexture9** GWDatReader::LoadColoredTextureFromModel(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4) {
    if (!model_file_id) {
        return nullptr;
    }

    ColoredModelTextureKey key;
    key.revision = 7;
    key.model_file_id = model_file_id;
    key.dye_tint = dye_tint;
    key.dye1 = dye1;
    key.dye2 = dye2;
    key.dye3 = dye3;
    key.dye4 = dye4;

    std::shared_ptr<GwImg> gwimg_ptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_textures_mutex);
        auto found = g_textures_by_colored_model.find(key);
        if (found != g_textures_by_colored_model.end()) {
            found->second->Touch();
            return &found->second->m_tex;
        }
        gwimg_ptr = std::make_shared<GwImg>(key.model_file_id);
        g_textures_by_colored_model[key] = gwimg_ptr;
    }

    EnqueueCpuTask([gwimg_ptr, key]() {
        if (gwimg_ptr->m_evicted) {
            return;
        }

        std::shared_ptr<DecodedTexture> mask;
        std::shared_ptr<DecodedTexture> base;
        // Prefer the offline linked-icon resolve (base + dye mask); fall back to
        // typed reads via the game functions.
        if (!ResolveLinkedIconTexturesFromDat(key.model_file_id, base, mask)) {
            base = DecodeTypedTexture(key.model_file_id, 1);
            if (!mask) {
                mask = DecodeTypedTexture(key.model_file_id, 0x0c);
            }
        }
        if (!base) {
            base = DecodeTexture(key.model_file_id);
        }
        if (!base) {
            return;
        }

        if (key.dye1 != 0 || key.dye2 != 0 || key.dye3 != 0 || key.dye4 != 0) {
            if (!mask) {
                mask = DecodeTypedTexture(key.model_file_id, 0x0c);
            }
            if (mask) {
                ApplyClientDyeModel(*base, *mask, key.dye_tint, key.dye1, key.dye2, key.dye3, key.dye4);
            }
        }

        if (gwimg_ptr->m_evicted) {
            return;
        }
        gwimg_ptr->m_dims = base->dims;
        EnqueueDxTask([gwimg_ptr, base](IDirect3DDevice9* device) {
            if (gwimg_ptr->m_evicted) {
                return;
            }
            gwimg_ptr->m_tex = CreateTexture(device, *base);
        });
    });
    return &gwimg_ptr->m_tex;
}

IDirect3DTexture9* GWDatReader::GetColoredModelTexture(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4) {
    if (!EnsureHooks() || !model_file_id || !IsDatTextureLoadSafe(d3d_device_)) {
        return nullptr;
    }
    auto texture = LoadColoredTextureFromModel(model_file_id, dye_tint, dye1, dye2, dye3, dye4);
    return texture ? *texture : nullptr;
}

void GWDatReader::CleanupOldTextures(int timeout_seconds) {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<GwImg>> expired;

    {
        std::lock_guard<std::recursive_mutex> lock(g_textures_mutex);
        for (auto it = g_textures_by_file_id.begin(); it != g_textures_by_file_id.end();) {
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->m_last_used).count();
            if (duration > timeout_seconds) {
                it->second->m_evicted = true;
                expired.push_back(it->second);
                it = g_textures_by_file_id.erase(it);
            }
            else {
                ++it;
            }
        }
        for (auto it = g_textures_by_colored_model.begin(); it != g_textures_by_colored_model.end();) {
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->m_last_used).count();
            if (duration > timeout_seconds) {
                it->second->m_evicted = true;
                expired.push_back(it->second);
                it = g_textures_by_colored_model.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    for (auto& gwimg_ptr : expired) {
        EnqueueDxTask([gwimg_ptr](IDirect3DDevice9*) {
            if (gwimg_ptr->m_tex) {
                gwimg_ptr->m_tex->Release();
                gwimg_ptr->m_tex = nullptr;
            }
        });
    }
}

}  // namespace GW::textures

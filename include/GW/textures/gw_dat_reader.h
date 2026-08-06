#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <string>
#include <vector>

#include <d3d9.h>

// GWDatReader: reads and decodes assets out of the game's GW.dat archive.
// Owns the resolved game functions used for dat access and image decoding, and
// manages dat-backed D3D textures (async CPU decode -> device upload).
namespace GW::textures {

/* ---------------- Resolved-symbol surface (module-owned) ---------------- */

// Opaque game record object handed back by the file-open functions.
class RecObj;

struct Vec2i {
    int x = 0;
    int y = 0;
};

enum GR_FORMAT : uint32_t {
    GR_FORMAT_A8R8G8B8 = 0,
    GR_FORMAT_UNK = 0x4,
    GR_FORMAT_DXT1 = 0xF,
    GR_FORMAT_DXT2,
    GR_FORMAT_DXT3,
    GR_FORMAT_DXT4,
    GR_FORMAT_DXT5,
    GR_FORMAT_DXTA,
    GR_FORMAT_DXTL,
    GR_FORMAT_DXTN,
    GR_FORMATS
};

using gw_image_bits = uint8_t*;

using OpenFileByFileId_pt = RecObj*(__cdecl*)(uint32_t archive, uint32_t file_id, uint32_t stream_id, uint32_t flags, uint32_t* error_out);
using FileIdToRecObj_pt = RecObj*(__cdecl*)(const wchar_t* file_hash, int unk1_1, int unk2_0);
using GetRecObjectBytes_pt = uint8_t*(__cdecl*)(RecObj* rec, int* size_out);
using DecodeImage_pt = uint32_t(__cdecl*)(int size, uint8_t* bytes, gw_image_bits* bits, uint8_t* palette, GR_FORMAT* format, Vec2i* dims, int* levels);
using UnkRecObjBytes_pt = void(__cdecl*)(RecObj* rec, uint8_t* bytes);
using CloseRecObj_pt = void(__cdecl*)(RecObj* rec);
using AllocateImage_pt = gw_image_bits(__cdecl*)(GR_FORMAT format, Vec2i* dest_dims, uint32_t levels, uint32_t unk2);
using Depalletize_pt = void(__cdecl*)(
    gw_image_bits dest_bits, uint8_t* dest_palette, GR_FORMAT dest_format, int* dest_mip_widths,
    gw_image_bits source_bits, uint8_t* source_palette, GR_FORMAT source_format, int* source_mip_widths,
    Vec2i* source_dims, uint32_t source_levels, uint32_t unk1_0, int* unk2_0);

extern OpenFileByFileId_pt g_open_file_by_file_id_func;
extern FileIdToRecObj_pt g_file_hash_to_rec_obj_func;
extern GetRecObjectBytes_pt g_read_file_buffer_func;
extern DecodeImage_pt g_decode_image_func;
extern UnkRecObjBytes_pt g_free_file_buffer_func;
extern CloseRecObj_pt g_close_rec_obj_func;
extern AllocateImage_pt g_allocate_image_func;
extern Depalletize_pt g_depalletize_func;

// Resolver ownership (bodies in gw_dat_reader_patterns.cpp).
bool ResolveDecodeImageFunc();
bool ResolveOpenFileByFileIdFunc();
bool ResolveFileHashToRecObjFunc();
bool ResolveReadFileBufferFunc();
bool ResolveFreeFileBufferFunc();
bool ResolveCloseRecObjFunc();
bool ResolveAllocateImageFunc();
bool ResolveDepalletizeFunc();

/* ---------------- Lifecycle ---------------- */

bool Initialize();
void Shutdown();

/* ---------------- Public callable surface ---------------- */

class GWDatReader {
public:
    static GWDatReader& Instance();

    void SetDevice(IDirect3DDevice9* device);
    void CpuUpdate();
    void DxUpdate(IDirect3DDevice9* device);
    IDirect3DTexture9* GetTexture(const std::wstring& texture_key);
    IDirect3DTexture9* GetTextureByFileId(uint32_t file_id);
    // Dyed/colored model texture: base icon + dye mask blended with the client
    // dye colors (dye_tint 0..0x31, dye1..4 are dye color indices 1..13).
    IDirect3DTexture9* GetColoredModelTexture(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4);
    void CleanupOldTextures(int timeout_seconds = 30);
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);
    static IDirect3DTexture9** LoadColoredTextureFromModel(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4);

    static bool IsDatTextureKey(const std::wstring& texture_key);
    static uint32_t ParseFileId(const std::wstring& texture_key);
    static bool ReadDatFileById(uint32_t file_id, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);
    static bool ReadDatFile(const wchar_t* file_hash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    // Resolve the game functions this reader needs (idempotent). Lazily invoked
    // on first texture request; also called eagerly from Initialize().
    bool EnsureHooks();

private:
    GWDatReader() = default;
    ~GWDatReader();
    GWDatReader(const GWDatReader&) = delete;
    GWDatReader& operator=(const GWDatReader&) = delete;

    IDirect3DDevice9* d3d_device_ = nullptr;
    bool hooks_initialized_ = false;
    bool hooks_ready_ = false;
};

}  // namespace GW::textures

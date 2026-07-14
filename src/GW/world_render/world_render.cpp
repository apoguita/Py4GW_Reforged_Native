#include "base/error_handling.h"

#include "GW/world_render/world_render.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "GW/render/render.h"
#include "GW/map/map.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <d3d9.h>

namespace GW::world_render {

DdiDispatchFn g_ddi_func = nullptr;
DdiDispatchFn g_ddi_original = nullptr;

namespace {

// Dx9 DDI command dispatcher facts (RE of FUN_006c6c40 / GWCA RenderMgr):
//   cmd[1] = DDI opcode. 0x0F = PRESENT (handler calls EndScene @vtbl+0xA8 then
//   Present @vtbl+0x44). At that point the whole world is rendered and the depth
//   buffer is fully populated -> correct occluded draw point.
//   device = *(IDirect3DDevice9**)(ddi_ctx + 0x1B8).
constexpr uint32_t kPresentOpcode = 0x0F;
constexpr uintptr_t kDeviceOffset = 0x1B8;

struct Entry {
    int token;
    DrawCallback cb;
};

std::mutex g_mutex;
std::vector<Entry> g_callbacks;
int g_next_token = 1;

std::mutex g_hook_mutex;
std::atomic<bool> g_resolved{false};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_draw_enabled{true};
std::atomic<bool> g_scan_enabled{false};   // per-opcode depth scan (diagnostic, opt-in)
std::atomic<bool> g_shutting_down{false};
std::atomic<int> g_active{0};

// Hard map-validity check: only draw in a fully loaded, non-loading map. Anything
// else (loading screen, char select, map transition) -> deactivate, so the hook
// never touches D3D while the device/render targets are being torn down/rebuilt.
bool IsMapValid() {
    return GW::map::GetIsMapLoaded() &&
           GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;
}

// diagnostics
std::atomic<uint32_t> g_diag_dispatch{0};   // total DDI dispatcher calls
std::atomic<uint32_t> g_diag_present{0};    // opcode 0x0F occurrences
std::atomic<uint32_t> g_diag_cbs{0};        // times callbacks invoked
std::atomic<uintptr_t> g_diag_device{0};    // DDI device (*(ddi_ctx+0x1B8)) at 0x0F
std::atomic<uintptr_t> g_diag_dev_gw{0};    // GW::render::GetDevice() (must match DDI device)
std::atomic<uint32_t> g_diag_shaders{0};    // shaders bound before clear: bit1=vs bit0=ps
std::atomic<uint32_t> g_diag_last_opcode{0xFFFFFFFF};

// DDI opcode we draw at (settable). 0x1E is the empirically-confirmed draw point
// on this build: the DDI dispatcher opcode where the world depth buffer is live so
// overlays occlude. (0x0F/present is too late - depth already discarded there; the
// depth scan flagged 0x15/0x25 but 0x1E is the one that actually occludes.)
std::atomic<uint32_t> g_draw_opcode{0x1E};

// Per-opcode depth scan. For each DDI opcode we periodically probe how many pixels
// of a z=0.5 full-screen quad PASS the depth test (pass << total => scene depth is
// live at that opcode). We cycle the probed opcode across frames (one query per
// frame, double-buffered, non-blocking) and track the opcode with the FEWEST
// passing pixels = the most populated depth = the correct draw point.
constexpr uint32_t kMaxOpcode = 0x26;
uint32_t g_op_pass[kMaxOpcode];   // last pass count per opcode (render thread only)
uint32_t g_op_total[kMaxOpcode];  // viewport pixel count per opcode
std::atomic<uint32_t> g_scan_total{0};
std::atomic<uint32_t> g_max_total{0};      // largest viewport seen (= main scene)
std::atomic<uint32_t> g_best_op{0xFFFFFFFF};
std::atomic<uint32_t> g_best_ratio{1000};  // best (lowest) pass/total*1000 on a full-size target
uint8_t g_probed_round[kMaxOpcode];        // this opcode already probed this round?

void ResetScan() {
    for (uint32_t i = 0; i < kMaxOpcode; ++i) {
        g_op_pass[i] = 0xFFFFFFFF;
        g_op_total[i] = 0;
        g_probed_round[i] = 0;
    }
    g_max_total = 0;
    g_best_op = 0xFFFFFFFF;
    g_best_ratio = 1000;
}

// Called on EVERY DDI dispatch. Reads the pending query result, and (when at the
// current target opcode with no query in flight) issues a fresh probe.
void RunDepthProbe(IDirect3DDevice9* device, uint32_t current_opcode) {
    static IDirect3DQuery9* query = nullptr;
    static bool pending = false;
    static uint32_t pending_op = 0;
    static uint32_t pending_total = 0;
    static IDirect3DDevice9* query_device = nullptr;

    // Device changed (reset/recreate) -> the query is stale; drop it.
    if (query_device != device) {
        if (query) { query->Release(); query = nullptr; }
        pending = false;
        query_device = device;
    }

    if (pending && query) {
        DWORD pixels = 0;
        if (query->GetData(&pixels, sizeof(pixels), 0) == S_OK) {
            if (pending_op < kMaxOpcode) {
                g_op_pass[pending_op] = pixels;
                g_op_total[pending_op] = pending_total;
                if (pending_total > g_max_total.load()) g_max_total.store(pending_total);
                // best = lowest pass/total ratio among near-full-size targets (the
                // main scene), so small offscreen passes (shadow maps) don't win.
                if (pending_total != 0) {
                    const uint32_t ratio =
                        static_cast<uint32_t>(static_cast<uint64_t>(pixels) * 1000 / pending_total);
                    if (pending_total * 2 >= g_max_total.load() &&
                        ratio + 20 < g_best_ratio.load()) {
                        g_best_ratio.store(ratio);
                        g_best_op.store(pending_op);
                    }
                }
            }
            pending = false;
        }
    }
    if (pending) return;
    // Probe each opcode that actually fires, once per round (no fixed cycle, so
    // opcodes that never fire can't stall the scan).
    if (current_opcode >= kMaxOpcode || g_probed_round[current_opcode]) return;
    if (!query && (FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query)) || !query)) {
        return;
    }

    IDirect3DStateBlock9* sb = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &sb))) sb = nullptr;
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetTexture(0, nullptr);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);

    D3DVIEWPORT9 vp = {};
    device->GetViewport(&vp);
    pending_total = vp.Width * vp.Height;
    g_scan_total.store(pending_total, std::memory_order_relaxed);
    const float w = static_cast<float>(vp.Width);
    const float h = static_cast<float>(vp.Height);
    struct V { float x, y, z, rhw; D3DCOLOR c; };
    V quad[4] = {
        { 0.0f, 0.0f, 0.5f, 1.0f, 0xFFFFFFFF },
        { w,    0.0f, 0.5f, 1.0f, 0xFFFFFFFF },
        { 0.0f, h,    0.5f, 1.0f, 0xFFFFFFFF },
        { w,    h,    0.5f, 1.0f, 0xFFFFFFFF },
    };
    query->Issue(D3DISSUE_BEGIN);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V));
    query->Issue(D3DISSUE_END);
    pending = true;
    pending_op = current_opcode;
    if (current_opcode < kMaxOpcode) g_probed_round[current_opcode] = 1;

    if (sb) { sb->Apply(); sb->Release(); }
}

void RunCallbacks(IDirect3DDevice9* device) {
    std::vector<DrawCallback> local;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        local.reserve(g_callbacks.size());
        for (auto& e : g_callbacks) {
            local.push_back(e.cb);
        }
    }
    if (local.empty()) return;

    // Protect GW's own render state from our draws.
    IDirect3DStateBlock9* state_block = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state_block))) {
        state_block = nullptr;
    }

    // CRITICAL: GW renders with programmable shaders bound. Our overlay draws are
    // fixed-function (DrawPrimitiveUP + FVF + SetTransform); a bound vertex shader
    // would reinterpret our vertices and wreck depth. Force fixed function for our
    // draws - the state block restores GW's shaders afterward.
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);

    for (auto& cb : local) {
        if (!cb) continue;
        try {
            cb(device);
        } catch (...) {
        }
    }
    if (state_block) {
        state_block->Apply();
        state_block->Release();
    }
}

}  // namespace

// Detour for the Dx9 DDI command dispatcher. On the PRESENT opcode (0x0F) the
// frame is fully rendered and depth is populated: run our draws there, then the
// original (which does EndScene + Present). Mirrors newer GWCA RenderMgr exactly.
uint32_t __cdecl OnDdiDispatch(void* p1, void* ddi_ctx, uint32_t* cmd, uint32_t* out) {
    PY4GW::HookBase::EnterHook();
    ++g_active;

    // HARD GATE: the detour is a pure passthrough unless we have registered draws,
    // drawing is enabled, and we're in a valid loaded map. Nothing stays "armed" at
    // idle / char-select / map load, so no D3D work happens while the device and
    // render targets are being reset.
    bool active = false;
    if (!g_shutting_down.load() && cmd && ddi_ctx && g_draw_enabled.load() && IsMapValid()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        active = !g_callbacks.empty();
    }

    if (active) {
        const uint32_t opcode = cmd[1];
        g_diag_dispatch.fetch_add(1, std::memory_order_relaxed);
        g_diag_last_opcode.store(opcode, std::memory_order_relaxed);

        IDirect3DDevice9* device = *reinterpret_cast<IDirect3DDevice9**>(
            reinterpret_cast<uint8_t*>(ddi_ctx) + kDeviceOffset);
        if (device) {
            // Per-opcode depth scan is diagnostic and opt-in (off by default).
            if (g_scan_enabled.load()) {
                RunDepthProbe(device, opcode);
            }

            if (opcode == g_draw_opcode.load()) {
                g_diag_present.fetch_add(1, std::memory_order_relaxed);
                g_diag_device.store(reinterpret_cast<uintptr_t>(device), std::memory_order_relaxed);
                g_diag_dev_gw.store(reinterpret_cast<uintptr_t>(GW::render::GetDevice()),
                                    std::memory_order_relaxed);

                IDirect3DVertexShader9* vs = nullptr;
                IDirect3DPixelShader9* ps = nullptr;
                device->GetVertexShader(&vs);
                device->GetPixelShader(&ps);
                g_diag_shaders.store((vs ? 2u : 0u) | (ps ? 1u : 0u), std::memory_order_relaxed);
                if (vs) vs->Release();
                if (ps) ps->Release();

                g_diag_cbs.fetch_add(1, std::memory_order_relaxed);
                RunCallbacks(device);
            }
        }
    }

    const uint32_t ret = g_ddi_original ? g_ddi_original(p1, ddi_ctx, cmd, out) : 0;

    --g_active;
    PY4GW::HookBase::LeaveHook();
    return ret;
}

static void EnsureHookInstalled() {
    if (!g_resolved.load() || g_installed.load() || g_shutting_down.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    if (g_installed.load()) {
        return;
    }
    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_ddi_func),
        reinterpret_cast<void*>(&OnDdiDispatch),
        reinterpret_cast<void**>(&g_ddi_original));
    if (!Logger::AssertHook("Dx9DdiDispatch_Func", status, "world_render")) {
        return;
    }
    PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_ddi_func));
    g_installed = true;
    g_enabled = true;
}

bool Initialize() {
    CrashContextScope context("startup", "world_render", "initialize");
    if (g_resolved.load()) {
        return true;
    }
    g_shutting_down = false;
    ResetScan();

    // Resolve only; the hook installs lazily on first RegisterDraw so startup is
    // never perturbed.
    if (!ResolveDdiDispatch()) {
        Logger::Instance().LogWarning(
            "[world_render] could not resolve the Dx9 DDI dispatcher; occluded draws disabled.",
            "world_render");
        return false;
    }
    g_resolved = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "world_render", "shutdown");
    g_shutting_down = true;

    if (g_installed.load()) {
        std::lock_guard<std::mutex> lock(g_hook_mutex);
        if (g_ddi_func) {
            PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_ddi_func));
        }
        for (int i = 0; i < 125 && g_active.load() > 0; ++i) {
            ::Sleep(4);
        }
        if (g_ddi_func) {
            PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_ddi_func));
        }
        g_installed = false;
        g_enabled = false;
    }

    std::vector<Entry> dead;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        dead.swap(g_callbacks);
    }
    dead.clear();

    g_ddi_func = nullptr;
    g_ddi_original = nullptr;
    g_resolved = false;
}

int RegisterDraw(DrawCallback callback) {
    if (!callback) {
        return -1;
    }
    int token;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        token = g_next_token++;
        g_callbacks.push_back({token, std::move(callback)});
    }
    EnsureHookInstalled();
    return token;
}

void UnregisterDraw(int token) {
    Entry removed;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = std::find_if(g_callbacks.begin(), g_callbacks.end(),
                               [token](const Entry& e) { return e.token == token; });
        if (it != g_callbacks.end()) {
            removed = std::move(*it);
            g_callbacks.erase(it);
        }
    }
    // removed destructs outside the lock (GIL-safe deleter).
}

void ClearDraws() {
    std::vector<Entry> dead;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        dead.swap(g_callbacks);
    }
}

bool IsActive() {
    return g_enabled.load();
}

void SetEnabled(bool enabled) {
    g_draw_enabled = enabled;
}

void SetDrawOpcode(int opcode) {
    g_draw_opcode = static_cast<uint32_t>(opcode);
}

void SetScanEnabled(bool enabled) {
    g_scan_enabled = enabled;
}

std::string GetDiagnostics() {
    char buf[448];
    std::snprintf(buf, sizeof(buf),
                  "installed=%d enabled=%d draw_op=0x%X dispatch=%u drawn=%u cbs=%u "
                  "dev_ddi=0x%08X dev_gw=0x%08X shaders=%u\n"
                  "SCAN best_op=0x%X best_ratio(pass/total permil)=%u max_total=%u "
                  "(best_op = full-size opcode with most depth; set draw_op there)",
                  g_installed.load() ? 1 : 0,
                  g_draw_enabled.load() ? 1 : 0,
                  g_draw_opcode.load(),
                  g_diag_dispatch.load(), g_diag_present.load(), g_diag_cbs.load(),
                  static_cast<unsigned>(g_diag_device.load()),
                  static_cast<unsigned>(g_diag_dev_gw.load()),
                  g_diag_shaders.load(),
                  g_best_op.load(), g_best_ratio.load(), g_max_total.load());
    std::string out = buf;
    out += "\nop_pass(pass/total):";
    for (uint32_t i = 0; i < kMaxOpcode; ++i) {
        if (g_op_pass[i] != 0xFFFFFFFF) {
            char b[40];
            std::snprintf(b, sizeof(b), " 0x%X=%u/%u", i, g_op_pass[i], g_op_total[i]);
            out += b;
        }
    }
    return out;
}

}  // namespace GW::world_render

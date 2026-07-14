#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/patterns.h"

#include "GW/world_render/world_render.h"

namespace GW::world_render {

// Resolves GW's Dx9 DDI command dispatcher (FUN_006c6c40). GWCA locates it via the
// instruction `MOV ECX,0x1910` (B9 10 19 00 00) inside it, then walks to the
// function start (within ~0x300 bytes). We do the same via offsets/world_render.json.
bool ResolveDdiDispatch() {
    CrashContextScope context("startup", "world_render", "resolve_ddi_dispatch");
    return PY4GW::Patterns::Resolve("world_render.ddi_dispatch_func", &g_ddi_func);
}

}  // namespace GW::world_render

#include "base/py_bindings.h"

#include "GW/world_render/world_render.h"

#include <memory>
#include <utility>

namespace py = pybind11;

// PyWorldRender: register Python draw callbacks that run INSIDE GW's world render
// pass (after the world block, before the HUD) so 3D overlays are occluded by
// world geometry. Callbacks take no args; draw via PyDXOverlay's 3D methods,
// which fetch the device and set up the (now correct) matrices/depth states.
PYBIND11_EMBEDDED_MODULE(PyWorldRender, m) {
    m.doc() = "Occluded in-world overlay draw callbacks (GW world render pass).";

    m.def(
        "register_draw",
        [](py::function fn) -> int {
            // The py::function is kept alive for the render thread. It MUST be
            // destroyed under the GIL (destroying a py object off-GIL crashes),
            // so wrap it in a shared_ptr whose deleter acquires the GIL.
            auto holder = std::shared_ptr<py::function>(
                new py::function(std::move(fn)),
                [](py::function* p) {
                    py::gil_scoped_acquire gil;
                    delete p;
                });

            return GW::world_render::RegisterDraw(
                [holder](IDirect3DDevice9*) {
                    py::gil_scoped_acquire gil;
                    try {
                        (*holder)();
                    } catch (py::error_already_set& e) {
                        e.discard_as_unraisable("PyWorldRender draw callback");
                    } catch (...) {
                    }
                });
        },
        py::arg("callback"),
        "Register a no-arg draw callback invoked in the world pass. Returns a token.");

    m.def(
        "unregister_draw",
        [](int token) { GW::world_render::UnregisterDraw(token); },
        py::arg("token"), "Remove a previously registered draw callback.");

    m.def("clear", []() { GW::world_render::ClearDraws(); },
          "Remove all registered draw callbacks.");

    m.def("is_active", []() { return GW::world_render::IsActive(); },
          "True if the world-render hook is installed and enabled.");

    m.def("get_diagnostics", []() { return GW::world_render::GetDiagnostics(); },
          "DDI dispatcher / present / callback counts + device pointer.");

    m.def("set_enabled", [](bool enabled) { GW::world_render::SetEnabled(enabled); },
          py::arg("enabled"),
          "Enable/disable overlay drawing without removing the hook (A/B test).");

    m.def("set_draw_opcode", [](int opcode) { GW::world_render::SetDrawOpcode(opcode); },
          py::arg("opcode"),
          "DDI opcode to draw at (default 0x1E). Use SCAN best_op from diagnostics.");

    m.def("set_scan_enabled", [](bool enabled) { GW::world_render::SetScanEnabled(enabled); },
          py::arg("enabled"),
          "Enable the diagnostic per-opcode depth scan (off by default).");

    m.def("heartbeat", []() { GW::world_render::Heartbeat(); },
          "Ping the idle watchdog. Call every frame while alive; if pings stop (script "
          "closed) the draw callbacks are cleared so nothing is left drawn.");
}

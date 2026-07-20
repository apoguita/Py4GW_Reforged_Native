#include "base/py_bindings.h"

#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "system/system.h"

#include <memory>
#include <utility>

namespace py = pybind11;

namespace {

bool MapReady() {
    return GW::map::GetIsMapLoaded() &&
        GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyGameThread, m) {
    m.doc() = "Py4GW GameThread bindings";

    m.def("clear_calls", []() {
        GW::game_thread::ClearCalls();
    });

    m.def("is_in_game_thread", []() -> bool {
        return GW::game_thread::IsInGameThread();
    });

    // Enqueue a Python callable to run on the GW game thread. Parity port of the
    // legacy EnqueuePythonCallback: guarded by a map-ready check both here and
    // again on the game thread (the map can change in between). Deviation from
    // legacy: a failing callback is reported to the console instead of being
    // silently swallowed.
    m.def("enqueue", [](py::function fn) {
        if (!MapReady()) {
            return;
        }
        // The game thread destroys queued std::functions (game_thread
        // CallFunctions '.clear()' / ClearCalls) WITHOUT holding the GIL. A bare
        // py::function capture would then decref a Python object off-GIL -> access
        // violation (0xC0000005 in Py_Dealloc). Own the callable through a
        // shared_ptr whose deleter reacquires the GIL, so the final destruction is
        // always GIL-safe (guarded against interpreter finalization so we never
        // touch Python state during shutdown).
        std::shared_ptr<py::function> callable(
            new py::function(std::move(fn)),
            [](py::function* p) {
                if (Py_IsInitialized()) {
                    py::gil_scoped_acquire gil;
                    delete p;
                }
                // else: interpreter finalized -> intentionally leak the small handle
                // rather than perform an off-GIL decref during shutdown.
            });
        GW::game_thread::Enqueue([callable]() {
            if (!MapReady()) {
                return;
            }
            py::gil_scoped_acquire gil;
            try {
                (*callable)();
            } catch (const py::error_already_set& error) {
                PY4GW::System::Instance().WriteConsoleMessage(
                    "PyGameThread", MessageType::Error, std::string("Game thread callback error: ") + error.what());
            }
        });
    }, py::arg("fn"), "Enqueue a Python callable to run on the GW game thread");
}

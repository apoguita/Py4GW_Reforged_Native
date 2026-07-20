#include "base/py_bindings.h"

#include "GW/render/render.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyRender, m) {
    m.doc() = "Py4GW Render bindings";

    m.def("get_is_in_render_loop", []() -> bool {
        return GW::render::GetIsInRenderLoop();
    });

    m.def("get_is_fullscreen", []() -> int {
        return GW::render::GetIsFullscreen();
    });

    m.def("get_viewport_width", []() -> uint32_t {
        return GW::render::GetViewportWidth();
    });

    m.def("get_viewport_height", []() -> uint32_t {
        return GW::render::GetViewportHeight();
    });

    m.def("get_field_of_view", []() -> float {
        return GW::render::GetFieldOfView();
    });
}

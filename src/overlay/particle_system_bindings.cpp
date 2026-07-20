#include "base/py_bindings.h"

#include "overlay/particle_system.h"

#include <memory>

namespace py = pybind11;

using PY4GW::particles::EmitterConfig;
using PY4GW::particles::ParticleEmitter;

// PyParticles: a C++-simulated, reusable particle system. Python only configures.
//   e = PyParticles.create_emitter()   # hold e to keep it alive
//   e.config.mode = PyParticles.ORBITAL
//   e.config.rate = 60
//   e.config.color = 0xFFFF8A3C
//   e.set_origin(x, y, z)              # each frame if it moves
// The system advances + draws it (occluded) every frame; drop e to stop + free.
PYBIND11_EMBEDDED_MODULE(PyParticles, m) {
    m.doc() = "C++ particle system; Python configures. Emitters are camera-facing "
              "billboard quads (no textures), drawn occluded in the world pass.";

    m.attr("BALLISTIC") = static_cast<int>(PY4GW::particles::EMIT_BALLISTIC);
    m.attr("ORBITAL")   = static_cast<int>(PY4GW::particles::EMIT_ORBITAL);

    py::class_<EmitterConfig>(m, "EmitterConfig")
        .def_readwrite("enabled", &EmitterConfig::enabled)
        .def_readwrite("mode", &EmitterConfig::mode)
        .def_readwrite("max_particles", &EmitterConfig::max_particles)
        .def_readwrite("rate", &EmitterConfig::rate)
        .def_readwrite("origin_x", &EmitterConfig::origin_x)
        .def_readwrite("origin_y", &EmitterConfig::origin_y)
        .def_readwrite("origin_z", &EmitterConfig::origin_z)
        .def_readwrite("dir_x", &EmitterConfig::dir_x)
        .def_readwrite("dir_y", &EmitterConfig::dir_y)
        .def_readwrite("dir_z", &EmitterConfig::dir_z)
        .def_readwrite("speed", &EmitterConfig::speed)
        .def_readwrite("speed_var", &EmitterConfig::speed_var)
        .def_readwrite("spread", &EmitterConfig::spread)
        .def_readwrite("grav_x", &EmitterConfig::grav_x)
        .def_readwrite("grav_y", &EmitterConfig::grav_y)
        .def_readwrite("grav_z", &EmitterConfig::grav_z)
        .def_readwrite("drag", &EmitterConfig::drag)
        .def_readwrite("orbit_radius", &EmitterConfig::orbit_radius)
        .def_readwrite("orbit_radius_var", &EmitterConfig::orbit_radius_var)
        .def_readwrite("orbit_spin", &EmitterConfig::orbit_spin)
        .def_readwrite("orbit_rise", &EmitterConfig::orbit_rise)
        .def_readwrite("orbit_height", &EmitterConfig::orbit_height)
        .def_readwrite("orbit_radius_end", &EmitterConfig::orbit_radius_end)
        .def_readwrite("spawn_radius", &EmitterConfig::spawn_radius)
        .def_readwrite("radial_speed", &EmitterConfig::radial_speed)
        .def_readwrite("turbulence", &EmitterConfig::turbulence)
        .def_readwrite("stretch", &EmitterConfig::stretch)
        .def_readwrite("life", &EmitterConfig::life)
        .def_readwrite("life_var", &EmitterConfig::life_var)
        .def_readwrite("size", &EmitterConfig::size)
        .def_readwrite("size_var", &EmitterConfig::size_var)
        .def_readwrite("size_end", &EmitterConfig::size_end)
        .def_readwrite("color", &EmitterConfig::color)
        .def_readwrite("color_end", &EmitterConfig::color_end)
        .def_readwrite("hot_frac", &EmitterConfig::hot_frac)
        .def_readwrite("additive", &EmitterConfig::additive);

    py::class_<ParticleEmitter, std::shared_ptr<ParticleEmitter>>(m, "ParticleEmitter")
        .def_readwrite("config", &ParticleEmitter::config,
                       "The live config; set any field, e.g. e.config.rate = 60.")
        .def("set_origin", &ParticleEmitter::SetOrigin, py::arg("x"), py::arg("y"), py::arg("z"),
             "Move the emitter origin (world; up = -z). Call each frame to attach it to a target. "
             "This also FEEDS the timeout: stop calling it (or keepalive) for 100ms and the emitter "
             "clears + stops (same rule as the overlay class); it resumes when fed again.")
        .def("keepalive", &ParticleEmitter::Keepalive,
             "Feed the timeout without moving (keep a static effect alive). Call each frame.")
        .def("emit", &ParticleEmitter::Emit, py::arg("count"), "Spawn a burst of `count` particles now.")
        .def("clear", &ParticleEmitter::Clear, "Kill all current particles.")
        .def("count", &ParticleEmitter::Count, "Live particle count.");

    m.def("create_emitter", &PY4GW::particles::CreateEmitter,
          "Create a system-managed emitter. HOLD the returned object to keep the effect "
          "alive; when it is dropped (or the script stops) the effect stops and frees.");
    m.def("shutdown", &PY4GW::particles::Shutdown, "Remove all emitters and unregister the draw.");
}

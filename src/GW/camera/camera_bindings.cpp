#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <cstdint>

#include "GW/camera/camera.h"
#include "GW/context/context.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyCamera, m) {
    m.doc() = "Py4GW Camera bindings";

    m.def("forward_movement", [](float amount, bool true_forward) {
        return GW::camera::ForwardMovement(amount, true_forward);
    }, py::arg("amount"), py::arg("true_forward") = false);

    m.def("vertical_movement", [](float amount) {
        return GW::camera::VerticalMovement(amount);
    }, py::arg("amount"));

    m.def("rotate_movement", [](float angle) {
        return GW::camera::RotateMovement(angle);
    }, py::arg("angle"));

    m.def("side_movement", [](float amount) {
        return GW::camera::SideMovement(amount);
    }, py::arg("amount"));

    m.def("set_max_dist", [](float dist) {
        return GW::camera::SetMaxDist(dist);
    }, py::arg("dist") = 900.0f);

    m.def("set_field_of_view", [](float fov) {
        return GW::camera::SetFieldOfView(fov);
    }, py::arg("fov"));

    m.def("compute_cam_pos", [](float dist) -> py::tuple {
        auto pos = GW::camera::ComputeCamPos(dist);
        return py::make_tuple(pos.x, pos.y, pos.z);
    }, py::arg("dist") = 0.0f);

    m.def("update_camera_pos", []() {
        return GW::camera::UpdateCameraPos();
    });

    m.def("get_field_of_view", []() -> float {
        return GW::camera::GetFieldOfView();
    });

    m.def("get_yaw", []() -> float {
        return GW::camera::GetYaw();
    });

    m.def("unlock_cam", [](bool flag) {
        return GW::camera::UnlockCam(flag);
    }, py::arg("flag"));

    m.def("get_camera_unlock", []() -> bool {
        return GW::camera::GetCameraUnlock();
    });

    m.def("set_fog", [](bool flag) {
        return GW::camera::SetFog(flag);
    }, py::arg("flag"));

    // Address of the live camera context (GW::Context::Camera). Python casts a
    // CameraStruct ctypes mirror at this address to read the ~30 data fields
    // (yaw/pitch/position/etc.) directly - the reforged data-via-context path,
    // replacing the legacy PyCamera.PyCamera() data class. 0 if unresolved.
    m.def("get_context_ptr", []() -> uintptr_t {
        return reinterpret_cast<uintptr_t>(GW::Context::GetCamera());
    }, "Address of the live camera context; cast a CameraStruct at it in Python.");
}

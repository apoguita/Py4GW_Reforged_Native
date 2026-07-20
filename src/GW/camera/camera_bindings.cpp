#include "base/py_bindings.h"

#include <cstdint>

#include "GW/camera/camera.h"
#include "GW/context/context.h"
#include "GW/context/camera.h"
#include "GW/game_thread/game_thread.h"

namespace py = pybind11;

namespace {

// ── PyCamera class (parity with legacy py_camera.h PyCamera) ──
struct PyCamera {
    uint32_t look_at_agent_id = 0;
    uint32_t h0004 = 0;
    float h0008 = 0;
    float h000C = 0;
    float max_distance = 0;
    float h0014 = 0;
    float yaw = 0;
    float current_yaw = 0;
    float pitch = 0;
    float camera_zoom = 0;
    std::vector<uint32_t> h0024;
    float yaw_right_click = 0;
    float yaw_right_click2 = 0;
    float pitch_right_click = 0;
    float distance2 = 0;
    float acceleration_constant = 0;
    float time_since_last_keyboard_rotation = 0;
    float time_since_last_mouse_rotation = 0;
    float time_since_last_mouse_move = 0;
    float time_since_last_agent_selection = 0;
    float time_in_the_map = 0;
    float time_in_the_district = 0;
    float yaw_to_go = 0;
    float pitch_to_go = 0;
    float dist_to_go = 0;
    float max_distance2 = 0;
    std::vector<float> h0070;
    struct Vec3 { float x = 0, y = 0, z = 0; };
    Vec3 position, camera_pos_to_go, cam_pos_inverted, cam_pos_inverted_to_go;
    Vec3 look_at_target, look_at_to_go;
    float field_of_view = -1.0f;
    float field_of_view2 = -1.0f;

    PyCamera() { h0024.resize(4, 0); h0070.resize(2, 0); GetContext(); }

    void ResetContext() {
        look_at_agent_id = 0; h0004 = 0; h0008 = 0; h000C = 0;
        max_distance = 0; h0014 = 0;
        yaw = 0; current_yaw = 0; pitch = 0; camera_zoom = 0;
        h0024.assign(4, 0);
        yaw_right_click = 0; yaw_right_click2 = 0; pitch_right_click = 0;
        distance2 = 0; acceleration_constant = 0;
        time_since_last_keyboard_rotation = 0; time_since_last_mouse_rotation = 0;
        time_since_last_mouse_move = 0; time_since_last_agent_selection = 0;
        time_in_the_map = 0; time_in_the_district = 0;
        yaw_to_go = 0; pitch_to_go = 0; dist_to_go = 0; max_distance2 = 0;
        h0070.assign(2, 0);
        position = camera_pos_to_go = cam_pos_inverted = cam_pos_inverted_to_go = Vec3{};
        look_at_target = look_at_to_go = Vec3{};
        field_of_view = field_of_view2 = -1.0f;
    }

    void GetContext() {
        auto* cam = GW::Context::GetCamera();
        if (!cam) { ResetContext(); return; }
        look_at_agent_id = cam->look_at_agent_id;
        yaw = cam->yaw;
        current_yaw = cam->GetCurrentYaw();
        pitch = cam->pitch;
        camera_zoom = cam->distance;
        max_distance = cam->max_distance;
        yaw_right_click = cam->yaw_right_click;
        yaw_right_click2 = cam->yaw_right_click2;
        pitch_right_click = cam->pitch_right_click;
        distance2 = cam->distance2;
        acceleration_constant = cam->acceleration_constant;
        time_since_last_keyboard_rotation = cam->time_since_last_keyboard_rotation;
        time_since_last_mouse_rotation = cam->time_since_last_mouse_rotation;
        time_since_last_mouse_move = cam->time_since_last_mouse_move;
        time_since_last_agent_selection = cam->time_since_last_agent_selection;
        time_in_the_map = cam->time_in_the_map;
        time_in_the_district = cam->time_in_the_district;
        yaw_to_go = cam->yaw_to_go;
        pitch_to_go = cam->pitch_to_go;
        dist_to_go = cam->dist_to_go;
        max_distance2 = cam->max_distance2;
        field_of_view = cam->field_of_view;
        field_of_view2 = cam->field_of_view2;
        position = {cam->position.x, cam->position.y, cam->position.z};
        camera_pos_to_go = {cam->camera_pos_to_go.x, cam->camera_pos_to_go.y, cam->camera_pos_to_go.z};
        cam_pos_inverted = {cam->cam_pos_inverted.x, cam->cam_pos_inverted.y, cam->cam_pos_inverted.z};
        look_at_target = {cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z};
    }

    void SetYaw(float v)   { GW::game_thread::Enqueue([v] { auto* c = GW::Context::GetCamera(); if(c) c->SetYaw(v); }); }
    void SetPitch(float v) { GW::game_thread::Enqueue([v] { auto* c = GW::Context::GetCamera(); if(c) c->SetPitch(v); }); }
    void SetCameraPos(float x, float y, float z) { GW::game_thread::Enqueue([x,y,z] { auto* c = GW::Context::GetCamera(); if(c) { c->position = GW::Vec3f(x,y,z); } }); }
    void SetLookAtTarget(float x, float y, float z) { GW::game_thread::Enqueue([x,y,z] { auto* c = GW::Context::GetCamera(); if(c) { c->look_at_target = GW::Vec3f(x,y,z); } }); }
    void SetMaxDist(float d) { GW::game_thread::Enqueue([d] { GW::camera::SetMaxDist(d); }); }
    void SetFieldOfView(float f) { GW::game_thread::Enqueue([f] { GW::camera::SetFieldOfView(f); }); }
    void UnlockCam(bool u) { GW::game_thread::Enqueue([u] { GW::camera::UnlockCam(u); }); }
    bool GetCameraUnlock() { return GW::camera::GetCameraUnlock(); }
    void SetFog(bool f) { GW::game_thread::Enqueue([f] { GW::camera::SetFog(f); }); }
    void ForwardMovement(float a, bool tf) { GW::game_thread::Enqueue([a,tf] { GW::camera::ForwardMovement(a, tf); }); }
    void VerticalMovement(float a) { GW::game_thread::Enqueue([a] { GW::camera::VerticalMovement(a); }); }
    void SideMovement(float a) { GW::game_thread::Enqueue([a] { GW::camera::SideMovement(a); }); }
    void RotateMovement(float a) { GW::game_thread::Enqueue([a] { GW::camera::RotateMovement(a); }); }
    py::tuple ComputeCameraPos() { auto p = GW::camera::ComputeCamPos(0); return py::make_tuple(p.x, p.y, p.z); }
    void UpdateCameraPos() { GW::game_thread::Enqueue([] { GW::camera::UpdateCameraPos(); }); }
};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyCamera, m) {
    m.doc() = "Py4GW Camera bindings";

    // ── Vec3 helper ──
    py::class_<PyCamera::Vec3>(m, "Point3D")
        .def(py::init<>())
        .def_readwrite("x", &PyCamera::Vec3::x)
        .def_readwrite("y", &PyCamera::Vec3::y)
        .def_readwrite("z", &PyCamera::Vec3::z);

    // ── PyCamera class (legacy surface) ──
    py::class_<PyCamera>(m, "PyCamera")
        .def(py::init<>())
        .def("GetContext", &PyCamera::GetContext)
        .def_readwrite("look_at_agent_id", &PyCamera::look_at_agent_id)
        .def_readwrite("yaw", &PyCamera::yaw)
        .def_readwrite("pitch", &PyCamera::pitch)
        .def_readwrite("camera_zoom", &PyCamera::camera_zoom)
        .def_readwrite("max_distance", &PyCamera::max_distance)
        .def_readwrite("yaw_right_click", &PyCamera::yaw_right_click)
        .def_readwrite("yaw_right_click2", &PyCamera::yaw_right_click2)
        .def_readwrite("pitch_right_click", &PyCamera::pitch_right_click)
        .def_readwrite("distance2", &PyCamera::distance2)
        .def_readwrite("acceleration_constant", &PyCamera::acceleration_constant)
        .def_readwrite("time_since_last_keyboard_rotation", &PyCamera::time_since_last_keyboard_rotation)
        .def_readwrite("time_since_last_mouse_rotation", &PyCamera::time_since_last_mouse_rotation)
        .def_readwrite("time_since_last_mouse_move", &PyCamera::time_since_last_mouse_move)
        .def_readwrite("time_since_last_agent_selection", &PyCamera::time_since_last_agent_selection)
        .def_readwrite("time_in_the_map", &PyCamera::time_in_the_map)
        .def_readwrite("time_in_the_district", &PyCamera::time_in_the_district)
        .def_readwrite("yaw_to_go", &PyCamera::yaw_to_go)
        .def_readwrite("pitch_to_go", &PyCamera::pitch_to_go)
        .def_readwrite("dist_to_go", &PyCamera::dist_to_go)
        .def_readwrite("max_distance2", &PyCamera::max_distance2)
        .def_readwrite("field_of_view", &PyCamera::field_of_view)
        .def_readwrite("field_of_view2", &PyCamera::field_of_view2)
        .def_readwrite("h0024", &PyCamera::h0024)
        .def_readwrite("h0070", &PyCamera::h0070)
        .def_readwrite("position", &PyCamera::position)
        .def_readwrite("camera_pos_to_go", &PyCamera::camera_pos_to_go)
        .def_readwrite("cam_pos_inverted", &PyCamera::cam_pos_inverted)
        .def_readwrite("cam_pos_inverted_to_go", &PyCamera::cam_pos_inverted_to_go)
        .def_readwrite("look_at_target", &PyCamera::look_at_target)
        .def_readwrite("look_at_to_go", &PyCamera::look_at_to_go)
        .def("SetYaw", &PyCamera::SetYaw, py::arg("_yaw"))
        .def("SetPitch", &PyCamera::SetPitch, py::arg("_pitch"))
        .def("SetMaxDist", &PyCamera::SetMaxDist, py::arg("dist"))
        .def("SetFieldOfView", &PyCamera::SetFieldOfView, py::arg("fov"))
        .def("UnlockCam", &PyCamera::UnlockCam, py::arg("unlock"))
        .def("GetCameraUnlock", &PyCamera::GetCameraUnlock)
        .def("ForwardMovement", &PyCamera::ForwardMovement, py::arg("amount"), py::arg("true_forward"))
        .def("VerticalMovement", &PyCamera::VerticalMovement, py::arg("amount"))
        .def("SideMovement", &PyCamera::SideMovement, py::arg("amount"))
        .def("RotateMovement", &PyCamera::RotateMovement, py::arg("angle"))
        .def("ComputeCameraPos", &PyCamera::ComputeCameraPos)
        .def("UpdateCameraPos", &PyCamera::UpdateCameraPos)
        .def("SetCameraPos", &PyCamera::SetCameraPos, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("SetLookAtTarget", &PyCamera::SetLookAtTarget, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("SetFog", &PyCamera::SetFog, py::arg("fog"));

    // ── Free-function surface (modern) ──
    m.def("forward_movement", [](float a, bool tf) { return GW::camera::ForwardMovement(a, tf); }, py::arg("amount"), py::arg("true_forward") = false);
    m.def("vertical_movement", [](float a) { return GW::camera::VerticalMovement(a); }, py::arg("amount"));
    m.def("rotate_movement", [](float a) { return GW::camera::RotateMovement(a); }, py::arg("angle"));
    m.def("side_movement", [](float a) { return GW::camera::SideMovement(a); }, py::arg("amount"));
    m.def("set_max_dist", [](float d) { return GW::camera::SetMaxDist(d); }, py::arg("dist") = 900.0f);
    m.def("set_field_of_view", [](float f) { return GW::camera::SetFieldOfView(f); }, py::arg("fov"));
    m.def("compute_cam_pos", [](float d) -> py::tuple { auto p = GW::camera::ComputeCamPos(d); return py::make_tuple(p.x, p.y, p.z); }, py::arg("dist") = 0.0f);
    m.def("update_camera_pos", []() { return GW::camera::UpdateCameraPos(); });
    m.def("get_field_of_view", []() -> float { return GW::camera::GetFieldOfView(); });
    m.def("get_yaw", []() -> float { return GW::camera::GetYaw(); });
    m.def("unlock_cam", [](bool f) { return GW::camera::UnlockCam(f); }, py::arg("flag"));
    m.def("get_camera_unlock", []() -> bool { return GW::camera::GetCameraUnlock(); });
    m.def("set_fog", [](bool f) { return GW::camera::SetFog(f); }, py::arg("flag"));
    m.def("get_context_ptr", []() -> uintptr_t { return reinterpret_cast<uintptr_t>(GW::Context::GetCamera()); });
}

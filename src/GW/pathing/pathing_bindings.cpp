#include "base/py_bindings.h"

#include "GW/pathing/pathing.h"

namespace py = pybind11;

using GW::pathing::PathPlanner;

PYBIND11_EMBEDDED_MODULE(PyPathing, m) {
    py::enum_<PathPlanner::Status>(m, "PathStatus")
        .value("Idle", PathPlanner::Status::Idle)
        .value("Pending", PathPlanner::Status::Pending)
        .value("Ready", PathPlanner::Status::Ready)
        .value("Failed", PathPlanner::Status::Failed)
        .export_values();

    py::class_<PathPlanner>(m, "PathPlanner")
        .def(py::init<>())

        .def("plan", &PathPlanner::PlanPath,
            py::arg("start_x"), py::arg("start_y"), py::arg("start_z"),
            py::arg("goal_x"), py::arg("goal_y"), py::arg("goal_z"),
            "Submit a path planning task to the game thread")

        .def("compute_immediate", &PathPlanner::ComputePathImmediate,
            py::arg("start_x"), py::arg("start_y"), py::arg("start_z"),
            py::arg("goal_x"), py::arg("goal_y"), py::arg("goal_z"),
            "Compute the path immediately and return it as a list of (x, y, z) tuples")

        .def("get_status", &PathPlanner::GetStatus,
            "Get current planning status (Idle, Pending, Ready, Failed)")

        .def("is_ready", &PathPlanner::IsReady,
            "Check if the planned path is ready")

        .def("was_successful", &PathPlanner::WasSuccessful,
            "Check if the path planning was successful")

        .def("get_path", &PathPlanner::GetPath,
            py::return_value_policy::reference,
            "Retrieve the calculated path as a list of (x, y, z) tuples")

        .def("reset", &PathPlanner::Reset,
            "Reset the planner to Idle state");
}

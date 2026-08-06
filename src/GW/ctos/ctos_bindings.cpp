#include "base/py_bindings.h"

#include "GW/ctos/ctos.h"

#include <utility>
#include <vector>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyCtoS, m) {
    m.doc() =
        "Generic client-to-server packet sender. Build a packet as a list of "
        "dwords where words[0] is the raw opcode. Sends are queued on the game "
        "thread and dropped when the map or connection is not ready.";

    m.def("SendPacket", [](std::vector<uint32_t> words) -> bool {
        return GW::CToS::QueuePacket(std::move(words));
    }, py::arg("words"),
        "Queue a raw CToS packet. Returns false for an empty or oversized packet; "
        "true means it was queued, not that it reached the server.");
}

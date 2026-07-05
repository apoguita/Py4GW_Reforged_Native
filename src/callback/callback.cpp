#include "base/error_handling.h"

#include "callback/callback.h"

#include "base/CrashHandler.h"
#include "profiler/profiler.h"
#include "system/system.h"

#include <algorithm>

namespace py = pybind11;

namespace PY4GW {

std::mutex& PyCallback::Mutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<PyCallback::Task>& PyCallback::Tasks() {
    static std::vector<Task> tasks;
    return tasks;
}

CallbackId PyCallback::Register(
    const std::string& name,
    Phase phase,
    py::function fn,
    int priority,
    Context context) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();

    // Replace by (name, phase, context), keeping id + order.
    for (auto& t : tasks) {
        if (t.name == name && t.phase == phase && t.context == context) {
            t.fn = std::move(fn);
            t.priority = priority;
            return t.id;
        }
    }

    const CallbackId id = next_id_++;
    tasks.push_back(Task{
        id,
        name,
        phase,
        context,
        priority,
        next_order_++,
        std::move(fn)});
    return id;
}

bool PyCallback::RemoveById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& t) { return t.id == id; });
    if (it == tasks.end()) {
        return false;
    }
    tasks.erase(it, tasks.end());
    return true;
}

bool PyCallback::RemoveByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& t) { return t.name == name; });
    if (it == tasks.end()) {
        return false;
    }
    tasks.erase(it, tasks.end());
    return true;
}

void PyCallback::RemoveAll() {
    std::lock_guard<std::mutex> lock(Mutex());
    Tasks().clear();
}

bool PyCallback::PauseById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (auto& t : Tasks()) {
        if (t.id == id) {
            t.paused = true;
            return true;
        }
    }
    return false;
}

bool PyCallback::ResumeById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (auto& t : Tasks()) {
        if (t.id == id) {
            t.paused = false;
            return true;
        }
    }
    return false;
}

bool PyCallback::IsPaused(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (const auto& t : Tasks()) {
        if (t.id == id) {
            return t.paused;
        }
    }
    return false;
}

bool PyCallback::IsRegistered(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (const auto& t : Tasks()) {
        if (t.id == id) {
            return true;
        }
    }
    return false;
}

// Debug gate (owner): callbacks fire only for phases up to and including
// g_max_enabled_phase; ExecutePhase returns immediately for any later phase (the
// callbacks still register, they just never run). The three phases execute in the
// order PreUpdate -> Data -> Update, and we bring them online one layer at a time
// as each proves stable on injection:
//     PreUpdate -> Data -> Update (ALL ENABLED).
// PreUpdate + Data confirmed stable (2026-07-04) after the ExecutePhase
// snapshot-by-value fix removed the use-after-realloc crash; now all three phases
// are enabled. Compile-time only on purpose - no Python control, because reaching
// Python requires the surface to already inject.
static constexpr PyCallback::Phase g_max_enabled_phase = PyCallback::Phase::Update;

void PyCallback::ExecutePhase(Phase phase, Context context) {
    if (static_cast<uint8_t>(phase) > static_cast<uint8_t>(g_max_enabled_phase)) {
        return;
    }
    // The runtime keeps the GIL released outside python_runtime calls; acquire it
    // up front because the snapshot below copies each py::function (an inc_ref on a
    // Python object requires the GIL).
    py::gil_scoped_acquire gil;

    // Snapshot the matching tasks BY VALUE. A callback may Register/Remove during
    // this phase (which reallocates/erases the Tasks() vector); iterating raw Task*
    // pointers into that vector was a use-after-free - the root cause of the NULL/
    // garbage-callable crashes. Copying each Task (including its py::function, which
    // keeps the callable alive for this frame even if it is later removed) makes the
    // loop immune to any mutation. Lock order is GIL -> Mutex, matching Register
    // (called from Python with the GIL held), so there is no deadlock.
    std::vector<Task> phase_tasks;
    {
        std::lock_guard<std::mutex> lock(Mutex());
        for (const auto& t : Tasks()) {
            if (t.phase == phase && t.context == context && !t.paused) {
                phase_tasks.push_back(t);
            }
        }
    }

    if (phase_tasks.empty()) {
        return;
    }

    std::sort(phase_tasks.begin(), phase_tasks.end(), [](const Task& a, const Task& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        return a.order < b.order;
    });

    const uint64_t frame_id = System::GetTickCount64();

    const char* ctx_name = (context == Context::Draw) ? "Draw" : "Update";
    ctx_name = (context == Context::Main) ? "Main" : ctx_name;
    const char* phase_suffix;
    switch (phase) {
    case Phase::PreUpdate: phase_suffix = "PreUpdate"; break;
    case Phase::Data: phase_suffix = "Data"; break;
    default: phase_suffix = "Update"; break;
    }

    size_t task_index = 0;
    for (const Task& t : phase_tasks) {
        const std::string full_prof_name = std::string(ctx_name) + ".Callback." + phase_suffix + "." + t.name;

        Profiler::Start(full_prof_name);
        // Stamp the crash context so an UNCATCHABLE fault (access violation) inside a
        // callback is attributable in the sidecar. operation carries the phase/context
        // + slot index (from the ExecutePhase args + loop counter) so a fault always
        // identifies where we are; the callback name goes in the detail field.
        const std::string op = std::string(ctx_name) + "." + phase_suffix + "[" +
            std::to_string(static_cast<unsigned>(task_index)) + "/" +
            std::to_string(static_cast<unsigned>(phase_tasks.size())) + "]";
        CrashHandler::SetContext("runtime", "callback", op.c_str(), t.name.c_str());
        // Per-callback isolation (parity with legacy's per-widget `except
        // Exception`): a single callback failing must NOT stop the others in
        // this phase. Catch EVERYTHING. For Python errors use
        // discard_as_unraisable (reports via sys.unraisablehook and cleans up
        // safely) rather than error_already_set::what(), which re-enters Python
        // to format a traceback and is fragile inside this tight loop.
        try {
            t.fn();
        } catch (py::error_already_set& e) {
            e.discard_as_unraisable(t.name.c_str());
        } catch (const std::exception& e) {
            System::Instance().WriteConsoleMessage("PyCallback", MessageType::Error,
                std::string("Callback '") + t.name + "' error: " + e.what());
        } catch (...) {
            System::Instance().WriteConsoleMessage("PyCallback", MessageType::Error,
                std::string("Callback '") + t.name + "' unknown error.");
        }
        Profiler::End(frame_id, full_prof_name);
        ++task_index;
    }
}

std::vector<std::tuple<uint64_t, std::string, int, int, int, uint64_t, bool>> PyCallback::GetCallbackInfo() {
    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<std::tuple<uint64_t, std::string, int, int, int, uint64_t, bool>> out;
    auto& tasks = Tasks();
    out.reserve(tasks.size());
    for (const auto& t : tasks) {
        out.emplace_back(
            t.id,
            t.name,
            static_cast<int>(t.phase),
            static_cast<int>(t.context),
            t.priority,
            t.order,
            t.paused);
    }
    return out;
}

void PyCallback::Clear() {
    std::lock_guard<std::mutex> lock(Mutex());
    Tasks().clear();
}

}  // namespace PY4GW

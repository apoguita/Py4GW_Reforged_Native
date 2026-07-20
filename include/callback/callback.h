#pragma once

#include "base/error_handling.h"

#include "base/py_bindings.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace PY4GW {

using CallbackId = uint64_t;

// Phased Python callback scheduler. Parity port of the legacy PyCallback.
// Standalone class: it uses the Profiler to time each callback (as legacy did),
// but the two are otherwise independent and separately bound.
class PyCallback {
public:
    enum class Phase : uint8_t {
        PreUpdate = 0,
        Data = 1,
        Update = 2
    };

    enum class Context : uint8_t {
        Update = 0,
        Draw = 1,
        Main = 2
    };

    struct Task {
        CallbackId id;
        std::string name;
        Phase phase;
        Context context;
        int priority;
        uint64_t order;  // registration order
        pybind11::function fn;
        bool paused = false;
    };

    static CallbackId Register(
        const std::string& name,
        Phase phase,
        pybind11::function fn,
        int priority = 99,
        Context context = Context::Draw);

    static bool RemoveById(CallbackId id);
    static bool RemoveByName(const std::string& name);
    static void RemoveAll();

    static bool PauseById(CallbackId id);
    static bool ResumeById(CallbackId id);
    static bool IsPaused(CallbackId id);
    static bool IsRegistered(CallbackId id);

    // Runs every registered task for (phase, context) in priority then
    // registration order, timing each through the Profiler.
    static void ExecutePhase(Phase phase, Context context);

    static std::vector<std::tuple<uint64_t, std::string, int, int, int, uint64_t, bool>> GetCallbackInfo();
    static void Clear();

private:
    static std::mutex& Mutex();
    static std::vector<Task>& Tasks();
    static inline CallbackId next_id_ = 1;
    static inline uint64_t next_order_ = 1;
};

}  // namespace PY4GW

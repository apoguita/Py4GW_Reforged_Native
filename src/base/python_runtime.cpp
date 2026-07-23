#include "base/error_handling.h"

#include "base/python_runtime.h"

#include "GW/shared_memory/manager.h"
#include "base/logger.h"
#include "base/process_manager.h"
#include "system/system.h"
#include "base/timer.h"

#include <imgui.h>
#include "base/py_bindings.h"

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
namespace py = pybind11;

namespace PY4GW::python_runtime {

namespace {

py::scoped_interpreter* g_python_runtime = nullptr;
PyThreadState* g_python_thread_state = nullptr;
py::object g_console_scope;

struct DeferredMixedCommand {
    std::function<void()> action;
    int delay_ms = 0;
    PY4GW::Timer timer;
    bool active = false;
};

DeferredMixedCommand g_mixed_deferred;

std::string Narrow(std::wstring_view value) {
    std::string out;
    out.reserve(value.size());
    for (const wchar_t ch : value) {
        out.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return out;
}

py::object GetCallableIfExists(py::object& module, const char* name) {
    if (!module || module.is_none() || !py::hasattr(module, name)) {
        return py::object();
    }

    py::object obj = module.attr(name);
    if (!obj || obj.is_none() || !PyCallable_Check(obj.ptr())) {
        return py::object();
    }
    return obj;
}

std::string LoadPythonScript(const std::string& file_path) {
    std::ifstream script_file(file_path);
    if (!script_file.is_open()) {
        System::Instance().WriteConsoleMessage("Py4GW", MessageType::Error, "Failed to open script file: " + file_path);
        return {};
    }

    std::stringstream buffer;
    buffer << script_file.rdbuf();
    if (buffer.str().empty()) {
        System::Instance().WriteConsoleMessage("Py4GW", MessageType::Error, "Script file is empty: " + file_path);
    }
    return buffer.str();
}

// One independent Python script environment: module, entry points, run state,
// and timer. Two instances exist - the user/console script and the always-on
// widget manager - so both run side by side without sharing state.
class ScriptHost {
public:
    ScriptHost(std::string console_name, std::string display)
        : console_name_(std::move(console_name)), display_(std::move(display)) {}

    bool SetPath(const std::string& path) {
        selected_path_ = path;
        return !selected_path_.empty();
    }
    const std::string& GetPath() const { return selected_path_; }

    ScriptState State() const { return state_; }
    const char* StateLabel() const {
        switch (state_) {
        case ScriptState::Running: return "Running";
        case ScriptState::Paused: return "Paused";
        case ScriptState::Stopped: return "Stopped";
        }
        return "Unknown";
    }
    double ElapsedMs() const { return timer_.getElapsedTime(); }
    bool HasLoaded() const { return !loaded_content_.empty(); }
    py::object& Module() { return module_; }

    void Reset(bool announce) {
        loaded_content_.clear();
        module_ = py::object();
        main_fn_ = py::object();
        update_fn_ = py::object();
        draw_fn_ = py::object();
        state_ = ScriptState::Stopped;
        if (announce) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, "Python environment reset.");
        }
    }

    void MarkStarted() {
        state_ = ScriptState::Running;
        timer_.reset();
    }

    bool Load() {
        if (selected_path_.empty()) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, display_ + " path is empty.");
            return false;
        }

        py::gil_scoped_acquire gil;
        Reset(false);
        loaded_content_ = LoadPythonScript(selected_path_);
        if (loaded_content_.empty()) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, "Failed to load " + LowerDisplay() + ".");
            return false;
        }

        try {
            py::module_ py_compile = py::module_::import("py_compile");
            try {
                py_compile.attr("compile")(selected_path_, py::none(), py::none(), py::bool_(true));
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " compiled successfully.");
            } catch (const py::error_already_set& error) {
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Python syntax error: ") + error.what());
                state_ = ScriptState::Stopped;
                return false;
            }

            py::object types_module = py::module_::import("types");
            module_ = types_module.attr("ModuleType")("py4gw_script_module");
            module_.attr("__file__") = py::str(selected_path_);
            try {
                py::exec(loaded_content_, module_.attr("__dict__"));
            } catch (const py::error_already_set& error) {
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Python error: ") + error.what());
                state_ = ScriptState::Stopped;
                return false;
            }

            main_fn_ = GetCallableIfExists(module_, "main");
            update_fn_ = GetCallableIfExists(module_, "update");
            draw_fn_ = GetCallableIfExists(module_, "draw");

            if (draw_fn_ && !draw_fn_.is_none()) {
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, "draw() function found.");
            }
            if (update_fn_ && !update_fn_.is_none()) {
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, "update() function found.");
            }
            if (main_fn_ && !main_fn_.is_none()) {
                System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, "main() function found.");
            }

            if ((main_fn_ && !main_fn_.is_none()) ||
                (update_fn_ && !update_fn_.is_none()) ||
                (draw_fn_ && !draw_fn_.is_none())) {
                return true;
            }

            state_ = ScriptState::Stopped;
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, "No main()/update()/draw() function found in the " + LowerDisplay() + ".");
        } catch (const py::error_already_set& error) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Python error: ") + error.what());
            state_ = ScriptState::Stopped;
        } catch (const std::exception& error) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Standard exception: ") + error.what());
            state_ = ScriptState::Stopped;
        } catch (...) {
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, "Unknown error occurred during script execution.");
            state_ = ScriptState::Stopped;
        }
        return false;
    }

    bool Start() {
        if (state_ == ScriptState::Paused) {
            state_ = ScriptState::Running;
            timer_.Resume();
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " resumed.");
            return true;
        }
        if (!Load()) {
            {
                // Reset() clears module_/*_fn_, decref-ing the module Load() just
                // exec'd. The console UI runs off-GIL, so this must hold the GIL like
                // Load()/Stop() do - otherwise the off-GIL decref crashes the game
                // thread. Only a script with no main()/update()/draw() reaches here.
                py::gil_scoped_acquire gil;
                Reset(false);
            }
            state_ = ScriptState::Stopped;
            timer_.stop();
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " stopped.");
            return false;
        }
        MarkStarted();
        System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " started.");
        return true;
    }

    bool Run() {
        if (state_ == ScriptState::Stopped && Load()) {
            MarkStarted();
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " started.");
            return true;
        }
        return false;
    }

    void Stop() {
        py::gil_scoped_acquire gil;
        Reset(true);
        state_ = ScriptState::Stopped;
        timer_.stop();
        System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " stopped.");
    }

    bool Pause() {
        if (state_ != ScriptState::Running) {
            return false;
        }
        state_ = ScriptState::Paused;
        timer_.Pause();
        System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " paused.");
        return true;
    }

    bool Resume() {
        if (state_ != ScriptState::Paused) {
            return false;
        }
        state_ = ScriptState::Running;
        timer_.Resume();
        System::Instance().WriteConsoleMessage(console_name_, MessageType::Notice, display_ + " resumed.");
        return true;
    }

    void ExecuteUpdate() {
        if (state_ != ScriptState::Running || loaded_content_.empty()) {
            return;
        }
        py::gil_scoped_acquire gil;
        CallSafe(update_fn_, "update()");
    }

    void ExecuteDraw() {
        if (state_ != ScriptState::Running || loaded_content_.empty()) {
            return;
        }
        py::gil_scoped_acquire gil;
        // draw() and main() both run every draw frame, ungated from each other
        // (equal priority - NOT a draw()-first fallback). update() runs only in
        // ExecuteUpdate on the update loop. CallSafe no-ops on an absent function.
        CallSafe(draw_fn_, "draw()");
        CallSafe(main_fn_, "main()");
    }

private:
    std::string LowerDisplay() const {
        std::string out = display_;
        for (char& ch : out) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
        return out;
    }

    bool CallSafe(py::object& fn, const char* label) {
        if (!fn || fn.is_none()) {
            return false;
        }
        try {
            fn();
            return true;
        } catch (const py::error_already_set& error) {
            state_ = ScriptState::Stopped;
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Python error (") + label + "): " + error.what());
        } catch (const std::exception& error) {
            state_ = ScriptState::Stopped;
            System::Instance().WriteConsoleMessage(console_name_, MessageType::Error, std::string("Runtime error (") + label + "): " + error.what());
        }
        return false;
    }

    std::string console_name_;
    std::string display_;
    ScriptState state_ = ScriptState::Stopped;
    std::string selected_path_;
    std::string loaded_content_;
    py::object module_;
    py::object main_fn_;
    py::object update_fn_;
    py::object draw_fn_;
    PY4GW::Timer timer_;
};

ScriptHost g_user_host("Py4GW", "Script");
ScriptHost g_widget_host("WidgetManager", "Widget manager");

void ScheduleDeferredAction(std::function<void()> fn, int delay_ms) {
    g_mixed_deferred.action = std::move(fn);
    g_mixed_deferred.delay_ms = delay_ms;
    g_mixed_deferred.timer.reset();
    g_mixed_deferred.timer.start();
    g_mixed_deferred.active = true;
}

PYBIND11_EMBEDDED_MODULE(Py4GW, m) {
    m.doc() = "Embedded Py4GW runtime module.";
    m.def("version", []() { return "0.1.0"; });

    py::module_ shared_memory = m.def_submodule("SharedMemory", "Shared memory publisher bindings");
    shared_memory.def("is_ready", []() {
        return GW::shared_memory::RuntimeManager().IsValid();
    });
    shared_memory.def("get_name", []() {
        const auto& name = GW::shared_memory::RuntimeManager().Name();
        return Narrow(name);
    });
    shared_memory.def("get_size", []() {
        return GW::shared_memory::RuntimeManager().Size();
    });
    shared_memory.def("get_sequence", []() {
        const auto* header = GW::shared_memory::RuntimeManager().Header();
        return header ? header->sequence : 0u;
    });
}

}

bool Initialize() {
    auto& logger = Logger::Instance();
    logger.LogInfo("[python] Initialize: begin.");

    // Pre-init environment report: which python DLL is actually mapped into
    // the game process, and what the interpreter will see for path config.
    
    logger.LogInfo(std::string("[python] Py_IsInitialized (pre): ") + std::to_string(Py_IsInitialized()));

    try {
        // Explicit PyConfig: on a path-configuration failure CPython returns a
        // PyStatus that pybind11 converts into std::runtime_error with the real
        // reason, instead of Py_Initialize() calling abort() (0xc0000409).
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.parse_argv = 0;
        config.install_signal_handlers = 0;
        logger.LogInfo("[python] Step 1: creating interpreter (Py_InitializeFromConfig).");
        g_python_runtime = new py::scoped_interpreter(&config);
        logger.LogInfo("[python] Step 1 OK: interpreter created.");

        // Scope every py:: object so it is destroyed while the GIL is still
        // held. Anything alive past PyEval_SaveThread() below dec_refs
        // without the GIL - pybind11 (Debug builds) turns that into a throw
        // from a destructor, i.e. std::terminate/abort with no log output.
        {
        logger.LogInfo("[python] Step 2: importing sys.");
        py::module_ sys = py::module_::import("sys");
        logger.LogInfo(std::string("[python] Step 2 OK. sys.prefix: ") + py::cast<std::string>(sys.attr("prefix")));

        logger.LogInfo("[python] Step 3: configuring sys.path.");
        const auto root = process_manager::GetModuleDirectory();
        logger.LogInfo(std::string("[python] module directory: ") + root.string());
        if (!root.empty()) {
            sys.attr("path").attr("insert")(0, root.string());
            sys.attr("path").attr("insert")(0, (root / "scripts").string());

            // The injected process inherits the game's working directory, but widgets
            // and scripts open payload files (Widgets/, Config/, offsets/, ...) by paths
            // relative to the DLL. Point the working directory at the module directory so
            // those relative reads resolve against the payload, not the game folder.
            if (::SetCurrentDirectoryW(root.wstring().c_str())) {
                logger.LogInfo(std::string("[python] working directory set to module dir: ") + root.string());
            } else {
                logger.LogError("[python] failed to set working directory to module dir; relative file reads may fail.");
            }
        }
        logger.LogInfo("[python] Step 3 OK: sys.path configured.");

        static constexpr const char* kEmbeddedModules[] = {
            "Py4GW",
            "PySystem",
            "PySettings",
            "PyJson",
            "PyProfiler",
            "PyCallback",
            "PyListeners",
            "PyAgent",
            "PyAgentRecolor",
            "PyCamera",
            "PyChat",
            "PyChatCommands",
            "PyAgentEvents",
            "PyEffects",
            "PyFriendList",
            "PyGameThread",
            "PyGuild",
            "PyItem",
            "PyInventory",
            "PyMap",
            "PyMerchant",
            "PyNameObfuscator",
            "PyPacketSniffer",
            "PyParty",
            "PyPathing",
            "PyPing",
            "PyPlayer",
            "PyQuest",
            "PyRender",
            "PyScanner",
            "PySkill",
            "PySkillbar",
            "PyTrade",
            "PyUIManager",
            "PyTexture",
            "PyDialog",
            "PyOverlay",
            "PyDXOverlay",
            "PyWorldRender",
            "PyParticles",
            "PyKeystroke",
            "PyMouse",
            "PyImGui",
        };
        logger.LogInfo("[python] Step 4: importing embedded modules.");
        for (const char* module_name : kEmbeddedModules) {
            logger.LogInfo(std::string("[python] importing ") + module_name);
            py::module_::import(module_name);
        }
        logger.LogInfo("[python] Step 4 OK: all embedded modules imported.");

        logger.LogInfo("[python] Step 5: importing __main__.");
        g_console_scope = py::module_::import("__main__");
        logger.LogInfo("[python] Step 5 OK.");
        }  // All local py:: objects released here, GIL still held.

        logger.LogInfo("[python] Step 6: releasing GIL (PyEval_SaveThread).");
        g_python_thread_state = PyEval_SaveThread();
        logger.LogInfo("[python] Step 6 OK: Python runtime initialized.");
        return true;
    } catch (py::error_already_set& error) {
        // Log while the interpreter is still alive; the exception object must
        // be discarded before the interpreter is torn down below.
        logger.LogError(std::string("Python initialization failed (python error): ") + error.what());
        error.discard_as_unraisable("Py4GW python_runtime::Initialize");
    } catch (const std::exception& error) {
        logger.LogError(std::string("Python initialization failed: ") + error.what());
    } catch (...) {
        logger.LogError("Python initialization failed: unknown exception.");
    }

    delete g_python_runtime;
    g_python_runtime = nullptr;
    return false;
}

void Shutdown() {
    if (!g_python_runtime) {
        return;
    }
    if (g_python_thread_state != nullptr) {
        PyEval_RestoreThread(g_python_thread_state);
        g_python_thread_state = nullptr;
    }
    g_user_host.Reset(false);
    g_widget_host.Reset(false);
    g_console_scope = py::object();
    delete g_python_runtime;
    g_python_runtime = nullptr;
}

void ExecutePythonUpdate() {
    g_user_host.ExecuteUpdate();
}

void ExecutePythonDraw() {
    g_user_host.ExecuteDraw();
}

// Widget manager: an always-on second script host (Py4GW_widget_manager.py).
bool StartWidgetManager() {
    g_widget_host.SetPath((process_manager::GetModuleDirectory() / "Py4GW_widget_manager.py").string());
    return g_widget_host.Run();
}

void StopWidgetManager() {
    g_widget_host.Stop();
}

void ExecuteWidgetManagerUpdate() {
    g_widget_host.ExecuteUpdate();
}

void ExecuteWidgetManagerDraw() {
    g_widget_host.ExecuteDraw();
}

ScriptState GetWidgetManagerState() {
    return g_widget_host.State();
}

std::string GetWidgetManagerStatus() {
    return g_widget_host.StateLabel();
}

void ProcessDeferredActions() {
    if (g_mixed_deferred.active && g_mixed_deferred.timer.hasElapsed(g_mixed_deferred.delay_ms)) {
        if (g_mixed_deferred.action) {
            g_mixed_deferred.action();
        }
        g_mixed_deferred.active = false;
    }
}

bool SetSelectedScriptPath(const std::string& path) {
    return g_user_host.SetPath(path);
}

const std::string& GetSelectedScriptPath() {
    return g_user_host.GetPath();
}

bool LoadSelectedScript() {
    return g_user_host.Load();
}

bool StartSelectedScript() {
    return g_user_host.Start();
}

bool RunScript() {
    return g_user_host.Run();
}

void StopScript() {
    g_user_host.Stop();
}

bool PauseScript() {
    return g_user_host.Pause();
}

bool ResumeScript() {
    return g_user_host.Resume();
}

std::string GetScriptStatus() {
    return g_user_host.StateLabel();
}

double GetScriptElapsedMilliseconds() {
    return g_user_host.ElapsedMs();
}

void DeferLoadAndRun(const std::string& path, int delay_ms) {
    ScheduleDeferredAction([path]() {
        g_user_host.SetPath(path);
        if (g_user_host.Load()) {
            g_user_host.MarkStarted();
            System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Deferred: script loaded and started.");
        }
    }, delay_ms);
}

void DeferStopLoadAndRun(const std::string& path, int delay_ms) {
    ScheduleDeferredAction([path]() {
        g_user_host.Stop();
        g_user_host.SetPath(path);
        if (g_user_host.Load()) {
            g_user_host.MarkStarted();
            System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Deferred: stopped, loaded and started.");
        }
    }, delay_ms);
}

void DeferStopAndRun(int delay_ms) {
    ScheduleDeferredAction([]() {
        StopScript();
        RunScript();
        System::Instance().WriteConsoleMessage("Py4GW", MessageType::Notice, "Deferred: stopped and restarted.");
    }, delay_ms);
}

bool ExecuteCommand(const std::string& command) {
    py::gil_scoped_acquire gil;
    try {
        py::object scope = g_user_host.Module();
        if (!scope || scope.is_none()) {
            if (!g_console_scope || g_console_scope.is_none()) {
                g_console_scope = py::module_::import("__main__");
            }
            scope = g_console_scope;
        }

        py::object result = py::eval(command, scope.attr("__dict__"));
        System::Instance().WriteConsoleMessage("Python", MessageType::Info, ">>> " + command);
        if (!result.is_none()) {
            System::Instance().WriteConsoleMessage("Python", MessageType::Info, py::str(result).cast<std::string>());
        }
        return true;
    } catch (const py::error_already_set& error) {
        System::Instance().WriteConsoleMessage("Python", MessageType::Error, std::string("Error executing command: ") + command + "\n" + error.what());
    } catch (const std::exception& error) {
        System::Instance().WriteConsoleMessage("Python", MessageType::Error, std::string("Standard error executing command: ") + command + "\n" + error.what());
    } catch (...) {
        System::Instance().WriteConsoleMessage("Python", MessageType::Error, "Unknown error executing command: " + command);
    }
    return false;
}

ScriptState GetScriptState() {
    return g_user_host.State();
}

const char* GetScriptStateLabel() {
    return g_user_host.StateLabel();
}

bool HasLoadedScript() {
    return g_user_host.HasLoaded();
}

}  // namespace PY4GW::python_runtime

# Runtime Lifecycle

## Main Entry Points

The exported C API is declared in `include/Py4GW.h`:

- `Py4GW_Initialize()`
- `Py4GW_Shutdown()`
- `Py4GW_RequestShutdown()`

It also declares:

- `py4gw::RuntimeThread(LPVOID)`

## DLL Attachment

`src/dllmain.cpp` is intentionally minimal:

1. On `DLL_PROCESS_ATTACH`
   - disables thread library notifications
   - stores the module handle in `process_manager`
   - creates a dedicated runtime thread
2. On `DLL_PROCESS_DETACH`
   - calls `Py4GW_Shutdown()` only when `reserved == nullptr`
   - terminates the crash handler after runtime shutdown

This keeps heavy work out of `DllMain`, which is the correct shape for an injected DLL.

## Runtime Ownership Model

`src/Py4GW.cpp` owns the runtime state machine.

Internal globals:

- `g_runtime_mutex`
- `g_running`
- `g_shutdown_requested`

The mutex serializes initialize/shutdown transitions. There is no broader fine-grained subsystem lock hierarchy here; lifecycle is treated as a coarse, process-wide state transition.

## Initialization Sequence

`Py4GW_Initialize()` does the following in order:

1. Acquire the runtime mutex
2. Return early if already running
3. Configure the logger output path
4. Clear the shutdown-request flag
5. Initialize `Scanner`
6. Initialize `Patterns`
7. Initialize the crash handler
8. Initialize the embedded Python runtime
9. Initialize the Guild Wars runtime layer
10. Register ImGui shutdown callback
11. Register Guild Wars render/reset callbacks
12. Mark the runtime as running
13. Log success

The crash handler is installed at step 7, immediately after `Patterns`. It needs the scanner and
the pattern catalogue for its Guild Wars anchor path (`InstallPathC`) and nothing else, so this is
the earliest point it can be installed. It used to be installed after step 9, which meant a failure
during Python bring-up or any GW module init produced no crash report of any kind, not even an
injection-log line. That mattered in practice: `GW::StoC::Initialize` defers its real init onto the
game thread (`stoc.cpp`), so a failed invariant there kills the process asynchronously and takes the
whole init sequence with it. See `11-error-handling-and-panic.md` ("Keep crash capture process-wide
and initialized for as much of process lifetime as practical"). A panic before step 7 is still only
visible in `Py4GW_injection_log.txt`.

Failure handling:

- Python init failure aborts immediately
- Guild Wars init failure logs and triggers Python shutdown rollback

## Steady-State Runtime Loop

`RuntimeThread()` calls `Py4GW_Initialize()` and then loops until shutdown is requested.

Per-iteration behavior:

- `UpdateLoopStep()` runs the Python update path and then sleeps for 10 ms

## Render Callback Flow

The hook layer invokes:

- `DrawLoop(IDirect3DDevice9* device)`
- `OnReset(IDirect3DDevice9* device)`

`DrawLoop` checks `g_running` and delegates to `imgui_manager::Render`.

`OnReset` calls:

- `imgui_manager::InvalidateDeviceObjects()`

## Shutdown Request Path

Shutdown can be triggered by:

- explicit external call to `Py4GW_RequestShutdown()`
- ImGui shutdown button via callback

`BeginShutdown()` first stops any running script and then sets the shutdown flag. Real teardown is deferred to the runtime thread.

## Shutdown Sequence

`Py4GW_Shutdown()` does the following:

1. Acquire runtime mutex
2. Return early if not running
3. Stop any running script
4. Remove render/reset callbacks from the GW layer
5. Shut down ImGui manager
6. Shut down Guild Wars runtime layer
7. Shut down Python runtime
8. Clear running and shutdown-request flags
9. Log completion

Inside `gw::Shutdown()`, the current order is:

1. shut down `render`
2. shut down `camera`
3. disable `memory_patcher`
4. shut down `game_thread`

That order is intentional:

- remove frame-entry activity before destroying render-owned synchronization state
- let `camera` restore its patches before the global memory patcher is disabled
- shut down `game_thread` last so manager-level teardown does not lose the shared callback infrastructure too early

## Shutdown Contract

Shutdown must be treated as a strict state transition, not a best-effort sequence.

Required rules:

- mark shutdown intent before destructive teardown starts
- stop new work before waiting on old work
- clear callbacks before disabling hooks if those callbacks can re-enter project code
- disable hooks before removing them
- wait for in-flight hook activity to drain before deleting synchronization primitives or clearing function pointers
- destroy crash capture last so teardown crashes still emit artifacts

This is especially important for injected render hooks. A short sleep is not a correctness boundary.

## Race-Avoidance Rule

Modules that own callbacks or detours must make late arrivals harmless.

Preferred pattern:

- `g_shutting_down` style gate for the module
- clear callbacks at shutdown start
- no-op late hook bodies when shutdown is active
- explicit in-flight counters for module-owned hook paths when global hook counts are too broad

If a module deletes a critical section, clears a trampoline, or nulls a function pointer while a detour may still execute, the module is not shutdown-safe.

## Thread Exit

After shutdown is requested, `RuntimeThread()`:

1. sleeps 50 ms to let the requesting frame unwind
2. calls `Py4GW_Shutdown()`
3. unloads the module with `FreeLibraryAndExitThread()`

That final unload depends on `process_manager::GetModuleHandle()` having been set during process attach.

## Lifecycle Constraints

Current lifecycle assumptions:

- single injected runtime per process
- no restart while partially initialized
- callbacks are only valid while `g_running == true`
- the runtime thread is the owner of the final unload

## Notable Design Tradeoffs

### Good

- Heavy initialization is outside `DllMain`
- Initialization rollback is at least partially present
- Shutdown is centralized
- crash capture survives until final process detach teardown

### Thin Areas

- There is no state enum, only booleans
- Partial initialization states are not deeply modeled
- No explicit diagnostics are exposed to tell which subsystem failed besides logs

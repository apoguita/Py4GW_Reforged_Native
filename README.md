# Py4GW Reforged Native

Py4GW Reforged Native is the C++ backend for Py4GW Reforged. It is a Windows
only, 32-bit DLL (`Py4GW.dll`) injected into the Guild Wars client. It is a
ground-up rework of the original Py4GW backend and replaces the retired
C++ project at https://github.com/apoguita/Py4GW_cpp_files.

Inside the game process it:

- runs its own runtime thread,
- hooks the Guild Wars Direct3D 9 render pipeline,
- hosts an embedded CPython interpreter (via pybind11),
- renders a Dear ImGui overlay and runs user Python scripts.

## The two projects

- Py4GW_Reforged_Native (this repo) - builds `Py4GW.dll`, the injected backend.
- Py4GW_Reforged - the Python library that runs inside that DLL:
  https://github.com/apoguita/Py4GW_Reforged

## Requirements

- Windows.
- A 32-bit toolchain (32-bit is mandatory; the build fails otherwise).
- CMake and Visual Studio 2022.

## Build

Using the CMake presets (output goes to ./build):

    cmake --preset vs2022-win32
    cmake --build --preset vs2022-win32-relwithdebinfo

Or plainly:

    cmake -S . -B build -A Win32
    cmake --build build --config RelWithDebInfo

The resulting `Py4GW.dll` is written to the repo root, next to the runtime
payload directories (`fonts/`, `scripts/`, `offsets/`) it loads at startup, so
no copy step is needed.

## Contributing

Contributions are welcome:

1. Fork the repository.
2. Create a branch for your feature or fix.
3. Commit your changes and push the branch.
4. Open a pull request for review.

------------------------------------------------------------------------------

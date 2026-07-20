#pragma once

#include "base/error_handling.h"

// Single include for every Python binding translation unit.
//
// Include THIS instead of picking pybind11 headers individually. The reason is
// not tidiness - it is correctness:
//
//   pybind11/stl.h is what teaches pybind how to convert between Python
//   sequences and std::vector / std::map / etc. That conversion is a template,
//   so each .cpp that mentions std::vector<T> at the Python boundary emits its
//   own copy. A .cpp that includes stl.h emits the real converter; a .cpp that
//   does NOT emits the fallback "opaque C++ object" version instead. Both carry
//   the same symbol name, so at link time the linker - which is required to
//   assume duplicate symbols are identical, and does not check - keeps one
//   arbitrarily and discards the other.
//
//   The result is that ONE file missing this include silently breaks list
//   conversion for the ENTIRE DLL, in unrelated files, with no compiler warning
//   and no linker error. That is exactly what happened: camera_bindings.cpp
//   exposed std::vector<uint32_t> via def_readwrite without stl.h, which broke
//   crafter_buy_item / collector_buy_item over in the merchant bindings.
//
// So: one include, no per-file judgement calls, nothing to remember.

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/eval.h>

#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>

namespace gw {

template <typename T>
class GwArray {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() { return buffer; }
    const_iterator begin() const { return buffer; }
    iterator end() { return buffer + size_value; }
    const_iterator end() const { return buffer + size_value; }

    T& at(size_t index) {
        PY4GW_ASSERT(buffer && index < size_value);
        return buffer[index];
    }

    const T& at(size_t index) const {
        PY4GW_ASSERT(buffer && index < size_value);
        return buffer[index];
    }

    T& operator[](uint32_t index) {
        return at(index);
    }

    const T& operator[](uint32_t index) const {
        return at(index);
    }

    bool valid() const {
        return buffer != nullptr;
    }

    void clear() {
        size_value = 0;
    }

    uint32_t size() const {
        return size_value;
    }

    uint32_t capacity() const {
        return capacity_value;
    }

    T* buffer = nullptr;
    uint32_t capacity_value = 0;
    uint32_t size_value = 0;
    uint32_t param = 0;
};

static_assert(sizeof(GwArray<uint32_t>) == 0x10, "GwArray has incorrect size");

}  // namespace gw

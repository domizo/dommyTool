#pragma once

// ONLY for single-translation-unit standalone fixtures. Replacing the global
// allocator in the multi-source core test executable would affect other suites.
#include <cstddef>
#include <cstdlib>
#include <new>

namespace fault {
bool armed = false;
std::size_t selected = 0U, allocations = 0U;
bool triggered = false;

inline void arm(std::size_t allocation)
{
    selected = allocation;
    allocations = 0U;
    triggered = false;
    armed = true;
}
} // namespace fault

void* operator new(std::size_t size)
{
    if (fault::armed && ++fault::allocations == fault::selected) {
        fault::triggered = true;
        throw std::bad_alloc();
    }
    void* result = std::malloc(size ? size : 1U);
    if (!result) throw std::bad_alloc();
    return result;
}

void operator delete(void* value) noexcept { std::free(value); }

#include <cassert>
#include <cstdlib>
#include <exception>
#include <new>
#include <utility>

#include "common/utility/DMayaEntryPointGuard.h"

static bool denyAllocation = false;
static int constructorFault = 0, liveObjects = 0;
void* operator new(std::size_t size) {
    if (denyAllocation) throw std::bad_alloc();
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }

static void construct() {
    if (constructorFault == 1) throw std::bad_alloc();
    if (constructorFault == 2) throw 42;
    ++liveObjects;
}
struct MObject {
    int value;
    explicit MObject(int input = 0) : value(input) {}
};
// The real Arnold reporting/factory bodies are included below. Only the C API
// calls are doubled; no Arnold SDK or renderer is exercised here.
struct AtNode {};
struct AtString { explicit AtString(const char*) {} };
inline void AiNodeSetByte(AtNode*, const AtString&, int) {}
static bool arnoldLoggerFault = false;
inline void AiMsgError(const char*, ...) { if (arnoldLoggerFault) throw 7; }
template <typename Function>
auto unguardedFactory(const char*, Function&& function) -> decltype(function()) {
    return function();
}
struct Factory {
    const char* name;
    void* (*create)();
    void (*destroy)(void*);
};
#include "host_factories.inc"

int main() {
    for (const Factory& factory : factories) {
        void* object = factory.create();
        assert(object != nullptr && liveObjects == 1);
        factory.destroy(object);
        assert(liveObjects == 0);
        denyAllocation = true;
        assert(factory.create() == nullptr);
        denyAllocation = false;
        assert(liveObjects == 0);
        for (int fault = 1; fault <= 2; ++fault) {
            constructorFault = fault;
            assert(factory.create() == nullptr && liveObjects == 0);
        }
        constructorFault = 0;
        object = factory.create();
        assert(object != nullptr && liveObjects == 1);
        factory.destroy(object);
        assert(liveObjects == 0);
    }
    int reported = 0;
    int* pointer = dommy::guardPointerFactory([]() -> int* { throw 42; },
        [&](const std::exception_ptr&) { ++reported; throw 7; });
    assert(pointer == nullptr && reported == 1);
    // Exercise the actual noexcept Arnold reporter, not just a throwing
    // reporter passed directly to the generic pointer-factory guard.
    arnoldLoggerFault = true;
    pointer = dommy::arnold::createObject("reporting fault", []() -> int* {
        throw std::bad_alloc();
    });
    assert(pointer == nullptr);
    pointer = dommy::arnold::createObject("unknown reporting fault", []() -> int* {
        throw 42;
    });
    assert(pointer == nullptr);
    arnoldLoggerFault = false;
}

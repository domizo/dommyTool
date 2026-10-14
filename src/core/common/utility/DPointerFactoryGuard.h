#ifndef DOMMY_DPOINTER_FACTORY_GUARD_H
#define DOMMY_DPOINTER_FACTORY_GUARD_H

#include <exception>
#include <type_traits>
#include <utility>

namespace dommy {

// Invoke in the caller's access context: private/protected constructors remain
// accessible from a lambda created in that class's factory. A successful raw
// pointer keeps the host's existing ownership contract.
template <typename Factory, typename Reporter>
auto guardPointerFactory(Factory&& factory, Reporter&& reporter) noexcept
    -> decltype(factory())
{
    static_assert(std::is_pointer<decltype(factory())>::value,
                  "Host factories must return a pointer.");
    try {
        return std::forward<Factory>(factory)();
    } catch (...) {
        try {
            std::forward<Reporter>(reporter)(std::current_exception());
        } catch (...) {
            // Reporting must not turn a failed allocation into another escape.
        }
        return nullptr;
    }
}

} // namespace dommy

#endif // DOMMY_DPOINTER_FACTORY_GUARD_H

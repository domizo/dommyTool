#pragma once

#include <memory>

// Preserve the legacy global name while delegating ownership, reference
// counting, exception safety, and thread-safe control-block updates to the
// C++ standard library.
template <typename T>
using shared_ptr = std::shared_ptr<T>;

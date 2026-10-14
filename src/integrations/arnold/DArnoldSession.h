#pragma once

#include <ai.h>

namespace dommy {
namespace arnold {

// AiBegin/AiEnd manage process-global Arnold state. Callers must also declare
// kGloballySerialize when the session is opened from Maya DG evaluation.
class Session {
  public:
    Session() { AiBegin(); }
    ~Session() { AiEnd(); }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

} // namespace arnold
} // namespace dommy


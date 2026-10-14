#pragma once

#include <maya/MStatus.h>

namespace dommy {
namespace maya {

// Only for handles acquired from a plug, never for borrowed MDataBlock handles.
// Declare after any MDGContextGuard so release occurs in the acquisition context.
template <typename Plug, typename Handle>
class PlugHandleOwner final
{
public:
    PlugHandleOwner() : acquired_(false) {}
    PlugHandleOwner(const PlugHandleOwner&) = delete;
    PlugHandleOwner& operator=(const PlugHandleOwner&) = delete;

    ~PlugHandleOwner() noexcept { reset(); }

    MStatus read(const Plug& plug)
    {
        reset();
        if (plug.isNull()) return MS::kFailure;
        plug_ = plug;
        const MStatus status = plug_.getValue(handle_);
        acquired_ = status == MS::kSuccess;
        return status;
    }

    Handle& handle() { return handle_; }
    const Handle& handle() const { return handle_; }

private:
    void reset() noexcept
    {
        if (!acquired_) return;
        acquired_ = false;
        // Maya APIs are not expected to throw; never let a host exception
        // escape cleanup during stack unwinding.
        try { plug_.destructHandle(handle_); } catch (...) {}
    }

    Plug plug_;
    Handle handle_;
    bool acquired_;
};

} // namespace maya
} // namespace dommy

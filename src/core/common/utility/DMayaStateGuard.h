#ifndef DOMMY_DMAYA_STATE_GUARD_H
#define DOMMY_DMAYA_STATE_GUARD_H

#include <maya/MAnimControl.h>
#include <maya/MComputation.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

namespace dommy {
namespace maya {

class ScopedCurrentTimeRestore final
{
public:
    ScopedCurrentTimeRestore()
        : originalTime_(MAnimControl::currentTime())
        , restorePending_(true)
    {
    }

    ~ScopedCurrentTimeRestore() noexcept
    {
        restore();
    }

    const MTime& originalTime() const noexcept
    {
        return originalTime_;
    }

    MStatus restore() noexcept
    {
        if (!restorePending_) {
            return MS::kSuccess;
        }

        try {
            const MStatus status = MGlobal::viewFrame(originalTime_);
            if (status == MS::kSuccess) {
                restorePending_ = false;
            }
            return status;
        } catch (...) {
            return MS::kFailure;
        }
    }

private:
    ScopedCurrentTimeRestore(const ScopedCurrentTimeRestore&);
    ScopedCurrentTimeRestore& operator=(const ScopedCurrentTimeRestore&);

    MTime originalTime_;
    bool restorePending_;
};


class ScopedComputation final
{
public:
    ScopedComputation()
        : active_(false)
    {
        computation_.beginComputation();
        active_ = true;
    }

    ~ScopedComputation() noexcept
    {
        finish();
    }

    bool isInterruptRequested()
    {
        return active_ && computation_.isInterruptRequested();
    }

    void finish() noexcept
    {
        if (!active_) {
            return;
        }

        try {
            computation_.endComputation();
        } catch (...) {
        }
        active_ = false;
    }

private:
    ScopedComputation(const ScopedComputation&);
    ScopedComputation& operator=(const ScopedComputation&);

    MComputation computation_;
    bool active_;
};

} // namespace maya
} // namespace dommy

#endif // DOMMY_DMAYA_STATE_GUARD_H

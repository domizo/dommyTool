#include "common/utility/DComputeGuard.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DDeformerGuard.h"
#include "common/utility/DMayaStateGuard.h"

MStatus compileComputeExceptionBoundary(bool fail) try
{
    if (fail) {
        throw 1;
    }
    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("compileComputeExceptionBoundary")

MStatus compileCommandExceptionBoundary(bool fail) try
{
    if (fail) {
        throw 1;
    }
    return MS::kSuccess;
}
DOMMY_COMMAND_CATCH("compileCommandExceptionBoundary")

MStatus compileDeformerExceptionBoundary(bool fail) try
{
    if (fail) {
        throw 1;
    }
    return MS::kSuccess;
}
DOMMY_DEFORMER_CATCH("compileDeformerExceptionBoundary")

bool compileMayaStateGuards()
{
    MGlobal::viewFrame(MTime(12.0));
    {
        dommy::maya::ScopedCurrentTimeRestore restoreTime;
        MGlobal::viewFrame(MTime(24.0));
    }
    if (MAnimControl::currentTime().value() != 12.0) {
        return false;
    }

    const int beginCount = MComputation::beginCount();
    const int endCount = MComputation::endCount();
    {
        dommy::maya::ScopedComputation computation;
        if (computation.isInterruptRequested()) {
            return false;
        }
    }

    return MComputation::beginCount() == beginCount + 1 &&
           MComputation::endCount() == endCount + 1;
}

#include "modules/volume/VDB/Common/DVDBComputeGuard.h"

MStatus compileVdbComputeExceptionBoundary(bool fail) try
{
    if (fail) {
        throw openvdb::Exception();
    }
    return MS::kSuccess;
}
DOMMY_VDB_COMPUTE_CATCH("compileVdbComputeExceptionBoundary")

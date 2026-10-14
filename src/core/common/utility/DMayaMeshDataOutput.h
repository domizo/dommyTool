#pragma once

#include <maya/MFnMeshData.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

namespace dommy {
namespace maya {

// A null owner makes MFnMesh::create add a transform to the scene's DAG.
// Data-only compute/query callers must stop before constructing geometry.
inline MStatus createMeshDataObject(MObject& output) {
    output = MObject::kNullObj;
    MStatus status;
    MFnMeshData creator;
    const MObject candidate = creator.create(&status);
    if (!status) return status;
    if (candidate.isNull()) return MS::kFailure;
    output = candidate;
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy

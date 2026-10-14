#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MBoundingBox.h>
#include <maya/MDagPath.h>
#include <maya/MFnFluid.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>

#include <cmath>
#include <cstddef>

MStatus compileFluidGridApi(MFnFluid& fluid)
{
    unsigned int xResolution = 0U;
    unsigned int yResolution = 0U;
    unsigned int zResolution = 0U;
    MStatus status = fluid.getResolution(
        xResolution, yResolution, zResolution);
    if (!status) return status;

    double xDimension = 0.0;
    double yDimension = 0.0;
    double zDimension = 0.0;
    status = fluid.getDimensions(xDimension, yDimension, zDimension);
    if (!status) return status;

    const MBoundingBox bounds = fluid.boundingBox(&status);
    if (!status) return status;

    dommy::fluid::GridPlan plan;
    if (!dommy::fluid::buildGridPlan(
            xResolution, yResolution, zResolution,
            xDimension, yDimension, zDimension,
            bounds.min().x, bounds.min().y, bounds.min().z,
            1U, true, dommy::generation::kMaxWorkItemCount, plan)) {
        return MS::kFailure;
    }

    const unsigned int scalarSize = fluid.gridSize(&status);
    if (!status || static_cast<std::size_t>(scalarSize) !=
                       plan.scalar.voxelCount) {
        return MS::kFailure;
    }

    MPoint voxelCenter;
    status = fluid.voxelCenterPosition(0, 0, 0, voxelCenter);
    if (!status || !dommy::maya::isFinitePoint(voxelCenter)) return MS::kFailure;
    const int voxelIndex = fluid.index(0, 0, 0);
    std::size_t checkedIndex = 0U;
    if (!dommy::generation::checkedCount(
            voxelIndex, plan.scalar.voxelCount - 1U, checkedIndex)) {
        return MS::kFailure;
    }

    float* velocity[3] = {NULL, NULL, NULL};
    status = fluid.getVelocity(velocity[0], velocity[1], velocity[2]);
    if (!status) return status;
    int velocitySizes[3] = {0, 0, 0};
    status = fluid.velocityGridSizes(
        velocitySizes[0], velocitySizes[1], velocitySizes[2]);
    if (!status) return status;

    MDagPath path;
    status = fluid.getPath(path);
    if (!status) return status;
    const MMatrix inclusiveMatrix = path.inclusiveMatrix(&status);
    if (!status || !dommy::maya::isFiniteMatrix(inclusiveMatrix) ||
        !std::isfinite(inclusiveMatrix.det4x4()) ||
        inclusiveMatrix.det4x4() == 0.0) {
        return MS::kFailure;
    }
    return MS::kSuccess;
}

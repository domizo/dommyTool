//
//  DDeformMath.cpp
//  dommyTool
//
//  Created by yuzo on 2018/07/03.
//

#include "DDeformMath.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"

#include <cmath>


bool deformBend(double factor, MVector* point)
{
    if (!point || !std::isfinite(factor) ||
        !dommy::maya::isFiniteVector(*point)) {
        return false;
    }
    if (std::fabs(factor) <= 1.0e-7) return true;

    const double x = point->x;
    const double y = point->y;
    const double z = point->z;
    const double theta = y * factor;
    const double sine = std::sin(theta);
    const double cosine = std::cos(theta);
    const double inverseFactor = 1.0 / factor;
    const MVector result(
        x,
        -(z - inverseFactor) * sine,
        (z - inverseFactor) * cosine + inverseFactor);
    if (!dommy::maya::isFiniteVector(result)) return false;
    *point = result;
    return true;
}


bool deformBendWithCvs(
    double angle,
    double weight,
    const MDoubleArray& weights,
    MPointArray& cvs)
{
    if (!std::isfinite(angle) || !std::isfinite(weight)) return false;
    const std::size_t pointCount =
        static_cast<std::size_t>(cvs.length());
    if (pointCount < 2U) return true;
    if (static_cast<std::size_t>(weights.length()) < pointCount - 1U) {
        return false;
    }
    for (std::size_t index = 0U; index < pointCount; ++index) {
        if (!dommy::maya::isFinitePoint(
                cvs[static_cast<unsigned int>(index)])) {
            return false;
        }
        if (index + 1U < pointCount &&
            !std::isfinite(weights[static_cast<unsigned int>(index)])) {
            return false;
        }
    }

    const MVector rotationAxis(1.0, 0.0, 0.0);
    MPointArray result;
    if (dommy::maya::copyCheckedArray(cvs, result) != MS::kSuccess) {
        return false;
    }
    for (std::size_t pivotIndex = 0U; pivotIndex + 1U < pointCount;
         ++pivotIndex) {
        const unsigned int mayaPivotIndex =
            static_cast<unsigned int>(pivotIndex);
        const MPoint pivot(result[mayaPivotIndex]);
        const double rotationAngle =
            angle * weights[mayaPivotIndex] * weight;
        if (!std::isfinite(rotationAngle)) return false;

        MTransformationMatrix rotMat;
        if (rotMat.setRotatePivot(
                pivot, MSpace::kObject, true) != MS::kSuccess ||
            rotMat.setToRotationAxis(
                rotationAxis, rotationAngle) != MS::kSuccess) {
            return false;
        }
        const MMatrix rotationMatrix = rotMat.asMatrix();
        if (!dommy::maya::isFiniteMatrix(rotationMatrix)) return false;

        for (std::size_t pointIndex = pivotIndex + 1U;
             pointIndex < pointCount; ++pointIndex) {
            const unsigned int mayaPointIndex =
                static_cast<unsigned int>(pointIndex);
            const MPoint transformed =
                result[mayaPointIndex] * rotationMatrix;
            if (!dommy::maya::isFinitePoint(transformed)) return false;
            result[mayaPointIndex] = transformed;
        }
    }
    // Native copy failure may leave a partial destination. Callers must discard
    // the private curve on false; never publish it as a successful deformation.
    return dommy::maya::copyCheckedArray(result, cvs) == MS::kSuccess;
}

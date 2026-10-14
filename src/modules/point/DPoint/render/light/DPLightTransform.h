#pragma once

#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MTransformationMatrix.h>

// Persisted Maya enum values. Keep these stable for archived scenes.
enum DP_MAT_APPLY_TYPE {
    DP_MAT_APPLY_NONE = 0,
    DP_MAT_FULL_MAT_IGNORE_ALL = 1,
    DP_MAT_FULL_MAT_IGNORE_DIR = 2,
    DP_MAT_PRE_TRANSLATE = 3,
    DP_MAT_LOOKAT = 4,
    DP_MAT_APPLY_TYPE_COUNT = 5
};

namespace dommy {
namespace light {

// Preserve the historical row-vector/decomposition formulas in Double.
// Only inputs used by the selected mode participate in matrix arithmetic.
// Renderer callers separately admit the completed matrix to Float storage.
// Output is unchanged on a returned API/numeric failure (or an exception).
inline MStatus buildTransform(
    int lightType, int mode, const MVector& position,
    const MVector& sourceDirection, const MVector& globalScale, double pointScale,
    const MMatrix& animation, const MVector& lookAt, MMatrix& output)
{
    if (lightType < 0 || lightType > 6 ||
        mode < DP_MAT_APPLY_NONE || mode >= DP_MAT_APPLY_TYPE_COUNT)
        return MS::kInvalidParameter;
    if (mode != DP_MAT_APPLY_NONE && !maya::isFiniteMatrix(animation))
        return MS::kInvalidParameter;
    if (mode == DP_MAT_FULL_MAT_IGNORE_ALL) {
        output = animation;
        return MS::kSuccess;
    }
    if (!maya::isFiniteVector(position))
        return MS::kInvalidParameter;

    MTransformationMatrix base;
    if (lightType > 2) {
        double scale[3];
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!numeric::scaledProduct(
                    std::array<double, 2>{{globalScale[axis], pointScale}}, scale[axis]))
                return MS::kInvalidParameter;
        }
        DOMMY_RETURN_IF_FAILURE(base.addScale(scale, MSpace::kWorld));
    }

    MMatrix candidate;
    if (mode == DP_MAT_FULL_MAT_IGNORE_DIR) {
        const MMatrix combined = base.asMatrix() * animation;
        if (!maya::isFiniteMatrix(combined)) return MS::kInvalidParameter;
        MTransformationMatrix transformed(combined);
        DOMMY_RETURN_IF_FAILURE(transformed.addTranslation(position, MSpace::kWorld));
        candidate = transformed.asMatrix();
    } else {
        MVector currentPosition = position;
        MVector direction = sourceDirection;
        if (mode == DP_MAT_LOOKAT) {
            const MTransformationMatrix transformed(animation);
            MStatus status;
            const MVector translation = transformed.getTranslation(MSpace::kWorld, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            currentPosition = currentPosition + translation;
            direction = lookAt - currentPosition;
        }
        if (!maya::isFiniteVector(currentPosition) || !maya::isFiniteVector(direction))
            return MS::kInvalidParameter;
        const double directionLength = direction.length();
        if (!std::isfinite(directionLength) || directionLength <= 1.0e-12)
            return MS::kInvalidParameter;
        const MQuaternion rotation = MVector(0.0, 0.0, -1.0).rotateTo(direction);
        if (!maya::isFiniteQuaternion(rotation)) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(base.addRotationQuaternion(
            rotation.x, rotation.y, rotation.z, rotation.w, MSpace::kWorld));
        if (mode == DP_MAT_PRE_TRANSLATE) {
            const MMatrix combined = base.asMatrix() * animation;
            if (!maya::isFiniteMatrix(combined)) return MS::kInvalidParameter;
            base = combined;
        }
        DOMMY_RETURN_IF_FAILURE(base.addTranslation(currentPosition, MSpace::kWorld));
        candidate = base.asMatrix();
    }
    if (!maya::isFiniteMatrix(candidate)) return MS::kInvalidParameter;
    output = candidate;
    return MS::kSuccess;
}

} // namespace light
} // namespace dommy

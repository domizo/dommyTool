//
//  DTransformOp.cpp
//  dommyTool
//
//  Created by yuzo on 2017/07/02.
//

#include "DTransformOp.h"

#include "DDataOp.h"
#include "DPData.h"
#include "DPointCommon.h"
#include "DPoint/Common/DPAttributeAccess.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DPointPayloadBudget.h"
#include "dommyMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <tbb/parallel_for.h>

namespace {

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool isUsableDirection(const MVector& value)
{
    return isFiniteVector(value) && value.length() > 1.0e-12;
}

bool isFiniteMatrix(const MMatrix& value)
{
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            if (!std::isfinite(value[row][column])) return false;
        }
    }
    return true;
}

bool hasFiniteParameters(const DTransformParams& param)
{
    const MVector vectors[] = {
        param.mLocalTranslate,
        param.mGlobalTranslate,
        param.mLocalRot,
        param.mGlobalRot,
        param.mGlobalScale,
        param.mLocalTransRndMin,
        param.mLocalTransRndMax,
        param.mGlobalTransRndMin,
        param.mGlobalTransRndMax,
        param.mLocalRotRndMin,
        param.mLocalRotRndMax,
        param.mGlobalRotRndMin,
        param.mGlobalRotRndMax,
        param.mGlobalScaleRndMin,
        param.mGlobalScaleRndMax,
        param.mUniformScaleMinMax
    };
    for (std::size_t index = 0U;
         index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        if (!isFiniteVector(vectors[index])) return false;
    }

    const MVector randomBounds[] = {
        param.mLocalTransRndMin, param.mLocalTransRndMax,
        param.mGlobalTransRndMin, param.mGlobalTransRndMax,
        param.mLocalRotRndMin, param.mLocalRotRndMax,
        param.mGlobalRotRndMin, param.mGlobalRotRndMax,
        param.mGlobalScaleRndMin, param.mGlobalScaleRndMax,
        param.mUniformScaleMinMax
    };
    for (const MVector& bound : randomBounds) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::canRepresentAsFloat(bound[axis])) return false;
        }
    }
    return std::isfinite(param.mUniformScale) &&
           dommy::numeric::canRepresentAsFloat(param.mUniformScale) &&
           isUsableDirection(param.mNormalBaseAxis) &&
           isUsableDirection(param.mTangentBaseAxis) &&
           isUsableDirection(param.mNormalVec) &&
           isUsableDirection(param.mTangentVec) &&
           param.mAnimMatrixType >= MAT_ANIM_NONE &&
           param.mAnimMatrixType < MAT_ANIM_TYPE_COUNT;
}

bool loadVectorAttribute(const DPData& data,
                         const std::string& name,
                         std::size_t count,
                         const MVector& defaultValue,
                         std::vector<MVector>& output)
{
    output.assign(count, defaultValue);
    if (name.empty() || !data.isExistAttr(name)) return true;

    MVectorArray values;
    if (!getCheckedMVectorArrayFromAttribute(data, name, count, values)) {
        return false;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        output[index] = values[static_cast<unsigned int>(index)];
    }
    return true;
}

bool loadUniformScaleAttribute(const DPData& data,
                               const DTransformParams& param,
                               std::size_t count,
                               std::vector<double>& output)
{
    output.assign(count, param.mUniformScale);
    if (param.mUniformScaleAttribute.empty() ||
        !data.isExistAttr(param.mUniformScaleAttribute)) {
        return true;
    }

    DScalarDataList values;
    if (!getCheckedDScalarArrayFromAttribute(
            data, param.mUniformScaleAttribute, count, values)) {
        return false;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        const double candidate =
            param.mUniformScale * static_cast<double>(values[index]);
        if (!std::isfinite(candidate)) return false;
        output[index] = candidate;
    }
    return true;
}

bool loadAnimationMatrices(const DPData& data,
                           const DTransformParams& param,
                           std::size_t count,
                           std::vector<MMatrix>& output)
{
    output.assign(count, MMatrix());
    if (param.mAnimMatrixType == MAT_ANIM_NONE) return true;

    MMatrixArray values;
    if (param.mAnimMatrixType == MAT_ANIM_ATTRIBUTE) {
        if (param.mAnimationAttribute.empty() ||
            !getCheckedMMatrixArrayFromAttribute(
                data, param.mAnimationAttribute, count, values)) {
            return false;
        }
    } else if (param.mAnimMatrixType == MAT_ANIM_INPUT_DATA) {
        const MStatus status = param.mAnimationInputIsData
            ? getMatrixArrayFromData(param.mAnim_Mat_InputData_Object, values)
            : getMatrixArrayFromData(param.mNodeObj, param.mAnim_Mat_InputData_Object, values);
        if (status != MS::kSuccess) {
            return false;
        }
    } else {
        return false;
    }

    const std::size_t copyCount = std::min<std::size_t>(
        count, static_cast<std::size_t>(values.length()));
    for (std::size_t index = 0U; index < copyCount; ++index) {
        const MMatrix& candidate = values[static_cast<unsigned int>(index)];
        if (!isFiniteMatrix(candidate)) return false;
        output[index] = candidate;
    }
    return true;
}

bool isFiniteRandomValues(const DVec3DataList& values,
                          std::size_t expectedCount)
{
    if (values.size() != expectedCount) return false;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (!std::isfinite(static_cast<double>(values[index][0])) ||
            !std::isfinite(static_cast<double>(values[index][1])) ||
            !std::isfinite(static_cast<double>(values[index][2]))) {
            return false;
        }
    }
    return true;
}

void generateRandomVectors(std::size_t seed,
                           const MVector& minimum,
                           const MVector& maximum,
                           std::size_t count,
                           DVec3DataList& output)
{
    std::vector<float> xValues;
    std::vector<float> yValues;
    std::vector<float> zValues;
    dommy::random::generateFloatValues(
        static_cast<std::uint32_t>(seed),
        static_cast<float>(minimum.x), static_cast<float>(maximum.x),
        count, xValues);
    dommy::random::generateFloatValues(
        static_cast<std::uint32_t>(seed + 1U),
        static_cast<float>(minimum.y), static_cast<float>(maximum.y),
        count, yValues);
    dommy::random::generateFloatValues(
        static_cast<std::uint32_t>(seed + 2U),
        static_cast<float>(minimum.z), static_cast<float>(maximum.z),
        count, zValues);

    output.resize(count);
    for (std::size_t index = 0U; index < count; ++index) {
        output[index] = DVec3Data(
            xValues[index], yValues[index], zValues[index]);
    }
}

} // namespace

MStatus DGenerateTransformMatrixFromDP(const DPData* dpdata,
                                       const DTransformParams& param,
                                       std::vector<MMatrix>& matrixArray)
{
    if (!dpdata || !dpdata->hasValidPointTopology() ||
        !hasFiniteParameters(param)) {
        return MS::kInvalidParameter;
    }

    const std::size_t count = dpdata->numberOfPoints();
    if (count > dommy::generation::kMaxGeneratedTransformCount ||
        count > static_cast<std::size_t>(
                    std::numeric_limits<unsigned int>::max())) {
        return MS::kInvalidParameter;
    }

    // Admit all retained arrays together before allocating any of them. The
    // extra vector/scalar rows cover attribute conversion and random staging;
    // the shared four-copy allowance is not a native peak-memory guarantee.
    dommy::point::PointPayloadBudget workspace;
    try {
        workspace.add(count, 2U * sizeof(MMatrix) + 9U * sizeof(MVector) +
                                 sizeof(double) + 5U * sizeof(DVec3Data) +
                                 4U * sizeof(float) + sizeof(MStatus));
        if (count != 0U) workspace.add(matrixArray.capacity(), sizeof(MMatrix));
    } catch (const std::length_error&) {
        return MS::kInvalidParameter;
    }

    const DPointList* const points = dpdata->getPositionPtrConst();
    if (!points || points->size() != count) return MS::kFailure;
    for (std::size_t index = 0U; index < count; ++index) {
        if (!std::isfinite(static_cast<double>((*points)[index][0])) ||
            !std::isfinite(static_cast<double>((*points)[index][1])) ||
            !std::isfinite(static_cast<double>((*points)[index][2]))) {
            return MS::kInvalidParameter;
        }
    }

    // Maya plugs, function sets, and plug-in data are accessed serially.
    // Only the value-type matrix calculation below is parallelized.
    std::vector<MMatrix> animationMatrices;
    std::vector<MVector> localTranslations;
    std::vector<MVector> globalTranslations;
    std::vector<MVector> localRotations;
    std::vector<MVector> globalRotations;
    std::vector<MVector> globalScales;
    std::vector<MVector> normals;
    std::vector<MVector> tangents;
    std::vector<double> uniformScales;

    if (!loadAnimationMatrices(*dpdata, param, count, animationMatrices) ||
        !loadVectorAttribute(*dpdata, param.mLocalTransAttribute, count,
                             param.mLocalTranslate, localTranslations) ||
        !loadVectorAttribute(*dpdata, param.mGlobalTransAttribute, count,
                             param.mGlobalTranslate, globalTranslations) ||
        !loadVectorAttribute(*dpdata, param.mLocalRotAttribute, count,
                             param.mLocalRot, localRotations) ||
        !loadVectorAttribute(*dpdata, param.mGlobalRotAttribute, count,
                             param.mGlobalRot, globalRotations) ||
        !loadVectorAttribute(*dpdata, param.mGlobalScaleAttribute, count,
                             param.mGlobalScale, globalScales) ||
        !loadVectorAttribute(*dpdata, param.mNormalVecAttribute, count,
                             param.mNormalVec, normals) ||
        !loadVectorAttribute(*dpdata, param.mTangentVecAttribute, count,
                             param.mTangentVec, tangents) ||
        !loadUniformScaleAttribute(*dpdata, param, count, uniformScales)) {
        return MS::kInvalidParameter;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        if (!isUsableDirection(normals[index]) ||
            !isUsableDirection(tangents[index])) {
            return MS::kInvalidParameter;
        }
    }

    DVec3DataList localTranslationRandom(count, DVec3Data(0.0, 0.0, 0.0));
    DVec3DataList globalTranslationRandom(count, DVec3Data(0.0, 0.0, 0.0));
    DVec3DataList localRotationRandom(count, DVec3Data(0.0, 0.0, 0.0));
    DVec3DataList globalRotationRandom(count, DVec3Data(0.0, 0.0, 0.0));
    DVec3DataList scaleRandom(count, DVec3Data(0.0, 0.0, 0.0));
    DScalarDataList uniformScaleRandom(count, 0.0f);

    generateRandomVectors(
        param.mSeed, param.mLocalTransRndMin, param.mLocalTransRndMax,
        count, localTranslationRandom);
    generateRandomVectors(
        param.mSeed + 3U, param.mGlobalTransRndMin,
        param.mGlobalTransRndMax, count, globalTranslationRandom);
    generateRandomVectors(
        param.mSeed + 6U, param.mLocalRotRndMin, param.mLocalRotRndMax,
        count, localRotationRandom);
    generateRandomVectors(
        param.mSeed + 9U, param.mGlobalRotRndMin,
        param.mGlobalRotRndMax, count, globalRotationRandom);
    generateRandomVectors(
        param.mSeed + 12U, param.mGlobalScaleRndMin,
        param.mGlobalScaleRndMax, count, scaleRandom);
    dommy::random::generateFloatValues(
        static_cast<std::uint32_t>(param.mSeed + 15U),
        static_cast<float>(param.mUniformScaleMinMax[0]),
        static_cast<float>(param.mUniformScaleMinMax[1]), count,
        uniformScaleRandom);

    if (!isFiniteRandomValues(localTranslationRandom, count) ||
        !isFiniteRandomValues(globalTranslationRandom, count) ||
        !isFiniteRandomValues(localRotationRandom, count) ||
        !isFiniteRandomValues(globalRotationRandom, count) ||
        !isFiniteRandomValues(scaleRandom, count) ||
        uniformScaleRandom.size() != count) {
        return MS::kFailure;
    }
    for (std::size_t index = 0U; index < uniformScaleRandom.size(); ++index) {
        if (!std::isfinite(static_cast<double>(uniformScaleRandom[index]))) {
            return MS::kFailure;
        }
    }

    std::vector<MMatrix> candidate(count, MMatrix());
    std::vector<MStatus> statuses(count, MS::kSuccess);
    const std::size_t grainSize = DGetGlobalProcessNum(count);
    const tbb::blocked_range<std::size_t> range(0U, count, grainSize);
    tbb::parallel_for(range, [&](const tbb::blocked_range<std::size_t>& block) {
        for (std::size_t index = block.begin(); index != block.end(); ++index) {
            // Keep each point transactional and report errors on the caller
            // thread, without publishing a partially built matrix array.
            statuses[index] = [&]() -> MStatus {
                const MQuaternion normalRotation =
                    param.mNormalBaseAxis.rotateTo(normals[index].normal());
                const MVector rotatedTangent = param.mTangentBaseAxis.rotateBy(normalRotation);
                const MQuaternion tangentRotation =
                    rotatedTangent.rotateTo(tangents[index].normal());

                MTransformationMatrix transform;
                if (param.mAnimMatrixType != MAT_ANIM_NONE) {
                    transform = animationMatrices[index];
                }

                const double uniformScale = uniformScales[index] + uniformScaleRandom[index];
                const double scale[3] = {
                    (globalScales[index][0] + scaleRandom[index][0]) * uniformScale,
                    (globalScales[index][1] + scaleRandom[index][1]) * uniformScale,
                    (globalScales[index][2] + scaleRandom[index][2]) * uniformScale};
                MStatus status = transform.addScale(scale, MSpace::kObject);
                if (!status)
                    return status;

                const double localRotation[3] = {
                    dommy::math::degreesToRadians(localRotations[index][0] +
                                                  localRotationRandom[index][0]),
                    dommy::math::degreesToRadians(localRotations[index][1] +
                                                  localRotationRandom[index][1]),
                    dommy::math::degreesToRadians(localRotations[index][2] +
                                                  localRotationRandom[index][2])};
                status = transform.addRotation(localRotation, MTransformationMatrix::kXYZ,
                                               MSpace::kObject);
                if (!status)
                    return status;

                const double globalRotation[3] = {
                    dommy::math::degreesToRadians(globalRotations[index][0] +
                                                  globalRotationRandom[index][0]),
                    dommy::math::degreesToRadians(globalRotations[index][1] +
                                                  globalRotationRandom[index][1]),
                    dommy::math::degreesToRadians(globalRotations[index][2] +
                                                  globalRotationRandom[index][2])};
                status = transform.addRotation(globalRotation, MTransformationMatrix::kXYZ,
                                               MSpace::kWorld);
                if (!status)
                    return status;

                const MVector localRandom(localTranslationRandom[index][0],
                                          localTranslationRandom[index][1],
                                          localTranslationRandom[index][2]);
                status = transform.addTranslation(localTranslations[index] + localRandom,
                                                  MSpace::kObject);
                if (!status)
                    return status;

                MTransformationMatrix alignment;
                status = alignment.addRotationQuaternion(tangentRotation.x, tangentRotation.y,
                                                         tangentRotation.z, tangentRotation.w,
                                                         MSpace::kObject);
                if (!status)
                    return status;
                status = alignment.addRotationQuaternion(normalRotation.x, normalRotation.y,
                                                         normalRotation.z, normalRotation.w,
                                                         MSpace::kObject);
                if (!status)
                    return status;
                transform = MTransformationMatrix(transform.asMatrix() * alignment.asMatrix());

                const MVector globalRandom(globalTranslationRandom[index][0],
                                           globalTranslationRandom[index][1],
                                           globalTranslationRandom[index][2]);
                status = transform.addTranslation(globalTranslations[index] + globalRandom +
                                                      DPToMV((*points)[index]),
                                                  MSpace::kWorld);
                if (!status)
                    return status;
                candidate[index] = transform.asMatrix();
                return MS::kSuccess;
            }();
        }
    });

    for (std::size_t index = 0U; index < candidate.size(); ++index) {
        if (!statuses[index])
            return statuses[index];
        if (!isFiniteMatrix(candidate[index]))
            return MS::kFailure;
    }
    matrixArray.swap(candidate);
    return MS::kSuccess;
}

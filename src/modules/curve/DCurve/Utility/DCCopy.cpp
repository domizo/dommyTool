//
//  DCCopy.cpp
//  dommyTool
//
//  Created by yuzo on 1/27/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCCopy.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "DCurveCopyBudget.h"
#include "DCurveCopyMath.h"
#include "DNodeOperation.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DMayaValueValidation.h"
#include "math/DFrameAlignment.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "DLogConfig.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "dommyMath.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <vector>

const MTypeId DCCopy::typeId(ID_DC_COPY);

MObject DCCopy::inCopyObject;

MObject DCCopy::inDP;
MObject DCCopy::inDC;
MObject DCCopy::inObject;

MObject DCCopy::inReferenceObject;

MObject DCCopy::aCountU;
MObject DCCopy::aCountV;

MObject DCCopy::aAimVector;
MObject DCCopy::aNormalVector;

MObject DCCopy::aDPNormalAttributeName;
MObject DCCopy::aDPTangentAttributeName;

MObject DCCopy::aConnectType;
MObject DCCopy::aConnectUVSwitch;

MObject DCCopy::aTangentMode;
MObject DCCopy::aTangentVector;

MObject DCCopy::aUniformScale;
MObject DCCopy::aScale;

MObject DCCopy::aSeed;
MObject DCCopy::aRandomScale;
MObject DCCopy::aRandomRotate;
MObject DCCopy::aIsRandScaleAbsolute;
MObject DCCopy::aIsRandRotateabsolute;

MObject DCCopy::outDC;

void* DCCopy::creator() {
    return ::dommy::maya::createObject("DCCopy", [&]() {
        return new DCCopy();
    });
}

namespace {

using CopyMatrixRows = std::vector<std::vector<MMatrix>>;

struct CopyParameters {
    MVector aim, up, normal;
    std::array<double, 3> scale = {{1.0, 1.0, 1.0}};
    std::array<double, 3> randomScale = {{0.0, 0.0, 0.0}};
    std::array<double, 3> randomRotate = {{0.0, 0.0, 0.0}};
    double uniformScale = 1.0;
    int seed = 1;
    short tangentMode = 0;
    bool connected = false, absoluteScale = false, absoluteRotate = false;
};

struct CopyReference {
    MFnNurbsCurve curve;
    MFnNurbsSurface surface;
    bool useCurve = false, useSurface = false;

    MStatus read(MDataBlock& data, const MObject& node, const MObject& attribute) {
        MStatus status;
        const bool connected = MPlug(node, attribute).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected) return MS::kSuccess;
        const MDataHandle handle = data.inputValue(attribute, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dommy::maya::isFiniteMatrix(handle.geometryTransformMatrix()))
            return MS::kInvalidParameter;
        if (handle.type() == MFnData::kNurbsCurve) {
            const MObject object = handle.asNurbsCurveTransformed();
            if (object.isNull())
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(curve.setObject(object));
            useCurve = true;
        } else if (handle.type() == MFnData::kNurbsSurface) {
            const MObject object = handle.asNurbsSurfaceTransformed();
            if (object.isNull())
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(surface.setObject(object));
            useSurface = true;
        } else {
            return MS::kInvalidParameter;
        }
        return MS::kSuccess;
    }

    MStatus apply(const MPoint& point, MVector& tangent, MVector& normal, bool surfaceTangent,
                  bool allowCurve = true) const {
        if (!dommy::maya::isFinitePoint(point))
            return MS::kInvalidParameter;
        MStatus status;
        if (useCurve && allowCurve) {
            double parameter = 0.0;
            curve.closestPoint(point, &parameter, kMFnNurbsEpsilon, MSpace::kObject, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (!std::isfinite(parameter))
                return MS::kInvalidParameter;
            MPoint target;
            DOMMY_RETURN_IF_FAILURE(curve.getPointAtParam(parameter, target, MSpace::kObject));
            if (!dommy::maya::isFinitePoint(target))
                return MS::kInvalidParameter;
            normal = target - point;
        } else if (useSurface) {
            double u = 0.0, v = 0.0;
            surface.closestPoint(point, &u, &v, false, kMFnNurbsEpsilon, MSpace::kObject, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (!std::isfinite(u) || !std::isfinite(v))
                return MS::kInvalidParameter;
            normal = surface.normal(u, v, MSpace::kObject, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (surfaceTangent) {
                MVector tangentV;
                DOMMY_RETURN_IF_FAILURE(
                    surface.getTangents(u, v, tangent, tangentV, MSpace::kObject));
            }
        }
        return MS::kSuccess;
    }
};

// Reuse the common robust frame builder and alignment matrix. A finite zero or
// parallel normal has no roll information, so choose a deterministic up axis.
// Invalid positions/directions and a zero tangent are errors, not empty output.
MStatus makeCopyMatrix(const CopyParameters& parameters, const MPoint& point,
                       const MVector& tangent, const MVector& normal, MMatrix& result,
                       MVector* binormal = nullptr) {
    if (!dommy::maya::isFinitePoint(point) || !dommy::maya::isFiniteVector(tangent) ||
        !dommy::maya::isFiniteVector(normal))
        return MS::kInvalidParameter;
    dommy::math::FrameDirection forward = {{tangent.x, tangent.y, tangent.z}};
    dommy::math::FrameDirection up = {{normal.x, normal.y, normal.z}};
    if (!dommy::math::frame_detail::normalize(forward))
        return MS::kInvalidParameter;
    dommy::math::FrameRotation frame;
    if (!dommy::math::frame_detail::makeFrame(forward, up, frame)) {
        up = std::fabs(forward[1]) < 0.9 ? dommy::math::FrameDirection{{0.0, 1.0, 0.0}}
                                         : dommy::math::FrameDirection{{1.0, 0.0, 0.0}};
        if (!dommy::math::frame_detail::makeFrame(forward, up, frame))
            return MS::kInvalidParameter;
    }
    MMatrix candidate;
    DOMMY_RETURN_IF_FAILURE(getAlignSpaceMatrix(MVector(frame[0][0], frame[0][1], frame[0][2]),
                                                MVector(frame[1][0], frame[1][1], frame[1][2]),
                                                parameters.aim, parameters.up, candidate));
    candidate[3][0] = point.x;
    candidate[3][1] = point.y;
    candidate[3][2] = point.z;
    if (!dommy::maya::isFiniteMatrix(candidate))
        return MS::kInvalidParameter;
    result = candidate;
    if (binormal)
        *binormal = MVector(frame[2][0], frame[2][1], frame[2][2]);
    return MS::kSuccess;
}

MStatus makeCurveCopyMatrices(const MFnNurbsCurve& curve, std::size_t count,
                              const CopyParameters& parameters, const CopyReference& reference,
                              std::vector<MMatrix>& matrices) {
    if (count == 0U)
        return MS::kSuccess;
    MStatus status;
    const double length = curve.length(kMFnNurbsEpsilon, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    double start = 0.0, end = 0.0;
    DOMMY_RETURN_IF_FAILURE(curve.getKnotDomain(start, end));
    if (!std::isfinite(length) || length <= 0.0 ||
        !dommy::curve::validCopyParameterDomain(start, end))
        return MS::kInvalidParameter;
    matrices.reserve(count);
    MVector previousBinormal;
    // Interior arc-length samples: the first and last endpoints are excluded.
    for (std::size_t index = 1U; index <= count; ++index) {
        const double distance =
            length * (static_cast<double>(index) / static_cast<double>(count + 1U));
        double parameter = curve.findParamFromLength(distance, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!std::isfinite(parameter))
            return MS::kInvalidParameter;
        parameter = std::max(start, std::min(end, parameter));
        MPoint point;
        DOMMY_RETURN_IF_FAILURE(curve.getPointAtParam(parameter, point, MSpace::kObject));
        MVector tangent = curve.tangent(parameter, MSpace::kObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        MVector normal;
        if (parameters.tangentMode == 1) {
            normal = parameters.normal;
        } else if (parameters.tangentMode == 2 && (reference.useCurve || reference.useSurface)) {
            DOMMY_RETURN_IF_FAILURE(reference.apply(point, tangent, normal, false));
        } else if (index == 1U) {
            normal = curve.normal(parameter, MSpace::kObject, &status);
            DOMMY_RETURN_IF_FAILURE(status);
        } else {
            // Normalize before crossing to avoid overflow on finite large derivatives.
            dommy::math::FrameDirection direction = {{tangent.x, tangent.y, tangent.z}};
            if (!dommy::math::frame_detail::normalize(direction))
                return MS::kInvalidParameter;
            normal = previousBinormal ^ MVector(direction[0], direction[1], direction[2]);
        }
        MMatrix matrix;
        DOMMY_RETURN_IF_FAILURE(
            makeCopyMatrix(parameters, point, tangent, normal, matrix, &previousBinormal));
        matrices.push_back(matrix);
    }
    return MS::kSuccess;
}

MStatus appendCopyRows(const CopyMatrixRows& rows, std::size_t instanceCount,
                       const MPointArray& copyCvs, const MDoubleArray& copyKnots, int copyDegree,
                       const CopyParameters& parameters, int seed, DCurveOutputBatch& output) {
    std::size_t actualCount = 0U, budget = 0U;
    for (const auto& row : rows) {
        if (!dommy::generation::checkedSum(actualCount, row.size(),
                                           dommy::generation::kMaxGeneratedCurveCount, actualCount))
            return MS::kInvalidParameter;
    }
    if (actualCount != instanceCount ||
        !dommy::curve::checkedCurveCopyBudget(instanceCount, copyCvs.length(), budget))
        return MS::kInvalidParameter;
    if (instanceCount == 0U)
        return MS::kSuccess;
    std::array<MDoubleArray, 3> randomScale, randomRotate;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        const int axisSeed = dommy::random::requireOffsetNonnegativeSeed(seed, axis);
        // Zero ranges also go through checked allocation. Preserve the historical
        // exclusive upper bound and the shared scale/rotation seeds.
        DOMMY_RETURN_IF_FAILURE(dommy::maya::generateRandomArray(
            axisSeed, parameters.absoluteScale ? 0.0 : -parameters.randomScale[axis],
            parameters.randomScale[axis], instanceCount, randomScale[axis], false));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::generateRandomArray(
            axisSeed, parameters.absoluteRotate ? 0.0 : -parameters.randomRotate[axis],
            parameters.randomRotate[axis], instanceCount, randomRotate[axis], false));
    }
    std::size_t instance = 0U;
    for (const auto& row : rows) {
        MPointArray connectedCvs;
        for (std::size_t column = 0U; column < row.size(); ++column, ++instance) {
            MPointArray cvs;
            if (!parameters.connected)
                DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(copyCvs, cvs));
            for (unsigned int cv = 0U; cv < copyCvs.length(); ++cv) {
                MPoint point = copyCvs[cv];
                point.x *= parameters.scale[0] * parameters.uniformScale + randomScale[0][instance];
                point.y *= parameters.scale[1] * parameters.uniformScale + randomScale[1][instance];
                point.z *= parameters.scale[2] * parameters.uniformScale + randomScale[2][instance];
                double rotation[3];
                for (unsigned int axis = 0U; axis < 3U; ++axis)
                    rotation[axis] = dommy::math::degreesToRadians(randomRotate[axis][instance]);
                const MVector rotated =
                    MVector(point).rotateBy(rotation, MTransformationMatrix::kYZX);
                const MPoint result = MPoint(rotated) * row[column];
                if (!dommy::maya::isFinitePoint(result))
                    return MS::kInvalidParameter;
                if (parameters.connected) {
                    DOMMY_RETURN_IF_FAILURE(connectedCvs.append(result));
                    if ((column > 0U && cv == 0U) ||
                        (column + 1U < row.size() && cv + 1U == copyCvs.length()))
                        DOMMY_RETURN_IF_FAILURE(connectedCvs.append(result));
                } else {
                    cvs[cv] = result;
                }
            }
            if (!parameters.connected &&
                !output.append(DCItem(new DCurveInternalData(cvs, copyKnots, copyDegree))))
                return MS::kFailure;
        }
        if (parameters.connected && !row.empty()) {
            MDoubleArray knots;
            GenerateDCurveKnots(connectedCvs.length(), copyDegree, knots);
            if (!output.append(DCItem(new DCurveInternalData(connectedCvs, knots, copyDegree))))
                return MS::kFailure;
        }
    }
    return MS::kSuccess;
}

} // namespace

MStatus DCCopy::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;
    MStatus status;
    const bool copyConnected = MPlug(thisMObject(), inCopyObject).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!copyConnected) return MS::kFailure;
    MObject copyObject;
    DOMMY_RETURN_IF_FAILURE(DGetMayaCurveFromPlug(inCopyObject, data, copyObject));
    MFnNurbsCurve copyCurve(copyObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int copyDegree = copyCurve.degree(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MPointArray copyCvs;
    MDoubleArray copyKnots;
    DOMMY_RETURN_IF_FAILURE(copyCurve.getCVs(copyCvs, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(copyCurve.getKnots(copyKnots));
    if (!DCurveInternalData(copyCvs, copyKnots, copyDegree).hasValidCurveTopology())
        return MS::kInvalidParameter;

    const MDataHandle rawCountUHandle = data.inputValue(aCountU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawCountU = rawCountUHandle.asInt();
    const MDataHandle rawCountVHandle = data.inputValue(aCountV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawCountV = rawCountVHandle.asInt();
    const MDataHandle aimHandle = data.inputValue(aAimVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& aim = aimHandle.asDouble3();
    const MDataHandle upHandle = data.inputValue(aNormalVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& up = upHandle.asDouble3();
    const MDataHandle connectTypeHandle = data.inputValue(aConnectType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short connectType = connectTypeHandle.asShort();
    const MDataHandle uvSwitchHandle = data.inputValue(aConnectUVSwitch, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool uvSwitch = uvSwitchHandle.asBool();
    const MDataHandle tangentModeHandle = data.inputValue(aTangentMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short tangentMode = tangentModeHandle.asShort();
    const MDataHandle normalHandle = data.inputValue(aTangentVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& normal = normalHandle.asDouble3();
    const MDataHandle uniformScaleHandle = data.inputValue(aUniformScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double uniformScale = uniformScaleHandle.asDouble();
    const MDataHandle scaleHandle = data.inputValue(aScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& scale = scaleHandle.asDouble3();
    const MDataHandle rawSeedHandle = data.inputValue(aSeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSeed = rawSeedHandle.asInt();
    const MDataHandle randomScaleHandle = data.inputValue(aRandomScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& randomScale = randomScaleHandle.asDouble3();
    const MDataHandle randomRotateHandle = data.inputValue(aRandomRotate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double3& randomRotate = randomRotateHandle.asDouble3();
    const MDataHandle absoluteScaleHandle = data.inputValue(aIsRandScaleAbsolute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool absoluteScale = absoluteScaleHandle.asBool();
    const MDataHandle absoluteRotateHandle = data.inputValue(aIsRandRotateabsolute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool absoluteRotate = absoluteRotateHandle.asBool();

    std::size_t countU = 0U, countV = 0U, gridCount = 0U;
    if (!dommy::curve::checkedCurveCopyCounts(rawCountU, rawCountV, countU, countV, gridCount) ||
        connectType < 0 || connectType > 1 || tangentMode < 0 || tangentMode > 2 ||
        !std::isfinite(uniformScale))
        return MS::kInvalidParameter;
    CopyParameters parameters;
    parameters.aim = MVector(aim);
    parameters.up = MVector(up);
    parameters.normal = MVector(normal);
    parameters.uniformScale = uniformScale;
    parameters.seed = dommy::random::requireOffsetNonnegativeSeed(rawSeed, 0U);
    parameters.tangentMode = tangentMode;
    parameters.connected = connectType == 1;
    parameters.absoluteScale = absoluteScale;
    parameters.absoluteRotate = absoluteRotate;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!std::isfinite(aim[axis]) || !std::isfinite(up[axis]) || !std::isfinite(normal[axis]) ||
            !std::isfinite(scale[axis]) || !std::isfinite(randomScale[axis]) ||
            !std::isfinite(randomRotate[axis]))
            return MS::kInvalidParameter;
        parameters.scale[axis] = scale[axis];
        parameters.randomScale[axis] = randomScale[axis];
        parameters.randomRotate[axis] = randomRotate[axis];
    }
    MMatrix sourceFrame;
    DOMMY_RETURN_IF_FAILURE(getAlignSpaceMatrix(parameters.aim, parameters.up, parameters.aim,
                                                parameters.up, sourceFrame));
    if (tangentMode == 1 && parameters.normal.length() <= 1.0e-12)
        return MS::kInvalidParameter;
    // Retain the historical seed range validation even for empty input.
    dommy::random::requireOffsetNonnegativeSeed(parameters.seed, 2U);
    CopyReference reference;
    DOMMY_RETURN_IF_FAILURE(reference.read(data, thisMObject(), inReferenceObject));
    DCurveOutputBatch generatedCurves;

    const MObject* selectedInput = nullptr;
    const MObject* const candidates[] = {&inDP, &inDC, &inObject};
    for (const MObject* attribute : candidates) {
        const bool connected = MPlug(thisMObject(), *attribute).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (connected) {
            selectedInput = attribute;
            break;
        }
    }
    if (selectedInput == &inDP) {
        const MDataHandle handle = data.inputValue(inDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, input));
        if (!input || !input->hasValidPointTopology())
            return MS::kInvalidParameter;
        const auto& points = *input->getPositionPtrConst();
        std::size_t budget = 0U;
        if (!dommy::curve::checkedCurveCopyBudget(points.size(), copyCvs.length(), budget))
            return MS::kInvalidParameter;
        MVectorArray normals, tangents;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            points.size(),
            [&](unsigned int, MVector& value) -> MStatus {
                value = parameters.normal;
                return MS::kSuccess;
            },
            normals));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            points.size(),
            [](unsigned int, MVector& value) -> MStatus {
                value = MVector(1.0, 0.0, 0.0);
                return MS::kSuccess;
            },
            tangents));
        const MDataHandle normalNameHandle = data.inputValue(aDPNormalAttributeName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const std::string normalName = normalNameHandle.asString().asChar();
        const MDataHandle tangentNameHandle = data.inputValue(aDPTangentAttributeName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const std::string tangentName = tangentNameHandle.asString().asChar();
        if (!normalName.empty())
            getMVectorArrayFromAttribute(*input, normalName, normals);
        if (!tangentName.empty())
            getMVectorArrayFromAttribute(*input, tangentName, tangents);
        if (normals.length() != points.size() || tangents.length() != points.size())
            return MS::kInvalidParameter;
        CopyMatrixRows rows(1U);
        rows[0].reserve(points.size());
        for (std::size_t index = 0U; index < points.size(); ++index) {
            const MPoint point(points[index][0], points[index][1], points[index][2]);
            MVector tangent = tangents[static_cast<unsigned int>(index)];
            MVector normal = normals[static_cast<unsigned int>(index)];
            if (tangentMode == 2)
                DOMMY_RETURN_IF_FAILURE(reference.apply(point, tangent, normal, true));
            MMatrix matrix;
            DOMMY_RETURN_IF_FAILURE(makeCopyMatrix(parameters, point, tangent, normal, matrix));
            rows[0].push_back(matrix);
        }
        DOMMY_RETURN_IF_FAILURE(appendCopyRows(rows, points.size(), copyCvs, copyKnots, copyDegree,
                                               parameters, parameters.seed, generatedCurves));
    } else if (selectedInput == &inDC) {
        const DCurveData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
        if (!input)
            return MS::kInvalidParameter;
        std::size_t instances = 0U, budget = 0U;
        if (!dommy::generation::checkedProduct(input->numberOfCurves(), countU,
                                               dommy::generation::kMaxGeneratedCurveCount,
                                               instances) ||
            !dommy::curve::checkedCurveCopyBudget(instances, copyCvs.length(), budget))
            return MS::kInvalidParameter;
        std::size_t offset = 0U;
        if (countU != 0U) {
            for (auto curve = input->beginCurves(); curve != input->endCurves();
                 ++curve, ++offset) {
                MObject object;
                DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*curve, object));
                if (object.isNull())
                    return MS::kFailure;
                MFnNurbsCurve functionSet(object, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                CopyMatrixRows rows(1U);
                DOMMY_RETURN_IF_FAILURE(
                    makeCurveCopyMatrices(functionSet, countU, parameters, reference, rows[0]));
                const int seed =
                    dommy::random::requireOffsetNonnegativeSeed(parameters.seed, offset);
                DOMMY_RETURN_IF_FAILURE(appendCopyRows(rows, countU, copyCvs, copyKnots, copyDegree,
                                                       parameters, seed, generatedCurves));
            }
        }
    } else if (selectedInput == &inObject) {
        const MDataHandle handle = data.inputValue(inObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dommy::maya::isFiniteMatrix(handle.geometryTransformMatrix()) ||
            (handle.type() != MFnData::kNurbsCurve && handle.type() != MFnData::kNurbsSurface))
            return MS::kInvalidParameter;
        const bool surface = handle.type() == MFnData::kNurbsSurface;
        const std::size_t instances = surface ? gridCount : countU;
        std::size_t budget = 0U;
        if (!dommy::curve::checkedCurveCopyBudget(instances, copyCvs.length(), budget))
            return MS::kInvalidParameter;
        const MObject object =
            surface ? handle.asNurbsSurfaceTransformed() : handle.asNurbsCurveTransformed();
        if (object.isNull())
            return MS::kFailure;
        CopyMatrixRows rows;
        if (!surface) {
            MFnNurbsCurve curve(object, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            rows.resize(1U);
            DOMMY_RETURN_IF_FAILURE(
                makeCurveCopyMatrices(curve, countU, parameters, reference, rows[0]));
        } else if (instances != 0U) {
            MFnNurbsSurface functionSet;
            DOMMY_RETURN_IF_FAILURE(functionSet.setObject(object));
            double startU = 0.0, endU = 0.0, startV = 0.0, endV = 0.0;
            DOMMY_RETURN_IF_FAILURE(functionSet.getKnotDomain(startU, endU, startV, endV));
            if (!dommy::curve::validCopyParameterDomain(startU, endU) ||
                !dommy::curve::validCopyParameterDomain(startV, endV))
                return MS::kInvalidParameter;
            const std::size_t rowCount = uvSwitch ? countV : countU;
            const std::size_t columnCount = uvSwitch ? countU : countV;
            rows.resize(rowCount);
            for (std::size_t row = 0U; row < rowCount; ++row) {
                rows[row].reserve(columnCount);
                for (std::size_t column = 0U; column < columnCount; ++column) {
                    const std::size_t uIndex = uvSwitch ? column : row;
                    const std::size_t vIndex = uvSwitch ? row : column;
                    double u = 0.0, v = 0.0;
                    // Reuse inclusive interpolation with two excluded endpoints:
                    // sample (index + 1) / (count + 1), including the domain origin.
                    if (!dommy::curve::checkedSurfaceParameter(startU, endU, 1.0, 0.0, uIndex + 1U,
                                                               countU + 2U, true, u) ||
                        !dommy::curve::checkedSurfaceParameter(startV, endV, 1.0, 0.0, vIndex + 1U,
                                                               countV + 2U, true, v))
                        return MS::kInvalidParameter;
                    MPoint point;
                    MVector tangent, tangentV;
                    DOMMY_RETURN_IF_FAILURE(functionSet.getDerivativesAtParm(
                        u, v, point, tangent, tangentV, MSpace::kObject));
                    MVector normal = functionSet.normal(u, v, MSpace::kObject, &status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    if (tangentMode == 1)
                        normal = parameters.normal;
                    else if (tangentMode == 2)
                        DOMMY_RETURN_IF_FAILURE(
                            reference.apply(point, tangent, normal, false, false));
                    MMatrix matrix;
                    DOMMY_RETURN_IF_FAILURE(
                        makeCopyMatrix(parameters, point, tangent, normal, matrix));
                    rows[row].push_back(matrix);
                }
            }
        }
        DOMMY_RETURN_IF_FAILURE(appendCopyRows(rows, instances, copyCvs, copyKnots, copyDegree,
                                               parameters, parameters.seed, generatedCurves));
    }

    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output)
        return MS::kFailure;
    if (!generatedCurves.publishTo(*output) || !output->hasValidCurveTopology() ||
        output->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kFailure;
    output->updateBBox();
    MDataHandle handle = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        handle, pluginData.object(), DCurveData::id));
    handle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCCopy")

MStatus DCCopy::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inCopyObject = gAttr.create("inCopyObject", "ico", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCopyObject));

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    inObject = gAttr.create("inObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    inReferenceObject = gAttr.create("referenceObject", "refo", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inReferenceObject));

    aCountU = nAttr.create("CountU", "cntu", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCountU));

    aCountV = nAttr.create("CountV", "cntv", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCountV));

    aAimVector = nAttr.create("AimVector", "aimvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAimVector));

    aNormalVector = nAttr.create("NormalVector", "normvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalVector));

    const MObject aDPNormalAttributeNameDefault = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDPNormalAttributeName = tAttr.create("DPNormalAttributeName", "dpnattr", MFnData::kString,
                                          aDPNormalAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDPNormalAttributeName));

    const MObject aDPTangentAttributeNameDefault = fnStringData.create("T", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDPTangentAttributeName = tAttr.create("DPTangentAttributeName", "dptattr", MFnData::kString,
                                           aDPTangentAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDPTangentAttributeName));

    MFnEnumAttribute connectEnumFn;
    aConnectType = connectEnumFn.create("ConnectType", "cnctty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(connectEnumFn.addField("Split", 0));
    DOMMY_RETURN_IF_FAILURE(connectEnumFn.addField("Connect", 1));
    DOMMY_RETURN_IF_FAILURE(connectEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConnectType));

    aConnectUVSwitch =
        nAttr.create("ConnectUVSwitch", "cuvsw", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConnectUVSwitch));

    MFnEnumAttribute tangentEnum;
    aTangentMode = tangentEnum.create("TangentMode", "tam", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tangentEnum.addField("From Source Curve", 0));
    DOMMY_RETURN_IF_FAILURE(tangentEnum.addField("Use Tangent Vector", 1));
    DOMMY_RETURN_IF_FAILURE(tangentEnum.addField("From Reference Object", 2));
    DOMMY_RETURN_IF_FAILURE(tangentEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentMode));

    aTangentVector = nAttr.create("tangentVector", "tanvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));

    aUniformScale = nAttr.create("UniformScale", "uscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));

    aScale = nAttr.create("Scale", "scl", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aRandomScale = nAttr.create("RandomScale", "rndscl", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomScale));

    aRandomRotate = nAttr.create("RandomRotate", "rndrt", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomRotate));

    aIsRandScaleAbsolute =
        nAttr.create("IsRandScaleAbsolute", "irsabs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandScaleAbsolute));

    aIsRandRotateabsolute =
        nAttr.create("IsRandRotateabsolute", "irrabs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandRotateabsolute));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCopyObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inReferenceObject, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCountU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCountV, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAimVector, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalVector, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDPNormalAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDPTangentAttributeName, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConnectType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConnectUVSwitch, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentMode, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentVector, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomRotate, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsRandScaleAbsolute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsRandRotateabsolute, outDC));

    return MS::kSuccess;
}

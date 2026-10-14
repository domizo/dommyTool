//
//  DCMotionPath.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/03.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCMotionPath.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DPData.h"
#include "dcurve_ids.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DAttributePublication.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

bool isFiniteVector(const MVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool makeMotionPathFrame(MVector& tangent, MVector& normal, MVector& binormal) {
    if (!isFiniteVector(tangent) || tangent.length() <= 1.0e-12) {
        return false;
    }
    tangent /= tangent.length();

    if (!isFiniteVector(normal))
        normal = MVector::zero;
    normal -= tangent * (normal * tangent);
    if (normal.length() <= 1.0e-12) {
        const MVector fallback =
            std::fabs(tangent * MVector::yAxis) < 0.9 ? MVector::yAxis : MVector::xAxis;
        normal = fallback - tangent * (fallback * tangent);
    }
    if (!isFiniteVector(normal) || normal.length() <= 1.0e-12) {
        return false;
    }
    normal /= normal.length();

    binormal = tangent ^ normal;
    if (!isFiniteVector(binormal) || binormal.length() <= 1.0e-12) {
        return false;
    }
    binormal /= binormal.length();
    normal = binormal ^ tangent;
    if (!isFiniteVector(normal) || normal.length() <= 1.0e-12) {
        return false;
    }
    normal /= normal.length();
    return true;
}

} // namespace

const MTypeId DCMotionPath::typeId(ID_DC_MOTION_PATH);

MObject DCMotionPath::inDC;

MObject DCMotionPath::aType;
MObject DCMotionPath::aPathPosition;
MObject DCMotionPath::aPositionAttribute;

MObject DCMotionPath::aOffset;
MObject DCMotionPath::aSpeed;
MObject DCMotionPath::aOffsetAttribute;
MObject DCMotionPath::aSpeedAttribute;

MObject DCMotionPath::aWrapMode;

MObject DCMotionPath::aTangentMode;
MObject DCMotionPath::aTangentVector;

MObject DCMotionPath::aIsExportNormal;
MObject DCMotionPath::aNormalAttribute;

MObject DCMotionPath::aIsExportTangent;
MObject DCMotionPath::aTangentAttribute;

MObject DCMotionPath::aIsExportParam;
MObject DCMotionPath::aParamAttribute;

MObject DCMotionPath::outDP;

void* DCMotionPath::creator() {
    return ::dommy::maya::createObject("DCMotionPath", [&]() {
        return new DCMotionPath();
    });
}

MStatus DCMotionPath::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDP))
        return MS::kUnknownParameter;

    const DCurveData* inputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));
    if (!inputCurves)
        return MS::kFailure;

    MStatus status;
    const MDataHandle calculationTypeHandle = data.inputValue(aType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short calculationType = calculationTypeHandle.asShort();
    const MDataHandle pathPositionHandle = data.inputValue(aPathPosition, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double pathPosition = pathPositionHandle.asDouble();
    const MDataHandle offsetValueHandle = data.inputValue(aOffset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double offsetValue = offsetValueHandle.asDouble();
    const MDataHandle speedValueHandle = data.inputValue(aSpeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double speedValue = speedValueHandle.asDouble();
    const MDataHandle wrapModeHandle = data.inputValue(aWrapMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short wrapMode = wrapModeHandle.asShort();
    const MDataHandle tangentModeHandle = data.inputValue(aTangentMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short tangentMode = tangentModeHandle.asShort();

    if (calculationType < 0 || calculationType > 1 || wrapMode < 0 || wrapMode > 2 ||
        tangentMode < 0 || tangentMode > 1 || !std::isfinite(pathPosition) ||
        !std::isfinite(offsetValue) || !std::isfinite(speedValue)) {
        return MS::kInvalidParameter;
    }

    const MDataHandle positionAttributeHandle = data.inputValue(aPositionAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string positionAttribute = positionAttributeHandle.asString().asChar();
    const MDataHandle offsetAttributeHandle = data.inputValue(aOffsetAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string offsetAttribute = offsetAttributeHandle.asString().asChar();
    const MDataHandle speedAttributeHandle = data.inputValue(aSpeedAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string speedAttribute = speedAttributeHandle.asString().asChar();

    const MDataHandle tangentComponentsHandle = data.inputValue(aTangentVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float3& tangentComponents = tangentComponentsHandle.asFloat3();
    const MVector requestedNormal(tangentComponents[0], tangentComponents[1], tangentComponents[2]);
    if (!isFiniteVector(requestedNormal))
        return MS::kInvalidParameter;

    const MDataHandle exportNormalHandle = data.inputValue(aIsExportNormal, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool exportNormal = exportNormalHandle.asBool();
    const MDataHandle exportTangentHandle = data.inputValue(aIsExportTangent, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool exportTangent = exportTangentHandle.asBool();
    const MDataHandle exportParameterHandle = data.inputValue(aIsExportParam, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool exportParameter = exportParameterHandle.asBool();
    const MDataHandle normalAttributeHandle = data.inputValue(aNormalAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string normalAttribute = normalAttributeHandle.asString().asChar();
    const MDataHandle tangentAttributeHandle = data.inputValue(aTangentAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string tangentAttribute = tangentAttributeHandle.asString().asChar();
    const MDataHandle parameterAttributeHandle = data.inputValue(aParamAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string parameterAttribute = parameterAttributeHandle.asString().asChar();

    // Empty names suppress an export. Only enabled, named outputs share keys.
    const bool writeNormal = exportNormal && !normalAttribute.empty();
    const bool writeTangent = exportTangent && !tangentAttribute.empty();
    const bool writeParameter = exportParameter && !parameterAttribute.empty();
    const auto validOutputName = [](const std::string& name) {
        return dommy::attribute::validAttributeName(name) &&
               name.size() <= dommy::generation::kMaxGeneratedStringByteCount;
    };
    if ((writeNormal && !validOutputName(normalAttribute)) ||
        (writeTangent && !validOutputName(tangentAttribute)) ||
        (writeParameter && !validOutputName(parameterAttribute)) ||
        (writeNormal && writeTangent && normalAttribute == tangentAttribute) ||
        (writeNormal && writeParameter && normalAttribute == parameterAttribute) ||
        (writeTangent && writeParameter && tangentAttribute == parameterAttribute)) {
        return MS::kInvalidParameter;
    }

    const std::size_t curveCount = inputCurves->numberOfCurves();
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount ||
        inputCurves->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }
    MDoubleArray pathPositions, offsets, speeds;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        curveCount,
        [&](unsigned int, double& value) {
            value = pathPosition;
            return MS::kSuccess;
        },
        pathPositions));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        curveCount,
        [&](unsigned int, double& value) {
            value = offsetValue;
            return MS::kSuccess;
        },
        offsets));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        curveCount,
        [&](unsigned int, double& value) {
            value = speedValue;
            return MS::kSuccess;
        },
        speeds));
    if (calculationType == 1 && inputCurves->isExistAttr(positionAttribute)) {
        DCGetMDoubleArrayFromCurveAttribute(*inputCurves, positionAttribute, pathPositions);
    }
    if (inputCurves->isExistAttr(offsetAttribute)) {
        DCGetMDoubleArrayFromCurveAttribute(*inputCurves, offsetAttribute, offsets);
    }
    if (inputCurves->isExistAttr(speedAttribute)) {
        DCGetMDoubleArrayFromCurveAttribute(*inputCurves, speedAttribute, speeds);
    }
    if (pathPositions.length() != curveCount || offsets.length() != curveCount ||
        speeds.length() != curveCount) {
        return MS::kFailure;
    }

    MObject outputObject;
    DPData* outputPoints = NULL;
    status = dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, outputPoints);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputPoints)
        return MS::kFailure;

    MPointArray points;
    std::vector<Imath::V3f> normals;
    std::vector<Imath::V3f> tangents;
    std::vector<double> parameters;
    normals.reserve(curveCount);
    tangents.reserve(curveCount);
    parameters.reserve(curveCount);

    MVector previousBinormal;
    bool hasPreviousBinormal = false;
    auto curve = inputCurves->beginCurves();
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        curveCount,
        [&](unsigned int curveIndex, MPoint& point) -> MStatus {
            if (curve == inputCurves->endCurves() || !*curve ||
                !std::isfinite(pathPositions[curveIndex]) || !std::isfinite(offsets[curveIndex]) ||
                !std::isfinite(speeds[curveIndex])) {
                return MS::kInvalidParameter;
            }

            const MMatrix matrix = (*curve)->getTransformMatrix();
            if (!dommy::numeric::isFiniteAffineTransform(matrix))
                return MS::kInvalidParameter;
            const auto toWorldDirection = [&](MVector& direction, bool allowDegenerate) -> bool {
                std::array<double, 3> transformed;
                if (!dommy::numeric::checkedTransformDirection(direction, matrix, transformed))
                    return false;
                if (!dommy::curve::normalizeDirection(transformed)) {
                    if (!allowDegenerate)
                        return false;
                    transformed = {{0.0, 0.0, 0.0}};
                }
                direction = MVector(transformed[0], transformed[1], transformed[2]);
                return true;
            };

            MObject curveObject;
            DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*curve, curveObject));
            MFnNurbsCurve curveFn(curveObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);

            const double curveLength = curveFn.length(1.0e-6, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            double domainStart = 0.0;
            double domainEnd = 0.0;
            DOMMY_RETURN_IF_FAILURE(curveFn.getKnotDomain(domainStart, domainEnd));
            if (!std::isfinite(curveLength) || curveLength <= 1.0e-12 ||
                !std::isfinite(domainStart) || !std::isfinite(domainEnd) ||
                domainEnd - domainStart <= 1.0e-12) {
                return MS::kInvalidParameter;
            }

            double lengthRatio = pathPositions[curveIndex];
            if (wrapMode == 1) {
                lengthRatio = std::max(0.0, std::min(1.0, lengthRatio));
            } else if (wrapMode == 2 &&
                       !dommy::numeric::wrapPeriodic(lengthRatio, 0.0, 1.0, lengthRatio)) {
                return MS::kInvalidParameter;
            }
            const double partialLength = curveLength * lengthRatio;
            if (!std::isfinite(partialLength))
                return MS::kInvalidParameter;

            double parameter = curveFn.findParamFromLength(partialLength, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (wrapMode == 2) {
                if (!dommy::numeric::wrapPeriodicProductSum(
                        std::array<double, 2>{{parameter, offsets[curveIndex]}},
                        std::array<double, 2>{{1.0, speeds[curveIndex]}},
                        domainStart, domainEnd, parameter)) return MS::kInvalidParameter;
            } else {
                // None/Clamp preserve the historical multiplication/addition
                // rounding and rejection of an unrepresentable intermediate.
                const double parameterOffset = offsets[curveIndex] * speeds[curveIndex];
                if (!std::isfinite(parameter) || !std::isfinite(parameterOffset) ||
                    !std::isfinite(parameter + parameterOffset)) return MS::kInvalidParameter;
                parameter += parameterOffset;
                if (wrapMode == 1)
                    parameter = std::max(domainStart, std::min(domainEnd, parameter));
            }

            DOMMY_RETURN_IF_FAILURE(curveFn.getPointAtParam(parameter, point, MSpace::kObject));
            std::array<double, 3> worldPoint;
            if (!dommy::maya::isFinitePoint(point) ||
                !dommy::numeric::checkedTransformPoint(point, matrix, worldPoint))
                return MS::kInvalidParameter;
            point = MPoint(worldPoint[0], worldPoint[1], worldPoint[2]);
            if (!dommy::maya::isFloatRepresentablePoint(point))
                return MS::kInvalidParameter;

            MVector tangent = curveFn.tangent(parameter, MSpace::kObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!toWorldDirection(tangent, false))
                return MS::kInvalidParameter;
            MVector normal = requestedNormal;
            if (tangentMode == 0) {
                if (hasPreviousBinormal) {
                    normal = previousBinormal ^ tangent;
                } else {
                    normal = curveFn.normal(parameter, MSpace::kObject, &status);
                    // A straight section may not have a normal; other host errors
                    // must not be mistaken for that geometric fallback.
                    if (status == MS::kInvalidParameter)
                        normal = MVector::zero;
                    else
                        DOMMY_RETURN_IF_FAILURE(status);
                }
            }
            // Curve/requested normals are primitive-local directions, not surface
            // normals. Transported normals already use the common world frame.
            if (!(tangentMode == 0 && hasPreviousBinormal) && !toWorldDirection(normal, true))
                return MS::kInvalidParameter;

            MVector binormal;
            if (!makeMotionPathFrame(tangent, normal, binormal)) {
                return MS::kInvalidParameter;
            }

            double normalizedParameter = 0.0;
            if (!dommy::numeric::normalizeRange(parameter, domainStart, domainEnd,
                                                normalizedParameter)) {
                return MS::kInvalidParameter;
            }

            normals.push_back(Imath::V3f(static_cast<float>(normal.x), static_cast<float>(normal.y),
                                         static_cast<float>(normal.z)));
            tangents.push_back(Imath::V3f(static_cast<float>(tangent.x),
                                          static_cast<float>(tangent.y),
                                          static_cast<float>(tangent.z)));
            parameters.push_back(normalizedParameter);
            previousBinormal = binormal;
            hasPreviousBinormal = true;
            ++curve;
            return MS::kSuccess;
        },
        points));
    if (curve != inputCurves->endCurves() || points.length() != curveCount ||
        normals.size() != curveCount || tangents.size() != curveCount ||
        parameters.size() != curveCount) {
        return MS::kFailure;
    }

    outputPoints->setPosition(points);
    outputPoints->updateBBox();
    if (writeNormal) {
        DPAttributeData::Ptr candidate =
            std::make_shared<DPVec3fAttr>(normals, Imath::V3f(0.0F, 0.0F, 0.0F));
        if (!dommy::attribute::commitStagedAttribute(*outputPoints, normalAttribute, candidate,
                                                    curveCount)) return MS::kFailure;
    }
    if (writeTangent) {
        DPAttributeData::Ptr candidate =
            std::make_shared<DPVec3fAttr>(tangents, Imath::V3f(0.0F, 0.0F, 0.0F));
        if (!dommy::attribute::commitStagedAttribute(*outputPoints, tangentAttribute, candidate,
                                                    curveCount)) return MS::kFailure;
    }
    if (writeParameter) {
        DPAttributeData::Ptr candidate = std::make_shared<DPDoubleAttr>(parameters, 0.0);
        if (!dommy::attribute::commitStagedAttribute(*outputPoints, parameterAttribute, candidate,
                                                    curveCount)) return MS::kFailure;
    }
    if (outputPoints->numberOfPoints() != curveCount || !outputPoints->hasValidPointTopology()) {
        return MS::kInvalidParameter;
    }

    MDataHandle output = data.outputValue(outDP, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputObject, *outputPoints));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCMotionPath")

MStatus DCMotionPath::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute typeEnumFn;
    aType = typeEnumFn.create("Type", "ty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField("Position Parameter", 0));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));

    aPathPosition = nAttr.create("PathPosition", "ptprm", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPathPosition));

    const MObject aPositionAttributeDefault = fnStringData.create("param", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPositionAttribute = tAttr.create("PositionAttribute", "pattr", MFnData::kString,
                                      aPositionAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPositionAttribute));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    const MObject aOffsetAttributeDefault = fnStringData.create("offset", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOffsetAttribute = tAttr.create("OffsetAttribute", "offattr", MFnData::kString,
                                    aOffsetAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetAttribute));

    aSpeed = nAttr.create("Speed", "spd", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpeed));

    const MObject aSpeedAttributeDefault = fnStringData.create("speed", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSpeedAttribute =
        tAttr.create("SpeedAttribute", "spdattr", MFnData::kString, aSpeedAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpeedAttribute));

    MFnEnumAttribute wrapEnumFn;
    aWrapMode = wrapEnumFn.create("WrapMode", "wmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("Clamp", 1));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("Loop", 2));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWrapMode));

    MFnEnumAttribute tanEnumFn;
    aTangentMode = tanEnumFn.create("TangentMode", "tanmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.addField("From Curve", 0));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.addField("Tangent Vector", 1));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentMode));

    aTangentVector = nAttr.create("TangentVector", "tanvec", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));

    aIsExportNormal = nAttr.create("IsExportNormal", "isexn", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportNormal));

    const MObject aNormalAttributeDefault = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNormalAttribute =
        tAttr.create("NormalAttribute", "nattr", MFnData::kString, aNormalAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttribute));

    aIsExportTangent =
        nAttr.create("IsExportTangent", "isext", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportTangent));

    const MObject aTangentAttributeDefault = fnStringData.create("T", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTangentAttribute = tAttr.create("TangentAttribute", "tattr", MFnData::kString,
                                     aTangentAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentAttribute));

    aIsExportParam = nAttr.create("IsExportParam", "isexp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportParam));

    const MObject aParamAttributeDefault = fnStringData.create("param", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aParamAttribute =
        tAttr.create("ParamAttribute", "paraattr", MFnData::kString, aParamAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aParamAttribute));

    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPathPosition, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPositionAttribute, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpeedAttribute, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWrapMode, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentMode, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentVector, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsExportNormal, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsExportTangent, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentAttribute, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsExportParam, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aParamAttribute, outDP));

    return MS::kSuccess;
}

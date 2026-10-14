//
//  DCOffset.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/13.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCOffset.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DPointTransform.h"

#include <cmath>
#include <vector>

#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"



const MTypeId DCOffset::typeId( ID_DC_OFFSET );

MObject DCOffset::inDC;

MObject DCOffset::aDirectionType;
MObject DCOffset::aOffsetVector;
MObject DCOffset::aOffsetVectorAttribute;

MObject DCOffset::aUseAimVector;
MObject DCOffset::aAimVector;
MObject DCOffset::aUpVector;


MObject DCOffset::aDsitance;

MObject DCOffset::aSubdivide;

MObject DCOffset::aCheckOverlap;
MObject DCOffset::aTolerance;

MObject DCOffset::outDC;


void *DCOffset::creator()
{
    return ::dommy::maya::createObject("DCOffset", [&]() {
        return new DCOffset();
    });
}


MStatus DCOffset::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aDirectionType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short directionType = handle.asShort();
    handle = data.inputValue(aOffsetVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MVector offsetVector = handle.asVector();
    handle = data.inputValue(aOffsetVectorAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string offsetAttribute = handle.asString().asChar();
    handle = data.inputValue(aUseAimVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useAimVector = handle.asBool();
    handle = data.inputValue(aAimVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector aimVector = handle.asVector();
    handle = data.inputValue(aUpVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector upVector = handle.asVector();
    handle = data.inputValue(aDsitance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double distance = handle.asDouble();
    handle = data.inputValue(aSubdivide, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int subdivision = handle.asInt();
    handle = data.inputValue(aCheckOverlap, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool checkOverlap = handle.asBool();
    handle = data.inputValue(aTolerance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double tolerance = handle.asDouble();
    if (directionType < 0 || directionType > 2 || subdivision < 0 ||
        !std::isfinite(distance) || !std::isfinite(tolerance) || tolerance <= 0.0 ||
        !dommy::maya::isFiniteVector(offsetVector) ||
        !dommy::maya::isFiniteVector(aimVector) || !dommy::maya::isFiniteVector(upVector))
        return MS::kInvalidParameter;

    MVector xAxis = useAimVector ? upVector : MVector(1.0, 0.0, 0.0);
    MVector yAxis = useAimVector ? aimVector : MVector(0.0, 1.0, 0.0);
    if (distance != 0.0) {
        if (!dommy::curve::normalizeDirection(xAxis) || !dommy::curve::normalizeDirection(yAxis))
            return MS::kInvalidParameter;
        MVector crossAxis = xAxis ^ yAxis;
        if (!dommy::curve::normalizeDirection(crossAxis) ||
            (directionType == 1 && !dommy::curve::normalizeDirection(offsetVector)))
            return MS::kInvalidParameter;
    }

    std::vector<std::size_t> spanCounts;
    spanCounts.reserve(input->numberOfCurves());
    std::size_t plannedPoints = 0U;
    for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve) {
        std::size_t spans = 0U;
        if (!dommy::curve::appendOffsetRebuildPlan((*curve)->numberOfPoints(),
                (*curve)->getDegree(), subdivision, plannedPoints, spans)) {
            MGlobal::displayError("DCOffset: curve rebuild exceeds the safe generation budget.");
            return MS::kInvalidParameter;
        }
        spanCounts.push_back(spans);
    }

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(input->numberOfCurves());
    std::size_t curveIndex = 0U, actualPoints = 0U;
    for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve, ++curveIndex) {
        const bool isOpen = (*curve)->isCurveOpen();
        const MMatrix primitiveMatrix = (*curve)->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(primitiveMatrix)) return MS::kInvalidParameter;
        MObject sourceObject;
        status = generateMCurveFromDcurve(*curve, sourceObject);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        MFnNurbsCurve sourceCurve(sourceObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MObject rebuiltObject = sourceCurve.rebuild(
            static_cast<unsigned int>(spanCounts[curveIndex]), (*curve)->getDegree(),
            1, 0, true, true, false, 0.001f, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        MFnNurbsCurve rebuiltCurve(rebuiltObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int rawPointCount = rebuiltCurve.numCVs(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int degree = rebuiltCurve.degree(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        std::size_t pointCount = 0U, curvePointBudget = 0U;
        if (!dommy::generation::checkedCount(rawPointCount,
                dommy::generation::kMaxGeneratedPointCount, pointCount) ||
            pointCount < 2U || degree < 1 ||
            !dommy::generation::checkedSum(pointCount, 1U,
                dommy::generation::kMaxGeneratedPointCount, curvePointBudget) ||
            !dommy::generation::checkedSum(actualPoints, curvePointBudget,
                plannedPoints, actualPoints)) return MS::kInvalidParameter;
        MPointArray points;
        status = rebuiltCurve.getCVs(points, MSpace::kObject);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (points.length() != pointCount) return MS::kFailure;
        for (unsigned int i = 0U; i < points.length(); ++i)
            if (!dommy::maya::isFloatRepresentablePoint(points[i])) return MS::kInvalidParameter;

        MVector attributeVector;
        if (directionType == 2 && distance != 0.0) {
            std::vector<Imath::V3d> values;
            if (!dommy::attribute::convertVector3Attribute((*curve)->getAttr(offsetAttribute),
                    (*curve)->numberOfPoints(), values) || values.empty())
                return MS::kInvalidParameter;
            // The historical mode uses the first point's vector for the curve.
            attributeVector = MVector(values[0U][0], values[0U][1], values[0U][2]);
            if (!dommy::curve::normalizeDirection(attributeVector)) return MS::kInvalidParameter;
        }

        MVector previousBinormal;
        MPoint basePoint;
        MPointArray offsetPoints;
        status = dommy::maya::buildCheckedArray(points.length(),
            [&](unsigned int i, MPoint& result) -> MStatus {
                // Still rebuild at distance zero, but no frame is needed.
                if (distance == 0.0) { result = points[i]; return MS::kSuccess; }
                MVector tangent, normal, binormal;
                MStatus frameStatus = dommy::curve::frameAtClosestPoint(
                    rebuiltCurve, points[i], i == 0U ? nullptr : &previousBinormal,
                    kMFnNurbsEpsilon, MSpace::kObject, tangent, normal, binormal);
                if (!frameStatus) return frameStatus;
                if (i == 0U) {
                    const MVector direction = directionType == 0 ? normal
                        : (directionType == 1 ? offsetVector : attributeVector);
                    basePoint = MPoint(direction * distance);
                }
                const MQuaternion yRotation = yAxis.rotateTo(tangent);
                const MQuaternion xRotation = xAxis.rotateBy(yRotation).rotateTo(normal);
                if (!dommy::maya::isFiniteQuaternion(xRotation) ||
                    !dommy::maya::isFiniteQuaternion(yRotation)) return MS::kInvalidParameter;
                MTransformationMatrix transform;
                frameStatus = transform.addRotationQuaternion(
                    xRotation.x, xRotation.y, xRotation.z, xRotation.w, MSpace::kObject);
                if (!frameStatus) return frameStatus;
                frameStatus = transform.addRotationQuaternion(
                    yRotation.x, yRotation.y, yRotation.z, yRotation.w, MSpace::kObject);
                if (!frameStatus) return frameStatus;
                frameStatus = transform.addTranslation(MVector(points[i]), MSpace::kWorld);
                if (!frameStatus) return frameStatus;
                const MMatrix matrix = transform.asMatrix();
                if (!dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
                const MPoint candidate = basePoint * matrix;
                if (!dommy::maya::isFloatRepresentablePoint(candidate)) return MS::kInvalidParameter;
                previousBinormal = binormal;
                result = candidate;
                return MS::kSuccess;
            }, offsetPoints);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        std::vector<MPoint> retained;
        retained.reserve(curvePointBudget);
        if (checkOverlap && distance != 0.0 && offsetPoints.length() > 2U) {
            std::vector<bool> inner(offsetPoints.length(), false);
            for (unsigned int i = 0U; i < offsetPoints.length(); ++i) {
                MPoint closest;
                double parameter = 0.0;
                status = dommy::curve::closestPointAndParameter(rebuiltCurve, offsetPoints[i],
                    kMFnNurbsEpsilon, MSpace::kObject, closest, parameter);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                const double curveDistance = closest.distanceTo(offsetPoints[i]);
                const double difference = std::fabs(std::fabs(distance) - curveDistance);
                if (!std::isfinite(curveDistance) || !std::isfinite(difference))
                    return MS::kInvalidParameter;
                inner[i] = curveDistance < std::fabs(distance) && difference > tolerance;
            }
            // Preserve the historical overlap-selection/transition rule.
            bool previousInner = false;
            for (unsigned int i = 0U; i < offsetPoints.length(); ++i) {
                if (inner[i]) { previousInner = true; continue; }
                if (previousInner) {
                    MPoint closest;
                    double parameter = 0.0;
                    status = dommy::curve::closestPointAndParameter(rebuiltCurve, offsetPoints[i - 1U],
                        kMFnNurbsEpsilon, MSpace::kObject, closest, parameter);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    MVector direction = offsetPoints[i - 1U] - closest;
                    if (!dommy::curve::normalizeDirection(direction)) return MS::kInvalidParameter;
                    const MPoint boundary = closest + direction * std::fabs(distance);
                    if (!dommy::maya::isFloatRepresentablePoint(boundary)) return MS::kInvalidParameter;
                    retained.push_back(boundary);
                } else {
                    retained.push_back(offsetPoints[i]);
                }
                previousInner = false;
            }
            // A completely removed curve is an intentional empty result.
            if (retained.empty()) continue;
        } else {
            for (unsigned int i = 0U; i < offsetPoints.length(); ++i)
                retained.push_back(offsetPoints[i]);
        }
        if (retained.size() < 2U) return MS::kInvalidParameter;
        const bool topologyChanged = retained.size() != points.length();
        if (!isOpen) {
            if (topologyChanged) {
                if (retained.back() != retained.front()) retained.push_back(retained.front());
            } else {
                retained.back() = retained.front();
            }
        }
        MPointArray resultPoints;
        status = dommy::geometry::copyPointsToMayaArray(retained, resultPoints);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        DCItem generated;
        if (topologyChanged) {
            // Point removal changes knot cardinality; never reuse the old knots.
            generated.reset(new DCurveInternalData(resultPoints, degree, isOpen));
        } else {
            MDoubleArray knots;
            status = rebuiltCurve.getKnots(knots);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            generated.reset(new DCurveInternalData(resultPoints, knots, degree, isOpen));
        }
        generated->setTransformMatrix(primitiveMatrix);
        if (!generated->hasValidCurveTopology()) return MS::kInvalidParameter;
        generatedCurves.push_back(generated);
    }
    if (!outputCurves->insert(generatedCurves)) return MS::kFailure;
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCOffset")
        
        
        
        


MStatus DCOffset::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute dirEnumFn;
    aDirectionType = dirEnumFn.create("DirectionType", "dirt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField("Curve Normal", 0));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField("Use Offset Vector", 1));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField("From Attribute", 2));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionType));

    aOffsetVector = nAttr.create("OffsetVector", "ovec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetVector));

    const MObject aOffsetVectorAttributeDefault = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOffsetVectorAttribute = tAttr.create("OffsetVectorAttribute", "ovecattrn", MFnData::kString,
                                          aOffsetVectorAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetVectorAttribute));

    aUseAimVector = nAttr.create("UseAimVector", "uavec", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseAimVector));

    aAimVector = nAttr.create("AimVector", "avec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAimVector));

    aUpVector = nAttr.create("UpVector", "uvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpVector));

    aDsitance = nAttr.create("Dsitance", "dist", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDsitance));

    aSubdivide = nAttr.create("Subdivide", "sbd", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivide));

    aCheckOverlap = nAttr.create("CheckOverlap", "chkovl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCheckOverlap));

    aTolerance = nAttr.create("Tolerance", "tol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDirectionType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetVector, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetVectorAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseAimVector, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAimVector, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpVector, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDsitance, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSubdivide, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCheckOverlap, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, outDC));

    return MS::kSuccess;
}

//
//  DCurveFromCurve.cpp
//  dommyTool
//
//  Created by yuzo on 2016/05/04.
//

#include "DCurveFromCurve.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DCurveFromCurvePlan.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DMayaHeaders.h"
#include "DNodeOperation.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DRandomValueGeneration.h"
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

bool isFinitePoint(const MPoint& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
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

bool makeOrthonormalFrame(MVector& tangent,
                          MVector& normal,
                          MVector& previousBinormal)
{
    if (!isFiniteVector(tangent) || tangent.length() <= 1.0e-12) {
        return false;
    }
    tangent /= tangent.length();

    if (!isFiniteVector(normal)) normal = MVector::zero;
    normal -= tangent * (normal * tangent);
    if (normal.length() <= 1.0e-12) {
        const MVector axis = std::fabs(tangent * MVector::yAxis) < 0.9
            ? MVector::yAxis
            : MVector::xAxis;
        normal = axis - tangent * (axis * tangent);
    }
    if (!isFiniteVector(normal) || normal.length() <= 1.0e-12) {
        return false;
    }
    normal /= normal.length();

    MVector binormal = tangent ^ normal;
    if (!isFiniteVector(binormal) || binormal.length() <= 1.0e-12) {
        return false;
    }
    binormal /= binormal.length();
    normal = binormal ^ tangent;
    if (!isFiniteVector(normal) || normal.length() <= 1.0e-12) {
        return false;
    }
    normal /= normal.length();
    previousBinormal = binormal;
    return true;
}

bool makeSubdividedCvSamples(const MPointArray& sourceCvs,
                             std::size_t divideCount,
                             std::size_t expectedCount,
                             std::vector<MPoint>& samples)
{
    if (sourceCvs.length() < 2U) return false;
    samples.clear();
    samples.reserve(expectedCount);
    const double denominator = static_cast<double>(divideCount + 1U);
    for (unsigned int cvIndex = 0U;
         cvIndex + 1U < sourceCvs.length(); ++cvIndex) {
        if (!isFinitePoint(sourceCvs[cvIndex]) ||
            !isFinitePoint(sourceCvs[cvIndex + 1U])) {
            return false;
        }
        samples.push_back(sourceCvs[cvIndex]);
        for (std::size_t division = 0U;
             division < divideCount; ++division) {
            const double weight =
                static_cast<double>(division + 1U) / denominator;
            samples.push_back(
                sourceCvs[cvIndex] +
                (sourceCvs[cvIndex + 1U] - sourceCvs[cvIndex]) *
                    weight);
        }
    }
    samples.push_back(sourceCvs[sourceCvs.length() - 1U]);
    return samples.size() == expectedCount;
}

} // namespace

const MTypeId DCurveFromCurve::typeId(ID_DC_FROM_CURVE_GEN);

MObject DCurveFromCurve::inCurve;
MObject DCurveFromCurve::inDC;
MObject DCurveFromCurve::aProfileDP;
MObject DCurveFromCurve::aInTangentCurve;
MObject DCurveFromCurve::aSamplingType;
MObject DCurveFromCurve::sampling;
MObject DCurveFromCurve::aCurveDivide;
MObject DCurveFromCurve::offset;
MObject DCurveFromCurve::curveLength;
MObject DCurveFromCurve::aDegree;
MObject DCurveFromCurve::aIsMultiEndKnot;
MObject DCurveFromCurve::twist;
MObject DCurveFromCurve::tubeWidth;
MObject DCurveFromCurve::aTwistScale;
MObject DCurveFromCurve::aTubeScale;
MObject DCurveFromCurve::aSeed;
MObject DCurveFromCurve::aUseRandomRotation;
MObject DCurveFromCurve::aRandRotation;
MObject DCurveFromCurve::aUseRandomRadius;
MObject DCurveFromCurve::aRandRadius;
MObject DCurveFromCurve::outDC;
MObject DCurveFromCurve::aUseCachedParam;

void* DCurveFromCurve::creator()
{
    return ::dommy::maya::createObject("DCurveFromCurve", [&]() {
        return new DCurveFromCurve();
    });
}

void DCurveFromCurve::postConstructor() {
    dommy::maya::initializeNodeRamps("DCurveFromCurve", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), twist, {1.0f, 1.0f}, {0.0f, 1.0f},
            {MRampAttribute::kLinear, MRampAttribute::kLinear}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), tubeWidth, {1.0f}, {0.0f}, {MRampAttribute::kLinear}));
        return MS::kSuccess;
    });
}

MStatus DCurveFromCurve::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle samplingTypeHandle = data.inputValue(aSamplingType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short samplingType = samplingTypeHandle.asShort();
    const MDataHandle curveLengthScaleHandle = data.inputValue(curveLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double curveLengthScale = curveLengthScaleHandle.asDouble();
    const MDataHandle requestedSamplingHandle = data.inputValue(sampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int requestedSampling = requestedSamplingHandle.asInt();
    const MDataHandle curveDivideHandle = data.inputValue(aCurveDivide, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int curveDivide = curveDivideHandle.asInt();
    const MDataHandle parameterOffsetHandle = data.inputValue(offset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double parameterOffset = parameterOffsetHandle.asDouble();
    const MDataHandle degreeTypeHandle = data.inputValue(aDegree, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short degreeType = degreeTypeHandle.asShort();
    const MDataHandle useMultipleEndKnotsHandle = data.inputValue(aIsMultiEndKnot, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useMultipleEndKnots = useMultipleEndKnotsHandle.asBool();
    const MDataHandle twistScaleHandle = data.inputValue(aTwistScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double twistScale = twistScaleHandle.asDouble();
    const MDataHandle tubeScaleHandle = data.inputValue(aTubeScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double tubeScale = tubeScaleHandle.asDouble();
    const MDataHandle seedHandle = data.inputValue(aSeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int seed = seedHandle.asInt();
    const MDataHandle useRandomRotationHandle = data.inputValue(aUseRandomRotation, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useRandomRotation = useRandomRotationHandle.asBool();
    const MDataHandle useRandomRadiusHandle = data.inputValue(aUseRandomRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useRandomRadius = useRandomRadiusHandle.asBool();
    const MDataHandle randomRotationHandle = data.inputValue(aRandRotation, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double randomRotation = randomRotationHandle.asDouble();
    const MDataHandle randomRadiusHandle = data.inputValue(aRandRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double randomRadius = randomRadiusHandle.asDouble();

    if (!std::isfinite(parameterOffset) || !std::isfinite(twistScale) ||
        !std::isfinite(tubeScale) || !std::isfinite(randomRotation) ||
        !std::isfinite(randomRadius)) {
        return MS::kInvalidParameter;
    }

    // Parameter caching used only vector lengths as a cache key and therefore
    // returned stale parameters after an upstream curve changed shape. Keep the
    // archival attribute for scene compatibility, but always recompute.
    MDataHandle profileHandle = data.inputValue(aProfileDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* profile = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(profileHandle, DPData::id, profile));
    if (!profile || !profile->hasValidPointTopology() || profile->numberOfPoints() == 0U) {
        return MS::kInvalidParameter;
    }

    std::vector<MObject> sourceObjects;
    std::vector<std::size_t> sourceCvCounts;
    const DCurveData* inputCurves = nullptr;
    const MPlug mayaCurvePlug(thisMObject(), inCurve);
    const MPlug dCurvePlug(thisMObject(), inDC);
    const bool mayaCurveConnected = mayaCurvePlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (mayaCurveConnected) {
        MObject curveObject;
        DOMMY_RETURN_IF_FAILURE(DGetMayaCurveFromPlug(inCurve, data, curveObject));
        MFnNurbsCurve curveFn(curveObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int cvCount = curveFn.numCVs(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (cvCount < 0)
            return MS::kFailure;
        sourceCvCounts.push_back(static_cast<std::size_t>(cvCount));
        sourceObjects.push_back(curveObject);
    } else {
        // Query the lower-priority input only when the native input is absent.
        const bool dCurveConnected = dCurvePlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dCurveConnected) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));
        if (inputCurves->numberOfCurves() == 0U) {
            return MS::kInvalidParameter;
        }
        // Count source CVs before allocating any Maya curve geometry.
        sourceCvCounts.reserve(inputCurves->numberOfCurves());
        for (DCurveData::ConstDCurveIter curve = inputCurves->beginCurves();
             curve != inputCurves->endCurves(); ++curve) {
            sourceCvCounts.push_back((*curve)->numberOfPoints());
        }
    }

    dommy::curve::CurveFromCurvePlan plan;
    if (!dommy::curve::makeCurveFromCurvePlan(samplingType, requestedSampling, curveDivide,
                                              degreeType, curveLengthScale,
                                              profile->numberOfPoints(), sourceCvCounts, plan)) {
        return MS::kInvalidParameter;
    }

    if (inputCurves) {
        sourceObjects.reserve(inputCurves->numberOfCurves());
        std::size_t sourceIndex = 0U;
        bool countsChanged = false;
        for (DCurveData::ConstDCurveIter curve = inputCurves->beginCurves();
             curve != inputCurves->endCurves(); ++curve, ++sourceIndex) {
            MObject curveObject;
            DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*curve, curveObject));
            MFnNurbsCurve curveFn(curveObject, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const int cvCount = curveFn.numCVs(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (cvCount < 0)
                return MS::kFailure;
            const std::size_t actualCount = static_cast<std::size_t>(cvCount);
            countsChanged |= sourceCvCounts[sourceIndex] != actualCount;
            sourceCvCounts[sourceIndex] = actualCount;
            sourceObjects.push_back(curveObject);
        }
        // Maya may add CVs when closing a curve. Preserve that geometry and
        // recheck its exact budget before sampling or output allocation.
        if (countsChanged &&
            !dommy::curve::makeCurveFromCurvePlan(
                samplingType, requestedSampling, curveDivide, degreeType, curveLengthScale,
                profile->numberOfPoints(), sourceCvCounts, plan)) {
            return MS::kInvalidParameter;
        }
    }

    MObject tangentObject;
    MFnNurbsCurve tangentFn;
    bool useTangentCurve = false;
    const bool tangentConnected = MPlug(thisMObject(), aInTangentCurve).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (tangentConnected) {
        DOMMY_RETURN_IF_FAILURE(DGetMayaCurveFromPlug(aInTangentCurve, data, tangentObject));
        DOMMY_RETURN_IF_FAILURE(tangentFn.setObject(tangentObject));
        useTangentCurve = true;
    }

    MRampAttribute twistRamp(MPlug(thisMObject(), twist), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MRampAttribute widthRamp(MPlug(thisMObject(), tubeWidth), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    std::vector<double> randomRotations(sourceObjects.size(), 0.0);
    std::vector<double> randomRadii(sourceObjects.size(), 0.0);
    if (useRandomRotation && randomRotation != 0.0) {
        dommy::random::generateDoubleValues(static_cast<std::uint32_t>(seed),
                                            -std::fabs(randomRotation), std::fabs(randomRotation),
                                            sourceObjects.size(), randomRotations);
    }
    if (useRandomRadius && randomRadius != 0.0) {
        int radiusSeed = 0;
        if (!dommy::random::nextNonnegativeSeed(seed, radiusSeed))
            return MS::kInvalidParameter;
        dommy::random::generateDoubleValues(static_cast<std::uint32_t>(radiusSeed), 0.0,
                                            std::fabs(randomRadius), sourceObjects.size(),
                                            randomRadii);
    }

    MFnPluginData pluginData;
    DCurveData* output = NULL;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output)
        return MS::kFailure;

    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(plan.outputCurveCount);
    const DPointList* const profilePoints = profile->getPositionPtrConst();
    const MVector xAxis = MVector::xAxis;
    const MVector yAxis = MVector::yAxis;
    for (std::size_t sourceIndex = 0U; sourceIndex < sourceObjects.size(); ++sourceIndex) {
        MFnNurbsCurve curveFn(sourceObjects[sourceIndex], &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        double domainStart = 0.0;
        double domainEnd = 0.0;
        DOMMY_RETURN_IF_FAILURE(curveFn.getKnotDomain(domainStart, domainEnd));
        const double totalLength = curveFn.length(1.0e-3, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!std::isfinite(domainStart) || !std::isfinite(domainEnd) || domainStart >= domainEnd ||
            !std::isfinite(totalLength) || totalLength <= 1.0e-12) {
            return MS::kInvalidParameter;
        }

        const std::size_t totalSampling = plan.samplingPerSourceCurve[sourceIndex];
        std::vector<MPoint> cvSamples;
        if (plan.samplingType == 1) {
            MPointArray sourceCvs;
            DOMMY_RETURN_IF_FAILURE(curveFn.getCVs(sourceCvs, MSpace::kObject));
            if (!makeSubdividedCvSamples(sourceCvs, plan.curveDivide, totalSampling, cvSamples)) {
                return MS::kFailure;
            }
        }

        std::vector<MMatrix> matrices(totalSampling, MMatrix());
        std::vector<float> twistValues(totalSampling, 0.0f);
        std::vector<float> widthValues(totalSampling, 1.0f);
        MVector previousBinormal = MVector::zero;

        double tangentStart = 0.0;
        double tangentEnd = 0.0;
        if (useTangentCurve) {
            DOMMY_RETURN_IF_FAILURE(tangentFn.getKnotDomain(tangentStart, tangentEnd));
            if (!std::isfinite(tangentStart) || !std::isfinite(tangentEnd) ||
                tangentStart >= tangentEnd) {
                return MS::kInvalidParameter;
            }
        }

        for (std::size_t sampleIndex = 0U; sampleIndex < totalSampling; ++sampleIndex) {
            double parameter = 0.0;
            double rampPosition = 0.0;
            if (plan.samplingType == 0) {
                const double sampleLength =
                    totalLength * plan.curveLengthScale *
                    (static_cast<double>(sampleIndex) / static_cast<double>(totalSampling - 1U));
                parameter = curveFn.findParamFromLength(sampleLength, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                rampPosition = sampleLength / totalLength;
            } else {
                curveFn.closestPoint(cvSamples[sampleIndex], &parameter, kMFnNurbsEpsilon,
                                     MSpace::kObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                const double sampleLength = curveFn.findLengthFromParam(parameter, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                rampPosition = sampleLength / totalLength;
            }
            if (!std::isfinite(parameter) || !std::isfinite(rampPosition)) {
                return MS::kFailure;
            }
            rampPosition = std::max(0.0, std::min(1.0, rampPosition));

            float twistValue = 0.0f;
            float widthValue = 1.0f;
            twistRamp.getValueAtPosition(static_cast<float>(rampPosition), twistValue, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            widthRamp.getValueAtPosition(static_cast<float>(rampPosition), widthValue, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(twistValue) || !std::isfinite(widthValue)) {
                return MS::kInvalidParameter;
            }
            twistValues[sampleIndex] = twistValue;
            widthValues[sampleIndex] = widthValue;

            const double offsetParameter =
                std::max(domainStart, std::min(domainEnd, parameter + parameterOffset));
            MPoint center;
            DOMMY_RETURN_IF_FAILURE(
                curveFn.getPointAtParam(offsetParameter, center, MSpace::kObject));
            MVector tangent = curveFn.tangent(offsetParameter, MSpace::kObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);

            MVector normal;
            if (useTangentCurve) {
                const double normalizedParameter =
                    (offsetParameter - domainStart) / (domainEnd - domainStart);
                const double tangentParameter =
                    tangentStart + normalizedParameter * (tangentEnd - tangentStart);
                MPoint target;
                DOMMY_RETURN_IF_FAILURE(
                    tangentFn.getPointAtParam(tangentParameter, target, MSpace::kObject));
                normal = target - center;
            } else if (sampleIndex == 0U || previousBinormal.length() <= 1.0e-12) {
                normal = curveFn.normal(offsetParameter, MSpace::kObject, &status);
                if (!status)
                    normal = MVector::zero;
            } else {
                normal = previousBinormal ^ tangent;
            }

            if (!isFinitePoint(center) ||
                !makeOrthonormalFrame(tangent, normal, previousBinormal)) {
                return MS::kFailure;
            }

            const MQuaternion yRotation = yAxis.rotateTo(tangent);
            const MVector rotatedXAxis = xAxis.rotateBy(yRotation);
            const MQuaternion xRotation = rotatedXAxis.rotateTo(normal);
            MTransformationMatrix transform;
            DOMMY_RETURN_IF_FAILURE(transform.addRotationQuaternion(
                xRotation.x, xRotation.y, xRotation.z, xRotation.w, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(transform.addRotationQuaternion(
                yRotation.x, yRotation.y, yRotation.z, yRotation.w, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(transform.addTranslation(MVector(center), MSpace::kWorld));
            matrices[sampleIndex] = transform.asMatrix();
            if (!isFiniteMatrix(matrices[sampleIndex])) {
                return MS::kFailure;
            }
        }

        const short degree = dommy::curve::curveFromCurveDegree(plan.degreeType, totalSampling);
        MDoubleArray knots;
        GenerateDCurveKnots(totalSampling, degree, knots, !useMultipleEndKnots);

        for (std::size_t profileIndex = 0U; profileIndex < profilePoints->size(); ++profileIndex) {
            const MVector basePoint = DPToMV((*profilePoints)[profileIndex]);
            std::vector<MPoint> transformed(totalSampling);
            const tbb::blocked_range<std::size_t> range(0U, totalSampling,
                                                        DGetGlobalProcessNum(totalSampling));
            tbb::parallel_for(range, [&](const tbb::blocked_range<std::size_t>& block) {
                for (std::size_t sampleIndex = block.begin(); sampleIndex != block.end();
                     ++sampleIndex) {
                    const double angle = dommy::math::degreesToRadians(
                        twistValues[sampleIndex] * twistScale + randomRotations[sourceIndex]);
                    const double radius =
                        widthValues[sampleIndex] * tubeScale + randomRadii[sourceIndex];
                    const MVector point = basePoint.rotateBy(MVector::kYaxis, angle) * radius;
                    transformed[sampleIndex] = MPoint(point) * matrices[sampleIndex];
                }
            });

            MPointArray resultCvs;
            DOMMY_RETURN_IF_FAILURE(resultCvs.setLength(static_cast<unsigned int>(totalSampling)));
            for (std::size_t sampleIndex = 0U; sampleIndex < totalSampling; ++sampleIndex) {
                if (!isFinitePoint(transformed[sampleIndex])) {
                    return MS::kFailure;
                }
                resultCvs[static_cast<unsigned int>(sampleIndex)] = transformed[sampleIndex];
            }

            DCItem result(new DCurveInternalData(resultCvs, knots, degree));
            if (!result || !result->hasValidCurveTopology()) {
                return MS::kFailure;
            }
            generatedCurves.push_back(result);
        }
    }

    if (!output->insert(generatedCurves)) return MS::kFailure;
    generatedCurves.clear();
    std::size_t totalOutputPoints = 0U;
    if (!output->hasValidCurveTopology() || output->numberOfCurves() != plan.outputCurveCount ||
        !output->getCheckedTotalCvs(totalOutputPoints) ||
        totalOutputPoints != plan.outputPointCount) {
        return MS::kFailure;
    }
    output->updateBBox();

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveFromCurve")

MStatus DCurveFromCurve::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;

    inCurve = genericAttribute.create("inputCurve", "inc", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));

    inDC = typedAttribute.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aProfileDP = typedAttribute.create("ProfileDP", "prdp", DPData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aProfileDP));

    aInTangentCurve = genericAttribute.create("InTangentCurve", "intc", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInTangentCurve));

    MFnEnumAttribute samplingTypeAttribute;
    aSamplingType = samplingTypeAttribute.create("SamplingType", "smpty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(samplingTypeAttribute.addField("Sampling", 0));
    DOMMY_RETURN_IF_FAILURE(samplingTypeAttribute.addField("InputCurve Divide", 1));
    DOMMY_RETURN_IF_FAILURE(samplingTypeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSamplingType));

    curveLength =
        numericAttribute.create("curveLength", "cl", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1.0e-9));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));

    sampling = numericAttribute.create("sampling", "sa", MFnNumericData::kInt, 8, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(2));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    aCurveDivide =
        numericAttribute.create("CurveDivide", "crvdiv", MFnNumericData::kInt, 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveDivide));

    offset = numericAttribute.create("offset", "off", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));

    MFnEnumAttribute degreeAttribute;
    aDegree = degreeAttribute.create("Degree", "deg", 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Cubic", 1));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    aIsMultiEndKnot = numericAttribute.create("IsMultiEndKnot", "imuedkt", MFnNumericData::kBoolean,
                                              true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMultiEndKnot));

    twist = MRampAttribute::createCurveRamp("twist", "tw", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(twist));
    tubeWidth = MRampAttribute::createCurveRamp("tubeWidth", "tuw", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(tubeWidth));

    aTwistScale =
        numericAttribute.create("TwistScale", "twscl", MFnNumericData::kDouble, 360.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTwistScale));

    aTubeScale =
        numericAttribute.create("TubeScale", "tbscl", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTubeScale));

    aUseRandomRotation = numericAttribute.create("UseRandomRotation", "userndrot",
                                                 MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseRandomRotation));

    aUseRandomRadius = numericAttribute.create("UseRandomRadius", "userndrad",
                                               MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseRandomRadius));

    aSeed = numericAttribute.create("Seed", "se", MFnNumericData::kInt, 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aRandRotation =
        numericAttribute.create("RandRotation", "rndrot", MFnNumericData::kDouble, 360.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandRotation));

    aRandRadius =
        numericAttribute.create("RandRadius", "rndrad", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandRadius));

    outDC = typedAttribute.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    aUseCachedParam = numericAttribute.create("UseCachedParam", "uscah", MFnNumericData::kBoolean,
                                              false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseCachedParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCurve, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aProfileDP, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInTangentCurve, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSamplingType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(curveLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurveDivide, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsMultiEndKnot, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(twist, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(tubeWidth, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTwistScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTubeScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseRandomRotation, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandRotation, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseRandomRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseCachedParam, outDC));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DCurveFromCurve", "initialize", std::current_exception());
}

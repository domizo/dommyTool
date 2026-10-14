//
//  DCAhoGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/01.
//
//

#include "DCAhoGen.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DAhoGenerationPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaEntryPointGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "dommyMath.h"
#include "DRandomGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

MStatus preflightAhoInputStorage(
    const DCurveData& input, dommy::generation::CurveGenerationStorage& storage)
{
    // Only one input curve is converted to native data at a time. Account for
    // that largest CV/knot/native-construction workspace, not upstream DG data.
    std::size_t maximumPoints = 0U;
    for (auto curve = input.beginCurves(); curve != input.endCurves(); ++curve) {
        if (!*curve || !(*curve)->hasValidCurveTopology()) return MS::kInvalidParameter;
        maximumPoints = std::max(maximumPoints, (*curve)->numberOfPoints());
    }
    storage.add(maximumPoints, 128U);
    return MS::kSuccess;
}

} // namespace

const MTypeId DCAhoGen::typeId( ID_DC_AHO_GEN );


MObject DCAhoGen::inDC;

MObject DCAhoGen::aDistributeType;
MObject DCAhoGen::aCount;
MObject DCAhoGen::aSegment;
MObject DCAhoGen::aLength;
MObject DCAhoGen::aAngleType;
MObject DCAhoGen::aAngleSpread;
MObject DCAhoGen::aNormal;
MObject DCAhoGen::aSeed;

MObject DCAhoGen::aLengthRandom;

//MObject DCAhoGen::aBend;

MObject DCAhoGen::aIsStruggle;
MObject DCAhoGen::aStruggleRange;
MObject DCAhoGen::aStruggleStrength;
MObject DCAhoGen::aStruggleRamp;

MObject DCAhoGen::outDC;


void *DCAhoGen::creator() {
    return ::dommy::maya::createObject("DCAhoGen", [&]() {
        return new DCAhoGen();
    });
}


void DCAhoGen::postConstructor() {
    dommy::maya::initializeNodeRamps("DCAhoGen", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aStruggleRamp,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}



MStatus DCAhoGen::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const DCurveData* inputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));
    if (!inputCurves) return MS::kFailure;

    MDataHandle handle = data.inputValue(aDistributeType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short distributeType = handle.asShort();
    handle = data.inputValue(aSeed, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int seed = handle.asInt();
    handle = data.inputValue(aCount, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int count = handle.asInt();
    handle = data.inputValue(aSegment, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int segment = handle.asInt();
    handle = data.inputValue(aLength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double length = handle.asDouble();
    handle = data.inputValue(aAngleSpread, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float angleSpread = handle.asFloat();
    handle = data.inputValue(aAngleType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short angleType = handle.asShort();
    handle = data.inputValue(aNormal, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& rawNormal = handle.asFloat3();
    const MVector baseNormal(rawNormal[0], rawNormal[1], rawNormal[2]);
    handle = data.inputValue(aLengthRandom, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double lengthRandom = handle.asDouble();
    handle = data.inputValue(aIsStruggle, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useStruggle = handle.asBool();
    handle = data.inputValue(aStruggleRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& rawStruggleRange = handle.asFloat3();
    const MVector struggleRange(
        rawStruggleRange[0], rawStruggleRange[1], rawStruggleRange[2]);
    handle = data.inputValue(aStruggleStrength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float struggleStrength = handle.asFloat();

    const double finiteInputs[] = {
        length, angleSpread, baseNormal.x, baseNormal.y, baseNormal.z,
        lengthRandom, struggleRange.x, struggleRange.y, struggleRange.z,
        struggleStrength};
    dommy::generation::AhoGenerationPlan plan;
    if (distributeType < 0 || distributeType > 1 ||
        angleType < 0 || angleType > 2 || seed < 0 ||
        length <= std::numeric_limits<double>::epsilon() ||
        angleSpread < 0.0f || lengthRandom < 0.0 ||
        struggleRange.x < 0.0 || struggleRange.y < 0.0 ||
        struggleRange.z < 0.0 || struggleStrength < 0.0f ||
        baseNormal.length() <= std::numeric_limits<double>::epsilon() ||
        !dommy::generation::buildAhoGenerationPlan(
            inputCurves->numberOfCurves(), count, segment, finiteInputs,
            sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan) ||
        static_cast<std::size_t>(seed) >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) -
                std::max(plan.outputCurveCount, plan.inputCurveCount) - 3U) {
        MGlobal::displayError(
            "DCAhoGen: inputs exceed the safe generation limits.");
        return MS::kInvalidParameter;
    }

    dommy::generation::CurveGenerationStorage storage(plan.estimatedBytes);
    DOMMY_RETURN_IF_FAILURE(preflightAhoInputStorage(*inputCurves, storage));

    MRampAttribute struggleRamp(MPlug(thisMObject(), aStruggleRamp));
    MFloatArray rampValues;
    struggleRamp.sampleValueRamp(
        static_cast<unsigned int>(plan.pointsPerCurve), rampValues, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (rampValues.length() != plan.pointsPerCurve) return MS::kFailure;
    for (unsigned int index = 0U; index < rampValues.length(); ++index) {
        if (!std::isfinite(rampValues[index])) return MS::kInvalidParameter;
    }

    const double pointStep =
        length / static_cast<double>(plan.pointsPerCurve - 1U);
    if (!std::isfinite(pointStep)) return MS::kInvalidParameter;
    MPointArray basePoints;
    for (std::size_t point = 0U; point < plan.pointsPerCurve; ++point) {
        status = basePoints.append(
            MPoint(0.0, pointStep * static_cast<double>(point), 0.0));
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(
        pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MVector normalizedBaseNormal(baseNormal);
    status = normalizedBaseNormal.normalize();
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector yAxis(0.0, 1.0, 0.0);
    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(plan.outputCurveCount);
    std::size_t inputCurveIndex = 0U;
    for (DCurveData::ConstDCurveIter curve = inputCurves->beginCurves();
         curve != inputCurves->endCurves();
         ++curve, ++inputCurveIndex) {
        MObject curveObject;
        status = generateMCurveFromDcurve(*curve, curveObject);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        MFnNurbsCurve curveFunction(curveObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        double start = 0.0;
        double end = 0.0;
        status = curveFunction.getKnotDomain(start, end);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double curveLength =
            curveFunction.length(kMFnNurbsEpsilon, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!std::isfinite(start) || !std::isfinite(end) || end <= start ||
            !std::isfinite(curveLength) ||
            curveLength <= std::numeric_limits<double>::epsilon()) {
            continue;
        }

        const int inputSeed =
            seed + static_cast<int>(inputCurveIndex);
        MDoubleArray positions(
            static_cast<unsigned int>(plan.copiesPerCurve));
        MDoubleArray rollAngles(
            static_cast<unsigned int>(plan.copiesPerCurve));
        if (positions.length() != plan.copiesPerCurve ||
            rollAngles.length() != plan.copiesPerCurve) return MS::kFailure;
        dommy::math::RandomGenerator positionRandom(
            static_cast<unsigned int>(inputSeed));
        dommy::math::RandomGenerator rollRandom(
            static_cast<unsigned int>(inputSeed));
        for (std::size_t copy = 0U; copy < plan.copiesPerCurve; ++copy) {
            const unsigned int copyIndex = static_cast<unsigned int>(copy);
            positions[copyIndex] =
                distributeType == 0
                    ? positionRandom.uniform(0.0, curveLength)
                    : positionRandom.uniform(start, end);
            rollAngles[copyIndex] = rollRandom.uniform(0.0, 360.0);
        }

        MPointArray rootPoints(
            static_cast<unsigned int>(plan.copiesPerCurve));
        MVectorArray tangents(
            static_cast<unsigned int>(plan.copiesPerCurve));
        MVectorArray normals(
            static_cast<unsigned int>(plan.copiesPerCurve));
        MVectorArray rotatedNormals(
            static_cast<unsigned int>(plan.copiesPerCurve));
        if (rootPoints.length() != plan.copiesPerCurve || tangents.length() != plan.copiesPerCurve ||
            normals.length() != plan.copiesPerCurve || rotatedNormals.length() != plan.copiesPerCurve) {
            return MS::kFailure;
        }
        for (std::size_t copy = 0U; copy < plan.copiesPerCurve; ++copy) {
            double parameter = positions[static_cast<unsigned int>(copy)];
            if (distributeType == 0) {
                parameter = curveFunction.findParamFromLength(
                    parameter, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
            }
            if (!std::isfinite(parameter)) return MS::kFailure;

            MPoint rootPoint;
            status = curveFunction.getPointAtParam(
                parameter, rootPoint, MSpace::kWorld);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MVector tangent = curveFunction.tangent(
                parameter, MSpace::kWorld, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(rootPoint.x) || !std::isfinite(rootPoint.y) ||
                !std::isfinite(rootPoint.z) ||
                tangent.length() <= std::numeric_limits<double>::epsilon()) {
                return MS::kFailure;
            }
            status = tangent.normalize();
            CHECK_MSTATUS_AND_RETURN_IT(status);

            MTransformationMatrix rotation;
            // Random roll values are degrees; Maya's rotation API takes radians.
            status = rotation.setToRotationAxis(
                tangent, dommy::math::degreesToRadians(
                    rollAngles[static_cast<unsigned int>(copy)]));
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const MVector rotatedNormal =
                normalizedBaseNormal * rotation.asMatrix();
            if (!std::isfinite(rotatedNormal.x) ||
                !std::isfinite(rotatedNormal.y) ||
                !std::isfinite(rotatedNormal.z)) {
                return MS::kFailure;
            }
            rootPoints[static_cast<unsigned int>(copy)] = rootPoint;
            tangents[static_cast<unsigned int>(copy)] = tangent;
            normals[static_cast<unsigned int>(copy)] = normalizedBaseNormal;
            rotatedNormals[static_cast<unsigned int>(copy)] = rotatedNormal;
        }

        for (std::size_t copy = 0U; copy < plan.copiesPerCurve; ++copy) {
            const int outputSeed = seed + static_cast<int>(copy);
            dommy::math::RandomGenerator random(
                static_cast<unsigned int>(outputSeed));
            MVector angleOffset;
            if (angleSpread != 0.0f) {
                angleOffset = MVector(
                    random.uniform(-angleSpread, angleSpread),
                    random.uniform(-angleSpread, angleSpread),
                    random.uniform(-angleSpread, angleSpread));
            }

            const double randomizedLength =
                lengthRandom == 0.0
                    ? length
                    : length + random.uniform(-lengthRandom, lengthRandom);
            const double lengthMultiplier = randomizedLength / length;
            if (!std::isfinite(lengthMultiplier)) return MS::kFailure;

            // Standard vectors either have the planned length or throw into
            // the compute guard; failed allocation cannot leave a short array.
            std::vector<float> noiseX(plan.pointsPerCurve, 0.0f);
            std::vector<float> noiseY(plan.pointsPerCurve, 0.0f);
            std::vector<float> noiseZ(plan.pointsPerCurve, 0.0f);
            if (useStruggle && struggleRange.x > 0.0) {
                dommy::math::RandomGenerator randomX(
                    static_cast<unsigned int>(outputSeed));
                for (std::size_t point = 0U;
                     point < plan.pointsPerCurve; ++point) {
                    noiseX[point] =
                        static_cast<float>(randomX.uniform(
                            -struggleRange.x, struggleRange.x));
                }
            }
            if (useStruggle && struggleRange.y > 0.0) {
                dommy::math::RandomGenerator randomY(
                    static_cast<unsigned int>(outputSeed + 1));
                for (std::size_t point = 0U;
                     point < plan.pointsPerCurve; ++point) {
                    noiseY[point] =
                        static_cast<float>(randomY.uniform(
                            -struggleRange.y, struggleRange.y));
                }
            }
            if (useStruggle && struggleRange.z > 0.0) {
                dommy::math::RandomGenerator randomZ(
                    static_cast<unsigned int>(outputSeed + 2));
                for (std::size_t point = 0U;
                     point < plan.pointsPerCurve; ++point) {
                    noiseZ[point] =
                        static_cast<float>(randomZ.uniform(
                            -struggleRange.z, struggleRange.z));
                }
            }

            MVector lookDirection =
                angleType == 0
                    ? tangents[static_cast<unsigned int>(copy)]
                    : (angleType == 1
                        ? normals[static_cast<unsigned int>(copy)]
                        : rotatedNormals[static_cast<unsigned int>(copy)]);
            lookDirection += angleOffset;
            if (lookDirection.length() <=
                std::numeric_limits<double>::epsilon()) {
                return MS::kFailure;
            }
            status = lookDirection.normalize();
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const MQuaternion orientation =
                yAxis.rotateTo(lookDirection);

            MPointArray points(
                static_cast<unsigned int>(plan.pointsPerCurve));
            if (points.length() != plan.pointsPerCurve) return MS::kFailure;
            for (std::size_t point = 0U;
                 point < plan.pointsPerCurve; ++point) {
                const unsigned int pointIndex =
                    static_cast<unsigned int>(point);
                MVector value(basePoints[pointIndex]);
                value.y *= lengthMultiplier;
                value += MVector(
                    noiseX[pointIndex] * struggleStrength *
                        rampValues[pointIndex],
                    noiseY[pointIndex] * struggleStrength *
                        rampValues[pointIndex],
                    noiseZ[pointIndex] * struggleStrength *
                        rampValues[pointIndex]);
                value = value.rotateBy(orientation);
                const MPoint finalPoint =
                    rootPoints[static_cast<unsigned int>(copy)] + value;
                if (!std::isfinite(finalPoint.x) ||
                    !std::isfinite(finalPoint.y) ||
                    !std::isfinite(finalPoint.z)) {
                    return MS::kFailure;
                }
                points[pointIndex] = finalPoint;
            }

            const int degree = plan.pointsPerCurve < 4U ? 1 : 3;
            MDoubleArray knots;
            GenerateDCurveKnots(plan.pointsPerCurve, degree, knots);
            if (knots.length() != plan.pointsPerCurve +
                    static_cast<std::size_t>(degree - 1)) {
                return MS::kFailure;
            }
            generatedCurves.push_back(
                DCItem(new DCurveInternalData(points, knots, degree)));
        }
    }

    if (!generatedCurves.empty() && !outputCurves->insert(generatedCurves)) {
        return MS::kFailure;
    }
    if (outputCurves->numberOfCurves() != generatedCurves.size()) return MS::kFailure;
    generatedCurves.clear();
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCAhoGen")



MStatus DCAhoGen::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnGenericAttribute gAttr;
    
    
    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    
    MFnEnumAttribute distTypeEnumFn;
    aDistributeType = distTypeEnumFn.create( "DistributeType", "dity", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(distTypeEnumFn.addField( "Use Length", 0 ));
    DOMMY_RETURN_IF_FAILURE(distTypeEnumFn.addField( "Use Parametric", 1 ));
    DOMMY_RETURN_IF_FAILURE(distTypeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(distTypeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistributeType));
    
    aCount = nAttr.create( "Count", "cnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(20));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));
    
    aLength = nAttr.create( "Length", "len", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLength));
    
    aSegment = nAttr.create( "Segment", "seg", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSegment));
	
	
	
	MFnEnumAttribute angleTypeEnumFn;
	aAngleType = angleTypeEnumFn.create( "AngleType", "angty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(angleTypeEnumFn.addField( "Curve Direction", 0 ));
	DOMMY_RETURN_IF_FAILURE(angleTypeEnumFn.addField( "Normal Base", 1 ));
	DOMMY_RETURN_IF_FAILURE(angleTypeEnumFn.addField( "RotedNormal", 2 ));
	DOMMY_RETURN_IF_FAILURE(angleTypeEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(angleTypeEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAngleType));
	
	
    aAngleSpread = nAttr.create( "AngleSpread", "angspr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngleSpread));
	
	
	aNormal = nAttr.create( "Normal", "n", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNormal));
	
    aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	
	aLengthRandom = nAttr.create( "LengthRandom", "lenrnd", MFnNumericData::kDouble, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthRandom));
	
	
//	aBend = nAttr.create( "Bend", "be", MFnNumericData::kFloat );
//	nAttr.setDefault(0.0);
//	CHECK_MSTATUS(addAttribute(aBend));
	
	
	aIsStruggle = nAttr.create( "IsStruggle", "isstr", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStruggle));
	
	
	aStruggleRange = nAttr.create( "StruggleRange", "strrng", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleRange));
	
	aStruggleStrength = nAttr.create( "StruggleStrength", "strstr", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleStrength));
	
	aStruggleRamp = MRampAttribute::createCurveRamp("StruggleRamp", "strrmp", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleRamp));
	
    
    
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistributeType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCount, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSegment, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLength, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngleType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngleSpread, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormal, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outDC ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aLengthRandom, outDC ));
	
//	CHECK_MSTATUS(attributeAffects( aBend, outDC ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsStruggle, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleRange, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleStrength, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleRamp, outDC ));
	
	
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCAhoGen", "initialize", std::current_exception());
}

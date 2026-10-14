//
//  DPDFNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/13.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDFNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DNoiseOp.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "dpoint_ids.h"





#include <cmath>
#include <limits>
#include <string>
#include <vector>




const MTypeId DPDFNoise::typeId( ID_DP_DF_NOISE );

MObject DPDFNoise::inDD;

MObject DPDFNoise::aIsNoiseAttribute;
MObject DPDFNoise::aNoiseAttributeName;

MObject DPDFNoise::aPrecision;
MObject DPDFNoise::aEpsilon;

//normal vector
MObject DPDFNoise::aNormalVectorType;
MObject DPDFNoise::aNormalVector;
MObject DPDFNoise::aReferenceObject;

//deform
MObject DPDFNoise::aIsDeform;


//noise
MObject DPDFNoise::aNoiseType;
MObject DPDFNoise::aSeed;
MObject DPDFNoise::aUseSeedOffset;
MObject DPDFNoise::aAmplitube;
MObject DPDFNoise::aFrequency;
MObject DPDFNoise::aOctave;
MObject DPDFNoise::aLacunarity;
MObject DPDFNoise::aPersistence;

//voronoi
MObject DPDFNoise::aIsVoronoiDistance;
MObject DPDFNoise::aVoronoiDisplacement;

//turbulence
MObject DPDFNoise::aTurbulencePower;
MObject DPDFNoise::aTurbulenceRoughnesss;

//wood
MObject DPDFNoise::aWooodFrequency;
MObject DPDFNoise::aWooodScale;
MObject DPDFNoise::aWooodBias;
MObject DPDFNoise::aWooodGrain;
MObject DPDFNoise::aWooodAngle;

MObject DPDFNoise::aScale;
MObject DPDFNoise::aUniformScale;
MObject DPDFNoise::aOffset;

MObject DPDFNoise::aInvert;
MObject DPDFNoise::aNormalize;

MObject DPDFNoise::outDD;





void *DPDFNoise::creator() {
    return ::dommy::maya::createObject("DPDFNoise", [&]() {
        return new DPDFNoise();
    });
}

MStatus DPDFNoise::evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms) {
    MStatus status;
    MDataHandle handle;

    handle = data.inputValue(aNoiseType, &status);
    if (!status) return status;
    parms.mNoiseType = handle.asShort();
    handle = data.inputValue(aSeed, &status);
    if (!status) return status;
    parms.mSeed = handle.asInt();
    handle = data.inputValue(aFrequency, &status);
    if (!status) return status;
    parms.mFrequency = handle.asFloat();
    handle = data.inputValue(aOctave, &status);
    if (!status) return status;
    parms.mOctave = handle.asInt();
    handle = data.inputValue(aLacunarity, &status);
    if (!status) return status;
    parms.mLacunarity = handle.asFloat();
    handle = data.inputValue(aPersistence, &status);
    if (!status) return status;
    parms.mPersistence = handle.asFloat();

    handle = data.inputValue(aIsVoronoiDistance, &status);
    if (!status) return status;
    parms.mUseVoronoiDistance = handle.asBool();
    handle = data.inputValue(aVoronoiDisplacement, &status);
    if (!status) return status;
    parms.mVoronoiDisplace = handle.asFloat();

    handle = data.inputValue(aTurbulencePower, &status);
    if (!status) return status;
    parms.mTurbulancePower = handle.asFloat();
    handle = data.inputValue(aTurbulenceRoughnesss, &status);
    if (!status) return status;
    parms.mTurbulanceRoughness = handle.asInt();

    handle = data.inputValue(aWooodFrequency, &status);
    if (!status) return status;
    parms.mWooodFrequency = handle.asFloat();
    handle = data.inputValue(aWooodGrain, &status);
    if (!status) return status;
    parms.mWooodGrain = handle.asFloat();
    handle = data.inputValue(aWooodScale, &status);
    if (!status) return status;
    parms.mWooodScale = handle.asFloat();
    handle = data.inputValue(aWooodBias, &status);
    if (!status) return status;
    parms.mWooodBias = handle.asFloat();

    MDataHandle woodAngleHandle = data.inputValue(aWooodAngle, &status);
    if (!status)
        return status;
    const float3& woodAngle = woodAngleHandle.asFloat3();
    parms.mWooodAngle = MVector(woodAngle[0], woodAngle[1], woodAngle[2]);

    MDataHandle offsetHandle = data.inputValue(aOffset, &status);
    if (!status)
        return status;
    const float3& offset = offsetHandle.asFloat3();
    parms.mOffsetVec = MVector(offset[0], offset[1], offset[2]);

    MDataHandle scaleHandle = data.inputValue(aScale, &status);
    if (!status)
        return status;
    const float3& scale = scaleHandle.asFloat3();
    parms.mScaleVec = MVector(scale[0], scale[1], scale[2]);

    handle = data.inputValue(aUniformScale, &status);
    if (!status) return status;
    parms.mUniformScale = handle.asFloat();

    handle = data.inputValue(aAmplitube, &status);
    if (!status) return status;
    parms.mAmplitude = handle.asFloat();
    handle = data.inputValue(aNormalize, &status);
    if (!status) return status;
    parms.mNormalize = handle.asBool();
    handle = data.inputValue(aInvert, &status);
    if (!status) return status;
    parms.mInvert = handle.asBool();
    handle = data.inputValue(aEpsilon, &status);
    if (!status) return status;
    parms.epsilon = handle.asDouble();
    handle = data.inputValue(aNormalVectorType, &status);
    if (!status) return status;
    parms.normalType = handle.asShort();
    handle = data.inputValue(aNormalVector, &status);
    if (!status) return status;
    const float3& normal = handle.asFloat3();
    parms.normalVecBase = MVector(normal[0], normal[1], normal[2]);
    DOMMY_RETURN_IF_FAILURE(validateDFNoiseParams(parms));

    // Preserve the historical absent-reference fallback; a connected failure
    // must not be mistaken for a disconnected input.
    parms.m_referenceObject = MObject::kNullObj;
    parms.m_referenceObjectType = MFnData::kInvalid;
    if (parms.normalType == 2) {
        MPlug referencePlug(thisMObject(), aReferenceObject);
        const bool connected = referencePlug.isConnected(&status);
        if (!status) return status;
        if (!connected) {
            parms.normalType = 0;
        } else {
            handle = data.inputValue(aReferenceObject, &status);
            if (!status) return status;
            parms.m_referenceObjectType = handle.type();
            parms.m_referenceObject = handle.data();
            if (parms.m_referenceObject.isNull() ||
                (parms.m_referenceObjectType != MFnData::kMesh &&
                 parms.m_referenceObjectType != MFnData::kNurbsCurve &&
                 parms.m_referenceObjectType != MFnData::kNurbsSurface))
                return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

namespace {

template <typename Geometry>
MStatus evaluateDFNoiseGeometry(const Geometry& geometry, const NoiseBaseParams& params,
                                bool isDeform, DPointList& points, DVec3DataList& divergence)
{
    const std::size_t count = geometry.numberOfPoints();
    if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
        return MS::kInvalidParameter;
    points = *geometry.getPositionPtrConst();
    divergence.assign(count, DVec3Data(0.0f, 0.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(createDFNoiseFromPosition(params, points, divergence));
    if (isDeform) {
        for (std::size_t i = 0U; i < count; ++i)
            if (!dommy::point::offsetAlongVector(points[i], divergence[i],
                    params.mAmplitude, 1.0, 1.0, points[i])) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

DPAttributeData::Ptr makeDFNoiseAttribute(const DVec3DataList& divergence, short precision) {
    if (precision == DP_ATTR_PRE_FLOAT)
        return DPAttributeData::Ptr(new DPVec3fAttr(divergence));
    return DPAttributeData::Ptr(
        new DPVec3dAttr(std::vector<Imath::V3d>(divergence.begin(), divergence.end())));
}

template <typename Geometry>
MStatus deformDFNoiseGeometry(Geometry& geometry, const NoiseBaseParams& params,
                              bool isDeform, bool isOutAttribute, short precision,
                              const std::string& attributeName)
{
    if ((precision != DP_ATTR_PRE_FLOAT && precision != DP_ATTR_PRE_DOUBLE) ||
        (isOutAttribute && !dommy::attribute::validAttributeName(attributeName)))
        return MS::kInvalidParameter;
    DPointList points;
    DVec3DataList divergence;
    DOMMY_RETURN_IF_FAILURE(evaluateDFNoiseGeometry(geometry, params, isDeform, points, divergence));
    if (isDeform && !geometry.replacePositions(points)) return MS::kFailure;
    // All input families publish the unscaled vector. Amplitude affects only
    // displacement, once, and never changes the stored noise attribute.
    if (isOutAttribute) {
        if (!dommy::attribute::commitStagedAttribute(geometry, attributeName,
                makeDFNoiseAttribute(divergence, precision), points.size()))
            return MS::kFailure;
    }
    return MS::kSuccess;
}

MStatus validateDFNoiseSeedRange(int seed, std::size_t primitiveCount, bool useSeedOffset)
{
    if (seed < 0) return MS::kInvalidParameter;
    if (!useSeedOffset || primitiveCount == 0U) return MS::kSuccess;
    // Historical cumulative offsets: s, s+1, s+3, ... . Divide first to avoid
    // overflowing the intermediate product even on 32-bit size_t.
    std::size_t first = primitiveCount;
    std::size_t second = primitiveCount - 1U;
    if (first % 2U == 0U) first /= 2U;
    else second /= 2U;
    std::size_t offset = 0U;
    return dommy::generation::checkedProduct(first, second,
        static_cast<std::size_t>(std::numeric_limits<int>::max() - seed), offset)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

template <typename PrimitiveData>
MStatus deformDFNoisePrimitives(PrimitiveData& result, NoiseBaseParams params,
                                bool useSeedOffset, bool isDeform, bool isOutAttribute,
                                short precision, const std::string& attributeName)
{
    std::size_t offset = 0U;
    for (auto item = result.beginPrims(); item != result.endPrims(); ++item, ++offset) {
        if (!*item) return MS::kFailure;
        if (useSeedOffset) {
            std::size_t seed = 0U;
            if (params.mSeed < 0 || !dommy::generation::checkedSum(
                    static_cast<std::size_t>(params.mSeed), offset,
                    static_cast<std::size_t>(std::numeric_limits<int>::max()), seed))
                return MS::kInvalidParameter;
            params.mSeed = static_cast<int>(seed);
        }
        DOMMY_RETURN_IF_FAILURE(deformDFNoiseGeometry(
            **item, params, isDeform, isOutAttribute, precision, attributeName));
    }
    return MS::kSuccess;
}

} // namespace

MStatus DPDFNoise::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle inputHandle = data.inputValue(inDD, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MTypeId inputType = inputHandle.typeId();
    MDataHandle handle = data.inputValue(aUseSeedOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useSeedOffset = handle.asBool();
    handle = data.inputValue(aIsNoiseAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool isOutAttribute = handle.asBool();
    handle = data.inputValue(aNoiseAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string attributeName = handle.asString().asChar();
    handle = data.inputValue(aIsDeform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool isDeform = handle.asBool();
    handle = data.inputValue(aPrecision, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short precision = handle.asShort();
    NoiseBaseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseBaseParms(data, params));
    if ((precision != DP_ATTR_PRE_FLOAT && precision != DP_ATTR_PRE_DOUBLE) ||
        (isOutAttribute && !dommy::attribute::validAttributeName(attributeName)))
        return MS::kInvalidParameter;
    const std::size_t attributeWidth = !isOutAttribute ? 0U :
        (precision == DP_ATTR_PRE_FLOAT ? sizeof(Imath::V3f) : sizeof(Imath::V3d));
    const std::size_t pointBytes = sizeof(DPoint) + sizeof(DVec3Data) +
        (isOutAttribute && precision == DP_ATTR_PRE_DOUBLE ? sizeof(Imath::V3d) : 0U);

    MFnPluginData pluginData;
    if (inputType == DPData::id) {
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
        if (!input || !input->hasValidPointTopology() ||
            input->numberOfPoints() > dommy::generation::kMaxGeneratedPointCount)
            return MS::kInvalidParameter;
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPointPayload(*input, pointBytes, workspace);
        dommy::point::preflightGeneratedPointAttribute(input->numberOfPoints(), attributeName,
            attributeWidth, workspace);
        DPointList points;
        DVec3DataList divergence;
        DOMMY_RETURN_IF_FAILURE(evaluateDFNoiseGeometry(*input, params, isDeform, points, divergence));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) -> MStatus {
                DOMMY_RETURN_IF_FAILURE(output.copyRepeatedFrom(*input, 1U,
                    [&](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                        point = points[index]; return MS::kSuccess;
                    }));
                if (isOutAttribute && !dommy::attribute::commitStagedAttribute(output, attributeName,
                        makeDFNoiseAttribute(divergence, precision), points.size())) return MS::kFailure;
                return MS::kSuccess;
            }));
    } else if (inputType == DCurveData::id) {
        const DCurveData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input));
        std::size_t totalPoints = 0U;
        if (!input || !input->hasValidCurveTopology() ||
            !input->getCheckedTotalCvs(totalPoints) ||
            input->numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount ||
            totalPoints > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(validateDFNoiseSeedRange(
            params.mSeed, input->numberOfCurves(), useSeedOffset));
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPrimitivePointPayload(*input, input->numberOfCurves(), pointBytes,
            attributeName, attributeWidth, workspace);
        DCurveData* result = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, result));
        if (!result) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(result->copyFrom(*input));
        DOMMY_RETURN_IF_FAILURE(deformDFNoisePrimitives(
            *result, params, useSeedOffset, isDeform, isOutAttribute, precision, attributeName));
        result->updateBBox();
        MDataHandle output = data.outputValue(outDD, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
            output, pluginData.object(), DCurveData::id));
    } else if (inputType == DMeshData::id) {
        const DMeshData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DMeshData::id, input));
        std::size_t totalPoints = 0U;
        if (!input || !input->hasValidMeshTopology() ||
            !input->getCheckedTotalVertex(totalPoints) ||
            input->numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount ||
            totalPoints > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(validateDFNoiseSeedRange(
            params.mSeed, input->numberOfMesh(), useSeedOffset));
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPrimitivePointPayload(*input, input->numberOfMesh(), pointBytes,
            attributeName, attributeWidth, workspace);
        DMeshData* result = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, result));
        if (!result) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(result->copyFrom(*input));
        DOMMY_RETURN_IF_FAILURE(deformDFNoisePrimitives(
            *result, params, useSeedOffset, isDeform, isOutAttribute, precision, attributeName));
        result->updateBBox();
        MDataHandle output = data.outputValue(outDD, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
            output, pluginData.object(), DMeshData::id));
    } else {
        return MS::kInvalidParameter;
    }
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPDFNoise")

MStatus DPDFNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDD = gAttr.create("inDD", "idd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    MFnEnumAttribute enumFn;
    aNoiseType = enumFn.create("NoiseType", "noty", NOISE_TYPE_PERLIN, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Perlin", NOISE_TYPE_PERLIN));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Billow", NOISE_TYPE_BILLOW));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Voronoi", NOISE_TYPE_VORONOI));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Turblance", NOISE_TYPE_TURBULANCE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("RidgedMulti", NOISE_TYPE_RIGDEDMULTI));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Wood", NOISE_TYPE_WOOD));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseType));
    aIsNoiseAttribute =
        nAttr.create("IsNoiseAttribute", "isnattr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNoiseAttribute));
    MObject attrNoiseName = fnStringData.create("noiseval", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNoiseAttributeName =
        tAttr.create("NoiseAttributeName", "nattrn", MFnData::kString, attrNoiseName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseAttributeName));
    MFnEnumAttribute preEnumFn;
    aPrecision = preEnumFn.create("Precision", "prec", DP_ATTR_PRE_FLOAT, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(preEnumFn.addField("float", DP_ATTR_PRE_FLOAT));
    DOMMY_RETURN_IF_FAILURE(preEnumFn.addField("double", DP_ATTR_PRE_DOUBLE));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPrecision));
    aEpsilon = nAttr.create("Epsilon", "eps", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0001));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEpsilon));
    MFnEnumAttribute normalEnumFn;
    aNormalVectorType = normalEnumFn.create("NormalVectorType", "normty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(normalEnumFn.addField("From Space", 0));
    DOMMY_RETURN_IF_FAILURE(normalEnumFn.addField("Use Normal Vector", 1));
    DOMMY_RETURN_IF_FAILURE(normalEnumFn.addField("Use Reference Object", 2));
    DOMMY_RETURN_IF_FAILURE(normalEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(normalEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalVectorType));
    aNormalVector = nAttr.create("NormalVector", "nvec", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalVector));
    aReferenceObject = gAttr.create("ReferenceObject", "refobj", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aReferenceObject));
    aIsDeform = nAttr.create("IsDeform", "isdef", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeform));
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    aUseSeedOffset = nAttr.create("UseSeedOffset", "unoff", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseSeedOffset));
    aAmplitube = nAttr.create("Amplitube", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitube));
    aOctave = nAttr.create("Octave", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOctave));
    aFrequency = nAttr.create("Frequency", "freq", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrequency));
    aLacunarity = nAttr.create("Lacunarity", "lac", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLacunarity));
    aPersistence = nAttr.create("Persistence", "per", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPersistence));
    aIsVoronoiDistance =
        nAttr.create("IsVoronoiDistance", "isvd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVoronoiDistance));
    aVoronoiDisplacement =
        nAttr.create("VoronoiDisplacement", "vordisp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVoronoiDisplacement));
    aTurbulencePower = nAttr.create("TurbulencePower", "tpow", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTurbulencePower));
    aTurbulenceRoughnesss =
        nAttr.create("TurbulenceRoughnesss", "trough", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTurbulenceRoughnesss));
    aWooodFrequency = nAttr.create("WooodFrequency", "wfreq", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(16.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWooodFrequency));
    aWooodScale = nAttr.create("WooodScale", "wsc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.25));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWooodScale));
    aWooodBias = nAttr.create("WooodBias", "wbi", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.125));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWooodBias));
    aWooodGrain = nAttr.create("WooodGrain", "wgra", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.25));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWooodGrain));
    aWooodAngle = nAttr.create("WooodAngle", "wang", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(84.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWooodAngle));
    aScale = nAttr.create("Scale", "sc", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));
    aOffset = nAttr.create("Offset", "of", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));
    aUniformScale = nAttr.create("UniformScale", "uscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
    aInvert = nAttr.create("Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));
    aNormalize = nAttr.create("Normalize", "norm", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalize));
    outDD = gAttr.create("outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNoiseAttribute, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseAttributeName, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrecision, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEpsilon, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalVectorType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalVector, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aReferenceObject, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsDeform, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseSeedOffset, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsVoronoiDistance, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoronoiDisplacement, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulencePower, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulenceRoughnesss, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodFrequency, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodBias, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodGrain, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodAngle, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInvert, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalize, outDD));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPDFNoise", "initialize",
                                                  std::current_exception());
}

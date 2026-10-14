//
//  DPPyroNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2016/12/04.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPPyroNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointPayloadBudget.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "dpoint_ids.h"


#include "DNoiseOp.h"

#include "perlin.h"
#include "module.h"


#include <tbb/blocked_range.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool isFiniteDPoint(const DPoint& point)
{
    return std::isfinite(point[0]) && std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}


} // namespace




const MTypeId DPPyroNoise::typeId( ID_DP_PYRO_NOISE );

MObject DPPyroNoise::inDP;

MObject DPPyroNoise::aOutAttribute;

MObject DPPyroNoise::aPyroType;
MObject DPPyroNoise::aRadius;
MObject DPPyroNoise::aRadiusAttribute;

MObject DPPyroNoise::aTopRatio;
MObject DPPyroNoise::aUnderRatio;
MObject DPPyroNoise::aSideRatio;

MObject DPPyroNoise::aPosition;
MObject DPPyroNoise::inObject;
MObject DPPyroNoise::inTargetDP;

MObject DPPyroNoise::aNoiseType;
//noise
MObject DPPyroNoise::aSeed;
MObject DPPyroNoise::aAmplitube;
MObject DPPyroNoise::aFrequency;
MObject DPPyroNoise::aOctave;
MObject DPPyroNoise::aLacunarity;
MObject DPPyroNoise::aPersistence;

//voronoi
MObject DPPyroNoise::aIsVoronoiDistance;
MObject DPPyroNoise::aVoronoiDisplacement;

//turbulence
MObject DPPyroNoise::aTurbulencePower;
MObject DPPyroNoise::aTurbulenceRoughnesss;

//wood
MObject DPPyroNoise::aWooodFrequency;
MObject DPPyroNoise::aWooodScale;
MObject DPPyroNoise::aWooodBias;
MObject DPPyroNoise::aWooodGrain;
MObject DPPyroNoise::aWooodAngle;


MObject DPPyroNoise::aScale;
MObject DPPyroNoise::aUniformScale;
MObject DPPyroNoise::aOffset;

MObject DPPyroNoise::outDP;

void *DPPyroNoise::creator() {
    return ::dommy::maya::createObject("DPPyroNoise", [&]() {
        return new DPPyroNoise();
    });
}


MStatus DPPyroNoise::evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms) {
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
    handle = data.inputValue(aTopRatio, &status);
    if (!status) return status;
    parms.mTopRatio = handle.asFloat();
    handle = data.inputValue(aUnderRatio, &status);
    if (!status) return status;
    parms.mUnderRatio = handle.asFloat();
    handle = data.inputValue(aSideRatio, &status);
    if (!status) return status;
    parms.mSideRatio = handle.asFloat();
    return validatePyroNoiseParams(parms);
}

MStatus DPPyroNoise::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle handle = data.inputValue(inDP, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    handle = data.inputValue(aOutAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string outputName = handle.asString().asChar();
    handle = data.inputValue(aPyroType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short pyroType = handle.asShort();
    handle = data.inputValue(aRadius, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float radius = handle.asFloat();
    handle = data.inputValue(aPosition, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& value = handle.asFloat3();
    const DPoint center(value[0], value[1], value[2]);
    NoiseBaseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseBaseParms(data, params));
    if (!dommy::attribute::validAttributeName(outputName) ||
        (pyroType != 0 && pyroType != 1) || !std::isfinite(radius) || radius < 0.0f ||
        !isFiniteDPoint(center)) return MS::kInvalidParameter;

    const DPointList& points = *input->getPositionPtrConst();
    const DPointList* sources = nullptr;
    MObject object;
    MFnData::Type objectType = MFnData::kInvalid;
    std::size_t sourceCount = 1U;
    if (pyroType == 0) {
        handle = data.inputValue(inTargetDP, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!handle.data().isNull()) {
            const DPData* target = nullptr;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, target));
            if (!target || !target->hasValidPointTopology()) return MS::kInvalidParameter;
            sources = target->getPositionPtrConst();
            if (!sources) return MS::kFailure;
            sourceCount = sources->size();
        }
    } else {
        handle = data.inputValue(inObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        object = handle.data();
        objectType = handle.type();
        if (object.isNull() || (objectType != MFnData::kNurbsCurve && objectType != MFnData::kMesh))
            return MS::kInvalidParameter;
    }
    // Reject excessive Cartesian work before allocating density buffers.
    std::size_t work = 0U;
    if (points.size() > dommy::generation::kMaxGeneratedPointCount ||
        sourceCount > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::generation::checkedProduct(points.size(), sourceCount,
            dommy::generation::kMaxWorkItemCount, work) ||
        (sourceCount != 0U && sourceCount - 1U >
            static_cast<std::size_t>(std::numeric_limits<int>::max() - params.mSeed)))
        return MS::kInvalidParameter;
    for (const DPoint& point : points)
        if (!isFiniteDPoint(point)) return MS::kInvalidParameter;
    if (sources)
        for (const DPoint& point : *sources)
            if (!isFiniteDPoint(point)) return MS::kInvalidParameter;

    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*input, sizeof(DPoint) + 2U * sizeof(float), workspace);
    dommy::point::preflightGeneratedPointAttribute(points.size(), outputName, sizeof(float), workspace);
    DScalarDataList density(points.size(), 0.0f);
    if (pyroType == 0) {
        DScalarDataList temporary(points.size(), 0.0f);
        const int baseSeed = params.mSeed;
        for (std::size_t i = 0U; i < sourceCount && !points.empty(); ++i) {
            params.mSeed = baseSeed + static_cast<int>(i);
            DOMMY_RETURN_IF_FAILURE(createPyroNoiseFromPosition(params,
                sources ? (*sources)[i] : center, 1.0f, radius, points, temporary));
            for (std::size_t j = 0U; j < density.size(); ++j)
                density[j] = std::max(density[j], temporary[j]);
        }
    } else {
        // Preserve the historical Perlin-based object mode. Maya geometry
        // queries remain serial; only the pure point-source path uses TBB.
        noise::module::Perlin module;
        module.SetSeed(params.mSeed);
        module.SetOctaveCount(params.mOctave);
        module.SetFrequency(params.mFrequency);
        module.SetLacunarity(params.mLacunarity);
        module.SetPersistence(params.mPersistence);
        module.SetNoiseQuality(noise::QUALITY_BEST);
        const tbb::blocked_range<uint> range(0U, static_cast<uint>(points.size()), 10000U);
        if (objectType == MFnData::kNurbsCurve) {
            PyroNoiseFromCurve body(module, params, radius, 1.0f, object, points, &density);
            DOMMY_RETURN_IF_FAILURE(body(range));
        } else {
            PyroNoiseFromMesh body(module, params, radius, 1.0f, object, points, &density);
            DOMMY_RETURN_IF_FAILURE(body(range));
        }
    }

    MFnPluginData pluginData;
    DPData* result = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DPData::id, result));
    if (!result) return MS::kFailure;
    result->setDPData(*input);
    if (!dommy::attribute::commitStagedAttribute(*result, outputName,
            DPAttributeData::Ptr(new DPFloatAttr(density)), points.size())) return MS::kFailure;
    MDataHandle output = data.outputValue(outDP, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DPData>(
        output, pluginData.object(), DPData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPPyroNoise")

MStatus DPPyroNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    MObject nAttrName = fnStringData.create("density", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, nAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));
    MFnEnumAttribute pyEnumFn;
    aPyroType = pyEnumFn.create("PyroType", "pyty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(pyEnumFn.addField("Point", 0));
    DOMMY_RETURN_IF_FAILURE(pyEnumFn.addField("Object", 1));
    DOMMY_RETURN_IF_FAILURE(pyEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(pyEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPyroType));
    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    const MObject aRadiusAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "radattr", MFnData::kString,
                                    aRadiusAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
    aTopRatio = nAttr.create("TopRatio", "tprto", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTopRatio));
    aUnderRatio = nAttr.create("UnderRatio", "unrto", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUnderRatio));
    aSideRatio = nAttr.create("SideRatio", "sdrto", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSideRatio));
    aPosition = nAttr.create("Position", "ppos", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPosition));
    inObject = gAttr.create("inObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));
    inTargetDP = tAttr.create("inTargetDP", "intdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDP));
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
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kLong, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    aAmplitube = nAttr.create("Amplitube", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitube));
    aOctave = nAttr.create("Octave", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(6));
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
    aUniformScale = nAttr.create("UniformScale", "uscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
    aOffset = nAttr.create("Offset", "of", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPyroType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTopRatio, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUnderRatio, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSideRatio, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPosition, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inTargetDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsVoronoiDistance, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoronoiDisplacement, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulencePower, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulenceRoughnesss, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodFrequency, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodScale, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodBias, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodGrain, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodAngle, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDP));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPPyroNoise", "initialize",
                                                  std::current_exception());
}

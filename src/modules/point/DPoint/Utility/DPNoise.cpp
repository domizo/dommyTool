//
//  DPNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/09.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "dpoint_ids.h"

#include "DNoiseOp.h"




#include <cmath>
#include <string>
#include <vector>




const MTypeId DPNoise::typeId( ID_DP_NOISE );

MObject DPNoise::inDP;

MObject DPNoise::aNoiseType;
MObject DPNoise::aSeed;

//attribute
MObject DPNoise::aIsNoiseAttribute;
MObject DPNoise::aNoiseAttributeName;

//deform
MObject DPNoise::aDeformType;
MObject DPNoise::aDeformVector;
MObject DPNoise::aVectorAttributeName;



//noise
MObject DPNoise::aAmplitude;
MObject DPNoise::aFrequency;
MObject DPNoise::aOctave;
MObject DPNoise::aLacunarity;
MObject DPNoise::aPersistence;

//voronoi
MObject DPNoise::aIsVoronoiDistance;
MObject DPNoise::aVoronoiDisplacement;

//turbulence
MObject DPNoise::aTurbulencePower;
MObject DPNoise::aTurbulenceRoughnesss;

//wood
MObject DPNoise::aWooodFrequency;
MObject DPNoise::aWooodScale;
MObject DPNoise::aWooodBias;
MObject DPNoise::aWooodGrain;
MObject DPNoise::aWooodAngle;

MObject DPNoise::aScale;
MObject DPNoise::aUniformScale;
MObject DPNoise::aOffset;
MObject DPNoise::aIsAbsolute;

MObject DPNoise::outDP;





void *DPNoise::creator() {
    return ::dommy::maya::createObject("DPNoise", [&]() {
        return new DPNoise();
    });
}





MStatus DPNoise::evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms) {
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

    handle = data.inputValue(aAmplitude, &status);
    if (!status) return status;
    parms.mAmplitude = handle.asFloat();
    handle = data.inputValue(aIsAbsolute, &status);
    if (!status) return status;
    parms.isAbs = handle.asBool();
    return validateNoiseBaseParams(parms);
}

MStatus DPNoise::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle handle = data.inputValue(inDP, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    handle = data.inputValue(aIsNoiseAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool isNoiseAttribute = handle.asBool();
    handle = data.inputValue(aDeformType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short deformType = handle.asShort();
    handle = data.inputValue(aDeformVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& vector = handle.asFloat3();
    const Imath::V3d baseVector(vector[0], vector[1], vector[2]);
    handle = data.inputValue(aNoiseAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string noiseAttributeName = handle.asString().asChar();
    handle = data.inputValue(aVectorAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string vectorAttributeName = handle.asString().asChar();
    NoiseBaseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseBaseParms(data, params));
    if (deformType < NOISE_DEFROM_NONE || deformType > NOISE_DEFROM_ATTRIBUTE ||
        !std::isfinite(baseVector[0]) || !std::isfinite(baseVector[1]) ||
        !std::isfinite(baseVector[2]) ||
        (isNoiseAttribute && !dommy::attribute::validAttributeName(noiseAttributeName)))
        return MS::kInvalidParameter;

    const std::size_t count = input->numberOfPoints();
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(
        *input, sizeof(DPoint) + sizeof(DScalarData) + sizeof(Imath::V3d), workspace);
    if (isNoiseAttribute) {
        // Include the new field even when it replaces an existing one. The
        // conservative estimate covers staging and retained source metadata.
        workspace.add(count, sizeof(DScalarData));
        workspace.add(1U, sizeof(DScalarData));
        workspace.add(noiseAttributeName.size(), sizeof(char));
    }
    DPointList points = input->getPositionConst();
    DScalarDataList signals(count, 0.0f);
    DOMMY_RETURN_IF_FAILURE(createNoiseFromPosition(params, points, signals));
    std::vector<Imath::V3d> vectors(count, baseVector);
    if (deformType == NOISE_DEFROM_ATTRIBUTE && !dommy::attribute::convertVector3Attribute(
            input->getAttr(vectorAttributeName), count, vectors)) return MS::kInvalidParameter;
    if (deformType != NOISE_DEFROM_NONE) {
        for (std::size_t i = 0U; i < count; ++i)
            if (!dommy::point::offsetAlongVector(points[i], vectors[i],
                    params.mAmplitude, signals[i], 1.0, points[i])) return MS::kInvalidParameter;
    }
    status = dommy::maya::setPluginDataOutput<DPData>(data, outDP, DPData::id,
        [&](DPData& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(output.copyRepeatedFrom(*input, 1U,
                [&](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                    point = points[index];
                    return MS::kSuccess;
                }));
            if (isNoiseAttribute && !dommy::attribute::commitStagedAttribute(
                    output, noiseAttributeName, DPAttributeData::Ptr(new DPFloatAttr(signals)), count))
                return MS::kFailure;
            return MS::kSuccess;
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPNoise")

MStatus DPNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    MFnEnumAttribute enumFn;
    aNoiseType = enumFn.create("NoiseType", "noty", NOISE_TYPE_RANDOM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Random", NOISE_TYPE_RANDOM));
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNoiseAttribute));
    MFnStringData fnStringData;
    MObject attrNoiseName = fnStringData.create("noiseval", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNoiseAttributeName =
        tAttr.create("NoiseAttributeName", "nattrn", MFnData::kString, attrNoiseName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseAttributeName));
    MFnEnumAttribute defEnumFn;
    aDeformType = defEnumFn.create("DeformType", "defty", NOISE_DEFROM_VECTOR, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("None", NOISE_DEFROM_NONE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Vector", NOISE_DEFROM_VECTOR));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Attribute", NOISE_DEFROM_ATTRIBUTE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeformType));
    aDeformVector = nAttr.create("DeformVector", "defvec", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeformVector));
    MObject attrOffName = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttributeName =
        tAttr.create("VectorAttributeName", "vecattrn", MFnData::kString, attrOffName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttributeName));
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kLong, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    aAmplitude = nAttr.create("Amplitude", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitude));
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
    aOffset = nAttr.create("Offset", "of", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));
    aIsAbsolute = nAttr.create("IsAbsolute", "isabs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAbsolute));
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNoiseAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseAttributeName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorAttributeName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitude, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDP));
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAbsolute, outDP));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPNoise", "initialize",
                                                  std::current_exception());
}

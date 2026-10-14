//
//  DMNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/05.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DPointDeformMath.h"
#include <cmath>
#include <limits>
#include <vector>
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"

#include "DNoiseOp.h"


const MTypeId DMNoise::typeId( ID_DM_NOISE );

MObject DMNoise::inDM;

MObject DMNoise::aNoiseType;
MObject DMNoise::aSeed;
MObject DMNoise::aUseSeedOffset;

//attribute
MObject DMNoise::aIsNoiseAttribute;
MObject DMNoise::aNoiseAttributeName;

//deform
MObject DMNoise::aDeformType;
MObject DMNoise::aDeformVector;
MObject DMNoise::aVectorAttributeName;



//noise
MObject DMNoise::aAmplitube;
MObject DMNoise::aFrequency;
MObject DMNoise::aOctave;
MObject DMNoise::aLacunarity;
MObject DMNoise::aPersistence;

//voronoi
MObject DMNoise::aIsVoronoiDistance;
MObject DMNoise::aVoronoiDisplacement;

//turbulence
MObject DMNoise::aTurbulencePower;
MObject DMNoise::aTurbulenceRoughnesss;

//wood
MObject DMNoise::aWooodFrequency;
MObject DMNoise::aWooodScale;
MObject DMNoise::aWooodBias;
MObject DMNoise::aWooodGrain;
MObject DMNoise::aWooodAngle;

MObject DMNoise::aScale;
MObject DMNoise::aUniformScale;
MObject DMNoise::aOffset;

MObject DMNoise::outDM;



void *DMNoise::creator() {
    return ::dommy::maya::createObject("DMNoise", [&]() {
        return new DMNoise();
    });
}



MStatus DMNoise::evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms) {
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

    return validateNoiseBaseParams(parms);
}


MStatus DMNoise::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    MStatus status = getValidInputDM(inDM, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::size_t totalPoints = 0U;
    if (!input || !input->getCheckedTotalVertex(totalPoints) ||
        totalPoints > dommy::generation::kMaxGeneratedPointCount ||
        input->numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount)
        return MS::kInvalidParameter;

    MDataHandle handle = data.inputValue(aUseSeedOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useSeedOffset = handle.asBool();
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
    handle = data.inputValue(aAmplitube, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double amplitude = handle.asFloat();
    NoiseBaseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseBaseParms(data, params));

    if ((deformType != NOISE_DEFROM_NONE && deformType != NOISE_DEFROM_VECTOR &&
         deformType != NOISE_DEFROM_ATTRIBUTE && deformType != NOISE_DEFROM_MESH_NORMAL) ||
        !std::isfinite(amplitude) || !std::isfinite(baseVector[0]) ||
        !std::isfinite(baseVector[1]) || !std::isfinite(baseVector[2]) ||
        (isNoiseAttribute && !dommy::attribute::validAttributeName(noiseAttributeName)))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DMeshData* outputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, outputMeshes));
    if (!outputMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*input));
    std::size_t meshOffset = 0U;
    for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes(); ++mesh, ++meshOffset) {
        if (!*mesh) return MS::kFailure;
        // Keep the historical cumulative per-mesh seed sequence; reject overflow.
        if (useSeedOffset) {
            if (meshOffset > static_cast<std::size_t>(std::numeric_limits<int>::max() - params.mSeed))
                return MS::kInvalidParameter;
            params.mSeed += static_cast<int>(meshOffset);

        }
        DPointList points = (*mesh)->getPositionConst();
        const std::size_t count = points.size();
        if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        DScalarDataList signals(count, 0.0f);
        DOMMY_RETURN_IF_FAILURE(createNoiseFromPosition(params, points, signals));
        if (signals.size() != count) return MS::kFailure;

        std::vector<Imath::V3d> vectors(count, baseVector);
        if (deformType == NOISE_DEFROM_ATTRIBUTE) {
            if (!dommy::attribute::convertVector3Attribute(
                    (*mesh)->getAttr(vectorAttributeName), count, vectors)) return MS::kInvalidParameter;
        } else if (deformType == NOISE_DEFROM_MESH_NORMAL && count != 0U) {
            MObject object;
            status = generateMMeshFromDmesh(**mesh, object, DMeshPointSpace::Local);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnMesh function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            for (std::size_t i = 0U; i < count; ++i) {
                MVector normal;
                status = function.getVertexNormal(static_cast<int>(i), normal, MSpace::kObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
                vectors[i] = Imath::V3d(normal[0], normal[1], normal[2]);
            }
        }
        if (deformType != NOISE_DEFROM_NONE) {
            for (std::size_t i = 0U; i < count; ++i)
                if (!dommy::point::offsetAlongVector(points[i], vectors[i],
                        amplitude, signals[i], 1.0, points[i])) return MS::kInvalidParameter;
        }
        if (!(*mesh)->replacePositions(points)) return MS::kFailure;
        // All input attributes remain available until deformation has completed.
        if (isNoiseAttribute && !dommy::attribute::commitStagedAttribute(
                **mesh, noiseAttributeName, DPAttributeData::Ptr(new DPFloatAttr(signals)), count))
            return MS::kFailure;
    }
    outputMeshes->updateBBox();
    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMNoise")

MStatus DMNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

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

    aUseSeedOffset = nAttr.create("UseSeedOffset", "unoff", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseSeedOffset));

    MFnStringData fnStringData;
    MObject attrNoiseName = fnStringData.create("noiseval", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNoiseAttributeName =
        tAttr.create("NoiseAttributeName", "nattrn", MFnData::kString, attrNoiseName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseAttributeName));

    // deform
    MFnEnumAttribute defEnumFn;
    aDeformType = defEnumFn.create("DeformType", "defty", NOISE_DEFROM_VECTOR, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("None", NOISE_DEFROM_NONE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Vector", NOISE_DEFROM_VECTOR));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Attribute", NOISE_DEFROM_ATTRIBUTE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Mesh Normal", NOISE_DEFROM_MESH_NORMAL));
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

    // common noise
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
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

    // voronoi
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

    // turbulence
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

    // wood
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

    // noise transform
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

    // create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseSeedOffset, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNoiseAttribute, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseAttributeName, outDM));

    // deform
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformVector, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorAttributeName, outDM));

    // common noise
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, outDM));

    // voronoi
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsVoronoiDistance, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoronoiDisplacement, outDM));

    // turbulence
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulencePower, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulenceRoughnesss, outDM));

    // wood
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodFrequency, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodBias, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodGrain, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodAngle, outDM));

    // noise transform
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMNoise", "initialize",
                                                  std::current_exception());
}

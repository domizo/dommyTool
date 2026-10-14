//
//  DPFastNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPFastNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DPointPayloadBudget.h"
#include <cmath>
#include <limits>
#include <vector>
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DMayaValueValidation.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dpoint_ids.h"

#include "DNoiseOp.h"

#include "FastNoise.h"



const MTypeId DPFastNoise::typeId( ID_DP_FAST_NOISE );



MObject DPFastNoise::inDD;

MObject DPFastNoise::aNoiseType;
MObject DPFastNoise::aInterpType;


MObject DPFastNoise::aSeed;
MObject DPFastNoise::aUseSeedOffset;


//attach attribute
MObject DPFastNoise::aIsNoiseAttribute;
MObject DPFastNoise::aNoiseAttributeName;


//deform
MObject DPFastNoise::aDeformType;
MObject DPFastNoise::aDeformVector;
MObject DPFastNoise::aVectorAttributeName;


//noise
MObject DPFastNoise::aAmplitube;
MObject DPFastNoise::aFrequency;
MObject DPFastNoise::aOctave;
MObject DPFastNoise::aLacunarity;
MObject DPFastNoise::aGain;
MObject DPFastNoise::aFractalType;


//Cellular
MObject DPFastNoise::aCellularDistanceFunction;
MObject DPFastNoise::aCellularReturnType;
//    static MObject aCellularNoiseLookup;

//Position Warping
MObject DPFastNoise::aPositionWarpAmp;

//transform
MObject DPFastNoise::aScale;
MObject DPFastNoise::aUniformScale;
MObject DPFastNoise::aOffset;

MObject DPFastNoise::outDD;



void *DPFastNoise::creator() {
    return ::dommy::maya::createObject("DPFastNoise", [&]() {
        return new DPFastNoise();
    });
}



MStatus DPFastNoise::evalNoiseParms(MDataBlock& data, FastNoiseParams& parms)
{
    MStatus status;
    MDataHandle handle;
    handle = data.inputValue(aNoiseType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mNoiseType = handle.asShort();
    handle = data.inputValue(aInterpType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mInterpType = handle.asShort();
    handle = data.inputValue(aSeed, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mSeed = handle.asInt();
    handle = data.inputValue(aAmplitube, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mAmplitude = handle.asFloat();
    handle = data.inputValue(aFrequency, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mFrequency = handle.asFloat();
    handle = data.inputValue(aOctave, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int octave = handle.asInt();
    handle = data.inputValue(aLacunarity, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mLacunarity = handle.asFloat();
    handle = data.inputValue(aGain, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mFractalGain = handle.asFloat();
    handle = data.inputValue(aFractalType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mFractalType = handle.asShort();
    handle = data.inputValue(aCellularDistanceFunction, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mCellDistanceType = handle.asShort();
    handle = data.inputValue(aCellularReturnType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mCellReturnType = handle.asShort();
    handle = data.inputValue(aPositionWarpAmp, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mPositionWarpAmp = handle.asFloat();
    handle = data.inputValue(aUniformScale, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parms.mUniformScale = handle.asFloat();
    handle = data.inputValue(aOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& offset = handle.asFloat3();
    parms.mOffsetVec = MVector(offset[0], offset[1], offset[2]);
    handle = data.inputValue(aScale, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& scale = handle.asFloat3();
    parms.mScaleVec = MVector(scale[0], scale[1], scale[2]);
    if (octave < 1 || octave > 30) return MS::kInvalidParameter;
    parms.mOctave = static_cast<unsigned int>(octave);

    return validateFastNoiseParams(parms);
}


namespace {

template <typename Geometry, typename NormalReader>
MStatus evaluateFastNoiseGeometry(const Geometry& geometry, FastNoiseParams& params,
                                short deformType, const Imath::V3d& baseVector,
                                const std::string& vectorAttributeName,
                                DPointList& points, DScalarDataList& signals,
                                NormalReader readNormals)
{
    const std::size_t count = geometry.numberOfPoints();
    if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
        return MS::kInvalidParameter;
    points = *geometry.getPositionPtrConst();
    signals.assign(count, 0.0f);
    DOMMY_RETURN_IF_FAILURE(createFastNoiseFromPosition(params, points, signals));
    std::vector<Imath::V3d> vectors(count, baseVector);
    if (deformType == NOISE_DEFROM_ATTRIBUTE) {
        if (!dommy::attribute::convertVector3Attribute(
                geometry.getAttr(vectorAttributeName), count, vectors)) return MS::kInvalidParameter;
    } else if (deformType == NOISE_DEFROM_CURVE_NORMAL || deformType == NOISE_DEFROM_MESH_NORMAL) {
        if (count != 0U) DOMMY_RETURN_IF_FAILURE(readNormals(points, vectors));
    }
    if (deformType != NOISE_DEFROM_NONE)
        for (std::size_t i = 0U; i < count; ++i)
            if (!dommy::point::offsetAlongVector(points[i], vectors[i],
                    params.mAmplitude, signals[i], 1.0, points[i])) return MS::kInvalidParameter;
    return MS::kSuccess;
}

template <typename Geometry, typename NormalReader>
MStatus deformFastNoiseGeometry(Geometry& geometry, FastNoiseParams& params,
                                short deformType, const Imath::V3d& baseVector,
                                const std::string& vectorAttributeName,
                                bool isNoiseAttribute, const std::string& noiseAttributeName,
                                NormalReader readNormals)
{
    DPointList points;
    DScalarDataList signals;
    DOMMY_RETURN_IF_FAILURE(evaluateFastNoiseGeometry(geometry, params, deformType, baseVector,
        vectorAttributeName, points, signals, readNormals));
    const std::size_t count = points.size();
    if (!geometry.replacePositions(points)) return MS::kFailure;
    // Consume input aliases before publication; OFF leaves attributes intact.
    if (isNoiseAttribute && !dommy::attribute::commitStagedAttribute(geometry, noiseAttributeName,
            DPAttributeData::Ptr(new DPFloatAttr(signals)), count)) return MS::kFailure;
    return MS::kSuccess;
}

MStatus advanceFastNoiseSeed(FastNoiseParams& params, std::size_t offset, bool enabled)
{
    // Retain the historical cumulative sequence (s, s+1, s+3, ...).
    if (enabled) {
        if (offset > static_cast<std::size_t>(std::numeric_limits<int>::max() - params.mSeed))
            return MS::kInvalidParameter;
        params.mSeed += static_cast<int>(offset);
    }
    return MS::kSuccess;
}

} // namespace

MStatus DPFastNoise::compute(const MPlug& plug, MDataBlock& data) try
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

    FastNoiseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseParms(data, params));
    if ((deformType != NOISE_DEFROM_NONE && deformType != NOISE_DEFROM_VECTOR &&
         deformType != NOISE_DEFROM_ATTRIBUTE && deformType != NOISE_DEFROM_CURVE_NORMAL &&
         deformType != NOISE_DEFROM_MESH_NORMAL) ||
        !std::isfinite(baseVector[0]) ||
        !std::isfinite(baseVector[1]) || !std::isfinite(baseVector[2]) ||
        (isNoiseAttribute && !dommy::attribute::validAttributeName(noiseAttributeName)))
        return MS::kInvalidParameter;


    MFnPluginData pluginData;
    if (inputType == DPData::id) {
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
        if (!input || !input->hasValidPointTopology() || deformType > NOISE_DEFROM_ATTRIBUTE ||
            input->numberOfPoints() > dommy::generation::kMaxGeneratedPointCount)
            return MS::kInvalidParameter;
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPointPayload(*input,
            sizeof(DPoint) + sizeof(float) + sizeof(Imath::V3d), workspace);
        dommy::point::preflightGeneratedPointAttribute(input->numberOfPoints(), noiseAttributeName,
            isNoiseAttribute ? sizeof(float) : 0U, workspace);
        DPointList points;
        DScalarDataList signals;
        DOMMY_RETURN_IF_FAILURE(evaluateFastNoiseGeometry(*input, params, deformType, baseVector,
            vectorAttributeName, points, signals,
            [](const DPointList&, std::vector<Imath::V3d>&) -> MStatus { return MS::kInvalidParameter; }));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) -> MStatus {
                DOMMY_RETURN_IF_FAILURE(output.copyRepeatedFrom(*input, 1U,
                    [&](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                        point = points[index]; return MS::kSuccess;
                    }));
                if (isNoiseAttribute && !dommy::attribute::commitStagedAttribute(output,
                        noiseAttributeName, DPAttributeData::Ptr(new DPFloatAttr(signals)), points.size()))
                    return MS::kFailure;
                return MS::kSuccess;
            }));
    } else if (inputType == DCurveData::id) {
        const DCurveData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input));
        std::size_t totalPoints = 0U;
        if (!input || !input->hasValidCurveTopology() ||
            !input->getCheckedTotalCvs(totalPoints) ||
            deformType == NOISE_DEFROM_MESH_NORMAL ||
            input->numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount ||
            totalPoints > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPrimitivePointPayload(*input, input->numberOfCurves(),
            sizeof(DPoint) + sizeof(float) + sizeof(Imath::V3d), noiseAttributeName,
            isNoiseAttribute ? sizeof(float) : 0U, workspace);
        DCurveData* result = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, result));
        if (!result) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(result->copyFrom(*input));
        std::size_t offset = 0U;
        for (auto item = result->beginCurves(); item != result->endCurves(); ++item, ++offset) {
            if (!*item) return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(advanceFastNoiseSeed(params, offset, useSeedOffset));
            DOMMY_RETURN_IF_FAILURE(deformFastNoiseGeometry(**item, params, deformType, baseVector,
                vectorAttributeName, isNoiseAttribute, noiseAttributeName,
                [&](const DPointList& points, std::vector<Imath::V3d>& vectors) -> MStatus {
                    MObject object;
                    DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*item, object));
                    MFnNurbsCurve function(object, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    for (std::size_t i = 0U; i < points.size(); ++i) {
                        MVector normal;
                        DOMMY_RETURN_IF_FAILURE(dommy::curve::directionAtClosestPoint(
                            function, DPToMP(points[i]), true, kMFnNurbsEpsilon, MSpace::kObject, normal));
                        if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
                        vectors[i] = Imath::V3d(normal[0], normal[1], normal[2]);
                    }
                    return MS::kSuccess;
                }));
        }
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
            deformType == NOISE_DEFROM_CURVE_NORMAL ||
            input->numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount ||
            totalPoints > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPrimitivePointPayload(*input, input->numberOfMesh(),
            sizeof(DPoint) + sizeof(float) + sizeof(Imath::V3d), noiseAttributeName,
            isNoiseAttribute ? sizeof(float) : 0U, workspace);
        DMeshData* result = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, result));
        if (!result) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(result->copyFrom(*input));
        std::size_t offset = 0U;
        for (auto item = result->beginMeshes(); item != result->endMeshes(); ++item, ++offset) {
            if (!*item) return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(advanceFastNoiseSeed(params, offset, useSeedOffset));
            DOMMY_RETURN_IF_FAILURE(deformFastNoiseGeometry(**item, params, deformType, baseVector,
                vectorAttributeName, isNoiseAttribute, noiseAttributeName,
                [&](const DPointList& points, std::vector<Imath::V3d>& vectors) -> MStatus {
                    MObject object;
                    DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(**item, object, DMeshPointSpace::Local));
                    MFnMesh function(object, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    for (std::size_t i = 0U; i < points.size(); ++i) {
                        MVector normal;
                        DOMMY_RETURN_IF_FAILURE(function.getVertexNormal(
                            static_cast<int>(i), normal, MSpace::kObject));
                        if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
                        vectors[i] = Imath::V3d(normal[0], normal[1], normal[2]);
                    }
                    return MS::kSuccess;
                }));
        }
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
DOMMY_COMPUTE_CATCH("DPFastNoise")

MStatus DPFastNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    inDD = gAttr.create("inDD", "idd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    MFnEnumAttribute enumFn;
    aNoiseType = enumFn.create("NoiseType", "noty", FAST_NOISE_SIMPLEX, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Value", FAST_NOISE_VALUE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Value Fractal", FAST_NOISE_VALUEFRACTAL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Gradient", FAST_NOISE_PERLIN));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Gradient Fractal", FAST_NOISE_PERLINFRACTAL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Simplex", FAST_NOISE_SIMPLEX));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Simplex Fractal", FAST_NOISE_SIMPLEXFRACTAL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Cellular", FAST_NOISE_CELLULAR));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("White Noise", FAST_NOISE_WHITENOISE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Cubic", FAST_NOISE_CUBIC));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Cubic Fractal", FAST_NOISE_CUBICFRACTAL));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseType));
    MFnEnumAttribute interpEnum;
    aInterpType = interpEnum.create("InterpType", "itpty", FN_INTERP_HERMITE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(interpEnum.addField("Linear", FN_INTERP_LINEAR));
    DOMMY_RETURN_IF_FAILURE(interpEnum.addField("Hermite", FN_INTERP_HERMITE));
    DOMMY_RETURN_IF_FAILURE(interpEnum.addField("Quintic", FN_INTERP_QUINTIC));
    DOMMY_RETURN_IF_FAILURE(interpEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(interpEnum.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInterpType));
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
    MFnEnumAttribute defEnumFn;
    aDeformType = defEnumFn.create("DeformType", "defty", NOISE_DEFROM_VECTOR, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("None", NOISE_DEFROM_NONE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Vector", NOISE_DEFROM_VECTOR));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Attribute", NOISE_DEFROM_ATTRIBUTE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Curve Normal", NOISE_DEFROM_CURVE_NORMAL));
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
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1337));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    aAmplitube = nAttr.create("Amplitube", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitube));
    aOctave = nAttr.create("Octave", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3));
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
    aGain = nAttr.create("Gain", "gn", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGain));
    MFnEnumAttribute fraxfEnumFn;
    aFractalType = fraxfEnumFn.create("FractalType", "frcty", FN_FRAC_FBM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fraxfEnumFn.addField("FBM", FN_FRAC_FBM));
    DOMMY_RETURN_IF_FAILURE(fraxfEnumFn.addField("Billow", FN_FRAC_BILLOW));
    DOMMY_RETURN_IF_FAILURE(fraxfEnumFn.addField("RigidMulti", FN_FRAC_RIGIDMULTI));
    DOMMY_RETURN_IF_FAILURE(fraxfEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fraxfEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFractalType));
    MFnEnumAttribute cellDistEnumFn;
    aCellularDistanceFunction = cellDistEnumFn.create("CellularDistanceFunction", "celdistty",
                                                      FN_CELL_DIST_EUCLIDEAN, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cellDistEnumFn.addField("Euclidean", FN_CELL_DIST_EUCLIDEAN));
    DOMMY_RETURN_IF_FAILURE(cellDistEnumFn.addField("Manhattan", FN_CELL_DIST_MANHATTAN));
    DOMMY_RETURN_IF_FAILURE(cellDistEnumFn.addField("Natural", FN_CELL_DIST_NATURAL));
    DOMMY_RETURN_IF_FAILURE(cellDistEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(cellDistEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCellularDistanceFunction));
    MFnEnumAttribute cellRtnEnumFn;
    aCellularReturnType =
        cellRtnEnumFn.create("CellularReturnType", "celrtnty", FN_CELL_RTN_CELLVALUE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("CellValue", FN_CELL_RTN_CELLVALUE));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("NoiseLookup", FN_CELL_RTN_NOISELOOKUP));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance", FN_CELL_RTN_DISTANCE));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance2", FN_CELL_RTN_DISTANCE2));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance2Add", FN_CELL_RTN_DISTANCE2ADD));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance2Sub", FN_CELL_RTN_DISTANCE2SUB));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance2Mul", FN_CELL_RTN_DISTANCE2MUL));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.addField("Distance2Div", FN_CELL_RTN_DISTANCE2DIV));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(cellRtnEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCellularReturnType));
    aPositionWarpAmp =
        nAttr.create("PositionWarpAmp", "poswpamp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPositionWarpAmp));
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
    outDD = gAttr.create("outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInterpType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseSeedOffset, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNoiseAttribute, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseAttributeName, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformVector, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorAttributeName, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGain, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFractalType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCellularDistanceFunction, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCellularReturnType, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPositionWarpAmp, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDD));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPFastNoise", "initialize",
                                                  std::current_exception());
}

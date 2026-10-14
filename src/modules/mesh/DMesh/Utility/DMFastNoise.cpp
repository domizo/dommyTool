//
//  DMFastNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/13.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMFastNoise.h"
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


#include "FastNoise.h"



const MTypeId DMFastNoise::typeId( ID_DM_FAST_NOISE );



MObject DMFastNoise::inDM;

MObject DMFastNoise::aNoiseType;
MObject DMFastNoise::aInterpType;


MObject DMFastNoise::aSeed;
MObject DMFastNoise::aUseSeedOffset;


//attach attribute
MObject DMFastNoise::aIsNoiseAttribute;
MObject DMFastNoise::aNoiseAttributeName;


//deform
MObject DMFastNoise::aDeformType;
MObject DMFastNoise::aDeformVector;
MObject DMFastNoise::aVectorAttributeName;


//noise
MObject DMFastNoise::aAmplitube;
MObject DMFastNoise::aFrequency;
MObject DMFastNoise::aOctave;
MObject DMFastNoise::aLacunarity;
MObject DMFastNoise::aGain;
MObject DMFastNoise::aFractalType;


//Cellular
MObject DMFastNoise::aCellularDistanceFunction;
MObject DMFastNoise::aCellularReturnType;
//    static MObject aCellularNoiseLookup;

//Position Warping
MObject DMFastNoise::aPositionWarpAmp;

//transform
MObject DMFastNoise::aScale;
MObject DMFastNoise::aUniformScale;
MObject DMFastNoise::aOffset;

MObject DMFastNoise::outDM;



void *DMFastNoise::creator() {
    return ::dommy::maya::createObject("DMFastNoise", [&]() {
        return new DMFastNoise();
    });
}



MStatus DMFastNoise::evalNoiseParms(MDataBlock& data, FastNoiseParams& parms)
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


MStatus DMFastNoise::compute(const MPlug& plug, MDataBlock& data) try
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

    FastNoiseParams params;
    DOMMY_RETURN_IF_FAILURE(evalNoiseParms(data, params));
    const double amplitude = params.mAmplitude;
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
        DOMMY_RETURN_IF_FAILURE(createFastNoiseFromPosition(params, points, signals));
        for (const float signal : signals)
            if (!std::isfinite(signal)) return MS::kInvalidParameter;
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
DOMMY_COMPUTE_CATCH("DMFastNoise")

MStatus DMFastNoise::initialize() try {
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInterpType, outDM));
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGain, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFractalType, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCellularDistanceFunction, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCellularReturnType, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPositionWarpAmp, outDM));

    // noise transform
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMFastNoise", "initialize",
                                                  std::current_exception());
}

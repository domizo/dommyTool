//
//  DCNoise.cpp
//  dommyTool
//
//  Created by yuzo on 2/21/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCNoise.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "DStringOp.h"
#include "DNoiseOp.h"

#include <cmath>
#include <limits>

const MTypeId DCNoise::typeId(ID_DC_NOISE);

MObject DCNoise::inDC;

MObject DCNoise::aNoiseType;
MObject DCNoise::aSeed;
MObject DCNoise::aUseSeedOffset;

// attribute
MObject DCNoise::aIsNoiseAttribute;
MObject DCNoise::aNoiseAttributeName;

// deform
MObject DCNoise::aDeformType;
MObject DCNoise::aDeformVector;
MObject DCNoise::aVectorAttributeName;

// generate calc position
MObject DCNoise::aNoiseCalcPosition;
MObject DCNoise::aPositionAttribute;

// noise
MObject DCNoise::aAmplitube;
MObject DCNoise::aFrequency;
MObject DCNoise::aOctave;
MObject DCNoise::aLacunarity;
MObject DCNoise::aPersistence;

// voronoi
MObject DCNoise::aIsVoronoiDistance;
MObject DCNoise::aVoronoiDisplacement;

// turbulence
MObject DCNoise::aTurbulencePower;
MObject DCNoise::aTurbulenceRoughnesss;

// wood
MObject DCNoise::aWooodFrequency;
MObject DCNoise::aWooodScale;
MObject DCNoise::aWooodBias;
MObject DCNoise::aWooodGrain;
MObject DCNoise::aWooodAngle;

MObject DCNoise::aScale;
MObject DCNoise::aUniformScale;
MObject DCNoise::aOffset;

MObject DCNoise::outDC;

void* DCNoise::creator() {
    return ::dommy::maya::createObject("DCNoise", [&]() {
        return new DCNoise();
    });
}

MStatus DCNoise::evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms) {
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

MStatus DCNoise::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;

    MStatus status;
    MDataHandle control = data.inputValue(aUseSeedOffset, &status);
    if (!status)
        return status;
    const bool useSeedOffset = control.asBool();
    control = data.inputValue(aIsNoiseAttribute, &status);
    if (!status)
        return status;
    const bool isNoiseAttribute = control.asBool();
    control = data.inputValue(aDeformType, &status);
    if (!status)
        return status;
    const NoiseDeformType deformType = control.asShort();
    control = data.inputValue(aNoiseCalcPosition, &status);
    if (!status)
        return status;
    const NoiseCalcPositionType positionType = control.asShort();
    control = data.inputValue(aAmplitube, &status);
    if (!status)
        return status;
    const float amplitude = control.asFloat();

    MDataHandle deformVectorHandle = data.inputValue(aDeformVector, &status);
    if (!status)
        return status;
    const float3& deformVector = deformVectorHandle.asFloat3();
    const DVec3Data deformation(deformVector[0], deformVector[1], deformVector[2]);

    control = data.inputValue(aPositionAttribute, &status);
    if (!status)
        return status;
    const std::string positionAttributeName = control.asString().asChar();
    control = data.inputValue(aNoiseAttributeName, &status);
    if (!status)
        return status;
    const std::string noiseAttributeName = control.asString().asChar();
    control = data.inputValue(aVectorAttributeName, &status);
    if (!status)
        return status;
    const std::string vectorAttributeName = control.asString().asChar();

    if (deformType < NOISE_DEFROM_NONE || deformType > NOISE_DEFROM_CURVE_NORMAL ||
        positionType < NOISE_CALC_POS_WORLD || positionType >= NOISE_CALC_POSITION_COUNT ||
        !std::isfinite(amplitude) || !std::isfinite(deformation[0]) ||
        !std::isfinite(deformation[1]) || !std::isfinite(deformation[2]) ||
        (isNoiseAttribute && noiseAttributeName.empty())) {
        return MS::kInvalidParameter;
    }

    const DCurveData* inputCurves = NULL;
    status = getValidInputDC(inDC, data, inputCurves);
    if (!status)
        return status;
    if (!inputCurves ||
        inputCurves->numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount ||
        inputCurves->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    DCurveData* outputCurves = NULL;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    if (!status)
        return status;
    if (!outputCurves)
        return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*inputCurves));

    NoiseBaseParams baseParams;
    status = evalNoiseBaseParms(data, baseParams);
    if (!status)
        return status;
    const int baseSeed = baseParams.mSeed;

    bool hasInvalidAttribute = false;
    bool hasMismatchedAttribute = false;

    std::size_t curveOffset = 0U;
    for (auto curve = outputCurves->beginCurves(); curve != outputCurves->endCurves();
         ++curve, ++curveOffset) {
        if (!*curve)
            return MS::kFailure;

        dommy::geometry::StagedPositionEdits<
            DCurveInternalData> positionEdit;
        if (!positionEdit.add(**curve)) return MS::kFailure;
        DPointList* const points = &positionEdit.positions(0U);
        if (points->size() > dommy::generation::kMaxGeneratedPointCount) {
            return MS::kInvalidParameter;
        }
        for (const DPoint& point : *points) {
            if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2])) {
                return MS::kInvalidParameter;
            }
        }

        NoiseBaseParams params = baseParams;
        if (useSeedOffset) {
            const std::size_t maximumOffset =
                static_cast<std::size_t>(std::numeric_limits<int>::max() - baseSeed);
            if (curveOffset > maximumOffset) {
                return MS::kInvalidParameter;
            }
            params.mSeed = baseSeed + static_cast<int>(curveOffset);
        }

        DScalarDataList signals(points->size(), 0.0f);
        bool usedAttributePosition = false;
        if (positionType == NOISE_CALC_PARAM && (*curve)->isExistAttr(positionAttributeName)) {
            const DPAttributeData::ConstPtr positionAttribute =
                (*curve)->getAttr(positionAttributeName);
            if (!positionAttribute)
                return MS::kFailure;

            DPointList parameterPositions(points->size(), DPoint(0.0f, 0.0f, 0.0f));
            const DP_ATTR_DATA_TYPE attributeType = positionAttribute->getAttrType();
            if (isVector3DAttrType(attributeType)) {
                getDCPAttributeMVectorArrayFromVec3(*curve, positionAttributeName,
                                                    parameterPositions);
                usedAttributePosition = parameterPositions.size() == points->size();
            } else if (isSingleRealAttrType(attributeType)) {
                DScalarDataList parameterValues;
                getDCPAttributeFromScalar(*curve, positionAttributeName, parameterValues);
                if (parameterValues.size() == parameterPositions.size()) {
                    for (std::size_t index = 0U; index < parameterValues.size(); ++index) {
                        parameterPositions[index] = DPoint(parameterValues[index], 0.0f, 0.0f);
                    }
                    usedAttributePosition = true;
                }
            } else {
                hasInvalidAttribute = true;
            }

            if (usedAttributePosition) {
                DOMMY_RETURN_IF_FAILURE(createNoiseFromPosition(params, parameterPositions, signals));
            } else {
                hasMismatchedAttribute = true;
            }
        }
        if (!usedAttributePosition) {
            DOMMY_RETURN_IF_FAILURE(createNoiseFromPosition(params, *points, signals));
        }

        for (float& signal : signals) {
            signal *= amplitude;
            if (!std::isfinite(signal))
                return MS::kInvalidParameter;
        }

        if (deformType == NOISE_DEFROM_VECTOR) {
            for (std::size_t index = 0U; index < points->size(); ++index) {
                const DPoint result = (*points)[index] + deformation * signals[index];
                if (!std::isfinite(result[0]) || !std::isfinite(result[1]) ||
                    !std::isfinite(result[2])) {
                    return MS::kInvalidParameter;
                }
                (*points)[index] = result;
            }
        } else if (deformType == NOISE_DEFROM_ATTRIBUTE) {
            if (!(*curve)->isExistAttr(vectorAttributeName)) {
                hasInvalidAttribute = true;
            } else {
                const DPAttributeData::ConstPtr vectorAttribute =
                    (*curve)->getAttr(vectorAttributeName);
                if (!vectorAttribute || !isVector3DAttrType(vectorAttribute->getAttrType())) {
                    hasInvalidAttribute = true;
                } else {
                    DVec3DataList vectors;
                    getDCPAttributeMVectorArrayFromVec3(*curve, vectorAttributeName, vectors);
                    if (vectors.size() != signals.size()) {
                        hasMismatchedAttribute = true;
                    } else {
                        for (std::size_t index = 0U; index < points->size(); ++index) {
                            const DPoint result =
                                (*points)[index] + vectors[index] * signals[index];
                            if (!std::isfinite(result[0]) || !std::isfinite(result[1]) ||
                                !std::isfinite(result[2])) {
                                return MS::kInvalidParameter;
                            }
                            (*points)[index] = result;
                        }
                    }
                }
            }
        } else if (deformType == NOISE_DEFROM_CURVE_NORMAL && !points->empty()) {
            MObject curveObject;
            status = generateMCurveFromDcurve(*curve, curveObject);
            if (!status)
                return status;
            MFnNurbsCurve curveFunction(curveObject, &status);
            if (!status)
                return status;

            MVector previousBinormal = MVector::zero;
            for (std::size_t index = 0U; index < points->size(); ++index) {
                double parameter = 0.0;
                curveFunction.closestPoint(
                    MPoint((*points)[index][0], (*points)[index][1], (*points)[index][2]),
                    &parameter, kMFnNurbsEpsilon, MSpace::kObject, &status);
                if (!status)
                    return status;
                if (!std::isfinite(parameter)) {
                    return MS::kInvalidParameter;
                }

                const MVector rawTangent =
                    curveFunction.tangent(parameter, MSpace::kObject, &status);
                if (!status)
                    return status;
                MVector tangent;
                if (!dommy::maya::normalizedVector(rawTangent, tangent)) {
                    return MS::kInvalidParameter;
                }

                MVector rawNormal;
                if (index == 0U) {
                    rawNormal = curveFunction.normal(parameter, MSpace::kObject, &status);
                    if (!status || !dommy::maya::isFiniteVector(rawNormal)) {
                        status = MS::kSuccess;
                        const MVector referenceAxis = std::fabs(tangent.y) < 0.9
                                                          ? MVector(0.0, 1.0, 0.0)
                                                          : MVector(1.0, 0.0, 0.0);
                        rawNormal = referenceAxis ^ tangent;
                    }
                } else {
                    rawNormal = previousBinormal ^ tangent;
                }

                MVector normal;
                if (!dommy::maya::normalizedVector(rawNormal, normal)) {
                    return MS::kInvalidParameter;
                }
                MVector binormal;
                if (!dommy::maya::normalizedVector(tangent ^ normal, binormal)) {
                    return MS::kInvalidParameter;
                }

                const DPoint result = (*points)[index] + DPoint(static_cast<float>(normal.x),
                                                                static_cast<float>(normal.y),
                                                                static_cast<float>(normal.z)) *
                                                             signals[index];
                if (!std::isfinite(result[0]) || !std::isfinite(result[1]) ||
                    !std::isfinite(result[2])) {
                    return MS::kInvalidParameter;
                }
                (*points)[index] = result;
                previousBinormal = binormal;
            }
        }

        if (isNoiseAttribute) {
            if (points->size() != signals.size()) {
                hasMismatchedAttribute = true;
            } else {
                if (!dommy::attribute::commitStagedAttribute(**curve, noiseAttributeName,
                        DPAttributeData::Ptr(new DPFloatAttr(signals)), points->size()))
                    return MS::kFailure;
            }
        }
        if (!positionEdit.commit()) return MS::kFailure;
    }

    if (hasInvalidAttribute) {
        EchoWarning(name().asChar(),
                    "Some curve attributes are missing or use an unsupported type.");
    }
    if (hasMismatchedAttribute) {
        EchoWarning(name().asChar(), "Some curve attributes do not match their point count.");
    }

    MDataHandle output = data.outputValue(outDC, &status);
    if (!status)
        return status;
    status = dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id);
    if (!status)
        return status;
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCNoise")

MStatus DCNoise::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
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
    MFnEnumAttribute defEnumFn;
    aDeformType = defEnumFn.create("DeformType", "defty", NOISE_DEFROM_VECTOR, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("None", NOISE_DEFROM_NONE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Vector", NOISE_DEFROM_VECTOR));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Attribute", NOISE_DEFROM_ATTRIBUTE));
    DOMMY_RETURN_IF_FAILURE(defEnumFn.addField("Curve Normal", NOISE_DEFROM_CURVE_NORMAL));
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
    MFnEnumAttribute posEnumFn;
    aNoiseCalcPosition =
        posEnumFn.create("NoiseCalcPosition", "ncalp", NOISE_CALC_POS_WORLD, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(posEnumFn.addField("Point Position", NOISE_CALC_POS_WORLD));
    DOMMY_RETURN_IF_FAILURE(posEnumFn.addField("Attribute Param", NOISE_CALC_PARAM));
    DOMMY_RETURN_IF_FAILURE(posEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(posEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseCalcPosition));
    const MObject aPositionAttributeDefault = fnStringData.create("param", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPositionAttribute = tAttr.create("PositionAttribute", "posattrn", MFnData::kString,
                                      aPositionAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPositionAttribute));
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
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseSeedOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNoiseAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeformVector, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseCalcPosition, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPositionAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsVoronoiDistance, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoronoiDisplacement, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulencePower, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTurbulenceRoughnesss, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodFrequency, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodBias, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodGrain, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWooodAngle, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCNoise", "initialize",
                                                  std::current_exception());
}

//
//  DBubbleCrystal.cpp
//  dommyTool
//
//  Created by yuzo on 2015/02/10.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DBubbleCrystal.h"

#include <cmath>
#include <limits>
#include <vector>

// Attribute
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnCompoundAttribute.h>

// mesh
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>

// data
#include <maya/MPointArray.h>
#include <maya/MPoint.h>
#include <maya/MIntArray.h>

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "dommyMath.h"
#include "perlin.h"

// bubble shape
MObject DBubbleCrystal::side;
MObject DBubbleCrystal::radius;
MObject DBubbleCrystal::divide;
MObject DBubbleCrystal::midDivide;

MObject DBubbleCrystal::growth;

// randam
MObject DBubbleCrystal::seed;

MObject DBubbleCrystal::arcNoiseAmplitude;
MObject DBubbleCrystal::arcNoiseFrequency;
MObject DBubbleCrystal::arcNoiseSmooth;
MObject DBubbleCrystal::arcNoiseSpace;

MObject DBubbleCrystal::heightNoiseAmplitude;
MObject DBubbleCrystal::heightNoiseFrequency;
MObject DBubbleCrystal::heightNoiseSmooth;
MObject DBubbleCrystal::heightNoiseSpace;
MObject DBubbleCrystal::heightNoisePower;

// output
MObject DBubbleCrystal::outPutMesh;

const MTypeId DBubbleCrystal::typeId(0x00339);

void* DBubbleCrystal::creator() {
    return ::dommy::maya::createObject("DBubbleCrystal", [&]() {
        return new DBubbleCrystal();
    });
}

MStatus DBubbleCrystal::compute(const MPlug& plug, MDataBlock& block) {
    try {
        MStatus stat;

        if (plug == outPutMesh) {

            const MDataHandle sideValHandle = block.inputValue(side, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int sideVal = sideValHandle.asInt();
            const MDataHandle radiusValHandle = block.inputValue(radius, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double radiusVal = radiusValHandle.asDouble();
            const MDataHandle divideValHandle = block.inputValue(divide, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int divideVal = divideValHandle.asInt();
            const MDataHandle midDivHandle = block.inputValue(midDivide, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int midDiv = midDivHandle.asInt();
            const MDataHandle growthValHandle = block.inputValue(growth, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double growthVal = growthValHandle.asDouble();
            const MDataHandle seedValHandle = block.inputValue(seed, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int seedVal = seedValHandle.asInt();

            const MDataHandle arcNoiseAmplitudeValHandle = block.inputValue(arcNoiseAmplitude, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double arcNoiseAmplitudeVal = arcNoiseAmplitudeValHandle.asDouble();
            const MDataHandle arcNoiseFrequencyValHandle = block.inputValue(arcNoiseFrequency, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int arcNoiseFrequencyVal = arcNoiseFrequencyValHandle.asInt();

            const MDataHandle heightNoiseAmplitudeValHandle = block.inputValue(heightNoiseAmplitude, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double heightNoiseAmplitudeVal = heightNoiseAmplitudeValHandle.asDouble();
            const MDataHandle heightNoiseFrequencyValHandle = block.inputValue(heightNoiseFrequency, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int heightNoiseFrequencyVal = heightNoiseFrequencyValHandle.asInt();
            const MDataHandle heightNoisePowerValHandle = block.inputValue(heightNoisePower, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double heightNoisePowerVal = heightNoisePowerValHandle.asDouble();

            if (!std::isfinite(radiusVal) || !std::isfinite(growthVal) ||
                !std::isfinite(arcNoiseAmplitudeVal) || !std::isfinite(heightNoiseAmplitudeVal) ||
                !std::isfinite(heightNoisePowerVal) || heightNoisePowerVal < 0.0 ||
                radiusVal < 0.0 || growthVal < 0.0 || sideVal < 3 || divideVal < 0 || midDiv < 0 ||
                seedVal < 0 || seedVal == std::numeric_limits<int>::max() ||
                arcNoiseFrequencyVal < 0 || heightNoiseFrequencyVal < 0 ||
                arcNoiseFrequencyVal > 1000000 || heightNoiseFrequencyVal > 1000000) {
                MGlobal::displayError("DBubbleCrystal: invalid generation inputs.");
                return MS::kFailure;
            }

            std::size_t checkedSide = 0;
            std::size_t checkedDivide = 0;
            std::size_t checkedMidDivide = 0;
            std::size_t midStepCount = 0;
            std::size_t depthCount = 0;
            std::size_t totalSideCount = 0;
            std::size_t faceCount = 0;
            std::size_t vertexCount = 0;
            if (!dommy::generation::checkedCount(
                    sideVal, dommy::generation::kMaxGeneratedPointCount, checkedSide) ||
                !dommy::generation::checkedCount(
                    divideVal, dommy::generation::kMaxGeneratedPointCount, checkedDivide) ||
                !dommy::generation::checkedCount(midDiv, dommy::generation::kMaxGeneratedPointCount,
                                                 checkedMidDivide) ||
                !dommy::generation::checkedSum(checkedMidDivide, 1U,
                                               dommy::generation::kMaxGeneratedPointCount,
                                               midStepCount) ||
                !dommy::generation::checkedSum(
                    checkedDivide, 1U, dommy::generation::kMaxGeneratedPointCount, depthCount) ||
                !dommy::generation::checkedProduct(checkedSide, midStepCount,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   totalSideCount) ||
                !dommy::generation::checkedProduct(totalSideCount, depthCount,
                                                   dommy::generation::kMaxGeneratedFaceCount,
                                                   faceCount) ||
                !dommy::generation::checkedSum(
                    faceCount, 1U, dommy::generation::kMaxGeneratedPointCount, vertexCount)) {
                MGlobal::displayError("DBubbleCrystal: mesh exceeds the generation budget.");
                return MS::kFailure;
            }

            // メッシュ生成のデータ
            MFloatPointArray vertexArray;
            MIntArray finPolyCountArray, finPolyConnectArray;

            int midStep = midDiv + 1;
            float midDivVal = 1.0 / (float)midStep;

            // 回転の情報
            float stepDegree = 360.0 / (float)sideVal;

            // 分割によるの長さの決定
            float stepLength = 0.0f;
            if (!dommy::numeric::checkedFloatCast(
                    radiusVal / (float)(divideVal + 1), stepLength)) return MS::kInvalidParameter;

            unsigned long totalSides = static_cast<unsigned long>(totalSideCount);

            // 中心のポイントの追加
            if (vertexArray.append(MFloatPoint(0.0, 0.0, 0.0)) != MS::kSuccess) {
                return MS::kFailure;
            }

            // 円周点の追加
            for (int i = 0; i < sideVal; ++i) {

                // 元の点データ
                MVector inVec(0.0, 0.0, stepLength);
                inVec = inVec.rotateBy(MVector::kYaxis,
                                       dommy::math::degreesToRadians(-(stepDegree * i)));

                MVector ptVec = inVec;
                if (!dommy::maya::isFloatRepresentableVector(ptVec)) return MS::kInvalidParameter;

                if (vertexArray.append(MFloatPoint(ptVec.x, ptVec.y, ptVec.z)) != MS::kSuccess) {
                    return MS::kFailure;
                }

                // 中点の作成
                if (midStep > 1) {
                    MVector nextVec(0.0, 0.0, stepLength);
                    nextVec = nextVec.rotateBy(
                        MVector::kYaxis, dommy::math::degreesToRadians(-(stepDegree * (i + 1))));
                    MVector dirVec(nextVec.x - inVec.x, nextVec.y - inVec.y, nextVec.z - inVec.z);

                    for (int m = 1; m < midStep; ++m) {
                        MVector mVec = dirVec * (midDivVal * m);
                        const MVector point = mVec + ptVec;
                        if (!dommy::maya::isFloatRepresentableVector(point)) return MS::kInvalidParameter;
                        if (vertexArray.append(MFloatPoint(point.x, point.y, point.z)) != MS::kSuccess) {
                            return MS::kFailure;
                        }
                    }
                }
            }

            // 分割数分の頂点の追加
            for (int i = 0; i < divideVal; ++i) {
                const float ringDistance = stepLength * (i + 2);
                if (!std::isfinite(ringDistance)) return MS::kInvalidParameter;
                for (int j = 0; j < sideVal; ++j) {

                    // Sideの点データ
                    MVector inVec(0.0, 0.0, ringDistance);
                    inVec = inVec.rotateBy(MVector::kYaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * j)));

                    MVector ptVec = inVec;
                    if (!dommy::maya::isFloatRepresentableVector(ptVec)) return MS::kInvalidParameter;

                    if (vertexArray.append(MFloatPoint(ptVec.x, ptVec.y, ptVec.z)) !=
                        MS::kSuccess) {
                        return MS::kFailure;
                    }

                    // 中点の作成
                    if (midStep > 1) {
                        MVector nextVec(0.0, 0.0, ringDistance);
                        nextVec = nextVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(
                                                                        -(stepDegree * (j + 1))));
                        MVector dirVec(nextVec.x - inVec.x, nextVec.y - inVec.y,
                                       nextVec.z - inVec.z);

                        for (int m = 1; m < midStep; ++m) {
                            MVector mVec = dirVec * (midDivVal * m);
                            const MVector point = mVec + ptVec;
                            if (!dommy::maya::isFloatRepresentableVector(point)) return MS::kInvalidParameter;
                            if (vertexArray.append(MFloatPoint(point.x, point.y, point.z)) != MS::kSuccess) {
                                return MS::kFailure;
                            }
                        }
                    }
                }
            }
            MFloatArray arcNoiseArray;
            MFloatArray heightNoiseArray;
            MFloatArray maxLengthArray;

            noise::module::Perlin arcNoise;
            arcNoise.SetSeed(seedVal);
            arcNoise.SetFrequency(arcNoiseFrequencyVal);
            arcNoise.SetOctaveCount(2);
            noise::module::Perlin heightNoise;
            heightNoise.SetSeed(seedVal + 1);
            heightNoise.SetFrequency(heightNoiseFrequencyVal);
            heightNoise.SetOctaveCount(2);

            // ノイズの作成
            for (unsigned long i = 0; i < totalSides; i++) {
                unsigned long index = i + (divideVal * totalSides) + 1; // 一番外側の頂点リスト

                MFloatPoint pt = vertexArray[index];
                MVector noiseVec(pt);
                const double noiseVal = arcNoise.GetValue(noiseVec.x, noiseVec.y, noiseVec.z);
                const double heightNoiseVal =
                    heightNoise.GetValue(noiseVec.x, noiseVec.y + 50.0, noiseVec.z);

                float arcSample = 0.0f, heightSample = 0.0f;
                if (!dommy::numeric::checkedFloatCast(noiseVal, arcSample) ||
                    !dommy::numeric::checkedFloatCast(heightNoiseVal, heightSample) ||
                    arcNoiseArray.append(arcSample) != MS::kSuccess ||
                    heightNoiseArray.append(heightSample) != MS::kSuccess) {
                    return MS::kFailure;
                }

                // noiseを適用した際の最大距離
                noiseVec = noiseVec * noiseVal * arcNoiseAmplitudeVal;
                noiseVec += pt;
                float mDist = 0.0f;
                if (!dommy::numeric::checkedFloatCast(
                        dommy::math::distance(noiseVec, MVector(0.0, 0.0, 0.0)), mDist)) return MS::kInvalidParameter;
                if (!std::isfinite(mDist) || maxLengthArray.append(mDist) != MS::kSuccess) {
                    return MS::kFailure;
                }
            }

            // ノイズの適用

            for (int i = 0; i < divideVal + 1; ++i) {
                for (int j = 0; j < sideVal * midStep; ++j) {

                    unsigned long index = j + (i * totalSides) + 1;
                    MFloatPoint pt = vertexArray[index];
                    MVector noiseVec(pt);
                    double noiseVal = arcNoiseArray[j];
                    double heightNoise = heightNoiseArray[j];
                    noiseVec = noiseVec * noiseVal * arcNoiseAmplitudeVal;

                    noiseVec += pt;

                    float mDist = 0.0f;
                    if (!dommy::numeric::checkedFloatCast(
                            dommy::math::distance(noiseVec, MVector(0.0, 0.0, 0.0)), mDist)) return MS::kInvalidParameter;
                    float mLengthMax = maxLengthArray[j];

                    float finHeight = 0.0f;
                    if (mLengthMax > 1.0e-12f && growthVal > 1.0e-12) {
                        float noiseweight = 1.0f;
                        if (mDist > (mLengthMax * growthVal)) {
                            float growthLength = 0.0f;
                            if (!dommy::numeric::checkedFloatCast(mLengthMax * growthVal, growthLength) ||
                                growthLength <= 0.0f) return MS::kInvalidParameter;
                            const float mLengthBias = mDist / growthLength;
                            if (!std::isfinite(mLengthBias) || mLengthBias <= 0.0f) return MS::kInvalidParameter;
                            noiseVec /= mLengthBias;
                            noiseweight = 0.0f;
                        }
                        const double heightBase = std::fabs(
                            (heightNoise * heightNoiseAmplitudeVal) * (mDist / mLengthMax));
                        const double height =
                            std::pow(heightBase, heightNoisePowerVal) * noiseweight;
                        if (!dommy::numeric::checkedFloatCast(height, finHeight)) {
                            MGlobal::displayError(
                                "DBubbleCrystal: noise height exceeds finite float storage.");
                            return MS::kFailure;
                        }
                    } else {
                        noiseVec = MVector::zero;
                    }

                    if (!dommy::maya::isFloatRepresentableVector(noiseVec)) return MS::kInvalidParameter;
                    vertexArray[index].x = noiseVec.x;
                    vertexArray[index].y = finHeight;
                    vertexArray[index].z = noiseVec.z;
                }
            }

            // 中心部分の面、コネクトの作成
            for (int c = 0; c < sideVal * midStep; ++c) {
                if (finPolyCountArray.append(3) != MS::kSuccess) {
                    return MS::kFailure;
                }

                if (c == (sideVal * midStep - 1)) {
                    if (finPolyConnectArray.append(0) != MS::kSuccess ||
                        finPolyConnectArray.append(1) != MS::kSuccess ||
                        finPolyConnectArray.append(c + 1) != MS::kSuccess) {
                        return MS::kFailure;
                    }
                } else {
                    if (finPolyConnectArray.append(0) != MS::kSuccess ||
                        finPolyConnectArray.append(c + 2) != MS::kSuccess ||
                        finPolyConnectArray.append(c + 1) != MS::kSuccess) {
                        return MS::kFailure;
                    }
                }
            }

            for (int i = 0; i < divideVal; ++i) {
                for (int j = 0; j < sideVal * midStep; ++j) {

                    if (finPolyCountArray.append(4) != MS::kSuccess) {
                        return MS::kFailure;
                    }

                    int offset = sideVal * midStep * i;

                    if (j == (sideVal * midStep - 1)) {
                        if (finPolyConnectArray.append(j + 1 + offset) != MS::kSuccess ||
                            finPolyConnectArray.append(1 + offset) != MS::kSuccess ||
                            finPolyConnectArray.append(1 + sideVal * midStep + offset) !=
                                MS::kSuccess ||
                            finPolyConnectArray.append(j + sideVal * midStep + 1 + offset) !=
                                MS::kSuccess) {
                            return MS::kFailure;
                        }

                    } else {
                        if (finPolyConnectArray.append(j + 1 + offset) != MS::kSuccess ||
                            finPolyConnectArray.append(j + 2 + offset) != MS::kSuccess ||
                            finPolyConnectArray.append(j + sideVal * midStep + 2 + offset) !=
                                MS::kSuccess ||
                            finPolyConnectArray.append(j + sideVal * midStep + 1 + offset) !=
                                MS::kSuccess) {
                            return MS::kFailure;
                        }
                    }
                }
            }
            // メッシュの生成
            MDataHandle outputMeshHnd = block.outputValue(outPutMesh, &stat);
            if (!stat)
                return stat;

            MFnMeshData meshDataFn;
            MObject newOutMeshData = meshDataFn.create(&stat);
            if (!stat || newOutMeshData.isNull())
                return MS::kFailure;

            MFnMesh newMeshFn;
            const MObject mesh =
                newMeshFn.create(vertexArray.length(), finPolyCountArray.length(), vertexArray,
                                 finPolyCountArray, finPolyConnectArray, newOutMeshData, &stat);
            if (!stat || mesh.isNull())
                return MS::kFailure;

            stat = outputMeshHnd.set(newOutMeshData);
            if (!stat)
                return stat;
            return block.setClean(plug);

        } else {
            stat = MS::kUnknownParameter;
        }

        return stat;
    }
    DOMMY_COMPUTE_CATCH("DBubbleCrystal")
}

MStatus DBubbleCrystal::initialize() try {
    MStatus status;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;

    const auto addIntegerAttribute = [&nAttr](MObject& attribute, const char* longName,
                                              const char* shortName, int defaultValue, int minimum,
                                              int maximum, bool hidden) -> MStatus {
        MStatus localStatus;
        attribute =
            nAttr.create(longName, shortName, MFnNumericData::kInt, defaultValue, &localStatus);
        if (!localStatus)
            return localStatus;
        localStatus = nAttr.setMin(minimum);
        if (!localStatus)
            return localStatus;
        localStatus = nAttr.setMax(maximum);
        if (!localStatus)
            return localStatus;
        localStatus = nAttr.setKeyable(!hidden);
        if (!localStatus)
            return localStatus;
        return nAttr.setHidden(hidden);
    };
    const auto addDoubleAttribute = [&nAttr](MObject& attribute, const char* longName,
                                             const char* shortName, double defaultValue,
                                             bool hasMinimum, double minimum, bool hasMaximum,
                                             double maximum, bool hidden) -> MStatus {
        MStatus localStatus;
        attribute =
            nAttr.create(longName, shortName, MFnNumericData::kDouble, defaultValue, &localStatus);
        if (!localStatus)
            return localStatus;
        if (hasMinimum) {
            localStatus = nAttr.setMin(minimum);
            if (!localStatus)
                return localStatus;
        }
        if (hasMaximum) {
            localStatus = nAttr.setMax(maximum);
            if (!localStatus)
                return localStatus;
        }
        localStatus = nAttr.setKeyable(!hidden);
        if (!localStatus)
            return localStatus;
        return nAttr.setHidden(hidden);
    };
    const auto registerAttribute = [](const MObject& attribute) {
        return DBubbleCrystal::addAttribute(attribute);
    };

    status =
        addIntegerAttribute(side, "side", "si", 4, 3,
                            static_cast<int>(dommy::generation::kMaxGeneratedPointCount), false);
    if (!status)
        return status;
    status = addDoubleAttribute(radius, "radius", "r", 3.0, true, 0.0, false, 0.0, false);
    if (!status)
        return status;
    status =
        addIntegerAttribute(divide, "divide", "d", 1, 0,
                            static_cast<int>(dommy::generation::kMaxGeneratedPointCount), false);
    if (!status)
        return status;
    status =
        addIntegerAttribute(midDivide, "midDivide", "md", 0, 0,
                            static_cast<int>(dommy::generation::kMaxGeneratedPointCount), false);
    if (!status)
        return status;
    status = addDoubleAttribute(growth, "growth", "g", 1.0, true, 0.0, true, 1.0, false);
    if (!status)
        return status;
    status =
        addIntegerAttribute(seed, "seed", "se", 1, 0, std::numeric_limits<int>::max() - 1, false);
    if (!status)
        return status;
    status = addDoubleAttribute(arcNoiseAmplitude, "arcNoiseAmplitude", "ana", 0.0, false, 0.0,
                                false, 0.0, false);
    if (!status)
        return status;
    status =
        addIntegerAttribute(arcNoiseFrequency, "arcNoiseFrequency", "anf", 1, 0, 1000000, false);
    if (!status)
        return status;
    status = addDoubleAttribute(arcNoiseSmooth, "arcNoiseSmooth", "anm", 2.0, false, 0.0, false,
                                0.0, true);
    if (!status)
        return status;
    status = addDoubleAttribute(arcNoiseSpace, "arcNoiseSpace", "anp", 2.0, false, 0.0, false, 0.0,
                                true);
    if (!status)
        return status;
    status = addDoubleAttribute(heightNoiseAmplitude, "heightNoiseAmplitude", "hna", 0.0, false,
                                0.0, false, 0.0, false);
    if (!status)
        return status;
    status = addIntegerAttribute(heightNoiseFrequency, "heightNoiseFrequency", "hnf", 1, 0, 1000000,
                                 false);
    if (!status)
        return status;
    status = addDoubleAttribute(heightNoiseSmooth, "heightNoiseSmooth", "hnm", 2.0, false, 0.0,
                                false, 0.0, true);
    if (!status)
        return status;
    status = addDoubleAttribute(heightNoiseSpace, "heightNoiseSpace", "hnp", 2.0, false, 0.0, false,
                                0.0, true);
    if (!status)
        return status;
    status = addDoubleAttribute(heightNoisePower, "heightNoisePower", "hnw", 1.0, true, 0.0, false,
                                0.0, false);
    if (!status)
        return status;

    const MObject numericAttributes[] = {side,
                                         radius,
                                         divide,
                                         midDivide,
                                         growth,
                                         seed,
                                         arcNoiseAmplitude,
                                         arcNoiseFrequency,
                                         arcNoiseSmooth,
                                         arcNoiseSpace,
                                         heightNoiseAmplitude,
                                         heightNoiseFrequency,
                                         heightNoiseSmooth,
                                         heightNoiseSpace,
                                         heightNoisePower};
    for (const MObject& attribute : numericAttributes) {
        status = registerAttribute(attribute);
        if (!status)
            return status;
    }

    outPutMesh = gAttr.create("outPutMesh", "opm", &status);
    if (!status)
        return status;
    status = gAttr.addDataAccept(MFnData::kMesh);
    if (!status)
        return status;
    status = gAttr.setHidden(true);
    if (!status)
        return status;
    status = gAttr.setStorable(false);
    if (!status)
        return status;
    status = gAttr.setWritable(false);
    if (!status)
        return status;
    status = addAttribute(outPutMesh);
    if (!status)
        return status;

    const MObject affectingAttributes[] = {side,
                                           radius,
                                           divide,
                                           midDivide,
                                           growth,
                                           seed,
                                           arcNoiseAmplitude,
                                           arcNoiseFrequency,
                                           heightNoiseAmplitude,
                                           heightNoiseFrequency,
                                           heightNoisePower};
    for (const MObject& attribute : affectingAttributes) {
        status = attributeAffects(attribute, outPutMesh);
        if (!status)
            return status;
    }
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBubbleCrystal", "initialize", std::current_exception());
}

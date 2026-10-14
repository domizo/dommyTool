//
//  DPointPlaneEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointPlaneEmit.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"

#include "dpoint_ids.h"
#include "DPData.h"
#include "DPointDataAccess.h"
#include "DPAttributeAccess.h"

#include "DMayaHeaders.h"

#include "DRandomGenerator.h"
#include "DMayaDebug.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DBatchRange.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaRampSampling.h"
#include "DGlobalTex.h"
#include "DPointGeneration.h"

#include <algorithm>
#include <cmath>

#include "PoissonGenerator.h"

#include "geometry/DBlueNoiseSampler.h"

const MTypeId DPointPlaneEmit::typeId(ID_DP_PLANE_EMIT);

// Attribute

MObject DPointPlaneEmit::emitType;

MObject DPointPlaneEmit::sizeX;
MObject DPointPlaneEmit::sizeY;
MObject DPointPlaneEmit::gridX;
MObject DPointPlaneEmit::gridY;
MObject DPointPlaneEmit::count;

MObject DPointPlaneEmit::aTileStep;
MObject DPointPlaneEmit::aCenter;

MObject DPointPlaneEmit::aRandomType;
MObject DPointPlaneEmit::seed;
MObject DPointPlaneEmit::aPoissonMinDistance;
MObject DPointPlaneEmit::aRelaxIteration;

MObject DPointPlaneEmit::aRadiusRange;
MObject DPointPlaneEmit::aPackingType;
MObject DPointPlaneEmit::aPackingSampling;
MObject DPointPlaneEmit::aPackingGradientBiasRamp;
MObject DPointPlaneEmit::aUseRadiusBias;
MObject DPointPlaneEmit::aRadiusBiasRamp;
MObject DPointPlaneEmit::aBiasRange;
// for Map
MObject DPointPlaneEmit::inObject;
MObject DPointPlaneEmit::aMap;

// pre Packing DPoint
MObject DPointPlaneEmit::aPrePackingDP;
MObject DPointPlaneEmit::aRadiusAttribute;
MObject DPointPlaneEmit::aBaseRadius;
MObject DPointPlaneEmit::aPreDPGroupName;

//
MObject DPointPlaneEmit::aSpread;
MObject DPointPlaneEmit::aAngleType;
MObject DPointPlaneEmit::aCustomAngle;

MObject DPointPlaneEmit::outDP;

void* DPointPlaneEmit::creator() {
    return ::dommy::maya::createObject("DPointPlaneEmit", [&]() {
        return new DPointPlaneEmit();
    });
}

void DPointPlaneEmit::postConstructor() {
    dommy::maya::initializeNodeRamps("DPointPlaneEmit", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aRadiusBiasRamp,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(
            thisMObject(), aPackingGradientBiasRamp, {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}

bool DPointPlaneEmit::getMapValues(
    const std::vector<MPoint>& points, std::vector<float>& values) {
    if (points.empty() ||
        points.size() >
            static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return false;
    }

    MDoubleArray uCoordinates;
    MDoubleArray vCoordinates;
    MStatus status = uCoordinates.setLength(
        static_cast<unsigned int>(points.size()));
    if (!status) return false;
    status = vCoordinates.setLength(
        static_cast<unsigned int>(points.size()));
    if (!status) return false;

    for (std::size_t index = 0U; index < points.size(); ++index) {
        const MPoint& point = points[index];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) {
            return false;
        }

        double uValue = 0.0;
        double vValue = 0.0;
        switch (m_obj_hnd.type()) {
        case MFnData::kNurbsSurface:
            status = srfFn.getParamAtPoint(
                point, uValue, vValue, MSpace::kWorld);
            break;
        case MFnData::kMesh: {
            float2 uv = {0.0f, 0.0f};
            MPoint queryPoint(point);
            status = meshFn.getUVAtPoint(
                queryPoint, uv, MSpace::kWorld);
            uValue = uv[0];
            vValue = uv[1];
            break;
        }
        default:
            return false;
        }
        if (!status || !std::isfinite(uValue) ||
            !std::isfinite(vValue)) {
            return false;
        }
        status = uCoordinates.set(
            uValue, static_cast<unsigned int>(index));
        if (!status) return false;
        status = vCoordinates.set(
            vValue, static_cast<unsigned int>(index));
        if (!status) return false;
    }

    MVectorArray colors;
    MDoubleArray alphas;
    status = evaluateDynamics2dTextureInBatches(
        thisMObject(), aMap, uCoordinates, vCoordinates, colors, alphas);
    if (!status || alphas.length() != points.size()) {
        return false;
    }

    std::vector<float> sampledValues(points.size(), 0.0f);
    for (unsigned int index = 0U; index < alphas.length(); ++index) {
        if (!dommy::packing::clampUnit(
                static_cast<float>(alphas[index]), sampledValues[index])) {
            return false;
        }
    }
    values.swap(sampledValues);
    return true;
}

bool DPointPlaneEmit::getMitchellBestCandidate(PackedCircle& result) {
    float greatestDistance = -std::numeric_limits<float>::infinity();
    PackedCircle winningSample(MPoint(0.0, 0.0, 0.0), 0.0f);

    if (m_pack_type == D_PACK_MAP) {
        const std::size_t candidateCount =
            placedPoints.empty() ? 1U : m_sampling;
        for (std::size_t begin = 0U; begin < candidateCount;) {
            const dommy::batch::Range range = dommy::batch::rangeAt(
                candidateCount,
                dommy::batch::kMaximumShadingSampleCount, begin);
            if (range.empty()) return false;

            std::vector<MPoint> positions;
            positions.reserve(range.count);
            for (std::size_t index = 0U; index < range.count; ++index) {
                positions.push_back(MPoint(
                    m_distribx(m_eng), 0.0, m_distriby(m_eng)));
            }

            std::vector<float> mapValues;
            if (!getMapValues(positions, mapValues) ||
                mapValues.size() != positions.size()) {
                return false;
            }
            for (std::size_t index = 0U; index < positions.size(); ++index) {
                const PackedCircle candidate(
                    positions[index],
                    m_min_rad + m_rad_size * mapValues[index]);
                if (!dommy::packing::isFinite(candidate)) return false;
                if (placedPoints.empty()) {
                    result = candidate;
                    return true;
                }
                const float currentDistance =
                    getDistanceToNearestSample(candidate);
                if (!std::isfinite(currentDistance)) return false;
                if (currentDistance > greatestDistance) {
                    greatestDistance = currentDistance;
                    winningSample = candidate;
                }
            }
            begin = range.end();
        }

        result = winningSample;
        return dommy::packing::isFinite(result);
    }

    for (std::size_t i = 0; i < m_sampling; ++i) {
        MPoint pos;
        float radius = 0.0f;
        switch (m_pack_type) {
        case D_PACK_RANDOM: {
            if (m_use_rad_bias) {
                if (m_max_count == 0) {
                    return false;
                }
                const float placedCount =
                    static_cast<float>(placedPoints.empty() ? 1U : placedPoints.size());
                float biasPosition =
                    1.0f - placedCount / static_cast<float>(m_max_count);
                if (!dommy::packing::clampUnit(biasPosition, biasPosition)) {
                    return false;
                }
                float rampValue = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(
                        m_radiusBiasRamp, biasPosition, rampValue)) {
                    return false;
                }
                if (!dommy::packing::clampUnit(rampValue, rampValue)) {
                    return false;
                }
                const float maximumOffset = m_rad_size * rampValue;
                radius = dommy::packing::uniform(m_eng, m_min_rad + maximumOffset * m_bias_range,
                                                 m_min_rad + maximumOffset);
            } else {
                radius = dommy::packing::uniform(m_eng, m_min_rad, m_max_rad);
            }
            pos = MPoint(m_distribx(m_eng), 0.0, m_distriby(m_eng));
        } break;
        case D_PACK_GRADATION: {
            pos = MPoint(m_distribx(m_eng), 0.0, m_distriby(m_eng));
            float rampPosition = 0.0f;
            float rampValue = 0.0f;
            if (!dommy::packing::normalizedPosition(static_cast<float>(pos.x), m_grid_minX,
                                                    m_grid_sizeX, rampPosition)) {
                return false;
            }
            if (!dommy::maya::sampleFiniteRamp(
                    m_packGradientRamp, rampPosition, rampValue)) {
                return false;
            }
            if (!dommy::packing::clampUnit(rampValue, rampValue)) {
                return false;
            }
            radius = m_min_rad + m_rad_size * rampValue;
        } break;
        default:
            return false;
        }

        const PackedCircle candidate(pos, radius);
        if (!dommy::packing::isFinite(candidate)) {
            return false;
        }
        if (placedPoints.empty()) {
            result = candidate;
            return true;
        }

        const float currentDistance = getDistanceToNearestSample(candidate);
        if (!std::isfinite(currentDistance)) {
            return false;
        }
        if (currentDistance > greatestDistance) {
            greatestDistance = currentDistance;
            winningSample = candidate;
        }
    }

    result = winningSample;
    return dommy::packing::isFinite(result);
}

float DPointPlaneEmit::getDistanceToNearestSample(const PackedCircle& candidate) const {

    float shortestDistance = std::numeric_limits<float>::infinity();

    // Loop into all samples
    for (std::size_t i = 0; i < placedPoints.size(); ++i) {
        const PackedCircle& otherSample = placedPoints[i];
        // Measure current distance between the two points
        // Distance = distance from point a and point b - sum of the radii of the 2 circles
        float currentDistance = candidate.m_pt.distanceTo(otherSample.m_pt) -
                                (otherSample.m_radius + candidate.m_radius);
        // Get nearest sample (from previously placed ones)
        if (!std::isfinite(currentDistance))
            return currentDistance;
        if (currentDistance < shortestDistance) {
            shortestDistance = currentDistance;
        }
    }
    return shortestDistance;
}

bool DPointPlaneEmit::placeNewSample() {
    PackedCircle sample(MPoint(0.0, 0.0, 0.0), 0.0f);
    if (!getMitchellBestCandidate(sample)) {
        return false;
    }
    placedPoints.push_back(sample);
    return true;
}

MStatus DPointPlaneEmit::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {

        // attribute
        const MDataHandle emitTypeInputHandle = data.inputValue(emitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const DT_PLANE_EMIT_TYPE emitTypeVal = emitTypeInputHandle.asShort();
        const MDataHandle sizeXInputHandle = data.inputValue(sizeX, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float sizeXVal = sizeXInputHandle.asFloat();
        const MDataHandle sizeYInputHandle = data.inputValue(sizeY, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float sizeYVal = sizeYInputHandle.asFloat();
        const MDataHandle gridXInputHandle = data.inputValue(gridX, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawGridX = gridXInputHandle.asInt();
        const MDataHandle gridYInputHandle = data.inputValue(gridY, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawGridY = gridYInputHandle.asInt();
        const MDataHandle seedInputHandle = data.inputValue(seed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSeed = seedInputHandle.asInt();
        const MDataHandle countInputHandle = data.inputValue(count, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawCount = countInputHandle.asInt();
        const MDataHandle aCenterInputHandle = data.inputValue(aCenter, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MFloatVector centerVec = aCenterInputHandle.asFloatVector();

        std::size_t gridXVal = 0;
        std::size_t gridYVal = 0;
        std::size_t countVal = 0;
        std::size_t gridPointCount = 0;
        if (emitTypeVal < DT_PLANE_EMIT_GRID || emitTypeVal >= DT_PLANE_EMIT_TYPE_COUNT ||
            !std::isfinite(sizeXVal) || !std::isfinite(sizeYVal) || sizeXVal <= 0.0f ||
            sizeYVal <= 0.0f || rawSeed < 0 || !std::isfinite(centerVec.x) ||
            !std::isfinite(centerVec.y) || !std::isfinite(centerVec.z) ||
            !dommy::generation::checkedCount(rawGridX, dommy::generation::kMaxGeneratedPointCount,
                                             gridXVal) ||
            !dommy::generation::checkedCount(rawGridY, dommy::generation::kMaxGeneratedPointCount,
                                             gridYVal) ||
            !dommy::generation::checkedCount(rawCount, dommy::generation::kMaxGeneratedPointCount,
                                             countVal) ||
            !dommy::generation::checkedProduct(
                gridXVal, gridYVal, dommy::generation::kMaxGeneratedPointCount, gridPointCount)) {
            MGlobal::displayError("DPointPlaneEmit: invalid or excessive input value.");
            return MS::kFailure;
        }
        const unsigned int seedVal = static_cast<unsigned int>(rawSeed);

        //		const double radiusVal = data.inputValue( radius ).asDouble();

        //		const double voxelSizeVal = block.inputValue( voxelSize ).asDouble();

        // point array
        DPointList newPtArray;

        switch (emitTypeVal) {
        case DT_PLANE_EMIT_GRID: {
            newPtArray = DPointList(gridPointCount, DPoint(0.0, 0.0, 0.0));

            size_t countX = gridXVal;
            size_t countY = gridYVal;

            float stepX, stepY;
            float offsetX = sizeXVal * .5;
            float offsetY = sizeYVal * .5;

            if (countX < 2) {
                stepX = 0;
                offsetX = 0;
            } else {
                stepX = sizeXVal / (float)(countX - 1);
            }
            if (countY < 2) {
                stepY = 0;
                offsetY = 0;
            } else {
                stepY = sizeYVal / (float)(countY - 1);
            }

            size_t index = 0;
            size_t i = 0;
            while (i < countX) {
                size_t j = 0;
                while (j < countY) {
                    newPtArray[index][0] = stepX * i - offsetX;
                    newPtArray[index][2] = stepY * j - offsetY;
                    index++;
                    j++;
                }
                i++;
            }
        } break;
        case DT_PLANE_EMIT_TILE: {
            if (gridXVal > 0 && gridYVal > 0) {

                newPtArray.resize(gridPointCount);

                const MDataHandle aTileStepInputHandle = data.inputValue(aTileStep, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const float2& step = aTileStepInputHandle.asFloat2();
                if (!std::isfinite(step[0]) || !std::isfinite(step[1])) {
                    MGlobal::displayError("DPointPlaneEmit: tile step is not finite.");
                    return MS::kFailure;
                }

                size_t index = 0;
                for (size_t i = 0; i < gridXVal; i++) {
                    for (size_t j = 0; j < gridYVal; j++) {
                        newPtArray[index][0] = step[0] * i + centerVec[0];
                        newPtArray[index][1] = centerVec[1];
                        newPtArray[index][2] = step[1] * j + centerVec[2];
                        index++;
                    }
                }
            }
        } break;
        case DT_PLANE_EMIT_RANDOM: {
            const MDataHandle aRandomTypeInputHandle = data.inputValue(aRandomType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const short rndType = aRandomTypeInputHandle.asShort();
            if (rndType < D_DIST_UNIFORM || rndType >= D_DISTRIBUTE_TYPE_COUNT) {
                MGlobal::displayError("DPointPlaneEmit: unsupported random distribution.");
                return MS::kFailure;
            }

            float offsetX = sizeXVal * .5;
            float offsetY = sizeYVal * .5;

            if (rndType == D_DIST_UNIFORM) {

                std::mt19937 eng(seedVal);

                std::uniform_real_distribution<float> distribx(-offsetX, offsetX);
                std::uniform_real_distribution<float> distriby(-offsetY, offsetY);

                newPtArray.resize(countVal);

                for (size_t i = 0; i < countVal; i++) {
                    newPtArray[i][0] = distribx(eng);
                    newPtArray[i][1] = 0.0;
                    newPtArray[i][2] = distriby(eng);
                }

            } else if (rndType == D_DIST_POISSON) {

                const MDataHandle aPoissonMinDistanceInputHandle =
                    data.inputValue(aPoissonMinDistance, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const float mindist = aPoissonMinDistanceInputHandle.asFloat();
                if (!dommy::packing::isBoundedPoissonRequest(countVal, mindist)) {
                    MGlobal::displayError("DPointPlaneEmit: invalid or excessive Poisson request.");
                    return MS::kFailure;
                }
                if (countVal == 0U)
                    break;

                PoissonGenerator::DefaultPRNG PRNG(seedVal);
                const auto Points = PoissonGenerator::GeneratePoissonPoints(
                    countVal, PRNG, countVal, false, mindist);

                newPtArray.resize(Points.size());
                size_t index = 0;
                for (const auto& p : Points) {
                    newPtArray[index][0] = p.x * sizeXVal - offsetX;
                    newPtArray[index][1] = 0.0;
                    newPtArray[index][2] = p.y * sizeYVal - offsetY;

                    index++;
                }

            } else if (rndType == D_DIST_LOYDS_RELAXATION) {

                const MDataHandle aRelaxIterationInputHandle =
                    data.inputValue(aRelaxIteration, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const int maxIteration = aRelaxIterationInputHandle.asInt();
                std::size_t iterationCount = 0;
                std::size_t workItemCount = 0;
                if (!dommy::generation::checkedCount(
                        maxIteration, dommy::generation::kMaxWorkItemCount, iterationCount) ||
                    !dommy::generation::checkedProduct(countVal, iterationCount,
                                                       dommy::generation::kMaxWorkItemCount,
                                                       workItemCount)) {
                    MGlobal::displayError(
                        "DPointPlaneEmit: relaxation request exceeds the work limit.");
                    return MS::kFailure;
                }

                MPointArray newPoints;
                DOMMY_RETURN_IF_FAILURE(DGenerateRelaxPoint2D(
                    countVal, iterationCount, rawSeed, 0.0f, sizeXVal, 0.0f, sizeYVal, newPoints));

                size_t finCount = newPoints.length();

                if (finCount) {
                    newPtArray.resize(finCount);
                    for (size_t i = 0; i < finCount; ++i) {
                        newPtArray[i][0] = newPoints[i].x - offsetX;
                        newPtArray[i][1] = 0.0;
                        newPtArray[i][2] = newPoints[i].z - offsetY;
                    }
                }
            }
        } break;
        case DT_PLANE_EMIT_BLUENOISE: {
            float offsetX = sizeXVal * .5;
            float offsetY = sizeYVal * .5;
            const MDataHandle aPoissonMinDistanceInputHandle =
                data.inputValue(aPoissonMinDistance, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float mindist = aPoissonMinDistanceInputHandle.asFloat();
            if (!std::isfinite(mindist) || mindist < -1.0f) {
                MGlobal::displayError("DPointPlaneEmit: invalid blue-noise distance.");
                return MS::kFailure;
            }
            if (countVal == 0U)
                break;
            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
            std::vector<dommy::geometry::Point2> outputPoints;
            if (!dommy::sampling::generateBlueNoise2D(
                    countVal, mindist, randomGenerator, outputPoints)) {
                MGlobal::displayError(
                    "DPointPlaneEmit: blue-noise request is invalid or exceeds its safety budget.");
                return MS::kFailure;
            }

            newPtArray.resize(outputPoints.size());

            size_t index = 0;
            for (const auto& p : outputPoints) {
                newPtArray[index][0] = p.x * sizeXVal - offsetX;
                newPtArray[index][1] = 0.0;
                newPtArray[index][2] = p.y * sizeYVal - offsetY;
                index++;
            }

        } break;
        case DT_PLANE_EMIT_SPHERE_PACKING: {
#pragma mark -
#pragma mark case Sphere packing
            placedPoints.clear();
            placedPoints.reserve(countVal);

            const MDataHandle aPackingTypeInputHandle = data.inputValue(aPackingType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_pack_type = aPackingTypeInputHandle.asShort();
            if (m_pack_type < D_PACK_RANDOM || m_pack_type >= D_PACKING_TYPE_COUNT) {
                MGlobal::displayError("DPointPlaneEmit: unsupported packing mode.");
                return MS::kFailure;
            }

            if (m_pack_type == D_PACK_MAP) {
                MDataHandle objHnd = data.inputValue(inObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                MFnData::Type objType = objHnd.type();
                if (objType != MFnData::kNurbsSurface && objType != MFnData::kMesh) {
                    EchoWarning(name().asChar(), "Not Support input object type.");
                    return MS::kFailure;
                }

                MPlug mapPlug(thisMObject(), aMap);
                const bool mapConnected = mapPlug.isConnected(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!mapConnected) {
                    EchoWarning(name().asChar(), "Map plug is no connect to texture.");
                    return MS::kFailure;
                }
                m_obj_hnd = objHnd;

                if (m_obj_hnd.type() == MFnData::kNurbsSurface) {
                    DOMMY_RETURN_IF_FAILURE(srfFn.setObject(m_obj_hnd.data()));
                } else if (m_obj_hnd.type() == MFnData::kMesh) {
                    DOMMY_RETURN_IF_FAILURE(meshFn.setObject(m_obj_hnd.data()));
                }
            }

            const MDataHandle aPackingSamplingInputHandle =
                data.inputValue(aPackingSampling, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int rawPackingSampling = aPackingSamplingInputHandle.asInt();
            std::size_t packingWorkCount = 0;
            if (!dommy::generation::checkedCount(
                    rawPackingSampling, dommy::generation::kMaxWorkItemCount, m_sampling) ||
                m_sampling == 0 ||
                !dommy::generation::checkedProduct(
                    countVal, m_sampling, dommy::generation::kMaxWorkItemCount, packingWorkCount)) {
                MGlobal::displayError("DPointPlaneEmit: packing request exceeds the work limit.");
                return MS::kFailure;
            }

            MPlug rampPlug(thisMObject(), aPackingGradientBiasRamp);
            if (m_pack_type == D_PACK_GRADATION) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::bindValueRamp(rampPlug, m_packGradientRamp));
            }

            const MDataHandle aUseRadiusBiasInputHandle = data.inputValue(aUseRadiusBias, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_use_rad_bias = aUseRadiusBiasInputHandle.asBool();
            MPlug radrampPlug(thisMObject(), aRadiusBiasRamp);
            if (m_pack_type == D_PACK_RANDOM && m_use_rad_bias) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::bindValueRamp(radrampPlug, m_radiusBiasRamp));
            }

            const MDataHandle aRadiusRangeInputHandle = data.inputValue(aRadiusRange, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double2& radRange = aRadiusRangeInputHandle.asDouble2();
            m_min_rad = radRange[0];
            m_max_rad = radRange[1];
            if (m_min_rad > m_max_rad) {
                std::swap(m_min_rad, m_max_rad);
            }
            m_rad_size = m_max_rad - m_min_rad;
            const MDataHandle aBiasRangeInputHandle = data.inputValue(aBiasRange, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_bias_range = aBiasRangeInputHandle.asDouble();
            if (!std::isfinite(m_min_rad) || !std::isfinite(m_max_rad) || m_min_rad < 0.0f ||
                m_max_rad < 0.0f || !std::isfinite(m_bias_range) || m_bias_range < 0.0f ||
                m_bias_range > 1.0f) {
                MGlobal::displayError("DPointPlaneEmit: invalid packing values.");
                return MS::kFailure;
            }

            m_max_count = countVal;

            m_eng = std::mt19937(seedVal);

            float offsetX = sizeXVal * .5;
            float offsetY = sizeYVal * .5;

            m_grid_minX = -offsetX;
            m_grid_sizeX = sizeXVal;

            m_distribx = std::uniform_real_distribution<float>(-offsetX, offsetX);
            m_distriby = std::uniform_real_distribution<float>(-offsetY, offsetY);

#pragma mark check pre packing points
            MPlug prePackDPPlug(thisMObject(), aPrePackingDP);
            const MDataHandle aPreDPGroupNameInputHandle = data.inputValue(aPreDPGroupName, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const std::string prePackGroupName = aPreDPGroupNameInputHandle.asString().asChar();
            bool prePackeckGrp = false;
            DPIdList newGrpIds;
            const bool prePackingConnected = prePackDPPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (prePackingConnected) {
                const DPData* preDPData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDP(aPrePackingDP, data, preDPData));
                if (!preDPData) return MS::kFailure;
                // Release imported-batch scratch before generating new candidates.
                {
                    if (preDPData->numberOfPoints() > dommy::generation::kMaxGeneratedPointCount) {
                        MGlobal::displayError(
                            "DPointPlaneEmit: pre-packing data exceeds the point limit.");
                        return MS::kFailure;
                    }

                    const MDataHandle aBaseRadiusInputHandle = data.inputValue(aBaseRadius, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const double baseScale = aBaseRadiusInputHandle.asDouble();
                    const MDataHandle aRadiusAttributeInputHandle =
                        data.inputValue(aRadiusAttribute, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const std::string radAttributeName =
                        aRadiusAttributeInputHandle.asString().asChar();
                    if (!std::isfinite(baseScale) || baseScale < 0.0) {
                        MGlobal::displayError("DPointPlaneEmit: pre-packing radius is invalid.");
                        return MS::kFailure;
                    }

                    DScalarDataList preScaleList(preDPData->numberOfPoints(), baseScale);

                    if (preDPData->isExistAttr(radAttributeName)) {
                        getDScalarArrayFromAttribute(*preDPData, radAttributeName, preScaleList);
                    }
                    if (preScaleList.size() != preDPData->numberOfPoints()) {
                        MGlobal::displayError(
                            "DPointPlaneEmit: pre-packing radius count is invalid.");
                        return MS::kFailure;
                    }

                    size_t n = 0;
                    //                        placedPoints.resize(preDPData->numberOfPoints());

                    for (auto pIt = preDPData->beginPt(), epIt = preDPData->endPt(); pIt != epIt;
                         ++pIt) {
                        PackedCircle p(DPToMP(*pIt), preScaleList[n]);
                        if (!dommy::packing::isFinite(p)) {
                            MGlobal::displayError("DPointPlaneEmit: pre-packing data contains an "
                                                  "invalid point or radius.");
                            return MS::kFailure;
                        }
                        placedPoints.push_back(p);
                        n++;
                    }

                    // group
                    if (placedPoints.size() > 0) {
                        prePackeckGrp = true;
                        DPIdList preGrpIds(placedPoints.size(), true);
                        newGrpIds.insert(newGrpIds.end(), preGrpIds.begin(), preGrpIds.end());
                    }
                }
            }

#pragma mark generate packing point
            while (placedPoints.size() < countVal) {
                if (!placeNewSample()) {
                    MGlobal::displayError(
                        "DPointPlaneEmit: failed to evaluate a packing candidate.");
                    return MS::kFailure;
                }
            }
            DScalarDataList radius;
            for (const auto& p : placedPoints) {
                newPtArray.push_back(MPToDP(p.m_pt));
                radius.push_back(p.m_radius);
            }

            // pre dpointのgroupの調整
            if (prePackeckGrp) {
                for (size_t i = newGrpIds.size(), N = newPtArray.size(); i < N; i++) {
                    newGrpIds.push_back(false);
                }
            }

            MObject outputObject;
            DPData* dpData = nullptr;
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, dpData));
            if (!dpData)
                return MS::kFailure;

            dpData->setPosition(newPtArray);
            dpData->addAttrWithValues("pscale", radius);

            if (prePackeckGrp && newPtArray.size() == newGrpIds.size() &&
                !prePackGroupName.empty()) {
                dpData->removeGrp(prePackGroupName);
                dpData->addGrp(prePackGroupName, DPGroupData(newGrpIds));
            }

            dpData->updateBBox();

            if (dpData->numberOfPoints() != newPtArray.size())
                return MS::kFailure;
            MDataHandle outputData = data.outputValue(outDP, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishPluginDataObject(outputData, outputObject, *dpData));

            return data.setClean(plug);

        } break;
        case DT_PLANE_EMIT_PHYLLOTAXIS: {
#pragma mark -
#pragma mark case Phyllotaxis
            const MDataHandle aAngleTypeInputHandle = data.inputValue(aAngleType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const D_PHYLLOTAXIS_TYPE angleType = aAngleTypeInputHandle.asShort();
            const MDataHandle aSpreadInputHandle = data.inputValue(aSpread, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float spread = aSpreadInputHandle.asFloat();

            const MDataHandle aCustomAngleInputHandle = data.inputValue(aCustomAngle, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            float angle = aCustomAngleInputHandle.asFloat();
            switch (angleType) {
            case D_DF_ANGLE_A:
                angle = 137.3;
                break;
            case D_DF_ANGLE_B:
                angle = 137.5;
                break;
            case D_DF_ANGLE_C:
                angle = 137.6;
                break;
            case D_DF_ANGLE_CUSTOM:
                break;
            default:
                MGlobal::displayError("DPointPlaneEmit: unsupported phyllotaxis angle.");
                return MS::kFailure;
            }
            if (!std::isfinite(spread) || !std::isfinite(angle)) {
                MGlobal::displayError("DPointPlaneEmit: phyllotaxis values are not finite.");
                return MS::kFailure;
            }

            double c = spread;
            newPtArray.resize(countVal);

            for (size_t i = 0; i < countVal; ++i) {

                float a = angle * (float)i;
                float r = c * sqrt((float)i);

                float x = r * cos(a);
                float z = r * sin(a);

                newPtArray[i][0] = x;
                newPtArray[i][1] = 0.0f;
                newPtArray[i][2] = z;
            }

        } break;
        default:
            return MS::kFailure;
        }

        MObject outputObject;
        DPData* dpData = nullptr;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, dpData));
        if (!dpData)
            return MS::kFailure;

        dpData->setPosition(newPtArray);
        dpData->updateBBox();

        if (dpData->numberOfPoints() != newPtArray.size())
            return MS::kFailure;
        MDataHandle outputData = data.outputValue(outDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::publishPluginDataObject(outputData, outputObject, *dpData));

        return data.setClean(plug);

    } else {
        return MS::kUnknownParameter;
    }
}
DOMMY_COMPUTE_CATCH("DPointPlaneEmit")

MStatus DPointPlaneEmit::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    MFnEnumAttribute enumFn;
    emitType = enumFn.create("emitType", "emt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Grid", DT_PLANE_EMIT_GRID));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Tile", DT_PLANE_EMIT_TILE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Random", DT_PLANE_EMIT_RANDOM));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Blue Noise", DT_PLANE_EMIT_BLUENOISE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Sphere Packing", DT_PLANE_EMIT_SPHERE_PACKING));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Phyllotaxis", DT_PLANE_EMIT_PHYLLOTAXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(emitType));

    sizeX = nAttr.create("sizeX", "six", MFnNumericData::kFloat, 10.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sizeX));

    sizeY = nAttr.create("sizeY", "sizeY", MFnNumericData::kFloat, 10.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sizeY));

    gridX = nAttr.create("gridX", "grx", MFnNumericData::kInt, 5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(gridX));

    gridY = nAttr.create("gridY", "gry", MFnNumericData::kInt, 5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(gridY));

    count = nAttr.create("count", "c", MFnNumericData::kInt, 20, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(count));

    aTileStep = nAttr.create("TileStep", "tlstp", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f, 1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTileStep));

    aCenter = nAttr.create("Center", "cntr", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 0.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenter));

    MFnEnumAttribute rndEnumFn;
    aRandomType = rndEnumFn.create("RandomType", "rndty", D_DIST_UNIFORM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(rndEnumFn.addField("Uniform", D_DIST_UNIFORM));
    DOMMY_RETURN_IF_FAILURE(rndEnumFn.addField("Poisson", D_DIST_POISSON));
    DOMMY_RETURN_IF_FAILURE(rndEnumFn.addField("Lloyd's relaxation", D_DIST_LOYDS_RELAXATION));
    DOMMY_RETURN_IF_FAILURE(rndEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(rndEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomType));

    seed = nAttr.create("seed", "se", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));

    aPoissonMinDistance =
        nAttr.create("PoissonMinDistance", "pmindist", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPoissonMinDistance));

    aRelaxIteration = nAttr.create("RelaxIteration", "rlxiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRelaxIteration));

#pragma mark sphere packing
    aRadiusRange = nAttr.create("RadiusRange", "radrng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0001, 0.0001));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRange));

    aPackingGradientBiasRamp =
        MRampAttribute::createCurveRamp("PackingGradientBiasRamp", "pkgrdrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingGradientBiasRamp));

    MFnEnumAttribute packEnumFn;
    aPackingType = packEnumFn.create("PackingType", "pkty", D_PACK_RANDOM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("Random", D_PACK_RANDOM));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("Gradation", D_PACK_GRADATION));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("From Map", D_PACK_MAP));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingType));

    aPackingSampling = nAttr.create("PackingSampling", "pksmp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(64));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(512));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingSampling));

    aUseRadiusBias = nAttr.create("UseRadiusBias", "usradbs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseRadiusBias));

    aRadiusBiasRamp = MRampAttribute::createCurveRamp("RadiusBiasRamp", "radbsrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusBiasRamp));

    aBiasRange = nAttr.create("BiasRange", "bsrng", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBiasRange));

    inObject = gAttr.create("inObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    aMap = nAttr.createColor("Map", "map", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMap));

#pragma mark pre packing point

    aPrePackingDP = tAttr.create("PrePackingDP", "ppdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPrePackingDP));

    const MObject aRadiusAttributeDefault = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "radattr", MFnData::kString,
                                    aRadiusAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));

    aBaseRadius = nAttr.create("BaseRadius", "brad", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseRadius));

    const MObject aPreDPGroupNameDefault = fnStringData.create("pre_pack_grp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPreDPGroupName =
        tAttr.create("PreDPGroupName", "pdpgrp", MFnData::kString, aPreDPGroupNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPreDPGroupName));

    aSpread = nAttr.create("Spread", "spd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpread));

    MFnEnumAttribute angleEnumFn;
    aAngleType = angleEnumFn.create("AngleType", "aglty", D_DF_ANGLE_A, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.addField("A 137.3", D_DF_ANGLE_A));
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.addField("B 137.5", D_DF_ANGLE_B));
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.addField("C 137.6", D_DF_ANGLE_C));
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.addField("Custom", D_DF_ANGLE_CUSTOM));
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(angleEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngleType));

    aCustomAngle = nAttr.create("CustomAngle", "ctmagl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(137.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCustomAngle));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(emitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sizeX, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sizeY, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(gridX, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(gridY, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(count, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTileStep, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCenter, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPoissonMinDistance, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRelaxIteration, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingGradientBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseRadiusBias, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBiasRange, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMap, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrePackingDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPreDPGroupName, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpread, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAngleType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCustomAngle, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPointPlaneEmit", "initialize",
                                                  std::current_exception());
}

//
//  DPointVolumeEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointVolumeEmit.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DBatchRange.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaRampSampling.h"
#include "DPointPackingCommon.h"

#include "DMayaHeaders.h"

#include "dpoint_ids.h"
#include "DPData.h"
#include "DPointDataAccess.h"
#include "DPGroupData.h"
#include "DPAttributeAccess.h"
#include "DPointSeExpr.h"
#include "DOpenVDBUtil.h"

#include "DRandomGenerator.h"

#include <openvdb/tools/PointScatter.h>
#include <openvdb/util/NullInterrupter.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "../../../modules/volume/VDB/Edit/DOpenVDBScatterPolicy.h"

#include "DVdbOp.h"
#include "../../../modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "DGlobalTex.h"

namespace {

const float kMaxSafeCoordinate = std::sqrt(std::numeric_limits<float>::max());

bool isFinite(const DPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool isFinite(const PackedCircle3D& circle) {
    return isFinite(circle.m_pt) && std::isfinite(circle.m_radius) && circle.m_radius >= 0.0f;
}

MStatus readVolumeMeshScatter(MDataBlock& data, std::size_t count, int seed,
                               dommy::vdb::ScatterSettings& settings) {
    MStatus stat;
    settings.count = static_cast<openvdb::Index64>(count);
    settings.seed = seed;
    const MDataHandle aModeInputHandle = data.inputValue(DPointVolumeEmit::aMode, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int pmode = aModeInputHandle.asInt();
    if (pmode < 0 || pmode > 2) return MS::kInvalidParameter;
    settings.mode = pmode;
    const MDataHandle aSpreadInputHandle = data.inputValue(DPointVolumeEmit::aSpread, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float spread = aSpreadInputHandle.asFloat(); // Spread

    const MDataHandle aInteriorInputHandle = data.inputValue(DPointVolumeEmit::aInterior, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool interior =
        aInteriorInputHandle.asBool(); // Toggle to scatter inside level sets

    settings.spread = spread; settings.interior = interior;
    if (!std::isfinite(spread) || spread < 0 || spread > 1) return MS::kInvalidParameter;
    if (pmode == 1) {
        const MDataHandle aDensityInputHandle = data.inputValue(DPointVolumeEmit::aDensity, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float density = aDensityInputHandle.asFloat();
        const MDataHandle aIsLocalInputHandle = data.inputValue(DPointVolumeEmit::aIsLocal, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isLocal = aIsLocalInputHandle.asBool();
        if (!std::isfinite(density) || density < 0) return MS::kInvalidParameter;
        settings.density = density; settings.local = isLocal;
    } else if (pmode == 2) {
        const MDataHandle aPerVoxelInputHandle = data.inputValue(DPointVolumeEmit::aPerVoxel, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float ptsPerVox = aPerVoxelInputHandle.asFloat();
        if (!std::isfinite(ptsPerVox) || ptsPerVox < 0) return MS::kInvalidParameter;
        settings.perVoxel = ptsPerVox;
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DPointVolumeEmit::typeId(ID_DP_VOLUME_EMIT);

MObject DPointVolumeEmit::inputGeometry;
MObject DPointVolumeEmit::emitType;
MObject DPointVolumeEmit::aCount;
MObject DPointVolumeEmit::radius;
MObject DPointVolumeEmit::volumeSize;
MObject DPointVolumeEmit::aBoxGrid;

MObject DPointVolumeEmit::nSeed;
MObject DPointVolumeEmit::aSpread;
MObject DPointVolumeEmit::aMode;
MObject DPointVolumeEmit::aDensity;
MObject DPointVolumeEmit::aIsLocal;
MObject DPointVolumeEmit::aPerVoxel;
MObject DPointVolumeEmit::aInterior;

MObject DPointVolumeEmit::aVoxelType;
MObject DPointVolumeEmit::aVoxelSize;
MObject DPointVolumeEmit::aExteriorBandWidth;
MObject DPointVolumeEmit::aInteriorBandWidth;
MObject DPointVolumeEmit::aFillInterior;
MObject DPointVolumeEmit::aUnsignedDistanceField;

// Sphere Packing
MObject DPointVolumeEmit::aRadiusRange;
MObject DPointVolumeEmit::aPackingType;
MObject DPointVolumeEmit::aPackingAxisType;
MObject DPointVolumeEmit::aPackingSampling;
MObject DPointVolumeEmit::aPackingGradientBiasRamp;
MObject DPointVolumeEmit::aUseRadiusBias;
MObject DPointVolumeEmit::aRadiusBiasRamp;
MObject DPointVolumeEmit::aBiasRange;

// for Map
MObject DPointVolumeEmit::inPackingUVObject;
MObject DPointVolumeEmit::aMap;
MObject DPointVolumeEmit::aIs3DMap;
MObject DPointVolumeEmit::aExpression;
// pre Packing DPoint
MObject DPointVolumeEmit::aPrePackingDP;
MObject DPointVolumeEmit::aRadiusAttribute;
MObject DPointVolumeEmit::aBaseRadius;
MObject DPointVolumeEmit::aPreDPGroupName;

MObject DPointVolumeEmit::outDP;

void* DPointVolumeEmit::creator() {
    return ::dommy::maya::createObject("DPointVolumeEmit", [&]() {
        return new DPointVolumeEmit();
    });
}

#pragma mark -
#pragma mark SpherePacking Methods

bool DPointVolumeEmit::getMapValues(
    const std::vector<DPoint>& points, std::vector<float>& values) {
    if (points.empty() ||
        points.size() >
            static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return false;
    }

    MStatus status;
    std::vector<float> sampledValues(points.size(), 0.0f);

    if (m_is3d_calc) {
        const unsigned int pointCount =
            static_cast<unsigned int>(points.size());
        MDoubleArray uCoords(pointCount, 0.0);
        MDoubleArray vCoords(pointCount, 0.0);
        MFloatPointArray mayaPoints;
        status = mayaPoints.setLength(pointCount);
        if (!status) return false;
        for (unsigned int index = 0U; index < pointCount; ++index) {
            if (!isFinite(points[index])) {
                return false;
            }
            status = mayaPoints.set(
                MFloatPoint(points[index].x, points[index].y,
                            points[index].z),
                index);
            if (!status) return false;
        }

        MFloatVectorArray colors, alphas;
        status = getColorFromShaderNetwork(
            thisMObject(), aMap, mayaPoints, uCoords, vCoords, colors,
            alphas);
        if (status != MS::kSuccess || alphas.length() != pointCount) {
            return false;
        }
        for (unsigned int index = 0U; index < pointCount; ++index) {
            if (!dommy::packing::clampUnit(
                    alphas[index].x, sampledValues[index])) {
                return false;
            }
        }
        values.swap(sampledValues);
        return true;
    }

    MDoubleArray uCoords, vCoords;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (!isFinite(points[index])) {
            return false;
        }

        const MPoint point = DPToMP(points[index]);
        switch (m_obj_hnd.type()) {
        case MFnData::kNurbsSurface: {
            double uValue = 0.0;
            double vValue = 0.0;
            status = srfFn.getParamAtPoint(
                point, uValue, vValue, MSpace::kWorld);
            if (status != MS::kSuccess || !std::isfinite(uValue) ||
                !std::isfinite(vValue) || !uCoords.append(uValue) ||
                !vCoords.append(vValue)) {
                return false;
            }
            break;
        }
        case MFnData::kMesh: {
            float2 uv = {0.0f, 0.0f};
            MPoint queryPoint(point);
            status = meshFn.getUVAtPoint(
                queryPoint, uv, MSpace::kWorld);
            if (status != MS::kSuccess || !std::isfinite(uv[0]) ||
                !std::isfinite(uv[1]) || !uCoords.append(uv[0]) ||
                !vCoords.append(uv[1])) {
                return false;
            }
            break;
        }
        default:
            return false;
        }
    }

    MVectorArray colors;
    MDoubleArray alphas;
    status = evaluateDynamics2dTextureInBatches(
        thisMObject(), aMap, uCoords, vCoords, colors, alphas);
    if (status != MS::kSuccess ||
        static_cast<std::size_t>(alphas.length()) != points.size()) {
        return false;
    }
    for (unsigned int index = 0U; index < alphas.length(); ++index) {
        if (!dommy::packing::clampUnit(
                static_cast<float>(alphas[index]), sampledValues[index])) {
            return false;
        }
    }
    values.swap(sampledValues);
    return true;
}

bool DPointVolumeEmit::getMitchellBestCandidate(const MTime& evaluationTime,
                                                PackedCircle3D& result) {
    float greatestDistance = -std::numeric_limits<float>::infinity();
    PackedCircle3D winningSample(DPoint(0.0f, 0.0f, 0.0f), 0.0f);
    DPointSeExpr expression(m_expresion_str);

    if (m_pack_type == D_PACK_3D_EXPRESSION) {
        if (!expression.isValid() || expression.usesPointDataFunctions()) {
            return false;
        }
        if (!dommy::maya::setExpressionTime(expression, evaluationTime)) {
            return false;
        }
    }

    if (m_pack_type == D_PACK_3D_MAP) {
        const std::size_t candidateCount =
            placedPoints.empty() ? 1U : m_sampling;
        for (std::size_t begin = 0U; begin < candidateCount;) {
            const dommy::batch::Range range = dommy::batch::rangeAt(
                candidateCount,
                dommy::batch::kMaximumShadingSampleCount, begin);
            if (range.empty()) {
                return false;
            }

            std::vector<DPoint> positions;
            positions.reserve(range.count);
            for (std::size_t index = 0U; index < range.count; ++index) {
                positions.push_back(DPoint(
                    m_distribx(m_eng), m_distriby(m_eng),
                    m_distribz(m_eng)));
            }

            std::vector<float> mapValues;
            if (!getMapValues(positions, mapValues) ||
                mapValues.size() != positions.size()) {
                return false;
            }

            for (std::size_t index = 0U; index < positions.size(); ++index) {
                const PackedCircle3D candidate(
                    positions[index], m_min_rad + m_rad_size * mapValues[index]);
                if (!isFinite(candidate)) {
                    return false;
                }
                if (placedPoints.empty()) {
                    result = candidate;
                    return true;
                }

                const float currentDistance =
                    getDistanceToNearestSample(candidate);
                if (!std::isfinite(currentDistance)) {
                    return false;
                }
                if (currentDistance > greatestDistance) {
                    greatestDistance = currentDistance;
                    winningSample = candidate;
                }
            }
            begin = range.end();
        }

        result = winningSample;
        return isFinite(result);
    }

    for (std::size_t i = 0; i < m_sampling; ++i) {
        DPoint position(m_distribx(m_eng), m_distriby(m_eng), m_distribz(m_eng));
        float generatedRadius = 0.0f;

        switch (m_pack_type) {
        case D_PACK_3D_RANDOM: {
            if (m_use_rad_bias) {
                if (m_max_count == 0) {
                    return false;
                }
                const float placedCount =
                    static_cast<float>(placedPoints.empty() ? 1U : placedPoints.size());
                float biasPosition = 1.0f - placedCount / static_cast<float>(m_max_count);
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
                generatedRadius = dommy::packing::uniform(
                    m_eng, m_min_rad + maximumOffset * m_bias_range, m_min_rad + maximumOffset);
            } else {
                generatedRadius = dommy::packing::uniform(m_eng, m_min_rad, m_max_rad);
            }
            break;
        }
        case D_PACK_3D_GRADATION: {
            float axisValue = 0.0f;
            float axisMinimum = 0.0f;
            float axisExtent = 0.0f;
            switch (m_pack_axis_type) {
            case D_X_AXIS:
                axisValue = position.x;
                axisMinimum = m_grid_minX;
                axisExtent = m_grid_sizeX;
                break;
            case D_Y_AXIS:
                axisValue = position.y;
                axisMinimum = m_grid_minY;
                axisExtent = m_grid_sizeY;
                break;
            case D_Z_AXIS:
                axisValue = position.z;
                axisMinimum = m_grid_minZ;
                axisExtent = m_grid_sizeZ;
                break;
            default:
                return false;
            }
            float rampPosition = 0.0f;
            float rampValue = 0.0f;
            if (!dommy::packing::normalizedPosition(axisValue, axisMinimum, axisExtent,
                                                    rampPosition)) {
                return false;
            }
            if (!dommy::maya::sampleFiniteRamp(
                    m_packGradientRamp, rampPosition, rampValue)) {
                return false;
            }
            if (!dommy::packing::clampUnit(rampValue, rampValue)) {
                return false;
            }
            generatedRadius = m_min_rad + m_rad_size * rampValue;
            break;
        }
        case D_PACK_3D_EXPRESSION: {
            // All candidates compete for the same next output point.
            expression.setId(placedPoints.size());
            expression.setPos(position);
            SeVec3d expressionValue;
            if (!dommy::expression::evaluateChecked(expression, expressionValue)) {
                return false;
            }
            // Clamp in double before narrowing to the bounded float radius factor.
            const float scalarValue =
                static_cast<float>(std::max(0.0, std::min(1.0, expressionValue[0])));
            generatedRadius = m_min_rad + m_rad_size * scalarValue;
            break;
        }
        default:
            return false;
        }

        const PackedCircle3D candidate(position, generatedRadius);
        if (!isFinite(candidate)) {
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
    return isFinite(result);
}

float DPointVolumeEmit::getDistanceToNearestSample(const PackedCircle3D& candidate) const {
    float shortestDistance = std::numeric_limits<float>::infinity();
    for (const PackedCircle3D& otherSample : placedPoints) {
        const float currentDistance = (candidate.m_pt - otherSample.m_pt).length() -
                                      (otherSample.m_radius + candidate.m_radius);
        shortestDistance = std::min(shortestDistance, currentDistance);
    }
    return shortestDistance;
}

bool DPointVolumeEmit::placeNewSample(const MTime& evaluationTime) {
    PackedCircle3D sample(DPoint(0.0f, 0.0f, 0.0f), 0.0f);
    if (!getMitchellBestCandidate(evaluationTime, sample)) {
        return false;
    }
    placedPoints.push_back(sample);
    return true;
}

#pragma mark -
#pragma mark compute method

MStatus DPointVolumeEmit::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {

        MTime evaluationTime;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, evaluationTime));

        // アトリビュート値の取得
        const MDataHandle emitTypeInputHandle = data.inputValue(emitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const D_VOLUME_EMIT_TYPE emitTypeVal = emitTypeInputHandle.asInt();
        const MDataHandle aCountInputHandle = data.inputValue(aCount, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const openvdb::Index64 rawCount = aCountInputHandle.asInt64();
        const MDataHandle radiusInputHandle = data.inputValue(radius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float radiusVal = radiusInputHandle.asFloat();

        MDataHandle volumeSizeHandle = data.inputValue(volumeSize, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const float3& rawVolumeSize = volumeSizeHandle.asFloat3();
        const std::array<float, 3> volumeSizeVal = {
            {rawVolumeSize[0], rawVolumeSize[1], rawVolumeSize[2]}};

        MDataHandle densityHandle = data.inputValue(aBoxGrid, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const int3& rawDensity = densityHandle.asInt3();
        const std::array<int, 3> densityVal = {{rawDensity[0], rawDensity[1], rawDensity[2]}};

        const MDataHandle nSeedInputHandle = data.inputValue(nSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int seedVal = nSeedInputHandle.asInt();

        if (emitTypeVal < 0 || emitTypeVal >= D_VOLUME_EMIT_TYPE_COUNT || rawCount < 0 ||
            rawCount > static_cast<openvdb::Index64>(dommy::generation::kMaxGeneratedPointCount) ||
            seedVal < 0 || !std::isfinite(radiusVal) || radiusVal < 0.0f ||
            radiusVal > kMaxSafeCoordinate || !std::isfinite(volumeSizeVal[0]) ||
            volumeSizeVal[0] < 0.0f || volumeSizeVal[0] > kMaxSafeCoordinate ||
            !std::isfinite(volumeSizeVal[1]) || volumeSizeVal[1] < 0.0f ||
            volumeSizeVal[1] > kMaxSafeCoordinate || !std::isfinite(volumeSizeVal[2]) ||
            volumeSizeVal[2] < 0.0f || volumeSizeVal[2] > kMaxSafeCoordinate) {
            MGlobal::displayError("DPointVolumeEmit received invalid generation parameters.");
            return MS::kInvalidParameter;
        }
        const std::size_t count = static_cast<std::size_t>(rawCount);

        // emit Dpoint array
        DPointList newPtArray;

        switch (emitTypeVal) {
#pragma mark Sphere
        case D_VOLUME_EMIT_SPHERE: {
            newPtArray.reserve(count);
            std::mt19937 engine(static_cast<std::mt19937::result_type>(seedVal));
            std::normal_distribution<float> distribution(0.0f, 1.0f);
            std::size_t maximumAttempts = 0;
            if (!dommy::generation::checkedProduct(count, 2U, dommy::generation::kMaxWorkItemCount,
                                                   maximumAttempts)) {
                return MS::kInvalidParameter;
            }

            for (std::size_t attempt = 0; attempt < maximumAttempts && newPtArray.size() < count;
                 ++attempt) {
                const float x = distribution(engine);
                const float y = distribution(engine);
                const float z = distribution(engine);
                const float squaredLength = x * x + y * y + z * z;
                if (!std::isfinite(squaredLength) ||
                    squaredLength <= std::numeric_limits<float>::epsilon()) {
                    continue;
                }
                const float scale = radiusVal / std::sqrt(squaredLength);
                const DPoint point(x * scale, y * scale, z * scale);
                if (!isFinite(point)) {
                    return MS::kFailure;
                }
                newPtArray.push_back(point);
            }
            if (newPtArray.size() != count) {
                return MS::kFailure;
            }
        } break;
#pragma mark -
#pragma mark Sphere Fill
        case D_VOLUME_EMIT_SPHERE_FILL: {
            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
            newPtArray.reserve(count);
            std::size_t maximumAttempts = 0;
            if (!dommy::generation::checkedProduct(count, 8U, dommy::generation::kMaxWorkItemCount,
                                                   maximumAttempts)) {
                return MS::kInvalidParameter;
            }
            const float squaredRadius = radiusVal * radiusVal;
            for (std::size_t attempt = 0; attempt < maximumAttempts && newPtArray.size() < count;
                 ++attempt) {
                const float x = randomGenerator.uniform(-radiusVal, radiusVal);
                const float y = randomGenerator.uniform(-radiusVal, radiusVal);
                const float z = randomGenerator.uniform(-radiusVal, radiusVal);
                if (x * x + y * y + z * z <= squaredRadius) {
                    newPtArray.push_back(DPoint(x, y, z));
                }
            }
            if (newPtArray.size() != count) {
                return MS::kFailure;
            }
        } break;
#pragma mark -
#pragma mark Box
        case D_VOLUME_EMIT_BOX: {
            std::size_t densityX = 0;
            std::size_t densityY = 0;
            std::size_t densityZ = 0;
            if (!dommy::generation::checkedCount(
                    densityVal[0], dommy::generation::kMaxGeneratedPointCount, densityX) ||
                densityX == 0 ||
                !dommy::generation::checkedCount(
                    densityVal[1], dommy::generation::kMaxGeneratedPointCount, densityY) ||
                densityY == 0 ||
                !dommy::generation::checkedCount(
                    densityVal[2], dommy::generation::kMaxGeneratedPointCount, densityZ) ||
                densityZ == 0) {
                return MS::kInvalidParameter;
            }
            std::size_t densityXY = 0;
            std::size_t gridPointCount = 0;
            if (!dommy::generation::checkedProduct(
                    densityX, densityY, dommy::generation::kMaxGeneratedPointCount, densityXY) ||
                !dommy::generation::checkedProduct(densityXY, densityZ,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   gridPointCount)) {
                return MS::kInvalidParameter;
            }

            const float stepX =
                densityX > 1 ? volumeSizeVal[0] / static_cast<float>(densityX - 1) : 0.0f;
            const float stepY =
                densityY > 1 ? volumeSizeVal[1] / static_cast<float>(densityY - 1) : 0.0f;
            const float stepZ =
                densityZ > 1 ? volumeSizeVal[2] / static_cast<float>(densityZ - 1) : 0.0f;

            float halfX = volumeSizeVal[0] * .5;
            float halfY = volumeSizeVal[1] * .5;
            float halfZ = volumeSizeVal[2] * .5;

            newPtArray.reserve(gridPointCount);
            for (std::size_t x = 0; x < densityX; ++x) {
                const float valueX = densityX == 1 ? 0.0f : -halfX + stepX * static_cast<float>(x);
                for (std::size_t y = 0; y < densityY; ++y) {
                    const float valueY =
                        densityY == 1 ? 0.0f : -halfY + stepY * static_cast<float>(y);
                    for (std::size_t z = 0; z < densityZ; ++z) {
                        const float valueZ =
                            densityZ == 1 ? 0.0f : -halfZ + stepZ * static_cast<float>(z);
                        const DPoint point(valueX, valueY, valueZ);
                        if (!isFinite(point)) {
                            return MS::kFailure;
                        }
                        newPtArray.push_back(point);
                    }
                }
            }

        } break;
#pragma mark -
#pragma mark Box Fill
        case D_VOLUME_EMIT_BOX_FILL: {
            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
            newPtArray.reserve(count);

            float halfSizeX = volumeSizeVal[0] * .5;
            float halfSizeY = volumeSizeVal[1] * .5;
            float halfSizeZ = volumeSizeVal[2] * .5;

            for (std::size_t i = 0; i < count; ++i) {

                const DPoint point(randomGenerator.uniform(-halfSizeX, halfSizeX),
                                   randomGenerator.uniform(-halfSizeY, halfSizeY),
                                   randomGenerator.uniform(-halfSizeZ, halfSizeZ));
                if (!isFinite(point)) {
                    return MS::kFailure;
                }
                newPtArray.push_back(point);
            }
        } break;
#pragma mark -
#pragma mark Mesh Fill
        case D_VOLUME_EMIT_MESH: {
            if (count == 0) break; // Historical explicit empty output.
            dommy::vdb::ScatterSettings scatterSettings;
            DOMMY_RETURN_IF_FAILURE(readVolumeMeshScatter(data, count, seedVal, scatterSettings));
            MPlug meshPlug(thisMObject(), inputGeometry);
            const bool connected = meshPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (!connected) return MS::kInvalidParameter;
            const MDataHandle aVoxelTypeInputHandle = data.inputValue(aVoxelType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const short voxeltype = aVoxelTypeInputHandle.asShort();
            const MDataHandle aVoxelSizeInputHandle = data.inputValue(aVoxelSize, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float voxelSize = aVoxelSizeInputHandle.asFloat();
            const MDataHandle aExteriorBandWidthInputHandle = data.inputValue(aExteriorBandWidth, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float exteriorBandWidth = aExteriorBandWidthInputHandle.asFloat();
            const MDataHandle aInteriorBandWidthInputHandle = data.inputValue(aInteriorBandWidth, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float interiorBandWidth = aInteriorBandWidthInputHandle.asFloat();
            const MDataHandle aFillInteriorInputHandle = data.inputValue(aFillInterior, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool fillInterior = aFillInteriorInputHandle.asBool();
            const MDataHandle aUnsignedDistanceFieldInputHandle = data.inputValue(aUnsignedDistanceField, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool isUnsigned = aUnsignedDistanceFieldInputHandle.asBool();

            if (voxeltype < 0 || voxeltype > 1 || !std::isfinite(voxelSize) ||
                voxelSize <= 0.0f || !std::isfinite(exteriorBandWidth) ||
                exteriorBandWidth <= 0.0f || !std::isfinite(interiorBandWidth) ||
                interiorBandWidth <= 0.0f) return MS::kInvalidParameter;

            MDataHandle meshHandle = data.inputValue(inputGeometry, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MObject raw = meshHandle.asMesh();
            const MObject object = meshHandle.asMeshTransformed();
            if (raw.isNull() || object.isNull()) return MS::kInvalidParameter;
            dommy::vdb::TreeEditBudget budget;
            openvdb::FloatGrid::Ptr grid;
            if (!GenerateVdbMeshOp(voxeltype, voxelSize, object, exteriorBandWidth,
                    interiorBandWidth, isUnsigned, fillInterior, grid, &budget) || !grid || grid->empty())
                return MS::kFailure;
            dommy::vdb::ScatterPlan plan;
            DOMMY_RETURN_IF_FAILURE(dommy::vdb::planScatter(grid, scatterSettings, budget, plan));
            DOMMY_RETURN_IF_FAILURE(dommy::vdb::scatterPoints({plan}, scatterSettings, newPtArray));
        } break;
        case D_VOLUME_EMIT_SPHERE_PACKING: {
#pragma mark -
#pragma mark case Sphere packing
            placedPoints.clear();

            const MDataHandle aPackingTypeInputHandle = data.inputValue(aPackingType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_pack_type = aPackingTypeInputHandle.asShort();
            const MDataHandle aPackingAxisTypeInputHandle =
                data.inputValue(aPackingAxisType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_pack_axis_type = aPackingAxisTypeInputHandle.asShort();
            const MDataHandle aIs3DMapInputHandle = data.inputValue(aIs3DMap, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_is3d_calc = aIs3DMapInputHandle.asBool();
            if (m_pack_type < 0 || m_pack_type >= D_PACKING_3D_TYPE_COUNT ||
                m_pack_axis_type < D_X_AXIS || m_pack_axis_type > D_Z_AXIS) {
                return MS::kInvalidParameter;
            }

            if (m_pack_type == D_PACK_3D_MAP) {
                if (!m_is3d_calc) {
                    m_obj_hnd = data.inputValue(inPackingUVObject, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    if (m_obj_hnd.type() == MFnData::kNurbsSurface) {
                        DOMMY_RETURN_IF_FAILURE(srfFn.setObject(m_obj_hnd.data()));
                    } else if (m_obj_hnd.type() == MFnData::kMesh) {
                        DOMMY_RETURN_IF_FAILURE(meshFn.setObject(m_obj_hnd.data()));
                    } else {
                        EchoWarning(name().asChar(),
                                    "A mesh or NURBS surface is required for a 2D map.");
                        return MS::kInvalidParameter;
                    }
                }
                MPlug mapPlug(thisMObject(), aMap);
                const bool mapConnected = mapPlug.isConnected(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!mapConnected) {
                    EchoWarning(name().asChar(), "Map is not connected.");
                    return MS::kInvalidParameter;
                }
            } else if (m_pack_type == D_PACK_3D_EXPRESSION) {
                const MDataHandle aExpressionInputHandle = data.inputValue(aExpression, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                m_expresion_str = aExpressionInputHandle.asString().asChar();
                DPointSeExpr expression(m_expresion_str);
                if (!expression.isValid() || expression.usesPointDataFunctions()) {
                    EchoWarning(name().asChar(),
                                "Expression is invalid or uses unsupported point-data functions.");
                    return MS::kInvalidParameter;
                }
            }

            const MDataHandle aPackingSamplingInputHandle =
                data.inputValue(aPackingSampling, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int rawSampling = aPackingSamplingInputHandle.asInt();
            if (!dommy::generation::checkedCount(rawSampling, dommy::generation::kMaxWorkItemCount,
                                                 m_sampling) ||
                m_sampling == 0) {
                return MS::kInvalidParameter;
            }

            MPlug rampPlug(thisMObject(), aPackingGradientBiasRamp);
            if (m_pack_type == D_PACK_3D_GRADATION) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::bindValueRamp(rampPlug, m_packGradientRamp));
            }
            const MDataHandle aUseRadiusBiasInputHandle = data.inputValue(aUseRadiusBias, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_use_rad_bias = aUseRadiusBiasInputHandle.asBool();
            MPlug radiusRampPlug(thisMObject(), aRadiusBiasRamp);
            if (m_pack_type == D_PACK_3D_RANDOM && m_use_rad_bias) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::bindValueRamp(radiusRampPlug, m_radiusBiasRamp));
            }

            MDataHandle radiusRangeHandle = data.inputValue(aRadiusRange, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const float2& radiusRange = radiusRangeHandle.asFloat2();
            m_min_rad = radiusRange[0];
            m_max_rad = radiusRange[1];
            if (!std::isfinite(m_min_rad) || !std::isfinite(m_max_rad) || m_min_rad < 0.0f ||
                m_max_rad < 0.0f || m_min_rad > kMaxSafeCoordinate ||
                m_max_rad > kMaxSafeCoordinate) {
                return MS::kInvalidParameter;
            }
            if (m_max_rad < m_min_rad) {
                std::swap(m_min_rad, m_max_rad);
            }
            m_rad_size = m_max_rad - m_min_rad;
            const MDataHandle aBiasRangeInputHandle = data.inputValue(aBiasRange, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_bias_range = aBiasRangeInputHandle.asFloat();
            if (!std::isfinite(m_bias_range) || m_bias_range < 0.0f || m_bias_range > 1.0f) {
                return MS::kInvalidParameter;
            }

            m_max_count = count;
            m_eng.seed(static_cast<std::mt19937::result_type>(seedVal));

            const float offsetX = volumeSizeVal[0] * 0.5f;
            const float offsetY = volumeSizeVal[1] * 0.5f;
            const float offsetZ = volumeSizeVal[2] * 0.5f;
            m_grid_minX = -offsetX;
            m_grid_maxX = offsetX;
            m_grid_minY = -offsetY;
            m_grid_maxY = offsetY;
            m_grid_minZ = -offsetZ;
            m_grid_maxZ = offsetZ;
            m_grid_sizeX = volumeSizeVal[0];
            m_grid_sizeY = volumeSizeVal[1];
            m_grid_sizeZ = volumeSizeVal[2];
            m_distribx = std::uniform_real_distribution<float>(-offsetX, offsetX);
            m_distriby = std::uniform_real_distribution<float>(-offsetY, offsetY);
            m_distribz = std::uniform_real_distribution<float>(-offsetZ, offsetZ);

#pragma mark check pre packing points
            MPlug prePackDPPlug(thisMObject(), aPrePackingDP);
            const MDataHandle aPreDPGroupNameInputHandle = data.inputValue(aPreDPGroupName, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const std::string prePackGroupName = aPreDPGroupNameInputHandle.asString().asChar();
            bool hasPrePackingGroup = false;
            DPIdList newGroupIds;
            const bool prePackingConnected = prePackDPPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (prePackingConnected) {
                const DPData* preDPData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDP(aPrePackingDP, data, preDPData));
                if (!preDPData) return MS::kFailure;

                const std::size_t prePointCount =
                    static_cast<std::size_t>(preDPData->numberOfPoints());
                if (prePointCount > dommy::generation::kMaxGeneratedPointCount) {
                    return MS::kInvalidParameter;
                }
                const MDataHandle aBaseRadiusInputHandle = data.inputValue(aBaseRadius, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const float baseScale = aBaseRadiusInputHandle.asFloat();
                const MDataHandle aRadiusAttributeInputHandle =
                    data.inputValue(aRadiusAttribute, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const std::string radiusAttributeName =
                    aRadiusAttributeInputHandle.asString().asChar();
                if (!std::isfinite(baseScale) || baseScale < 0.0f) {
                    return MS::kInvalidParameter;
                }

                DScalarDataList preScaleList(prePointCount, baseScale);
                if (preDPData->isExistAttr(radiusAttributeName)) {
                    getDScalarArrayFromAttribute(*preDPData, radiusAttributeName, preScaleList);
                }
                if (preScaleList.size() != prePointCount) {
                    return MS::kFailure;
                }

                placedPoints.reserve(std::max(count, prePointCount));
                std::size_t index = 0;
                for (auto pointIterator = preDPData->beginPt(), endIterator = preDPData->endPt();
                     pointIterator != endIterator; ++pointIterator, ++index) {
                    const float sourceRadius = preScaleList[index];
                    if (!std::isfinite(sourceRadius) || sourceRadius < 0.0f) {
                        return MS::kInvalidParameter;
                    }
                    const PackedCircle3D packedPoint(*pointIterator, sourceRadius);
                    if (!isFinite(packedPoint)) {
                        return MS::kInvalidParameter;
                    }
                    placedPoints.push_back(packedPoint);
                }

                if (!placedPoints.empty()) {
                    hasPrePackingGroup = true;
                    newGroupIds.assign(placedPoints.size(), true);
                }
            } else {
                placedPoints.reserve(count);
            }

#pragma mark generate packing point
            const std::size_t pointsToGenerate =
                placedPoints.size() < count ? count - placedPoints.size() : 0U;
            std::size_t distanceChecks = 0;
            std::size_t workItems = 0;
            if (!dommy::generation::checkedProduct(pointsToGenerate, count,
                                                   dommy::generation::kMaxWorkItemCount,
                                                   distanceChecks) ||
                !dommy::generation::checkedProduct(
                    distanceChecks, m_sampling, dommy::generation::kMaxWorkItemCount, workItems)) {
                return MS::kInvalidParameter;
            }
            (void)workItems;
            while (placedPoints.size() < count) {
                if (!placeNewSample(evaluationTime)) {
                    return MS::kFailure;
                }
            }

            DScalarDataList radii;
            newPtArray.reserve(placedPoints.size());
            radii.reserve(placedPoints.size());
            for (const PackedCircle3D& packedPoint : placedPoints) {
                if (!isFinite(packedPoint)) {
                    return MS::kFailure;
                }
                newPtArray.push_back(packedPoint.m_pt);
                radii.push_back(packedPoint.m_radius);
            }
            if (hasPrePackingGroup) {
                newGroupIds.resize(newPtArray.size(), false);
            }

            MObject outputObject;
            DPData* dpData = nullptr;
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, dpData));
            if (!dpData)
                return MS::kFailure;

            dpData->setPosition(newPtArray);
            dpData->addAttrWithValues("pscale", radii);
            if (hasPrePackingGroup && newPtArray.size() == newGroupIds.size() &&
                !prePackGroupName.empty()) {
                dpData->removeGrp(prePackGroupName);
                dpData->addGrp(prePackGroupName, DPGroupData(newGroupIds));
            }
            dpData->updateBBox();

            if (dpData->numberOfPoints() != newPtArray.size())
                return MS::kFailure;
            MDataHandle outputData = data.outputValue(outDP, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishPluginDataObject(outputData, outputObject, *dpData));
            return data.setClean(plug);
        }
        default:
            return MS::kInvalidParameter;
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
    return MS::kSuccess;
}
DOMMY_VDB_COMPUTE_CATCH("DPointVolumeEmit")

MStatus DPointVolumeEmit::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inputGeometry = tAttr.create("inputGeometry", "igo", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputGeometry));

    MFnEnumAttribute enumFn;
    emitType = enumFn.create("emitType", "emt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Sphere", D_VOLUME_EMIT_SPHERE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Sphere Fill", D_VOLUME_EMIT_SPHERE_FILL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Box", D_VOLUME_EMIT_BOX));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Box Fill", D_VOLUME_EMIT_BOX_FILL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("In Mesh", D_VOLUME_EMIT_MESH));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Sphere Packing", D_VOLUME_EMIT_SPHERE_PACKING));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(emitType));

    aCount = nAttr.create("Count", "cnt", MFnNumericData::kInt64, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

    radius = nAttr.create("radius", "ra", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(radius));

    volumeSize = nAttr.create("volumeSize", "vs", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0, 5.0, 5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(volumeSize));

    aBoxGrid = nAttr.create("BoxGrid", "bgrd", MFnNumericData::k3Int, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(8, 8, 8));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBoxGrid));

    nSeed = nAttr.create("nSeed", "nse", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(nSeed));

    aSpread = nAttr.create("Spread", "spr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpread));

    MFnEnumAttribute eAttr;
    aMode = eAttr.create("Mode", "mo", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("count", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("density", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("points per voxel", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));

    aDensity = nAttr.create("Density", "dens", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDensity));

    aIsLocal = nAttr.create("IsLocal", "isl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsLocal));

    aPerVoxel = nAttr.create("PerVoxel", "pvx", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.05));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.05));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPerVoxel));

    aInterior = nAttr.create("Interior", "intr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInterior));

    MFnEnumAttribute vxtyAttr;
    aVoxelType = vxtyAttr.create("VoxelType", "vxlty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(vxtyAttr.addField("Level Set", 0));
    DOMMY_RETURN_IF_FAILURE(vxtyAttr.addField("Fog", 1));
    DOMMY_RETURN_IF_FAILURE(vxtyAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVoxelType));

    aVoxelSize = nAttr.create("VoxelSize", "voxelsize", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVoxelSize));

    aExteriorBandWidth =
        nAttr.create("ExteriorBandWidth", "exteriorbandwidth", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExteriorBandWidth));

    aInteriorBandWidth =
        nAttr.create("InteriorBandWidth", "interiorbandwidth", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInteriorBandWidth));

    aFillInterior =
        nAttr.create("FillInterior", "fillinterior", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFillInterior));

    aUnsignedDistanceField =
        nAttr.create("UnsignedDistanceField", "udf", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUnsignedDistanceField));

#pragma mark Sphere Packing

    aRadiusRange = nAttr.create("RadiusRange", "radrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001, 0.001));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRange));

    aPackingGradientBiasRamp =
        MRampAttribute::createCurveRamp("PackingGradientBiasRamp", "pkgrdrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingGradientBiasRamp));

    MFnEnumAttribute packEnumFn;
    aPackingType = packEnumFn.create("PackingType", "pkty", D_PACK_3D_RANDOM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("Random", D_PACK_3D_RANDOM));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("Gradation", D_PACK_3D_GRADATION));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("From Map", D_PACK_3D_MAP));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.addField("Expression", D_PACK_3D_EXPRESSION));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(packEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingType));

    MFnEnumAttribute packAxisEnumFn;
    aPackingAxisType = packAxisEnumFn.create("PackingAxisType", "pkaxty", D_X_AXIS, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(packAxisEnumFn.addField("X Axis", D_X_AXIS));
    DOMMY_RETURN_IF_FAILURE(packAxisEnumFn.addField("Y Axis", D_Y_AXIS));
    DOMMY_RETURN_IF_FAILURE(packAxisEnumFn.addField("Z Axis", D_Z_AXIS));
    DOMMY_RETURN_IF_FAILURE(packAxisEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(packAxisEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPackingAxisType));

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

    aBiasRange = nAttr.create("BiasRange", "bsrng", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBiasRange));

#pragma mark packing map
    MFnGenericAttribute gAttr;
    inPackingUVObject = gAttr.create("inPackingUVObject", "inopuv", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inPackingUVObject));

    aMap = nAttr.createColor("Map", "map", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMap));

    aIs3DMap = nAttr.create("Is3DMap", "is3d", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIs3DMap));

    MFnStringData fnStringData;

    const MObject aExpressionDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExpression = tAttr.create("Expression", "expn", MFnData::kString, aExpressionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));

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

    aBaseRadius = nAttr.create("BaseRadius", "brad", MFnNumericData::kFloat, 0.0, &stat);
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

#pragma mark outPut

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputGeometry, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(emitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(radius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(volumeSize, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBoxGrid, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(nSeed, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpread, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMode, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDensity, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsLocal, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPerVoxel, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInterior, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExteriorBandWidth, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInteriorBandWidth, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFillInterior, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUnsignedDistanceField, outDP));

    // sphere packing
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingAxisType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingGradientBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseRadiusBias, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBiasRange, outDP));
    // map
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inPackingUVObject, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMap, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIs3DMap, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExpression, outDP));
    // pre packing
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrePackingDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPreDPGroupName, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPointVolumeEmit", "initialize",
                                                  std::current_exception());
}

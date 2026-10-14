//
//  DPScatter.cpp
//  dommyTool
//
//  Created by yuzo on 5/16/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPScatter.h"
#include "common/utility/DStatusOperation.h"



#include "DMayaHeaders.h"

#include "DHashGrid.h"
#include "DNodeOperation.h"

#include "dpoint_ids.h"
#include "DPData.h"
#include "DPointCommon.h"
#include "DPAttributeAccess.h"
#include <algorithm>
#include <vector>
#include <openvdb/tools/VolumeToSpheres.h>

#include "DVdbOp.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointProjection.h"
#include "common/utility/DBatchRange.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DRandomValueGeneration.h"
#include "DGlobalTex.h"
#include "../../../modules/volume/VDB/Common/DVDBComputeGuard.h"

#include <cmath>
#include <limits>



const MTypeId DPScatter::typeId( ID_DP_SCATTER );

MObject DPScatter::inObject;
MObject DPScatter::aTotalCount;
MObject DPScatter::aEmitType;//emit type normal or sphere packing
MObject DPScatter::aSeed;
MObject DPScatter::aRadius;
MObject DPScatter::aMaxEffectiveRadius;
MObject DPScatter::aIsRelax;
MObject DPScatter::aRelaxIterations;
MObject DPScatter::aRelaxTolerance;
//sphere packing
MObject DPScatter::aRadiusRange;
MObject DPScatter::aPackingType;
MObject DPScatter::aPackingSampling;
MObject DPScatter::aPackingGradientBiasRamp;
MObject DPScatter::aUseRadiusBias;
MObject DPScatter::aRadiusBiasRamp;
MObject DPScatter::aBiasRange;

//for Map
MObject DPScatter::aMap;

//pre Packing DPoint
MObject DPScatter::aPrePackingDP;
MObject DPScatter::aRadiusAttribute;
MObject DPScatter::aBaseRadius;
MObject DPScatter::aPreDPGroupName;



//output
MObject DPScatter::outDP;




DPScatter::DPScatter() {
    
    clpGrid.reset(nullptr);
}

DPScatter::~DPScatter() = default;


void *DPScatter::creator() {
    return ::dommy::maya::createObject("DPScatter", [&]() {
        return new DPScatter();
    });
}



void inTriangle(const MPoint &pt1, const MPoint &pt2, const MPoint &pt3, float aVal, float bVal, MPoint &result) {
	
    float r = aVal;
    float s = bVal;
    
    
	MVector u = pt2 - pt1;
	MVector v = pt3 - pt1;
 
	if (r + s >= 1.0)
	{
		r = 1.0 - r;
		s = 1.0 - s;
	}
    
	//  Now add the two weighted vectors to a
	result = pt1 + ((u * r) + (v * s));
	
    
    
}






bool DPScatter::getMapValues(
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
        float2 uv = {0.0f, 0.0f};
        MPoint queryPoint(point);
        status = m_meshFn.getUVAtPoint(
            queryPoint, uv, MSpace::kWorld);
        if (!status || !std::isfinite(uv[0]) ||
            !std::isfinite(uv[1])) {
            return false;
        }
        status = uCoordinates.set(
            uv[0], static_cast<unsigned int>(index));
        if (!status) return false;
        status = vCoordinates.set(
            uv[1], static_cast<unsigned int>(index));
        if (!status) return false;
    }

    MVectorArray colors;
    MDoubleArray alphas;
    status = evaluateDynamics2dTextureInBatches(
        thisMObject(), aMap, uCoordinates, vCoordinates, colors, alphas);
    if (!status || alphas.length() != points.size()) return false;

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

bool DPScatter::getMitchellBestCandidate(PackedCircle& result) {
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
                const std::size_t triangleIndex =
                    static_cast<std::size_t>(m_triIdDist(m_eng));
                MPoint position;
                inTriangle(
                    m_pts[m_triIds[triangleIndex].id_a],
                    m_pts[m_triIds[triangleIndex].id_b],
                    m_pts[m_triIds[triangleIndex].id_c],
                    m_distribx(m_eng), m_distriby(m_eng), position);
                positions.push_back(position);
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
        float radius = 0.0f;
        const std::size_t genTriId = static_cast<std::size_t>(m_triIdDist(m_eng));

        MPoint pos;
        inTriangle( m_pts[m_triIds[genTriId].id_a], m_pts[m_triIds[genTriId].id_b], m_pts[m_triIds[genTriId].id_c],
                   m_distribx(m_eng), m_distriby(m_eng),
                   pos);

        switch (m_pack_type) {
            case D_PACK_RANDOM:
            {
                if (m_use_rad_bias) {
                    if (m_max_count == 0) {
                        return false;
                    }
                    const float placedCount = static_cast<float>(
                        placedPoints.empty() ? 1U : placedPoints.size());
                    float biasPosition =
                        1.0f - placedCount / static_cast<float>(m_max_count);
                    if (!dommy::packing::clampUnit(
                            biasPosition, biasPosition)) {
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
                    radius = dommy::packing::uniform(
                        m_eng,
                        m_min_rad + maximumOffset * m_bias_range,
                        m_min_rad + maximumOffset);
                } else {
                    radius = dommy::packing::uniform(m_eng, m_min_rad, m_max_rad);
                }
            }
                break;
            case D_PACK_GRADATION:
            {
                float rampPosition = 0.0f;
                float rampValue = 0.0f;
                if (!dommy::packing::normalizedPosition(
                        static_cast<float>(pos.x), m_grid_minX, m_grid_sizeX,
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
                radius = m_min_rad + m_rad_size * rampValue;
            }
                break;
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

float DPScatter::getDistanceToNearestSample(const PackedCircle& candidate) const {

    float shortestDistance = std::numeric_limits<float>::infinity();

    // Loop into all samples
    for (std::size_t i = 0; i < placedPoints.size(); ++i) {
        PackedCircle otherSample = placedPoints[i];
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

bool DPScatter::placeNewSample() {
    PackedCircle sample(MPoint(0.0, 0.0, 0.0), 0.0f);
    if (!getMitchellBestCandidate(sample)) {
        return false;
    }
    placedPoints.push_back(sample);
    return true;
}

MStatus DPScatter::setDependentsDirty(const MPlug& plug, MPlugArray& /*plugArray*/) {
    MPlug inObjPlug(thisMObject(), DPScatter::inObject);
    MPlug relaxTolPlug(thisMObject(), DPScatter::aRelaxTolerance);
    if (plug == inObjPlug || plug == relaxTolPlug) {
        clpGrid.reset(nullptr);
    }

    return MS::kSuccess;
}

MStatus DPScatter::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {

#pragma mark インプットオブジェクトの読み込み
        MPlug meshPlg(thisMObject(), inObject);
        const bool meshConnected = meshPlg.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!meshConnected) {
            EchoWarning(name().asChar(), "Not connected inObjectPlug to mesh node!");
            return MS::kFailure;
        }

#pragma mark アトリビュート値の取得

        const MDataHandle aTotalCountInputHandle = data.inputValue(aTotalCount, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawTotalCount = aTotalCountInputHandle.asInt();
        const MDataHandle aSeedInputHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSeed = aSeedInputHandle.asInt();

        const MDataHandle aRadiusInputHandle = data.inputValue(aRadius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const DReal radiusVal = aRadiusInputHandle.asFloat();
        const MDataHandle aMaxEffectiveRadiusInputHandle =
            data.inputValue(aMaxEffectiveRadius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const DReal maxEffectiveRadiusVal = aMaxEffectiveRadiusInputHandle.asFloat();
        const MDataHandle aIsRelaxInputHandle = data.inputValue(aIsRelax, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isRelaxVal = aIsRelaxInputHandle.asBool();
        const MDataHandle aRelaxIterationsInputHandle = data.inputValue(aRelaxIterations, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawRelaxIterations = aRelaxIterationsInputHandle.asInt();
        const MDataHandle aRelaxToleranceInputHandle = data.inputValue(aRelaxTolerance, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float relaxTolerance = aRelaxToleranceInputHandle.asFloat();

        const MDataHandle aEmitTypeInputHandle = data.inputValue(aEmitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short emitType = aEmitTypeInputHandle.asShort();

        std::size_t totalCount = 0;
        std::size_t relaxIterationsVal = 0;
        std::size_t relaxWorkCount = 0;
        if (rawSeed < 0 || emitType < 0 || emitType > 1 || !std::isfinite(radiusVal) ||
            radiusVal < 0.0 || (isRelaxVal && radiusVal <= 0.0) ||
            !std::isfinite(maxEffectiveRadiusVal) || maxEffectiveRadiusVal < 0.0 ||
            !std::isfinite(relaxTolerance) || relaxTolerance <= 0.0f ||
            !dommy::generation::checkedCount(
                rawTotalCount, dommy::generation::kMaxGeneratedPointCount, totalCount) ||
            !dommy::generation::checkedCount(
                rawRelaxIterations, dommy::generation::kMaxWorkItemCount, relaxIterationsVal) ||
            (isRelaxVal && !dommy::generation::checkedProduct(totalCount, relaxIterationsVal,
                                                              dommy::generation::kMaxWorkItemCount,
                                                              relaxWorkCount))) {
            MGlobal::displayError("DPScatter: invalid or excessive input value.");
            return MS::kFailure;
        }
        const unsigned int seedVal = static_cast<unsigned int>(rawSeed);

        if (totalCount == 0) {
            DPointList noPtArray;
            return DOutputDPData(data, plug, outDP, noPtArray);
        }

#pragma mark Get Mesh Data Information

        MBoundingBox emitRegion;
        MObject meshObj;
        stat = getMeshInformation(inObject, data, emitRegion, meshObj);
        if (stat != MS::kSuccess || meshObj == MObject::kNullObj) {
            EchoWarning(name().asChar(), "Failed input object.");
            return MS::kFailure;
        }

        m_obj_hnd = data.inputValue(inObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(m_meshFn.setObject(meshObj));

        const int rawVertexCount = m_meshFn.numVertices(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        std::size_t vertexCount = 0U;
        if (!dommy::generation::checkedCount(
                rawVertexCount, dommy::generation::kMaxGeneratedPointCount, vertexCount))
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(m_pts.clear());
        DOMMY_RETURN_IF_FAILURE(m_meshFn.getPoints(m_pts, MSpace::kWorld));
        if (m_pts.length() != vertexCount) {
            MGlobal::displayError("DPScatter: mesh vertex count changed during acquisition.");
            return MS::kFailure;
        }
        for (unsigned int index = 0; index < m_pts.length(); ++index) {
            if (!dommy::packing::isFinite(m_pts[index])) {
                MGlobal::displayError("DPScatter: mesh contains a non-finite vertex.");
                return MS::kFailure;
            }
        }

        MPoint minPoint, maxPoint;
        minPoint = emitRegion.min();
        maxPoint = emitRegion.max();
        if (!dommy::packing::isFinite(minPoint) || !dommy::packing::isFinite(maxPoint) ||
            maxPoint.x < minPoint.x) {
            MGlobal::displayError("DPScatter: mesh bounds are invalid.");
            return MS::kFailure;
        }
        m_grid_minX = static_cast<float>(minPoint.x);
        m_grid_sizeX = static_cast<float>(maxPoint.x - minPoint.x);
        if (!std::isfinite(m_grid_minX) || !std::isfinite(m_grid_sizeX)) {
            MGlobal::displayError("DPScatter: mesh X bounds exceed the supported range.");
            return MS::kFailure;
        }

        // emit DPoint array
        DPointList newPtArray;

        MIntArray triCountArray, triVtxArray;

        std::size_t faceCount = 0U, preflightTriangleCount = 0U;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshTriangles(
            m_meshFn, meshObj, faceCount, preflightTriangleCount));
        if (preflightTriangleCount == 0U) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(m_meshFn.getTriangles(triCountArray, triVtxArray));
        if (triCountArray.length() != faceCount) return MS::kFailure;

#pragma mark Calc total triagle count 三角形の総数の計算
        std::size_t totalTriangle = 0;
        for (std::size_t i = 0, count = triCountArray.length(); i < count; ++i) {
            const int faceTriangleCount = triCountArray[static_cast<unsigned int>(i)];
            std::size_t updatedTriangleCount = 0;
            if (faceTriangleCount < 0 ||
                !dommy::generation::checkedSum(
                    totalTriangle, static_cast<std::size_t>(faceTriangleCount),
                    dommy::generation::kMaxGeneratedFaceCount, updatedTriangleCount)) {
                MGlobal::displayError("DPScatter: mesh triangle count exceeds the safety limit.");
                return MS::kFailure;
            }
            totalTriangle = updatedTriangleCount;
        }
        std::size_t expectedTriangleVertexCount = 0;
        if (totalTriangle == 0 || totalTriangle != preflightTriangleCount ||
            !dommy::generation::checkedProduct(totalTriangle, 3U,
                                               dommy::generation::kMaxWorkItemCount,
                                               expectedTriangleVertexCount) ||
            expectedTriangleVertexCount != triVtxArray.length() ||
            totalTriangle > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            MGlobal::displayError("DPScatter: mesh has no valid triangle topology.");
            return MS::kFailure;
        }

        m_triIds.clear();
        m_triIds.shrink_to_fit();
        m_triIds.resize(totalTriangle);

        std::size_t triangleVertexIndex = 0;
        for (std::size_t i = 0; i < totalTriangle; ++i) {
            const int first = triVtxArray[static_cast<unsigned int>(triangleVertexIndex++)];
            const int second = triVtxArray[static_cast<unsigned int>(triangleVertexIndex++)];
            const int third = triVtxArray[static_cast<unsigned int>(triangleVertexIndex++)];
            if (first < 0 || second < 0 || third < 0 ||
                static_cast<unsigned int>(first) >= m_pts.length() ||
                static_cast<unsigned int>(second) >= m_pts.length() ||
                static_cast<unsigned int>(third) >= m_pts.length()) {
                MGlobal::displayError("DPScatter: mesh triangle references an invalid vertex.");
                return MS::kFailure;
            }
            m_triIds[i].id_a = static_cast<unsigned int>(first);
            m_triIds[i].id_b = static_cast<unsigned int>(second);
            m_triIds[i].id_c = static_cast<unsigned int>(third);
        }

        if (emitType == 0) {

            newPtArray.resize(totalCount);

            std::mt19937 eng(seedVal);
            std::uniform_real_distribution<float> distribx(0.0, 1.0);
            std::uniform_real_distribution<float> distriby(0.0, 1.0);
            std::vector<int> triangleRandIdList;
            dommy::random::generateIntValues(static_cast<int>(seedVal), 0,
                                             static_cast<int>(totalTriangle - 1), totalCount,
                                             triangleRandIdList);
            if (triangleRandIdList.size() != totalCount) {
                MGlobal::displayError("DPScatter: failed to generate triangle samples.");
                return MS::kFailure;
            }

#pragma mark normal emit
            // ポイントの生成
            std::size_t scalarCount = 0;
            if (!dommy::generation::checkedProduct(
                    totalCount, 3U, dommy::generation::kMaxWorkItemCount, scalarCount)) {
                return MS::kFailure;
            }
            std::vector<DReal> hPos(scalarCount);

            size_t genPtCount = 0;
            size_t ptOffsetIdx = 0;
            while (genPtCount < totalCount) {

                size_t genTriId = triangleRandIdList[genPtCount];

                MPoint newPt;

                inTriangle(m_pts[m_triIds[genTriId].id_a], m_pts[m_triIds[genTriId].id_b],
                           m_pts[m_triIds[genTriId].id_c], distribx(eng), distriby(eng), newPt);

                double* pptr = &newPt[0];
                for (int j = 0; j < 3; j++) {
                    hPos[ptOffsetIdx++] = newPtArray[genPtCount][j] = pptr[j];
                }

                genPtCount++;
            }

            // リラックス
            if (isRelaxVal && relaxIterationsVal > 0U) {

                ///////////////////////////////////////////////////Create VDB Surface for relax

                if (!clpGrid) {

                    const short voxeltype = 0; // levelset
                    const float voxelSize = relaxTolerance;
                    const float exteriorBandWidth = 3;
                    const float interiorBandWidth = 3;
                    const bool fillInterior = false;
                    const bool isUnsigned = false;

                    openvdb::FloatGrid::Ptr grid;
                    openvdb::math::Transform::Ptr transform =
                        openvdb::math::Transform::createLinearTransform(voxelSize);
                    bool success =
                        GenerateVdbMeshOp(voxeltype, voxelSize, meshObj, exteriorBandWidth,
                                          interiorBandWidth, isUnsigned, fillInterior, grid);

                    if (!success) {
                        return MS::kFailure;
                    }

                    clpGrid = openvdb::tools::ClosestSurfacePoint<openvdb::FloatGrid>::create(
                        *grid, 0.0 /*isovalue*/);
                }

                // 面積の計算
                MDataHandle meshHandle = data.inputValue(inObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                MItMeshPolygon pIt(meshHandle.data(), &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                DReal totalArea = 0;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshPolygons(
                    pIt, faceCount, [&](MItMeshPolygon& polygon) -> MStatus {
                        double area = 0.0;
                        DOMMY_RETURN_IF_FAILURE(polygon.getArea(area, MSpace::kWorld));
                        if (!std::isfinite(area) || area < 0.0)
                            return MS::kInvalidParameter;
                        totalArea += area;
                        return std::isfinite(totalArea) ? MS::kSuccess : MS::kInvalidParameter;
                    }));
                if (!std::isfinite(totalArea) || totalArea <= 0.0) {
                    MGlobal::displayError("DPScatter: cannot relax on a zero-area mesh.");
                    return MS::kFailure;
                }

                // 一つあたりの半径の計算
                DReal pointDistance = sqrt((4.0 * (totalArea / (DReal)totalCount)) / 1.732051);
                pointDistance *= radiusVal;

                DHashGrid nnGrid(3);

                DReal expansion = 0.5;

                DTools::Vec3 m_v3EnvMin = DTools::Vec3(
                    minPoint.x - expansion, minPoint.y - expansion, minPoint.z - expansion);
                DTools::Vec3 m_v3EnvMax = DTools::Vec3(
                    maxPoint.x + expansion, maxPoint.y + expansion, maxPoint.z + expansion);

                uint m_uMaxParticles = newPtArray.size();
                DReal h = maxEffectiveRadiusVal;
                if (h <= 0)
                    h = pointDistance;

                if (!nnGrid.Setup(m_v3EnvMin, m_v3EnvMax, h, static_cast<int>(m_uMaxParticles))) {
                    MGlobal::displayError("DPScatter: unsupported hash-grid bounds or size.");
                    return MS::kInvalidParameter;
                }

                for (std::size_t it = 0; it < relaxIterationsVal; ++it) {

                    if (!nnGrid.SetObjectToCell(hPos.data(), m_uMaxParticles)) {
                        return MS::kInvalidParameter;
                    }

                    for (std::size_t i = 0, count = newPtArray.size(); i < count; ++i) {

                        DTools::Vec3 pos;
                        pos[0] = hPos[3 * i + 0];
                        pos[1] = hPos[3 * i + 1];
                        pos[2] = hPos[3 * i + 2];

                        std::vector<DNeigh> ne;
                        if (!nnGrid.GetNN(pos, hPos.data(), m_uMaxParticles, ne, pointDistance)) {
                            return MS::kInvalidParameter;
                        }

                        if (ne.size() < 2) {
                            continue;
                        }

                        DVec3Data vel(0.0, 0.0, 0.0);
                        uint numNeighbours = 0;
                        for (auto itr = ne.begin(), eit = ne.end(); itr != eit; ++itr) {
                            int j = itr->Idx;
                            if (j < 0 || static_cast<std::size_t>(j) >= newPtArray.size()) {
                                return MS::kFailure;
                            }
                            if (static_cast<std::size_t>(j) == i)
                                continue;

                            DReal r = (newPtArray[i] - newPtArray[j]).length();
                            if (!std::isfinite(r))
                                return MS::kInvalidParameter;
                            DVec3Data targetVec = newPtArray[i] - newPtArray[j];
                            targetVec.normalize();

                            vel += targetVec * (1.0 - r / pointDistance);
                            numNeighbours++;
                        }

                        if (numNeighbours > 0) {
                            newPtArray[i] += vel / (DReal)numNeighbours;
                        }
                    }

                    // Project every current position, even an isolated point
                    // whose relaxation step took the early continue above.
                    if (!dommy::geometry::projectPointPositions<openvdb::Vec3R>(
                            newPtArray, [this](std::vector<openvdb::Vec3R>& points) {
                                if (!clpGrid)
                                    return false;
                                std::vector<float> distances;
                                return clpGrid->searchAndReplace(points, distances);
                            })) {
                        MGlobal::displayError(
                            "DPScatter: surface projection failed or returned invalid points.");
                        return MS::kFailure;
                    }

                    // update
                    if ((it + 1) < relaxIterationsVal) {
                        size_t ptOffsetIdx = 0;
                        for (size_t i = 0, N = newPtArray.size(); i < N; i++) {
                            const float* pptr = &newPtArray[i].x;
                            for (int j = 0; j < 3; j++) {
                                hPos[ptOffsetIdx++] = pptr[j];
                            }
                        }
                    }
                }
            }

            return DOutputDPData(data, plug, outDP, newPtArray);
        } else {

#pragma mark sphere packing emit

            placedPoints.clear();
            placedPoints.shrink_to_fit();

            // get packing type
            const MDataHandle aPackingTypeInputHandle = data.inputValue(aPackingType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            m_pack_type = aPackingTypeInputHandle.asShort();
            if (m_pack_type < D_PACK_RANDOM || m_pack_type >= D_PACKING_TYPE_COUNT) {
                MGlobal::displayError("DPScatter: unsupported packing mode.");
                return MS::kFailure;
            }

            if (m_pack_type == D_PACK_MAP) {

                // check conected map
                MPlug mapPlug(thisMObject(), aMap);
                const bool mapConnected = mapPlug.isConnected(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!mapConnected) {
                    EchoWarning(name().asChar(), "Map plug is no connect to texture.");
                    return MS::kFailure;
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
                !dommy::generation::checkedProduct(totalCount, m_sampling,
                                                   dommy::generation::kMaxWorkItemCount,
                                                   packingWorkCount)) {
                MGlobal::displayError("DPScatter: packing request exceeds the work limit.");
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
                MGlobal::displayError("DPScatter: invalid packing values.");
                return MS::kFailure;
            }

            m_max_count = totalCount;

            m_eng = std::mt19937(seedVal);

            m_distribx = std::uniform_real_distribution<float>(0.0, 1.0);
            m_distriby = std::uniform_real_distribution<float>(0.0, 1.0);
            m_triIdDist =
                std::uniform_int_distribution<int>(0, static_cast<int>(totalTriangle - 1));

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
                            "DPScatter: pre-packing data exceeds the point limit.");
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
                        MGlobal::displayError("DPScatter: base radius is invalid.");
                        return MS::kFailure;
                    }

                    DScalarDataList preScaleList(preDPData->numberOfPoints(), baseScale);

                    if (preDPData->isExistAttr(radAttributeName)) {
                        getDScalarArrayFromAttribute(*preDPData, radAttributeName, preScaleList);
                    }
                    if (preScaleList.size() != preDPData->numberOfPoints()) {
                        MGlobal::displayError("DPScatter: pre-packing radius count is invalid.");
                        return MS::kFailure;
                    }

                    size_t n = 0;

                    for (auto pIt = preDPData->beginPt(), epIt = preDPData->endPt(); pIt != epIt;
                         ++pIt) {

                        PackedCircle p(DPToMP(*pIt), preScaleList[n]);
                        if (!dommy::packing::isFinite(p)) {
                            MGlobal::displayError(
                                "DPScatter: pre-packing data contains an invalid point or radius.");
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

            // generate loop
            while (placedPoints.size() < totalCount) {
                if (!placeNewSample()) {
                    MGlobal::displayError("DPScatter: failed to evaluate a packing candidate.");
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

            // output point data

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

        } // sphere packing
    }

    return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DPScatter")

MStatus DPScatter::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inObject = gAttr.create("inObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    aTotalCount = nAttr.create("totalCount", "tc", MFnNumericData::kInt, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTotalCount));

    MFnEnumAttribute emitTypeEnumFn;
    aEmitType = emitTypeEnumFn.create("EmitType", "emty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(emitTypeEnumFn.addField("Normal", 0));
    DOMMY_RETURN_IF_FAILURE(emitTypeEnumFn.addField("Sphere Packing", 1));
    DOMMY_RETURN_IF_FAILURE(emitTypeEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(emitTypeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEmitType));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aRadius = nAttr.create("Radius", "ra", MFnNumericData::kFloat, 1.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aMaxEffectiveRadius =
        nAttr.create("MaxEffectiveRadius", "mer", MFnNumericData::kFloat, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxEffectiveRadius));

    aIsRelax = nAttr.create("IsRelax", "ir", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRelax));

    aRelaxIterations = nAttr.create("RelaxIterations", "rit", MFnNumericData::kInt, 4, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRelaxIterations));

    aRelaxTolerance = nAttr.create("RelaxTolerance", "rtol", MFnNumericData::kFloat, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRelaxTolerance));

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

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTotalCount, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEmitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxEffectiveRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsRelax, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRelaxIterations, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRelaxTolerance, outDP));

    // sphere packing
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPackingGradientBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseRadiusBias, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusBiasRamp, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBiasRange, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMap, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrePackingDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPreDPGroupName, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPScatter", "initialize",
                                                  std::current_exception());
}

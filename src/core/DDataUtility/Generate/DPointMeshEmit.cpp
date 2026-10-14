//
//  DPointMeshEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointMeshEmit.h"

#include "DPData.h"
#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dpoint_ids.h"

#include <vector>

#include "DVec.h"
#include "DPointGeneration.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DUniformSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPointArray.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

const MTypeId DPointMeshEmit::typeId(ID_DP_MESH_EMIT);

// Attribute
MObject DPointMeshEmit::inMesh;
MObject DPointMeshEmit::inDM;

MObject DPointMeshEmit::isVertexEmit;

MObject DPointMeshEmit::isEdgeEmit;
MObject DPointMeshEmit::edgeEmitType;
MObject DPointMeshEmit::edgeSampling;
MObject DPointMeshEmit::stepWidth;
MObject DPointMeshEmit::isFinishEdgeLength;

MObject DPointMeshEmit::isFaceEmit;
MObject DPointMeshEmit::faceEmitType;
MObject DPointMeshEmit::faceSampling;

MObject DPointMeshEmit::aIsFromUV;
MObject DPointMeshEmit::aUVSamplingType;
MObject DPointMeshEmit::aUVUniformCount;
MObject DPointMeshEmit::aUVRandomCount;

MObject DPointMeshEmit::aURange;
MObject DPointMeshEmit::aVRange;

MObject DPointMeshEmit::aUVIsRelax;
MObject DPointMeshEmit::aUVRelaxIteration;

MObject DPointMeshEmit::aSeed;

MObject DPointMeshEmit::aUVTolerance;

MObject DPointMeshEmit::outDP;

void* DPointMeshEmit::creator() {
    return ::dommy::maya::createObject("DPointMeshEmit", [&]() {
        return new DPointMeshEmit();
    });
}

namespace {

struct DSubTriangle {
    float s;        // subtriangle のサイズ
    DTools::Vec2 o; // offset

    DSubTriangle(float scale, const DTools::Vec2& offset) : s(scale), o(offset) {}

    std::vector<DSubTriangle> splitIntoSubTriangles() const {
        std::vector<DSubTriangle> out;
        out.reserve(4U);
        float subS = s * 0.5f;
        out.push_back(DSubTriangle(subS, o));
        out.push_back(DSubTriangle(subS, DTools::Vec2(o[0] + s * 0.5f, o[1])));
        out.push_back(DSubTriangle(subS, DTools::Vec2(o[0], o[1] + s * 0.5f)));
        out.push_back(DSubTriangle(-subS, DTools::Vec2(o[0] + s * 0.5f, o[1] + s * 0.5f)));

        return out;
    }
};

bool finitePoint(const DPoint& point) {
    return std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2]);
}

void appendPointChecked(DPointList& points, const DPoint& point) {
    if (!finitePoint(point)) {
        throw std::invalid_argument("DPointMeshEmit generated a non-finite point");
    }
    if (points.size() >= dommy::generation::kMaxGeneratedPointCount) {
        throw std::length_error("DPointMeshEmit point limit exceeded");
    }
    points.push_back(point);
}

void requireAdditionalCapacity(const DPointList& points, std::size_t count) {
    std::size_t total = 0U;
    if (!dommy::generation::checkedSum(points.size(), count,
                                       dommy::generation::kMaxGeneratedPointCount, total)) {
        throw std::length_error("DPointMeshEmit point limit exceeded");
    }
}

std::vector<DSubTriangle> getDivideTriangles(const std::vector<DSubTriangle>& origSubs) {
    std::vector<DSubTriangle> out;
    out.reserve(origSubs.size() * 4U);
    for (size_t i = 0, in = origSubs.size(); i < in; i++) {
        std::vector<DSubTriangle> newTri = origSubs[i].splitIntoSubTriangles();

        std::vector<DSubTriangle>::iterator it;
        it = out.end();

        out.insert(it, newTri.begin(), newTri.end());
    }
    return out;
}

bool sitesOrdered(const DPoint& s1, const DPoint& s2) {
    if (s1[1] < s2[1])
        return true;
    if (s1[1] == s2[1] && s1[0] < s2[0])
        return true;
    if (s1[1] == s2[1] && s1[0] == s2[0] && s1[2] < s2[2])
        return true;

    return false;
}

void emitDPointFromEdge(const MObject& mObj, const MFloatPointArray& pts, const short& edgeEmitType,
                        const int& sampling, const float& step, const bool& isFinish,
                        DPointList& outPoints) {
    if (sampling < 0 || (edgeEmitType != 0 && (!std::isfinite(step) || step <= 0.0f))) {
        throw std::invalid_argument("DPointMeshEmit edge settings are invalid");
    }
    MStatus status;
    MFnMesh meshFn(mObj, &status);
    if (!status)
        throw std::runtime_error("DPointMeshEmit received an invalid mesh");
    const int rawEdgeCount = meshFn.numEdges(&status);
    if (!status || rawEdgeCount < 0) {
        throw std::runtime_error("DPointMeshEmit could not read mesh edges");
    }
    const std::size_t edgeCount = static_cast<std::size_t>(rawEdgeCount);
    std::size_t requestedCount = 0U;
    if (!dommy::generation::checkedProduct(edgeCount, static_cast<std::size_t>(sampling),
                                           dommy::generation::kMaxWorkItemCount, requestedCount)) {
        throw std::length_error("DPointMeshEmit edge work limit exceeded");
    }
    requireAdditionalCapacity(outPoints, requestedCount);

    if (edgeEmitType == 0) { // sampling
        const std::size_t divisor = static_cast<std::size_t>(sampling) + 1U;
        for (std::size_t i = 0; i < edgeCount; ++i) {
            int2 vlist;
            status = meshFn.getEdgeVertices(static_cast<int>(i), vlist);
            if (!status || vlist[0] < 0 || vlist[1] < 0 ||
                static_cast<unsigned int>(vlist[0]) >= pts.length() ||
                static_cast<unsigned int>(vlist[1]) >= pts.length()) {
                throw std::runtime_error("DPointMeshEmit mesh edge is invalid");
            }

            MPoint p1 = pts[vlist[0]];
            MPoint p2 = pts[vlist[1]];
            MVector eVec = p2 - p1;
            for (std::size_t j = 1U; j < divisor; ++j) {
                MPoint pPos = p1 + (eVec / static_cast<double>(divisor)) * static_cast<double>(j);

                appendPointChecked(outPoints, DPoint(pPos[0], pPos[1], pPos[2]));
            }
        }
    } else { // step

        for (std::size_t i = 0; i < edgeCount; ++i) {
            int2 vlist;
            status = meshFn.getEdgeVertices(static_cast<int>(i), vlist);
            if (!status || vlist[0] < 0 || vlist[1] < 0 ||
                static_cast<unsigned int>(vlist[0]) >= pts.length() ||
                static_cast<unsigned int>(vlist[1]) >= pts.length()) {
                throw std::runtime_error("DPointMeshEmit mesh edge is invalid");
            }

            MPoint p1 = pts[vlist[0]];
            MPoint p2 = pts[vlist[1]];

            MVector eVec = p2 - p1;
            float length = eVec.length();

            if (!std::isfinite(length) || length <= 1.0e-12f)
                continue;
            eVec /= length;

            for (int j = 0; j < sampling; ++j) {
                const float currntLength = step * static_cast<float>(j);
                if (isFinish && (length < currntLength)) {
                    break;
                }
                MPoint pPos = p1 + eVec * step * static_cast<float>(j);

                appendPointChecked(outPoints, DPoint(pPos[0], pPos[1], pPos[2]));
            }
        }
    }
}

void emitDPointFromTriangles(const MObject& mObj, const MFloatPointArray& pts,
                             const size_t& sampling, DPointList& outPoints) {
    MStatus status;
    MFnMesh meshFn(mObj, &status);
    if (!status)
        throw std::runtime_error("DPointMeshEmit received an invalid mesh");

    // 三角ポリゴンの情報を取得
    MIntArray triCountArray, triVtxArray;
    status = meshFn.getTriangles(triCountArray, triVtxArray);
    if (!status)
        throw std::runtime_error("DPointMeshEmit could not triangulate the mesh");

    // トータルの三角ポリゴン数を取得
    std::size_t totalTriangle = 0U;
    for (unsigned int i = 0U; i < triCountArray.length(); ++i) {
        if (triCountArray[i] < 0 || !dommy::generation::checkedSum(
                                        totalTriangle, static_cast<std::size_t>(triCountArray[i]),
                                        dommy::generation::kMaxGeneratedFaceCount, totalTriangle)) {
            throw std::length_error("DPointMeshEmit triangle limit exceeded");
        }
    }
    std::size_t expectedTriangleIndices = 0U;
    if (!dommy::generation::checkedProduct(totalTriangle, 3U, dommy::generation::kMaxWorkItemCount,
                                           expectedTriangleIndices) ||
        expectedTriangleIndices != triVtxArray.length()) {
        throw std::runtime_error("DPointMeshEmit triangle topology is invalid");
    }
    std::size_t dividedTriangleCount = 1U;
    for (std::size_t level = 0U; level < sampling; ++level) {
        if (!dommy::generation::checkedProduct(dividedTriangleCount, 4U,
                                               dommy::generation::kMaxWorkItemCount,
                                               dividedTriangleCount)) {
            throw std::length_error("DPointMeshEmit subdivision limit exceeded");
        }
    }
    std::size_t pointsPerTriangle = 0U;
    if (!dommy::generation::checkedProduct(dividedTriangleCount, 3U,
                                           dommy::generation::kMaxGeneratedPointCount,
                                           pointsPerTriangle) ||
        !dommy::generation::checkedSum(
            pointsPerTriangle, 3U, dommy::generation::kMaxGeneratedPointCount, pointsPerTriangle)) {
        throw std::length_error("DPointMeshEmit subdivision point limit exceeded");
    }
    std::size_t maximumPointCount = 0U;
    if (!dommy::generation::checkedProduct(totalTriangle, pointsPerTriangle,
                                           dommy::generation::kMaxGeneratedPointCount,
                                           maximumPointCount)) {
        throw std::length_error("DPointMeshEmit subdivision point limit exceeded");
    }
    requireAdditionalCapacity(outPoints, maximumPointCount);

    DPointList pointList;

    // Generate sub triangles
    for (std::size_t i = 0U; i < totalTriangle; ++i) {

        DSubTriangle baseTri(1.0f, DTools::Vec2(0.0f, 0.0f));

        std::vector<DSubTriangle> totalDividedTriangles;
        totalDividedTriangles.push_back(baseTri);

        for (size_t d = 0; d < sampling; d++) {

            std::vector<DSubTriangle> subTris = getDivideTriangles(totalDividedTriangles);

            totalDividedTriangles.clear();

            std::vector<DSubTriangle>::iterator it;
            it = totalDividedTriangles.end();

            totalDividedTriangles.insert(it, subTris.begin(), subTris.end());
        }

        // Original triangle vertices.
        const int rawIndex0 = triVtxArray[3U * i];
        const int rawIndex1 = triVtxArray[3U * i + 1U];
        const int rawIndex2 = triVtxArray[3U * i + 2U];
        if (rawIndex0 < 0 || rawIndex1 < 0 || rawIndex2 < 0 ||
            static_cast<unsigned int>(rawIndex0) >= pts.length() ||
            static_cast<unsigned int>(rawIndex1) >= pts.length() ||
            static_cast<unsigned int>(rawIndex2) >= pts.length()) {
            throw std::runtime_error("DPointMeshEmit triangle index is invalid");
        }
        const std::size_t index0 = static_cast<std::size_t>(rawIndex0);
        const std::size_t index1 = static_cast<std::size_t>(rawIndex1);
        const std::size_t index2 = static_cast<std::size_t>(rawIndex2);

        MPoint basePt0 = pts[index0];
        MPoint basePt1 = pts[index1];
        MPoint basePt2 = pts[index2];

        appendPointChecked(pointList, MPToDP(basePt0));
        appendPointChecked(pointList, MPToDP(basePt1));
        appendPointChecked(pointList, MPToDP(basePt2));

        MVector vecA(basePt1 - basePt0);
        MVector vecB(basePt2 - basePt0);

        for (std::size_t j = 0U; j < totalDividedTriangles.size(); ++j) {

            DSubTriangle subTri = totalDividedTriangles[j];
            float scale = subTri.s;
            DTools::Vec2 offset = subTri.o;
            MVector sizedVecA(0, 0, 0), sizedVecB(0, 0, 0);
            sizedVecA = vecA * scale;
            sizedVecB = vecB * scale;

            MVector offsetVecU(vecA * offset[0]);
            MVector offsetVecV(vecB * offset[1]);
            MVector finOffsetVec = offsetVecU + offsetVecV;

            MPoint f0 = basePt0 + finOffsetVec;
            MPoint f1 = basePt0 + sizedVecA + finOffsetVec;
            MPoint f2 = basePt0 + sizedVecB + finOffsetVec;
            appendPointChecked(pointList, MPToDP(f0));
            appendPointChecked(pointList, MPToDP(f1));
            appendPointChecked(pointList, MPToDP(f2));
        }
    }

    // Remove shared subdivision vertices before appending the result.
    std::sort(pointList.begin(), pointList.end(), sitesOrdered);
    pointList.erase(std::unique(pointList.begin(), pointList.end(),
                                [](const DPoint& lhs, const DPoint& rhs) {
                                    return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
                                }),
                    pointList.end());
    for (auto it = pointList.begin(), eit = pointList.end(); it != eit; ++it) {
        appendPointChecked(outPoints, *it);
    }
}

// Query the full polygon sequence before publishing any generated points.
// The native/custom callers retain their historical world/object-space policy.
MStatus emitDPointFromFaceCenters(const MObject& object, MSpace::Space space, DPointList& points) {
    MStatus status;
    MFnMesh mesh(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawCount = mesh.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t count = 0U;
    if (!dommy::generation::checkedCount(rawCount, dommy::generation::kMaxGeneratedFaceCount,
                                         count))
        return MS::kInvalidParameter;
    requireAdditionalCapacity(points, count);
    if (count == 0U)
        return MS::kSuccess;

    MItMeshPolygon iterator(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(iterator.reset());
    for (std::size_t i = 0U; i < count; ++i) {
        const bool done = iterator.isDone(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (done)
            return MS::kFailure;
        const unsigned int index = iterator.index(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (index != i)
            return MS::kFailure;
        const MPoint center = iterator.center(space, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        float coordinates[3];
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::checkedFloatCast(center[axis], coordinates[axis]))
                return MS::kInvalidParameter;
        }
        appendPointChecked(points, DPoint(coordinates[0], coordinates[1], coordinates[2]));
        DOMMY_RETURN_IF_FAILURE(iterator.next());
    }
    const bool done = iterator.isDone(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    return done ? MS::kSuccess : MS::kFailure;
}

} // namespace

void DPointMeshEmit::emitDPointFromUVBase(MDataBlock& data, const MObject& mObj,
                                          DPointList& outPoints) {
    MStatus status;
    MFnMesh meshFn(mObj, &status);
    if (!status)
        throw std::runtime_error("DPointMeshEmit received an invalid UV mesh");

    const auto readInput = [&](const MObject& attribute) -> MDataHandle {
        MStatus readStatus;
        const MDataHandle handle = data.inputValue(attribute, &readStatus);
        if (!readStatus)
            throw std::runtime_error("DPointMeshEmit could not read a UV input");
        return handle;
    };
    const MDataHandle uHandle = readInput(aURange);
    const MDataHandle vHandle = readInput(aVRange);
    const float2& uRange = uHandle.asFloat2();
    const float2& vRange = vHandle.asFloat2();
    const short type = readInput(aUVSamplingType).asShort();
    const float tolerance = readInput(aUVTolerance).asFloat();
    if (!std::isfinite(uRange[0]) || !std::isfinite(uRange[1]) ||
        !std::isfinite(vRange[0]) || !std::isfinite(vRange[1]) ||
        uRange[0] > uRange[1] || vRange[0] > vRange[1] ||
        !std::isfinite(tolerance) || tolerance < 0.0f || type < 0 || type > 1) {
        throw std::invalid_argument("DPointMeshEmit UV settings are invalid");
    }

    // Keep the historical hit policy: uniform emits the first point, random
    // emits all overlapping-shell hits. A successful empty query is a no-hit;
    // Maya API failures must not silently become a partial successful output.
    const auto emitAtUV = [&](float u, float v, bool allHits) {
        if (!std::isfinite(u) || !std::isfinite(v))
            throw std::invalid_argument("DPointMeshEmit UV sample is not finite");
        MIntArray faces;
        MPointArray points;
        const float2 uv = {u, v};
        const MStatus queryStatus = meshFn.getPointsAtUV(
            faces, points, uv, MSpace::kWorld, nullptr, tolerance);
        if (!queryStatus || faces.length() != points.length())
            throw std::runtime_error("DPointMeshEmit UV query failed");
        const unsigned int hitCount = allHits ? points.length() :
            std::min(1U, points.length());
        requireAdditionalCapacity(outPoints, hitCount);
        for (unsigned int i = 0U; i < hitCount; ++i) {
            float coordinates[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!dommy::numeric::checkedFloatCast(points[i][axis], coordinates[axis]))
                    throw std::invalid_argument("DPointMeshEmit UV point is not representable");
            }
            appendPointChecked(outPoints, DPoint(coordinates[0], coordinates[1], coordinates[2]));
        }
    };

    if (type == 0) {
        std::size_t count = 0U, total = 0U;
        if (!dommy::generation::checkedCount(readInput(aUVUniformCount).asInt(),
                dommy::generation::kMaxGeneratedPointCount, count) ||
            !dommy::generation::checkedProduct(count, count,
                dommy::generation::kMaxWorkItemCount, total))
            throw std::length_error("DPointMeshEmit UV sample limit exceeded");
        requireAdditionalCapacity(outPoints, total);

        // Store only the two axes, not two redundant count-squared arrays.
        std::vector<float> uValues, vValues;
        if (!dommy::generation::generateUniformFloatSamples(uRange[0], uRange[1], count, uValues) ||
            !dommy::generation::generateUniformFloatSamples(vRange[0], vRange[1], count, vValues))
            throw std::invalid_argument("DPointMeshEmit UV range is invalid");
        for (float u : uValues)
            for (float v : vValues) emitAtUV(u, v, false);
    } else {
        const int seed = readInput(aSeed).asInt();
        const bool relax = readInput(aUVIsRelax).asBool();
        const int rawIterations = relax ? readInput(aUVRelaxIteration).asInt() : 0;
        std::size_t count = 0U, iterations = 0U, work = 0U;
        if (!dommy::generation::checkedCount(readInput(aUVRandomCount).asInt(),
                dommy::generation::kMaxGeneratedPointCount, count) ||
            !dommy::generation::checkedCount(rawIterations,
                dommy::generation::kMaxWorkItemCount, iterations) ||
            !dommy::generation::checkedProduct(count, std::max<std::size_t>(iterations, 1U),
                dommy::generation::kMaxWorkItemCount, work))
            throw std::length_error("DPointMeshEmit UV relaxation limit exceeded");
        requireAdditionalCapacity(outPoints, count);

        // Preserve archival random sampling: generate on [0,1]^2, then filter.
        MPointArray points;
        status = DGenerateRelaxPoint2D(count, iterations, seed,
                                       0.0f, 1.0f, 0.0f, 1.0f, points);
        if (!status || points.length() != count)
            throw std::runtime_error("DPointMeshEmit UV relaxation failed");
        for (unsigned int i = 0U; i < points.length(); ++i) {
            float u = 0.0f, v = 0.0f;
            if (!dommy::numeric::checkedFloatCast(points[i].x, u) ||
                !dommy::numeric::checkedFloatCast(points[i].z, v))
                throw std::invalid_argument("DPointMeshEmit random UV sample is invalid");
            if (u < uRange[0] || u > uRange[1] || v < vRange[0] || v > vRange[1]) continue;
            emitAtUV(u, v, true);
        }
    }
}

MStatus DPointMeshEmit::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {

        // アトリビュート値の取得
        const MDataHandle isVertexEmitValHandle = data.inputValue(isVertexEmit, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isVertexEmitVal = isVertexEmitValHandle.asBool();

        const MDataHandle isEdgeEmitValHandle = data.inputValue(isEdgeEmit, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isEdgeEmitVal = isEdgeEmitValHandle.asBool();
        const MDataHandle edgeEmitTypeValHandle = data.inputValue(edgeEmitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short edgeEmitTypeVal = edgeEmitTypeValHandle.asShort();
        const MDataHandle edgeSamplingValHandle = data.inputValue(edgeSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int edgeSamplingVal = edgeSamplingValHandle.asInt();
        const MDataHandle stepWidthValHandle = data.inputValue(stepWidth, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float stepWidthVal = stepWidthValHandle.asFloat();
        const MDataHandle isFinishEdgeLengthValHandle = data.inputValue(isFinishEdgeLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isFinishEdgeLengthVal = isFinishEdgeLengthValHandle.asBool();

        const MDataHandle isFaceEmitValHandle = data.inputValue(isFaceEmit, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isFaceEmitVal = isFaceEmitValHandle.asBool();
        const MDataHandle faceSamplingValHandle = data.inputValue(faceSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int faceSamplingVal = faceSamplingValHandle.asInt();

        const MDataHandle faceEmitTypeValHandle = data.inputValue(faceEmitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short faceEmitTypeVal = faceEmitTypeValHandle.asShort();

        const MDataHandle isUVEmitValHandle = data.inputValue(aIsFromUV, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isUVEmitVal = isUVEmitValHandle.asBool();
        std::size_t checkedFaceSampling = 0U;
        if (!dommy::generation::checkedCount(faceSamplingVal, dommy::generation::kMaxWorkItemCount,
                                             checkedFaceSampling) ||
            edgeEmitTypeVal < 0 || edgeEmitTypeVal > 1 || faceEmitTypeVal < 0 ||
            faceEmitTypeVal > 1) {
            return MS::kFailure;
        }

        MPlug inmeshPlug(thisMObject(), inMesh);
        MPlug dmPlug(thisMObject(), inDM);

        // Output Dpoint array
        DPointList newPtArray;

        const bool hasMesh = inmeshPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (hasMesh) {
#pragma mark Maya Mesh
            // インプットオブジェクトの読み込み
            MDataHandle meshHnd = data.inputValue(inMesh, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);

            if (meshHnd.type() != MFnData::kMesh) {
                EchoWarning(name().asChar(), "Not support type in inMesh.");
                return MS::kFailure;
            }

            MObject inputMeshObject = meshHnd.data();
            if (inputMeshObject.isNull()) {
                EchoWarning(name().asChar(), "inMesh handle is Null.");
                return MS::kFailure;
            }

            // インプットオブジェクトのセット
            MFnMesh meshFn;
            stat = meshFn.setObject(inputMeshObject);
            if (!stat)
                return stat;

            const int rawVertexCount = meshFn.numVertices(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            std::size_t vertexBudget = 0U;
            if (!dommy::generation::checkedCount(
                    rawVertexCount, dommy::generation::kMaxGeneratedPointCount, vertexBudget))
                return MS::kInvalidParameter;
            MFloatPointArray pts;
            DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(pts, MSpace::kWorld));
            if (pts.length() != vertexBudget)
                return MS::kFailure;

            size_t vertexCount = pts.length();

#pragma mark vertex emit

            if (isVertexEmitVal && vertexCount) {
                for (size_t i = 0; i < vertexCount; i++) {
                    appendPointChecked(newPtArray, DPoint(pts[i][0], pts[i][1], pts[i][2]));
                }
            }

#pragma mark edge emit
            if (isEdgeEmitVal) {
                emitDPointFromEdge(inputMeshObject, pts, edgeEmitTypeVal, edgeSamplingVal,
                                   stepWidthVal, isFinishEdgeLengthVal, newPtArray);
            }

#pragma mark face emit

            if (isFaceEmitVal) {

                if (faceEmitTypeVal == 0) { // Polygon center

                    DOMMY_RETURN_IF_FAILURE(
                        emitDPointFromFaceCenters(inputMeshObject, MSpace::kWorld, newPtArray));

                } else { // sub triangle sampling

                    emitDPointFromTriangles(inputMeshObject, pts, checkedFaceSampling, newPtArray);
                }
            }

#pragma mark uv emit
            if (isUVEmitVal) {

                emitDPointFromUVBase(data, inputMeshObject, newPtArray);
            }

        } else {
            const bool hasDMesh = dmPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (hasDMesh) {

#pragma mark DMesh

                const DMeshData* inputDMData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputDMData));

                size_t meshCount = inputDMData->numberOfMesh();
                if (meshCount > dommy::generation::kMaxGeneratedFaceCount) {
                    return MS::kFailure;
                }

                if (meshCount) {

                    for (auto mIt = inputDMData->beginMeshes(), emIt = inputDMData->endMeshes();
                         mIt != emIt; ++mIt) {

                        MFnMesh meshFn;
                        MObject mObj;

                        bool hasUV;
                        (*mIt)->hasUVData(hasUV);
                        if (hasUV && isUVEmitVal) {

                            stat = generateMMeshFromDmeshWithUV(*mIt->get(), mObj);
                            DOMMY_RETURN_IF_FAILURE(stat);
                        } else {
                            stat = generateMMeshFromDmesh(*mIt->get(), mObj);
                            DOMMY_RETURN_IF_FAILURE(stat);
                        }
                        stat = meshFn.setObject(mObj);
                        if (!stat)
                            return stat;

                        const int rawVertexCount = meshFn.numVertices(&stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        std::size_t vertexBudget = 0U;
                        if (!dommy::generation::checkedCount(
                                rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
                                vertexBudget))
                            return MS::kInvalidParameter;
                        MFloatPointArray pts;
                        DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(pts));
                        if (pts.length() != vertexBudget)
                            return MS::kFailure;

#pragma mark vertex emit
                        if (isVertexEmitVal) {
                            for (auto pIt = (*mIt)->beginPt(), epIt = (*mIt)->endPt(); pIt != epIt;
                                 ++pIt) {
                                appendPointChecked(newPtArray, *pIt);
                            }
                        }

#pragma mark edge emit
                        if (isEdgeEmitVal) {
                            emitDPointFromEdge(mObj, pts, edgeEmitTypeVal, edgeSamplingVal,
                                               stepWidthVal, isFinishEdgeLengthVal, newPtArray);
                        }

#pragma mark face emit

                        if (isFaceEmitVal) {

                            if (faceEmitTypeVal == 0) { // Polygon center

                                DOMMY_RETURN_IF_FAILURE(
                                    emitDPointFromFaceCenters(mObj, MSpace::kObject, newPtArray));

                            } else { // sub triangle sampling

                                emitDPointFromTriangles(mObj, pts, checkedFaceSampling, newPtArray);
                            }
                        }

#pragma mark uv emit
                        if (hasUV && isUVEmitVal) {

                            emitDPointFromUVBase(data, mObj, newPtArray);
                        }
                    }

                } // check mesh count

            } // connected DMesh
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
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPointMeshEmit")

MStatus DPointMeshEmit::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inMesh = tAttr.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    isVertexEmit = nAttr.create("isVertexEmit", "iv", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isVertexEmit));

    isEdgeEmit = nAttr.create("isEdgeEmit", "ie", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isEdgeEmit));

    MFnEnumAttribute edgeEnumFn;
    edgeEmitType = edgeEnumFn.create("edgeEmitType", "eemt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.addField("Sampling", 0));
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.addField("Step Sampling", 1));
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(edgeEmitType));

    edgeSampling = nAttr.create("edgeSampling", "es", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(edgeSampling));

    stepWidth = nAttr.create("stepWidth", "sw", MFnNumericData::kFloat, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(stepWidth));

    isFinishEdgeLength =
        nAttr.create("isFinishEdgeLength", "isfinedg", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isFinishEdgeLength));

    isFaceEmit = nAttr.create("isFaceEmit", "if", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isFaceEmit));

    MFnEnumAttribute enumFn;
    faceEmitType = enumFn.create("faceEmitType", "femt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Polygon", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Triangle Sample", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(faceEmitType));

    faceSampling = nAttr.create("faceSampling", "fs", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(faceSampling));

    aIsFromUV = nAttr.create("IsFromUV", "isuv", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFromUV));

    MFnEnumAttribute uvemenumFn;
    aUVSamplingType = uvemenumFn.create("UVSamplingType", "uvsmpty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(uvemenumFn.addField("Uniform", 0));
    DOMMY_RETURN_IF_FAILURE(uvemenumFn.addField("Random", 1));
    DOMMY_RETURN_IF_FAILURE(uvemenumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVSamplingType));

    aUVUniformCount = nAttr.create("UVUniformCount", "uvunicnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVUniformCount));

    aUVRandomCount = nAttr.create("UVRandomCount", "uvrndcnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(500));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVRandomCount));

    aURange = nAttr.create("URange", "urng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aURange));

    aVRange = nAttr.create("VRange", "vrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVRange));

    aUVIsRelax = nAttr.create("UVIsRelax", "uvisrlx", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVIsRelax));

    aUVRelaxIteration =
        nAttr.create("UVRelaxIteration", "uvrlxit", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVRelaxIteration));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aUVTolerance = nAttr.create("UVTolerance", "uvtol", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVTolerance));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(isVertexEmit, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isEdgeEmit, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(edgeEmitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(edgeSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(stepWidth, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isFinishEdgeLength, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(isFaceEmit, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(faceEmitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(faceSampling, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsFromUV, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVSamplingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVUniformCount, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVRandomCount, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aURange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVIsRelax, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVRelaxIteration, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVTolerance, outDP));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DPointMeshEmit", "initialize",
                                                  std::current_exception());
}

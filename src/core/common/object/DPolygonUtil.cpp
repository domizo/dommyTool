//
//  DPolygonUtil.cpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//

#include "DPolygonUtil.h"
#include "common/utility/DMeshIntersection.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace {

const int kMaximumSphereFrequency = 5;
const float kTriangleRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;

} // namespace


DMeshSphere::DMeshSphere(int frequency)
	: meshObj(MObject::kNullObj),
	  baseVertexArray(dommy::generation::kMaxGeneratedPointCount),
	  basePolyCountArray(dommy::generation::kMaxGeneratedFaceCount),
	  basePolyConnectArray(dommy::generation::kMaxWorkItemCount)
{
	if (frequency < 1 || frequency > kMaximumSphereFrequency) {
		return;
	}
	
	// Add the icosahedron vertices.
	addVertex( MFloatPoint( -1,  kTriangleRatio,  0) );
	addVertex( MFloatPoint(1,  kTriangleRatio,  0 ));
	addVertex( MFloatPoint(-1, -kTriangleRatio,  0));
	addVertex( MFloatPoint(1, -kTriangleRatio,  0 ));
	
	addVertex( MFloatPoint(0, -1,  kTriangleRatio ));
	addVertex( MFloatPoint(0,  1,  kTriangleRatio ));
	addVertex( MFloatPoint(0, -1, -kTriangleRatio ));
	addVertex( MFloatPoint(0,  1, -kTriangleRatio ));
	
	addVertex( MFloatPoint(kTriangleRatio,  0, -1 ));
	addVertex( MFloatPoint(kTriangleRatio,  0,  1 ));
	addVertex( MFloatPoint(-kTriangleRatio,  0, -1 ));
	addVertex( MFloatPoint(-kTriangleRatio,  0,  1 ));
	
	std::list<TriangleIndices> facelist;
	
	//add base face list
	facelist.push_back( TriangleIndices(0, 11, 5));
	facelist.push_back( TriangleIndices(0, 5, 1));
	facelist.push_back( TriangleIndices(0, 1, 7));
	facelist.push_back( TriangleIndices(0, 7, 10));
	facelist.push_back( TriangleIndices(0, 10, 11));
	
	facelist.push_back( TriangleIndices(1, 5, 9));
	facelist.push_back( TriangleIndices(5, 11, 4));
	facelist.push_back( TriangleIndices(11, 10, 2));
	facelist.push_back( TriangleIndices(10, 7, 6));
	facelist.push_back( TriangleIndices(7, 1, 8));
	
	facelist.push_back( TriangleIndices(3, 9, 4));
	facelist.push_back( TriangleIndices(3, 4, 2));
	facelist.push_back( TriangleIndices(3, 2, 6));
	facelist.push_back( TriangleIndices(3, 6, 8));
	facelist.push_back( TriangleIndices(3, 8, 9));
	
	facelist.push_back( TriangleIndices(4, 9, 5));
	facelist.push_back( TriangleIndices(2, 4, 11));
	facelist.push_back( TriangleIndices(6, 2, 10));
	facelist.push_back( TriangleIndices(8, 6, 7));
	facelist.push_back( TriangleIndices(9, 8, 1));
	
	if (frequency > 1) {
		for (int i = 0; i < frequency; i++) {
			std::list<TriangleIndices> facelist2;
			std::list<TriangleIndices>::iterator it = facelist.begin();
			while( it != facelist.end() ) {
				TriangleIndices tri = *it;
				// replace triangle by 4 triangles
				int a = getMiddlePoint(tri.v1, tri.v2);
				int b = getMiddlePoint(tri.v2, tri.v3);
				int c = getMiddlePoint(tri.v3, tri.v1);
				
				facelist2.push_back( TriangleIndices(tri.v1, a, c));
				facelist2.push_back( TriangleIndices(tri.v2, b, a));
				facelist2.push_back( TriangleIndices(tri.v3, c, b));
				facelist2.push_back( TriangleIndices(a, b, c));
				
				++it;
			}
			facelist = facelist2;
		}
	}
	
	if (!facelist.empty()) {
		for (std::size_t i = 0U; i < facelist.size(); ++i) {
			basePolyCountArray.append( 3 );
		}
		
		std::list<TriangleIndices>::iterator it = facelist.begin();
		while( it != facelist.end() ) {
			TriangleIndices tri = *it;
			basePolyConnectArray.append( tri.v1 );
			basePolyConnectArray.append( tri.v2 );
			basePolyConnectArray.append( tri.v3 );
			++it;
		}
	}
	
	// All arrays are local staging. A status failure throws through CheckedArray
	// to the owning compute guard; no partial native mesh is published.
	std::size_t expectedConnections = 0U;
	if (!dommy::generation::checkedProduct(facelist.size(), 3U,
			dommy::generation::kMaxWorkItemCount, expectedConnections) ||
		basePolyCountArray.length() != facelist.size() ||
		basePolyConnectArray.length() != expectedConnections) {
		throw std::runtime_error("Incomplete sphere topology arrays.");
	}
	MStatus status;
	MFnMeshData meshDataFunction;
	MObject candidate = meshDataFunction.create(&status);
	if (!status || candidate.isNull()) return;

	MFnMesh meshFunction;
	const MObject created = meshFunction.create(
		baseVertexArray.length(), basePolyCountArray.length(),
		baseVertexArray, basePolyCountArray, basePolyConnectArray,
		candidate, &status);
	if (!status || created.isNull()) return;
	meshObj = candidate;
}

int DMeshSphere::addVertex(MFloatPoint p) {
	const double length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	if (!std::isfinite(length) || length <= 0.0) {
		throw std::invalid_argument("Invalid sphere vertex.");
	}
	const unsigned int index = baseVertexArray.length();
	baseVertexArray.append(MFloatPoint(p.x/length, p.y/length, p.z/length));
	if (baseVertexArray.length() != index + 1U) {
		throw std::runtime_error("Incomplete sphere vertex array.");
	}
	return static_cast<int>(index);
}


int DMeshSphere::getMiddlePoint(int p1, int p2){
	if (p1 < 0 || p2 < 0 ||
		static_cast<unsigned int>(p1) >= baseVertexArray.length() ||
		static_cast<unsigned int>(p2) >= baseVertexArray.length()) {
		throw std::out_of_range("Invalid sphere edge vertex index.");
	}
	// first check if we have it already
	bool firstIsSmaller = p1 < p2;
	const std::uint64_t smallerIndex = static_cast<std::uint64_t>(
		firstIsSmaller ? p1 : p2);
	const std::uint64_t greaterIndex = static_cast<std::uint64_t>(
		firstIsSmaller ? p2 : p1);
	const std::uint64_t key = (smallerIndex << 32U) | greaterIndex;
	
	
	
	std::map<std::uint64_t, int>::iterator itr;
	
	// キーから値を検索
	itr = middlePointIndexCache.find(key);
	if ( itr != middlePointIndexCache.end() ) {// already exist
		int value = itr->second;
		if (value < 0 || static_cast<unsigned int>(value) >= baseVertexArray.length())
			throw std::out_of_range("Invalid cached sphere midpoint index.");
		return value;
	}
	
	
	// not in cache, calculate it
	MFloatPoint point1 = baseVertexArray[p1];
	MFloatPoint point2 = baseVertexArray[p2];
	MFloatPoint middle(
					   (point1.x + point2.x) / 2.0,
					   (point1.y + point2.y) / 2.0,
					   (point1.z + point2.z) / 2.0);
	
	// add vertex makes sure point is on unit sphere
	int i = addVertex(middle);
	
	// store it, return index
	
	middlePointIndexCache.insert(std::make_pair(key,i));
	
	
	return i;
}



MStatus getPrimDataFromMesh(MFnMesh& srcMesh, meshPrimData& primData)
{
    dommy::mesh::MeshMergePlan plan;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(srcMesh, plan));

    MPointArray points;
    MIntArray faces, connections;
    DOMMY_RETURN_IF_FAILURE(srcMesh.getPoints(points, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(srcMesh.getVertices(faces, connections));
    if (points.length() != plan.points || faces.length() != plan.faces ||
        connections.length() != plan.connections) return MS::kFailure;

    for (unsigned int i = 0U; i < points.length(); ++i)
        if (!dommy::maya::isFloatRepresentablePoint(points[i]))
            return MS::kInvalidParameter;
    std::size_t expectedConnections = 0U;
    for (unsigned int i = 0U; i < faces.length(); ++i) {
        if (faces[i] < 3 || !dommy::generation::checkedSum(
                expectedConnections, static_cast<std::size_t>(faces[i]),
                plan.connections, expectedConnections)) return MS::kInvalidParameter;
    }
    if (expectedConnections != plan.connections) return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < connections.length(); ++i)
        if (connections[i] < 0 ||
            static_cast<std::size_t>(connections[i]) >= plan.points)
            return MS::kInvalidParameter;

    // Maya arrays have no status-returning swap. All reads/validation finish
    // before copying; a failed native copy invalidates this caller-owned stage.
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(points, primData.m_pts));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(faces, primData.m_faces));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(connections, primData.m_connects));
    primData.m_obj = MObject::kNullObj;
    primData.hasExplicitNormals = false;
    primData.surfaceGroup.clear();
    primData.interiorGroup.clear();
    primData.seamGroup.clear();
    primData.seamPointGroup.clear();
    return MS::kSuccess;
}



MStatus meshPrimData::updatePoints() {
    MStatus status;
    MFnMesh meshFn(m_obj, &status);
    if (!status) return status;
    status = meshFn.setPoints(m_pts);
    if (!status) return status;
    return meshFn.updateSurface();
}


MStatus meshPrimData::duplicateTransform(
    const MTransformationMatrix& mat, meshPrimData& other) const {
    if (this == &other) return MS::kInvalidParameter;
    const MMatrix matrix = mat.asMatrix();
    dommy::mesh::MeshMergePlan plan;
    if (!dommy::maya::isFiniteMatrix(matrix) ||
        !dommy::mesh::appendMeshMergePlan(m_pts.length(), m_faces.length(),
            m_connects.length(), 0U, plan) ||
        !dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;

    MPointArray newPos;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(m_pts, newPos));
    for (unsigned int i = 0U; i < newPos.length(); ++i) {
        if (!dommy::maya::isFloatRepresentablePoint(newPos[i]))
            return MS::kInvalidParameter;
        // Keep the historical Maya point multiplication, including homogeneous w.
        newPos[i] = newPos[i] * matrix;
        if (!dommy::maya::isFloatRepresentablePoint(newPos[i]) ||
            !dommy::numeric::canRepresentAsFloat(newPos[i].w))
            return MS::kInvalidParameter;
    }

    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(newPos, other.m_pts));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(m_faces, other.m_faces));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(m_connects, other.m_connects));
    other.m_obj = MObject::kNullObj;
    other.hasExplicitNormals = false;
    other.surfaceGroup.clear();
    other.interiorGroup.clear();
    other.seamGroup.clear();
    other.seamPointGroup.clear();
    return MS::kSuccess;
}

MStatus meshPrimData::setNormalPerVtxPerFace(MVectorArray& normalArray,
    MIntArray& normalfaceIds, MIntArray& normalfvtxIds) {
    const unsigned int count = normalArray.length();
    if (count != normalfaceIds.length() || count != normalfvtxIds.length() ||
        count > dommy::generation::kMaxWorkItemCount) return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < count; ++i)
        if (!dommy::maya::isFloatRepresentableVector(normalArray[i]) ||
            normalfaceIds[i] < 0 || normalfvtxIds[i] < 0) return MS::kInvalidParameter;

    MStatus status;
    MFnMesh meshFn(m_obj, &status);
    if (!status) return status;
    const int faces = meshFn.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int points = meshFn.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (faces < 0 || points < 0) return MS::kFailure;
    for (unsigned int i = 0U; i < count; ++i)
        if (normalfaceIds[i] >= faces || normalfvtxIds[i] >= points)
            return MS::kInvalidParameter;

    status = meshFn.setFaceVertexNormals(
		normalArray, normalfaceIds, normalfvtxIds, MSpace::kObject);
    if (status) hasExplicitNormals = true;
    return status;
}






MStatus findCollapsibleEdge(
    MObject& meshObject, double tolerance, int& edgeIndex)
{
    edgeIndex = -1;
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawEdges = mesh.numEdges(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawFaces = mesh.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t edges = 0U, faces = 0U;
    if (!std::isfinite(tolerance) || tolerance < 0.0 ||
        !dommy::generation::checkedCount(rawEdges,
            dommy::generation::kMaxWorkItemCount, edges) ||
        !dommy::generation::checkedCount(rawFaces,
            dommy::generation::kMaxGeneratedFaceCount, faces)) {
        return MS::kInvalidParameter;
    }
    MItMeshEdge edgeIterator(meshObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MItMeshPolygon polygonIterator(meshObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);

    int candidateIndex = -1;
    status = dommy::maya::visitMeshElements(edgeIterator, edges,
        [&](MItMeshEdge& edge) -> MStatus {
            if (candidateIndex >= 0) return MS::kSuccess;
            double length = 0.0;
            DOMMY_RETURN_IF_FAILURE(edge.getLength(length, MSpace::kObject));
            if (!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
            bool collapse = length == 0.0;
            if (!collapse && length < tolerance) {
                MIntArray faceList;
                edge.getConnectedFaces(faceList, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (faceList.length() != 2U) return MS::kSuccess;
                for (unsigned int i = 0U; i < 2U; ++i) {
                    if (faceList[i] < 0 || static_cast<std::size_t>(faceList[i]) >= faces)
                        return MS::kInvalidParameter;
                    int previousIndex = 0;
                    DOMMY_RETURN_IF_FAILURE(polygonIterator.setIndex(faceList[i], previousIndex));
                    const bool zeroArea = polygonIterator.zeroArea(&status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    collapse = collapse || zeroArea;
                }
            }
            if (collapse) {
                candidateIndex = edge.index(&status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (candidateIndex < 0 || static_cast<std::size_t>(candidateIndex) >= edges)
                    return MS::kInvalidParameter;
            }
            return MS::kSuccess;
        });
    DOMMY_RETURN_IF_FAILURE(status);
    edgeIndex = candidateIndex;
    return MS::kSuccess;
}

MStatus DPolyIsIntersectMeshCheck(
    MObject srcMeshObj, std::vector<MObject>& targetObjects,
    std::vector<bool>& isCheckedArray, bool isCheckCell, double tolerance)
{
    float rayTolerance = 0.0f;
    if (!std::isfinite(tolerance) || tolerance < 0.0 ||
        !dommy::numeric::checkedFloatCast(std::max(tolerance, 1.0e-6), rayTolerance) ||
        targetObjects.size() > dommy::generation::kMaxConnectedGeometryCount) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnMesh source(srcMeshObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::maya::MeshAcceleratorGuard<MFnMesh> cache(source);
    MMeshIsectAccelParams acceleration = source.autoUniformGridParams();
    std::vector<bool> candidate(targetObjects.size(), false);

    for (std::size_t i = 0U; i < targetObjects.size(); ++i) {
        MFnMesh target(targetObjects[i], &status);
        DOMMY_RETURN_IF_FAILURE(status);
        bool keep = !isCheckCell;
        if (isCheckCell) {
            const int rawPoints = target.numVertices(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            const int rawEdges = target.numEdges(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            std::size_t points = 0U, edges = 0U;
            if (!dommy::generation::checkedCount(rawPoints,
                    dommy::generation::kMaxGeneratedPointCount, points) ||
                !dommy::generation::checkedCount(rawEdges,
                    dommy::generation::kMaxWorkItemCount, edges)) {
                return MS::kInvalidParameter;
            }
            bool inner = false, outer = false;
            MItMeshVertex vertices(targetObjects[i], &status);
            DOMMY_RETURN_IF_FAILURE(status);
            status = dommy::maya::visitMeshElements(vertices, points,
                [&](MItMeshVertex& vertex) -> MStatus {
                    if (inner && outer) return MS::kSuccess;
                    const MPoint position = vertex.position(MSpace::kObject, &status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    MFloatPoint raySource;
                    MFloatVector rayDirection;
                    if (!dommy::maya::isFinitePoint(position) ||
                        !dommy::maya::makeFloatRay(position, MVector(0.0, 1.0, 0.0),
                            raySource, rayDirection)) return MS::kInvalidParameter;
                    MFloatPointArray hits;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::checkedAllIntersections(
                        source, raySource, rayDirection, MSpace::kObject,
                        &acceleration, hits, std::numeric_limits<float>::max(), rayTolerance));
                    if (hits.length() % 2U != 0U) inner = true;
                    else outer = true; // A successful zero-hit ray is outside.
                    return MS::kSuccess;
                });
            DOMMY_RETURN_IF_FAILURE(status);
            keep = inner && !outer;

            if (!keep) {
                MItMeshEdge edgeIterator(targetObjects[i], &status);
                DOMMY_RETURN_IF_FAILURE(status);
                status = dommy::maya::visitMeshElements(edgeIterator, edges,
                    [&](MItMeshEdge& edge) -> MStatus {
                        if (keep) return MS::kSuccess;
                        const MPoint first = edge.point(0, MSpace::kObject, &status);
                        DOMMY_RETURN_IF_FAILURE(status);
                        const MPoint second = edge.point(1, MSpace::kObject, &status);
                        DOMMY_RETURN_IF_FAILURE(status);
                        const MVector direction = second - first;
                        const double length = direction.length();
                        if (!dommy::maya::isFinitePoint(first) ||
                            !dommy::maya::isFinitePoint(second) ||
                            !std::isfinite(length)) return MS::kInvalidParameter;
                        if (length <= tolerance) return MS::kSuccess;
                        float maximumParameter = 0.0f;
                        MFloatPoint raySource;
                        MFloatVector rayDirection;
                        if (!dommy::numeric::checkedFloatCast(length, maximumParameter) ||
                            !dommy::maya::makeFloatRay(first, direction / length,
                                raySource, rayDirection)) return MS::kInvalidParameter;
                        MFloatPointArray hits;
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::checkedAllIntersections(
                            source, raySource, rayDirection, MSpace::kObject,
                            &acceleration, hits, maximumParameter, rayTolerance));
                        keep = hits.length() != 0U;
                        return MS::kSuccess;
                    });
                DOMMY_RETURN_IF_FAILURE(status);
            }
        }
        if (!keep) continue;

        if (isCheckCell) {
            const int maximumCollapseCount = target.numEdges(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (maximumCollapseCount < 0 ||
                static_cast<std::size_t>(maximumCollapseCount) >
                    dommy::generation::kMaxWorkItemCount) return MS::kInvalidParameter;
            for (int count = 0; count < maximumCollapseCount; ++count) {
                int edgeIndex = -1;
                DOMMY_RETURN_IF_FAILURE(findCollapsibleEdge(targetObjects[i], tolerance, edgeIndex));
                if (edgeIndex < 0) break;
                MIntArray edgeIds;
                DOMMY_RETURN_IF_FAILURE(edgeIds.setLength(1U));
                if (edgeIds.length() != 1U) return MS::kFailure;
                edgeIds[0U] = edgeIndex;
                DOMMY_RETURN_IF_FAILURE(target.collapseEdges(edgeIds));
                DOMMY_RETURN_IF_FAILURE(target.updateSurface());
            }
        }
        MObjectArray operands;
        DOMMY_RETURN_IF_FAILURE(operands.append(targetObjects[i]));
        DOMMY_RETURN_IF_FAILURE(operands.append(srcMeshObj));
        DOMMY_RETURN_IF_FAILURE(target.booleanOps(MFnMesh::kIntersection, operands, true));
        candidate[i] = true;
    }

    DOMMY_RETURN_IF_FAILURE(cache.release());
    isCheckedArray.swap(candidate);
    return MS::kSuccess;
}

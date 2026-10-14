#include <maya/MFnMesh.h>

#include <cmath>

#include "dupMesh.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaMeshImport.h"

MStatus dupMesh::preflight(MObject object, ImportPlan& output)
{
	MStatus status;
	MFnMesh mesh(object, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	ImportPlan candidate;
	DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(mesh, candidate.geometry));
	const int normals = mesh.numNormals(&status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int edges = mesh.numEdges(&status);
	DOMMY_RETURN_IF_FAILURE(status);
	std::size_t normalBytes = 0U, edgeBytes = 0U;
	const std::size_t maximum = dommy::mesh::kMaxMeshOperationBytes;
	if (!dommy::generation::checkedCount(normals, dommy::generation::kMaxWorkItemCount, candidate.normals) ||
		!dommy::generation::checkedCount(edges, dommy::generation::kMaxWorkItemCount, candidate.edges) ||
		!dommy::generation::checkedProduct(candidate.normals, 16U, maximum, normalBytes) ||
		!dommy::generation::checkedProduct(candidate.edges, sizeof(int), maximum, edgeBytes) ||
		!dommy::generation::checkedSum(normalBytes, edgeBytes, maximum, candidate.extraBytes) ||
		!dommy::mesh::validMeshOperationStorage(candidate.geometry, candidate.extraBytes))
		return MS::kInvalidParameter;
	output = candidate;
	return MS::kSuccess;
}

bool dupMesh::matches(const ImportPlan& plan) const
{
	return isValid() && getTotalVertexNum() == plan.geometry.points &&
		getTotalPolygonNum() == plan.geometry.faces &&
		getTotalConnectionNum() == plan.geometry.connections &&
		getTotalUVNum() == plan.geometry.uvs && getNormalNum() == plan.normals &&
		getTotalEdgeNum() == plan.edges;
}

dupMesh::dupMesh(MObject obj)
{
	ImportPlan plan;
	if (preflight(obj, plan) != MS::kSuccess) return;
	MStatus status;
	MFnMesh baseMeshFn(obj, &status);
	if (!status) return;

	storage_->totalVertexNum = static_cast<unsigned int>(plan.geometry.points);
	status = baseMeshFn.getPoints(storage_->vertexArray, MSpace::kWorld);
	if (!status || storage_->vertexArray.length() != storage_->totalVertexNum) return;
	for (unsigned int vertex = 0U; vertex < storage_->vertexArray.length(); ++vertex) {
		const MFloatPoint& point = storage_->vertexArray[vertex];
		if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
			!std::isfinite(point.z)) {
			return;
		}
	}

	storage_->totalPolygonNum = static_cast<unsigned int>(plan.geometry.faces);
	std::size_t remainingConnections = plan.geometry.connections;
	for (unsigned int polygon = 0U; polygon < storage_->totalPolygonNum; ++polygon) {
		MIntArray vertexList;
		status = baseMeshFn.getPolygonVertices(
			static_cast<int>(polygon), vertexList);
		if (!status || vertexList.length() < 3U || vertexList.length() > remainingConnections ||
			storage_->polyCountsList.append(static_cast<int>(vertexList.length())) !=
				MS::kSuccess) {
			return;
		}
		remainingConnections -= vertexList.length();
		for (unsigned int connection = 0U;
			 connection < vertexList.length(); ++connection) {
			const int vertexIndex = vertexList[connection];
			if (vertexIndex < 0 ||
				static_cast<unsigned int>(vertexIndex) >= storage_->totalVertexNum ||
				storage_->polyConnectList.append(vertexIndex) != MS::kSuccess) {
				return;
			}
		}
	}
	if (remainingConnections != 0U) return;

	status = baseMeshFn.getNormals(storage_->normalArray, MSpace::kObject);
	if (!status || storage_->normalArray.length() != plan.normals) return;
	storage_->totalNormal = storage_->normalArray.length();
	for (unsigned int normal = 0U; normal < storage_->normalArray.length(); ++normal) {
		const MFloatVector& value = storage_->normalArray[normal];
		if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
			!std::isfinite(value.z)) {
			return;
		}
	}
	status = baseMeshFn.getNormalIds(storage_->normCountsArray, storage_->normIdsArray);
	if (!status || storage_->normCountsArray.length() != storage_->totalPolygonNum ||
		storage_->normIdsArray.length() != storage_->polyConnectList.length()) {
		return;
	}
	for (unsigned int polygon = 0U;
		 polygon < storage_->normCountsArray.length(); ++polygon) {
		if (storage_->normCountsArray[polygon] != storage_->polyCountsList[polygon]) return;
	}
	for (unsigned int normalId = 0U; normalId < storage_->normIdsArray.length(); ++normalId) {
		const int index = storage_->normIdsArray[normalId];
		if (index < 0 || static_cast<unsigned int>(index) >= storage_->totalNormal) return;
	}

	status = baseMeshFn.getUVs(storage_->uCoords, storage_->vCoords);
	if (!status || storage_->uCoords.length() != plan.geometry.uvs ||
		storage_->uCoords.length() != storage_->vCoords.length()) return;
	status = baseMeshFn.getAssignedUVs(storage_->uvCountsArray, storage_->uvIdsArray);
	if (!status || storage_->uvCountsArray.length() != storage_->totalPolygonNum) return;
	std::size_t uvIds = 0U;
	for (unsigned int face = 0U; face < storage_->totalPolygonNum; ++face) {
		const int count = storage_->uvCountsArray[face];
		if ((count != 0 && count != storage_->polyCountsList[face]) ||
			!dommy::generation::checkedSum(uvIds, static_cast<std::size_t>(count),
				plan.geometry.connections, uvIds)) return;
	}
	if (uvIds != storage_->uvIdsArray.length()) return;
	for (unsigned int uv = 0U; uv < storage_->uCoords.length(); ++uv) {
		if (!std::isfinite(storage_->uCoords[uv]) || !std::isfinite(storage_->vCoords[uv])) return;
	}
	for (unsigned int uvId = 0U; uvId < storage_->uvIdsArray.length(); ++uvId) {
		const int index = storage_->uvIdsArray[uvId];
		if (index < 0 || static_cast<unsigned int>(index) >= storage_->uCoords.length()) return;
	}

	storage_->totalEdgeNum = static_cast<unsigned int>(plan.edges);
	bool previousSmooth = false;
	for (unsigned int edge = 0U; edge < storage_->totalEdgeNum; ++edge) {
		const bool smooth = baseMeshFn.isEdgeSmooth(
			static_cast<int>(edge), &status);
		if (!status || storage_->edgeArray.append(smooth ? 1 : 0) != MS::kSuccess) return;
		if (edge > 0U && previousSmooth != smooth) storage_->allSameSmooth = false;
		previousSmooth = smooth;
	}
	if (storage_->allSameSmooth && storage_->totalEdgeNum > 0U) storage_->smoothType = previousSmooth ? 1 : 0;

	storage_->valid = true;
}

dupMesh::dupMesh(const DMItem& src)
{
	if (!src || !src->hasValidMeshData()) return;

	storage_->totalVertexNum = static_cast<unsigned int>(src->numberOfPoints());
	if (storage_->vertexArray.setLength(storage_->totalVertexNum) != MS::kSuccess) return;
	const DPointList& points = *src->getPositionPtrConst();
	for (unsigned int vertex = 0U; vertex < storage_->totalVertexNum; ++vertex) {
		const DPoint& point = points[vertex];
		storage_->vertexArray[vertex] = MFloatPoint(point.x, point.y, point.z);
	}
	storage_->totalPolygonNum = static_cast<unsigned int>(src->numFace());
	if (storage_->polyCountsList.copy(src->getFaceList()) != MS::kSuccess ||
		storage_->polyConnectList.copy(src->getConnectList()) != MS::kSuccess ||
		storage_->polyCountsList.length() != storage_->totalPolygonNum) return;
	std::size_t connections = 0U;
	for (unsigned int face = 0U; face < storage_->totalPolygonNum; ++face) {
		const int count = storage_->polyCountsList[face];
		if (count < 3 || !dommy::generation::checkedSum(connections,
			static_cast<std::size_t>(count),
			dommy::generation::kMaxWorkItemCount, connections)) return;
	}
	if (connections != storage_->polyConnectList.length()) return;

	bool hasUV = false;
	src->hasUVData(hasUV);
	if (hasUV) {
		if (src->getUvData(storage_->uCoords, storage_->vCoords,
			storage_->uvCountsArray, storage_->uvIdsArray) != MS::kSuccess) return;
		if (storage_->uCoords.length() == 0U ||
			storage_->uCoords.length() != storage_->vCoords.length() ||
			storage_->uvCountsArray.length() != storage_->totalPolygonNum) return;
		std::size_t uvIds = 0U;
		for (unsigned int face = 0U; face < storage_->totalPolygonNum; ++face) {
			const int count = storage_->uvCountsArray[face];
			if ((count != 0 && count != storage_->polyCountsList[face]) ||
				!dommy::generation::checkedSum(uvIds,
					static_cast<std::size_t>(count),
					dommy::generation::kMaxWorkItemCount, uvIds)) return;
		}
		if (uvIds != storage_->uvIdsArray.length()) return;
	}

	storage_->valid = true;
}

//
//  DMeshData.cpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMeshData.h"
#include "common/utility/DStatusOperation.h"
#include "../common/DMeshNormals.h"
#include "common/utility/DGroupPublication.h"

#include "common/utility/DAttributeValidation.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshUVTransfer.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMeshCopyBudget.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaMeshImport.h"


#include "DMayaHeaders.h"

#include "DPAttributeCommon.h"
#include "DGroupCommon.h"

#include "DMeshUtil.h"
#include "DStringOp.h"

#include "dmesh_ids.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

// DMeshInternalData
namespace {

typedef std::pair<int, int> MeshEdgeKey;
typedef std::map<MeshEdgeKey, std::size_t> MeshEdgeIndexMap;

bool buildMeshEdgeIndexMap(
    const DPointList& points,
    const MIntArray& faceCounts,
    const MIntArray& connections,
    MeshEdgeIndexMap& edgeIndices)
{
    edgeIndices.clear();
    if (points.size() > static_cast<std::size_t>(
            std::numeric_limits<int>::max()) ||
        faceCounts.length() > static_cast<unsigned int>(
            std::numeric_limits<int>::max()) ||
        points.size() > static_cast<std::size_t>(
            std::numeric_limits<unsigned int>::max())) {
        return false;
    }

    MStatus status;
    MFnMeshData meshDataFunction;
    MObject meshObject = meshDataFunction.create(&status);
    if (!status || meshObject.isNull()) return false;

    MPointArray mayaPoints;
    status = mayaPoints.setLength(
        static_cast<unsigned int>(points.size()));
    if (!status || mayaPoints.length() != points.size()) return false;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        mayaPoints[static_cast<unsigned int>(index)] =
            DPToMP(points[index]);
    }

    MFnMesh meshFunction;
    MObject createdMesh = meshFunction.create(
        static_cast<int>(points.size()),
        static_cast<int>(faceCounts.length()),
        mayaPoints, faceCounts, connections, meshObject, &status);
    if (!status || createdMesh.isNull()) return false;

    MItMeshEdge edgeIterator(meshObject, &status);
    if (!status) return false;
    for (;;) {
        const bool done = edgeIterator.isDone(&status);
        if (!status) return false;
        if (done) break;
        const int firstVertex = edgeIterator.index(0, &status);
        if (!status || firstVertex < 0) return false;
        const int secondVertex = edgeIterator.index(1, &status);
        if (!status || secondVertex < 0) return false;
        const int edgeIndex = edgeIterator.index(&status);
        if (!status || edgeIndex < 0) return false;

        const MeshEdgeKey key =
            std::minmax(firstVertex, secondVertex);
        if (!edgeIndices.emplace(
                key, static_cast<std::size_t>(edgeIndex)).second) {
            return false;
        }
        status = edgeIterator.next();
        if (!status) return false;
    }
    return true;
}

} // namespace







void DMeshInternalData::uvInitialize() {
    const MFloatArray coordinates;
    const MIntArray indices;
    if (!setUvData(coordinates, coordinates, indices, indices))
        throw std::runtime_error("Unable to clear DMesh UV arrays");
}

DMeshInternalData::DMeshInternalData(): DPrimBaseData() {}

MStatus DMeshInternalData::createFromMayaObject(
    const MObject& object,
    Ptr& result,
    MSpace::Space space,
    DMeshNormalImportPolicy normals)
try {
    if (normals != DMeshNormalImportPolicy::Recompute &&
        normals != DMeshNormalImportPolicy::PreserveFaceVertex) return MS::kInvalidParameter;
    MStatus status;
    MFnMesh meshFunction(object, &status);
    if (!status) return status;

    dommy::mesh::MeshMergePlan plan;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(meshFunction, plan));

    MPointArray points;
    status = meshFunction.getPoints(points, space);
    if (!status) return status;
    if (points.length() != plan.points) return MS::kFailure;

    MIntArray faceCounts;
    MIntArray connections;
    status = meshFunction.getVertices(faceCounts, connections);
    if (!status) return status;
    if (faceCounts.length() != plan.faces || connections.length() != plan.connections)
        return MS::kFailure;

    Ptr candidate(new DMeshInternalData(points, faceCounts, connections));
    if (!candidate->hasValidPolygonTopology()) return MS::kInvalidParameter;

    if (plan.uvs != 0U) {
        MFloatArray uCoordinates;
        MFloatArray vCoordinates;
        MIntArray uvCounts;
        MIntArray uvConnections;
        status = meshFunction.getUVs(uCoordinates, vCoordinates);
        if (!status) return status;
        if (uCoordinates.length() != plan.uvs || vCoordinates.length() != plan.uvs)
            return MS::kFailure;
        status = meshFunction.getAssignedUVs(uvCounts, uvConnections);
        if (!status) return status;
        if (uvCounts.length() != plan.faces || uvConnections.length() > plan.connections)
            return MS::kFailure;
        if (!candidate->setUvData(
                uCoordinates, vCoordinates, uvCounts, uvConnections)) {
            return MS::kInvalidParameter;
        }
    }

    if (normals == DMeshNormalImportPolicy::PreserveFaceVertex) {
        DOMMY_RETURN_IF_FAILURE(dommy::mesh::captureCornerNormals(object, *candidate, space));
    }
    if (!candidate->hasValidMeshData()) return MS::kInvalidParameter;
    result.swap(candidate);
    return MS::kSuccess;
} catch (const std::invalid_argument&) {
    return MS::kInvalidParameter;
} catch (...) {
    return MS::kFailure;
}

DMeshInternalData::DMeshInternalData(DMeshInternalData const &src): DPrimBaseData(src) {
    if (dommy::maya::copyCheckedArray(src.arrays_->mFaceList, arrays_->mFaceList) != MS::kSuccess ||
        dommy::maya::copyCheckedArray(src.arrays_->mConnectList, arrays_->mConnectList) != MS::kSuccess) {
        throw std::runtime_error("Unable to copy DMesh topology arrays");
    }
    
    //comp attribute
    CopyAttributeToAttribute(mFaceAttributeList, src.mFaceAttributeList);
    CopyAttributeToAttribute(mPVtxAttributeList, src.mPVtxAttributeList);
    
    
    //group
    CopyGroupToGroup(mFaceGroupList, src.mFaceGroupList);
    CopyGroupToGroup(mEdgeGroupList, src.mEdgeGroupList);
    
	
	//uv
    if (dommy::mesh::copyMeshUVArrays(
            src.arrays_->m_u_vals, src.arrays_->m_v_vals, src.arrays_->m_uv_count_list, src.arrays_->m_uv_connect_list,
            arrays_->m_u_vals, arrays_->m_v_vals, arrays_->m_uv_count_list, arrays_->m_uv_connect_list) != MS::kSuccess) {
        throw std::runtime_error("Unable to copy DMesh UV arrays");
    }
	
	m_mat = src.m_mat;
}

DMeshInternalData::DMeshInternalData(const MPointArray &pts, const MIntArray &faceCountList, const MIntArray &connectList): DPrimBaseData() {
	
	
	setPosition(pts);
    if (dommy::maya::copyCheckedArray(faceCountList, arrays_->mFaceList) != MS::kSuccess ||
        dommy::maya::copyCheckedArray(connectList, arrays_->mConnectList) != MS::kSuccess)
        throw std::runtime_error("Unable to copy DMesh topology arrays");
    
    m_mat = MMatrix();
}

DMeshInternalData::DMeshInternalData(const DPointList &pts, const MIntArray &faceCountList, const MIntArray &connectList) {
    setPosition(pts);
    if (dommy::maya::copyCheckedArray(faceCountList, arrays_->mFaceList) != MS::kSuccess ||
        dommy::maya::copyCheckedArray(connectList, arrays_->mConnectList) != MS::kSuccess)
        throw std::runtime_error("Unable to copy DMesh topology arrays");
    
    m_mat = MMatrix();
}

DMeshInternalData::DMeshInternalData(const DPointList &pts, const MIntArray &faceCountList, const MIntArray &connectList,
                                     const MFloatArray &ucoords, const MFloatArray &vcoords, const MIntArray &uvcounts, const MIntArray &uvconnects) {
    setPosition(pts);
    if (dommy::maya::copyCheckedArray(faceCountList, arrays_->mFaceList) != MS::kSuccess ||
        dommy::maya::copyCheckedArray(connectList, arrays_->mConnectList) != MS::kSuccess)
        throw std::runtime_error("Unable to copy DMesh topology arrays");
    if (dommy::mesh::copyMeshUVArrays(ucoords, vcoords, uvcounts, uvconnects,
            arrays_->m_u_vals, arrays_->m_v_vals, arrays_->m_uv_count_list, arrays_->m_uv_connect_list) != MS::kSuccess)
        throw std::runtime_error("Unable to copy DMesh UV arrays");
    
    m_mat = MMatrix();
}


DMeshInternalData::~DMeshInternalData() = default;

MStatus DMeshInternalData::copyTopology(MIntArray& faces, MIntArray& connections) const {
    if (&faces == &connections) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(arrays_->mFaceList, faces));
    return dommy::maya::copyCheckedArray(arrays_->mConnectList, connections);
}

bool DMeshInternalData::hasValidPolygonTopology() const {
    if (!dommy::mesh::validPolygonArrays(
            numberOfPoints(), arrays_->mFaceList, arrays_->mConnectList)) return false;

    for (ConstDPointIter point = beginPt(); point != endPt(); ++point) {
        if (!std::isfinite((*point)[0]) || !std::isfinite((*point)[1]) ||
            !std::isfinite((*point)[2])) {
            return false;
        }
    }

    return true;
}

bool DMeshInternalData::hasValidUVTopology() const {
    return dommy::mesh::validFullMeshUVArrays(
        arrays_->mFaceList, arrays_->m_u_vals, arrays_->m_v_vals, arrays_->m_uv_count_list, arrays_->m_uv_connect_list);
}

bool DMeshInternalData::hasValidMeshData() const {
    if (!hasValidPolygonTopology() ||
		!hasValidUVTopology() ||
		!dommy::attribute::isFiniteMatrix(m_mat)) {
		return false;
	}

    const std::size_t pointCount = numberOfPoints();
    const std::size_t faceCount = static_cast<std::size_t>(arrays_->mFaceList.length());
    const std::size_t faceVertexCount =
        static_cast<std::size_t>(arrays_->mConnectList.length());
    std::size_t workItems = 0U;
    if (!dommy::generation::checkedSum(
            pointCount, faceCount,
            dommy::generation::kMaxWorkItemCount, workItems) ||
        !dommy::generation::checkedSum(
            workItems, faceVertexCount,
            dommy::generation::kMaxWorkItemCount, workItems)) {
        return false;
    }

	dommy::attribute::ValidationBudget budget(workItems);
	if (!dommy::attribute::validateAttributeList(
			mAttributeList, pointCount, budget) ||
		!dommy::attribute::validateAttributeList(
			mFaceAttributeList, faceCount, budget) ||
		!dommy::attribute::validateAttributeList(
			mPVtxAttributeList, faceVertexCount, budget)) {
		return false;
    }

    for (ConstDPGrpIter group = mGrooupList.begin();
         group != mGrooupList.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != pointCount ||
            !dommy::generation::checkedSum(
                budget.workItems, pointCount,
                dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }
    for (ConstDPGrpIter group = mFaceGroupList.begin();
         group != mFaceGroupList.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != faceCount ||
            !dommy::generation::checkedSum(
                budget.workItems, faceCount,
                dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }

    // Edge enumeration is only needed to validate edge-group cardinalities.
    if (mEdgeGroupList.empty()) return true;
    std::set<std::pair<int, int> > edges;
    std::size_t connectionOffset = 0U;
    for (unsigned int face = 0U; face < arrays_->mFaceList.length(); ++face) {
        const std::size_t faceSize = static_cast<std::size_t>(arrays_->mFaceList[face]);
        for (std::size_t vertex = 0U; vertex < faceSize; ++vertex) {
            const int first = arrays_->mConnectList[static_cast<unsigned int>(
                connectionOffset + vertex)];
            const int second = arrays_->mConnectList[static_cast<unsigned int>(
                connectionOffset + (vertex + 1U) % faceSize)];
            edges.insert(std::minmax(first, second));
        }
        connectionOffset += faceSize;
    }
    for (ConstDPGrpIter group = mEdgeGroupList.begin();
         group != mEdgeGroupList.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != edges.size() ||
            !dommy::generation::checkedSum(
                budget.workItems, edges.size(),
                dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
DMeshInternalData::Ptr DMeshInternalData::copy() {
	return Ptr(new DMeshInternalData(*this));
}

DMeshInternalData::CPtr DMeshInternalData::copy() const {
	return CPtr(new DMeshInternalData(*this));
}


DMeshInternalData::Ptr DMeshInternalData::copyNewMesh() const {
	return Ptr(new DMeshInternalData(*this));
}



void DMeshInternalData::copy(const DMeshInternalData &other)
{
	if (this == &other) return;
    if (!other.hasValidMeshData()) {
        throw std::invalid_argument(
            "DMeshInternalData::copy rejected invalid mesh data");
    }

    DMeshInternalData candidate(other);
    if (!candidate.hasValidMeshData()) {
        throw std::runtime_error(
            "DMeshInternalData::copy could not build a valid copy");
    }

    swapState(candidate);
}


DMeshInternalData& DMeshInternalData::operator=(const DMeshInternalData& rhs) {
    copy(rhs);
    return *this;
}

void DMeshInternalData::swapState(DMeshInternalData& other) noexcept
{
    pointArray.swap(other.pointArray);
    mAttributeList.swap(other.mAttributeList);
    mGrooupList.swap(other.mGrooupList);
    mFaceAttributeList.swap(other.mFaceAttributeList);
    mPVtxAttributeList.swap(other.mPVtxAttributeList);
    mFaceGroupList.swap(other.mFaceGroupList);
    mEdgeGroupList.swap(other.mEdgeGroupList);
    arrays_.swap(other.arrays_);
    std::swap(m_mat, other.m_mat);
}




bool DMeshInternalData::replacePositions(
    const DPointList& points)
{
    if (!hasValidMeshData() ||
        points.size() != pointArray.size()) {
        return false;
    }
    return setReplaceMeshData(
        points,
        arrays_->mFaceList,
        arrays_->mConnectList,
        DMeshReplacementPolicy::RequireCompatibleMetadata);
}

MStatus DMeshInternalData::bakeTransform(const MMatrix& matrix) try {
    if (!hasValidMeshData() || !dommy::numeric::isFiniteAffineTransform(matrix))
        return MS::kInvalidParameter;

    auto normalEntry = mPVtxAttributeList.find(dommy::mesh::cornerNormalAttribute());
    const bool hasNormals = normalEntry != mPVtxAttributeList.end();
    std::size_t pointBytes = 0, normalBytes = 0, totalBytes = 0;
    const std::size_t limit = 128U * 1024U * 1024U;
    if (!dommy::generation::checkedProduct(pointArray.size(), sizeof(DPoint), limit, pointBytes) ||
        !dommy::generation::checkedProduct(hasNormals ? numberOfConnections() : 0,
                                           3 * sizeof(DPoint), limit, normalBytes) ||
        !dommy::generation::checkedSum(pointBytes, normalBytes, limit, totalBytes))
        return MS::kInvalidParameter;

    DPointList points;
    points.reserve(pointArray.size());
    for (const auto& point : pointArray) {
        std::array<float, 3> transformed;
        if (!dommy::numeric::checkedTransformPoint(point, matrix, transformed))
            return MS::kInvalidParameter;
        points.push_back(DPoint(transformed[0], transformed[1], transformed[2]));
    }
    DPAttributeData::Ptr normalAttribute;
    if (hasNormals) {
        DPointList normals;
        DOMMY_RETURN_IF_FAILURE(dommy::mesh::appendTransformedCornerNormals(*this, matrix, normals));
        normalAttribute = std::make_shared<DPVec3fAttr>(normals, DPoint(0, 0, 0));
    }

    // Commit only after every allocation and numerical conversion succeeds.
    pointArray.swap(points);
    if (hasNormals) normalEntry->second.swap(normalAttribute);
    m_mat = MMatrix::identity;
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

bool DMeshInternalData::reverseWinding() try {
    if (!hasValidMeshData()) return false;
    DMeshInternalData candidate(*this);
    // Reversed winding needs new geometric normals, not the previous authored directions.
    candidate.mPVtxAttributeList.erase(dommy::mesh::cornerNormalAttribute());
    DPIndexList cornerIds(numberOfConnections());
    std::size_t offset = 0U;
    const bool copyUVs = hasUV();
    for (unsigned int face = 0U; face < arrays_->mFaceList.length(); ++face) {
        const std::size_t count = static_cast<std::size_t>(arrays_->mFaceList[face]);
        for (std::size_t corner = 0U; corner < count; ++corner) {
            const std::size_t oldCorner = offset + count - 1U - corner;
            const unsigned int destination = static_cast<unsigned int>(offset + corner);
            cornerIds[destination] = oldCorner;
            candidate.arrays_->mConnectList[destination] =
                arrays_->mConnectList[static_cast<unsigned int>(oldCorner)];
            if (copyUVs)
                candidate.arrays_->m_uv_connect_list[destination] =
                    arrays_->m_uv_connect_list[static_cast<unsigned int>(oldCorner)];
        }
        offset += count;
    }
    for (auto& attribute : candidate.mPVtxAttributeList)
        if (!attribute.second->sortArrayFromIds(cornerIds)) return false;

    if (!mEdgeGroupList.empty()) {
        MeshEdgeIndexMap oldEdges, newEdges;
        if (!buildMeshEdgeIndexMap(pointArray, arrays_->mFaceList,
                arrays_->mConnectList, oldEdges) ||
            !buildMeshEdgeIndexMap(candidate.pointArray, candidate.arrays_->mFaceList,
                candidate.arrays_->mConnectList, newEdges) ||
            oldEdges.size() != newEdges.size()) return false;
        DPIndexList edgeIds(newEdges.size(), oldEdges.size());
        for (const auto& edge : newEdges) {
            const auto oldEdge = oldEdges.find(edge.first);
            if (oldEdge == oldEdges.end() || edge.second >= edgeIds.size() ||
                oldEdge->second >= oldEdges.size() || edgeIds[edge.second] != oldEdges.size())
                return false;
            edgeIds[edge.second] = oldEdge->second;
        }
        for (std::size_t id : edgeIds) if (id >= oldEdges.size()) return false;
        RemapFromGroupUseIds(candidate.mEdgeGroupList, edgeIds);
    }
    if (!candidate.hasValidMeshData()) return false;
    swapState(candidate);
    return true;
} catch (...) {
    return false;
}

bool DMeshInternalData::setReplaceMeshData(
    const DPointList &pts,
    const MIntArray& faceList,
    const MIntArray &connectList,
    DMeshReplacementPolicy policy)
{
    try {
        DMeshInternalData candidate(pts, faceList, connectList);
        candidate.m_mat = m_mat;

        if (policy ==
            DMeshReplacementPolicy::RequireCompatibleMetadata) {
            CopyAttributeToAttribute(
                candidate.mAttributeList, mAttributeList);
            CopyAttributeToAttribute(
                candidate.mFaceAttributeList, mFaceAttributeList);
            CopyAttributeToAttribute(
                candidate.mPVtxAttributeList, mPVtxAttributeList);
            // Geometry replacement/deformation invalidates captured normals.
            // Matrix-only transforms retain them and use inverse-transpose output.
            candidate.mPVtxAttributeList.erase(dommy::mesh::cornerNormalAttribute());
            CopyGroupToGroup(
                candidate.mGrooupList, mGrooupList);
            CopyGroupToGroup(
                candidate.mFaceGroupList, mFaceGroupList);
            CopyGroupToGroup(
                candidate.mEdgeGroupList, mEdgeGroupList);
            if (dommy::mesh::copyMeshUVArrays(
                    arrays_->m_u_vals, arrays_->m_v_vals,
                    arrays_->m_uv_count_list, arrays_->m_uv_connect_list,
                    candidate.arrays_->m_u_vals, candidate.arrays_->m_v_vals,
                    candidate.arrays_->m_uv_count_list, candidate.arrays_->m_uv_connect_list) != MS::kSuccess)
                return false;
        }

        if (!candidate.hasValidMeshData()) return false;

        swapState(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool DMeshInternalData::setReplaceMeshData(
    const MPointArray &pts,
    const MIntArray &faceList,
    const MIntArray &connectList,
    DMeshReplacementPolicy policy)
{
    try {
        DPointList convertedPoints;
        convertedPoints.reserve(pts.length());
        for (unsigned int index = 0U; index < pts.length(); ++index) {
            convertedPoints.push_back(MPToDP(pts[index]));
        }
        return setReplaceMeshData(
            convertedPoints, faceList, connectList, policy);
    } catch (...) {
        return false;
    }
}

bool DMeshInternalData::setReplaceMeshData(
    const MObject &meshObj,
    DMeshReplacementPolicy policy)
{
    MStatus status;
    MFnMesh meshFn(meshObj, &status);
    if (status != MS::kSuccess) return false;

    MPointArray pts;
    MIntArray faceList;
    MIntArray connectList;
    // Mesh-data objects have no DAG path; world-space queries are invalid.
    status = meshFn.getPoints(pts, MSpace::kObject);
    if (status != MS::kSuccess) return false;
    status = meshFn.getVertices(faceList, connectList);
    if (status != MS::kSuccess) return false;
    return setReplaceMeshData(pts, faceList, connectList, policy);
}



bool DMeshInternalData::deleteFaceFromIds(
    const DPIdList &ids,
    bool &isAllDelete)
{
    isAllDelete = false;
    if (!hasValidMeshData() ||
        numFace() < 0 ||
        ids.size() != static_cast<std::size_t>(numFace())) {
        return false;
    }

    const std::size_t deleteCount =
        static_cast<std::size_t>(
            std::count(ids.begin(), ids.end(), true));
    if (deleteCount == 0U) return true;
    if (deleteCount == ids.size()) {
        isAllDelete = true;
        return true;
    }

    try {
        DPointList retainedPoints;
        retainedPoints.reserve(numberOfPoints());
        DPIndexList retainedVertexIds;
        retainedVertexIds.reserve(numberOfPoints());
        DPIndexList oldToNewVertex(
            numberOfPoints(), std::numeric_limits<std::size_t>::max());
        DPIndexList retainedFaceIds;
        retainedFaceIds.reserve(
            static_cast<std::size_t>(numFace()) - deleteCount);
        DPIndexList retainedFaceVertexIds;
        retainedFaceVertexIds.reserve(arrays_->mConnectList.length());

        MIntArray retainedFaceCounts;
        MIntArray retainedConnections;
        MIntArray retainedUvCounts;
        MIntArray retainedUvConnections;
        const bool copyUvs = hasUV();

        std::size_t connectionOffset = 0U;
        for (unsigned int faceIndex = 0U;
             faceIndex < arrays_->mFaceList.length(); ++faceIndex) {
            const std::size_t faceVertexCount =
                static_cast<std::size_t>(arrays_->mFaceList[faceIndex]);
            if (!ids[faceIndex]) {
                if (!retainedFaceCounts.append(
                        arrays_->mFaceList[faceIndex])) {
                    return false;
                }
                retainedFaceIds.push_back(faceIndex);
                if (copyUvs &&
                    !retainedUvCounts.append(
                        arrays_->m_uv_count_list[faceIndex])) {
                    return false;
                }

                for (std::size_t localVertex = 0U;
                     localVertex < faceVertexCount; ++localVertex) {
                    const std::size_t faceVertexIndex =
                        connectionOffset + localVertex;
                    const int oldVertex =
                        arrays_->mConnectList[static_cast<unsigned int>(
                            faceVertexIndex)];
                    const std::size_t oldVertexIndex =
                        static_cast<std::size_t>(oldVertex);
                    std::size_t newVertexIndex =
                        oldToNewVertex[oldVertexIndex];
                    if (newVertexIndex ==
                        std::numeric_limits<std::size_t>::max()) {
                        newVertexIndex = retainedPoints.size();
                        if (newVertexIndex >
                            static_cast<std::size_t>(
                                std::numeric_limits<int>::max())) {
                            return false;
                        }
                        oldToNewVertex[oldVertexIndex] = newVertexIndex;
                        retainedVertexIds.push_back(oldVertexIndex);
                        retainedPoints.push_back(
                            pointArray[oldVertexIndex]);
                    }
                    if (!retainedConnections.append(
                            static_cast<int>(newVertexIndex))) {
                        return false;
                    }
                    retainedFaceVertexIds.push_back(faceVertexIndex);
                    if (copyUvs &&
                        !retainedUvConnections.append(
                            arrays_->m_uv_connect_list[
                                static_cast<unsigned int>(
                                    faceVertexIndex)])) {
                        return false;
                    }
                }
            }
            connectionOffset += faceVertexCount;
        }

        DMeshInternalData candidate(
            retainedPoints, retainedFaceCounts, retainedConnections);
        candidate.m_mat = m_mat;
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        DRemapAtttributeFromIds(
            candidate.mAttributeList, retainedVertexIds);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        RemapFromGroupUseIds(
            candidate.mGrooupList, retainedVertexIds);

        CopyAttributeToAttribute(
            candidate.mFaceAttributeList, mFaceAttributeList);
        DRemapAtttributeFromIds(
            candidate.mFaceAttributeList, retainedFaceIds);
        CopyGroupToGroup(
            candidate.mFaceGroupList, mFaceGroupList);
        RemapFromGroupUseIds(
            candidate.mFaceGroupList, retainedFaceIds);

        CopyAttributeToAttribute(
            candidate.mPVtxAttributeList, mPVtxAttributeList);
        DRemapAtttributeFromIds(
            candidate.mPVtxAttributeList, retainedFaceVertexIds);

        if (copyUvs &&
            !candidate.setUvData(
                arrays_->m_u_vals, arrays_->m_v_vals,
                retainedUvCounts, retainedUvConnections)) {
            return false;
        }

        if (!mEdgeGroupList.empty()) {
            MeshEdgeIndexMap oldEdgeIndices;
            MeshEdgeIndexMap newEdgeIndices;
            if (!buildMeshEdgeIndexMap(
                    pointArray, arrays_->mFaceList, arrays_->mConnectList,
                    oldEdgeIndices) ||
                !buildMeshEdgeIndexMap(
                    candidate.pointArray,
                    candidate.arrays_->mFaceList,
                    candidate.arrays_->mConnectList,
                    newEdgeIndices)) {
                return false;
            }

            DPIndexList retainedEdgeIds(newEdgeIndices.size());
            for (MeshEdgeIndexMap::const_iterator edge =
                     newEdgeIndices.begin();
                 edge != newEdgeIndices.end(); ++edge) {
                const int firstNewVertex = edge->first.first;
                const int secondNewVertex = edge->first.second;
                if (firstNewVertex < 0 || secondNewVertex < 0 ||
                    static_cast<std::size_t>(firstNewVertex) >=
                        retainedVertexIds.size() ||
                    static_cast<std::size_t>(secondNewVertex) >=
                        retainedVertexIds.size() ||
                    edge->second >= retainedEdgeIds.size()) {
                    return false;
                }
                const int firstOldVertex = static_cast<int>(
                    retainedVertexIds[
                        static_cast<std::size_t>(firstNewVertex)]);
                const int secondOldVertex = static_cast<int>(
                    retainedVertexIds[
                        static_cast<std::size_t>(secondNewVertex)]);
                const MeshEdgeIndexMap::const_iterator oldEdge =
                    oldEdgeIndices.find(
                        std::minmax(firstOldVertex, secondOldVertex));
                if (oldEdge == oldEdgeIndices.end()) return false;
                retainedEdgeIds[edge->second] = oldEdge->second;
            }
            CopyGroupToGroup(
                candidate.mEdgeGroupList, mEdgeGroupList);
            RemapFromGroupUseIds(
                candidate.mEdgeGroupList, retainedEdgeIds);
        }

        if (!candidate.hasValidMeshData()) return false;
        swapState(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

void DMeshInternalData::deleteEdgeFromIds(const DPIdList &ids) {
    DeleteFromGroupUseIds(mEdgeGroupList, ids);
}

// uv mesh info method

// -----------------------------------------------------------------------------
// UV maps


bool DMeshInternalData::hasUV() const {
    return arrays_->m_u_vals.length() != 0U && hasValidUVTopology();
}

void DMeshInternalData::hasUVData(bool &isUvs) {
    isUvs = this->hasUV();
}

void DMeshInternalData::hasUVData(bool &isUvs) const {
    isUvs = this->hasUV();
}


MStatus DMeshInternalData::getUvData(MFloatArray &ucoords, MFloatArray &vcoords, MIntArray &uvcounts, MIntArray &uvconnects ) {
    return static_cast<const DMeshInternalData&>(*this).getUvData(
        ucoords, vcoords, uvcounts, uvconnects);
}

MStatus DMeshInternalData::getUvData(MFloatArray &ucoords, MFloatArray &vcoords, MIntArray &uvcounts, MIntArray &uvconnects) const {
    return dommy::mesh::copyMeshUVArrays(
        arrays_->m_u_vals, arrays_->m_v_vals, arrays_->m_uv_count_list, arrays_->m_uv_connect_list,
        ucoords, vcoords, uvcounts, uvconnects);
}

bool DMeshInternalData::setUvData(const MFloatArray &ucoords, const MFloatArray &vcoords, const MIntArray &uvcounts, const MIntArray &uvconnects) try {
    if (!dommy::mesh::validFullMeshUVArrays(
            arrays_->mFaceList, ucoords, vcoords, uvcounts, uvconnects)) return false;
    std::unique_ptr<ArrayStorage> candidate(new ArrayStorage());
    if (dommy::maya::copyCheckedArray(arrays_->mFaceList, candidate->mFaceList) != MS::kSuccess ||
        dommy::maya::copyCheckedArray(arrays_->mConnectList, candidate->mConnectList) != MS::kSuccess ||
        dommy::mesh::copyMeshUVArrays(ucoords, vcoords, uvcounts, uvconnects,
            candidate->m_u_vals, candidate->m_v_vals,
            candidate->m_uv_count_list, candidate->m_uv_connect_list) != MS::kSuccess)
        return false;
    arrays_.swap(candidate);
    return true;
} catch (...) {
    return false;
}

void DMeshInternalData::clearUVs() {
	
	uvInitialize();
	
}


// -----------------------------------------------------------------------------
// Merge

MStatus DMeshInternalData::createMerged(
    const std::vector<const DMeshInternalData*>& sources, Ptr& result) try
{
    if (sources.empty() ||
        sources.size() > dommy::generation::kMaxGeneratedMeshCount) {
        return MS::kInvalidParameter;
    }
    dommy::mesh::MeshMergePlan plan;
    bool preserveUVs = true;
    bool preserveNormals = false;
    for (const DMeshInternalData* source : sources) {
        if (!source || !source->hasValidMeshData() ||
            !dommy::mesh::appendMeshMergePlan(
                source->pointArray.size(), source->arrays_->mFaceList.length(),
                source->arrays_->mConnectList.length(), source->arrays_->m_u_vals.length(), plan)) {
            return MS::kInvalidParameter;
        }
        if (!source->pointArray.empty() && !source->hasUV()) preserveUVs = false;
        preserveNormals = preserveNormals || dommy::mesh::hasCornerNormals(*source);
    }

    // Admit geometry even when no explicit normals are present. Normal
    // staging shares the operation budget rather than a separate allowance.
    std::size_t normalBytes = 0U;
    if ((preserveNormals && !dommy::generation::checkedProduct(
            plan.connections, 64U, 128U * 1024U * 1024U, normalBytes)) ||
        !dommy::mesh::validMeshOperationStorage(plan, normalBytes))
        return MS::kInvalidParameter;

    DPointList mergedNormals;
    if (preserveNormals) {
        mergedNormals.reserve(plan.connections);
        for (const auto* source : sources)
            DOMMY_RETURN_IF_FAILURE(dommy::mesh::appendCornerNormals(*source, true, mergedNormals));
    }

    Ptr candidate(new DMeshInternalData());
    candidate->pointArray.resize(plan.points);
    MStatus status = candidate->arrays_->mFaceList.setLength(
        static_cast<unsigned int>(plan.faces));
    if (!status) return status;
    status = candidate->arrays_->mConnectList.setLength(
        static_cast<unsigned int>(plan.connections));
    if (!status) return status;
    if (preserveUVs && plan.uvs != 0U) {
        status = candidate->arrays_->m_u_vals.setLength(static_cast<unsigned int>(plan.uvs));
        if (!status) return status;
        status = candidate->arrays_->m_v_vals.setLength(static_cast<unsigned int>(plan.uvs));
        if (!status) return status;
        status = candidate->arrays_->m_uv_count_list.setLength(
            static_cast<unsigned int>(plan.faces));
        if (!status) return status;
        status = candidate->arrays_->m_uv_connect_list.setLength(
            static_cast<unsigned int>(plan.connections));
        if (!status) return status;
    }

    std::size_t pointOffset = 0U, faceOffset = 0U, connectionOffset = 0U, uvOffset = 0U;
    for (const DMeshInternalData* source : sources) {
        for (std::size_t point = 0U; point < source->pointArray.size(); ++point) {
            std::array<float, 3> transformed;
            if (!dommy::numeric::checkedTransformPoint(
                    source->pointArray[point], source->m_mat, transformed)) {
                return MS::kInvalidParameter;
            }
            candidate->pointArray[pointOffset + point] =
                DPoint(transformed[0], transformed[1], transformed[2]);
        }
        for (unsigned int face = 0U; face < source->arrays_->mFaceList.length(); ++face) {
            candidate->arrays_->mFaceList[static_cast<unsigned int>(faceOffset + face)] =
                source->arrays_->mFaceList[face];
        }
        for (unsigned int corner = 0U; corner < source->arrays_->mConnectList.length(); ++corner) {
            candidate->arrays_->mConnectList[static_cast<unsigned int>(connectionOffset + corner)] =
                static_cast<int>(pointOffset +
                    static_cast<std::size_t>(source->arrays_->mConnectList[corner]));
        }
        if (preserveUVs && plan.uvs != 0U) {
            for (unsigned int uv = 0U; uv < source->arrays_->m_u_vals.length(); ++uv) {
                candidate->arrays_->m_u_vals[static_cast<unsigned int>(uvOffset + uv)] =
                    source->arrays_->m_u_vals[uv];
                candidate->arrays_->m_v_vals[static_cast<unsigned int>(uvOffset + uv)] =
                    source->arrays_->m_v_vals[uv];
            }
            for (unsigned int face = 0U; face < source->arrays_->m_uv_count_list.length(); ++face) {
                candidate->arrays_->m_uv_count_list[static_cast<unsigned int>(faceOffset + face)] =
                    source->arrays_->m_uv_count_list[face];
            }
            for (unsigned int corner = 0U;
                 corner < source->arrays_->m_uv_connect_list.length(); ++corner) {
                candidate->arrays_->m_uv_connect_list[
                    static_cast<unsigned int>(connectionOffset + corner)] =
                        static_cast<int>(uvOffset +
                            static_cast<std::size_t>(source->arrays_->m_uv_connect_list[corner]));
            }
        }
        pointOffset += source->pointArray.size();
        faceOffset += source->arrays_->mFaceList.length();
        connectionOffset += source->arrays_->mConnectList.length();
        uvOffset += source->arrays_->m_u_vals.length();
    }
    if (preserveNormals && !candidate->addCompAttr(D_COMP_PER_VTX,
            dommy::mesh::cornerNormalAttribute(), DPVec3fAttr(mergedNormals, DPoint(0, 0, 0))))
        return MS::kFailure;
    if (!candidate->hasValidMeshData()) return MS::kFailure;
    result.swap(candidate);
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

// -----------------------------------------------------------------------------
// Component attributes
const DPAttrList *DMeshInternalData::AttributeListConst(const DComponentType &cType) const{
    switch (cType) {
        case D_COMP_POINT:
            return &mAttributeList;
        case D_COMP_PER_VTX:
            return &mPVtxAttributeList;
        case D_COMP_FACE:
            return &mFaceAttributeList;
        case D_COMP_PRIM:
        case D_COMP_EDGE:
        default:
            return nullptr;
            
    }
}

DPAttrList *DMeshInternalData::AttributeList(const DComponentType &cType) {
    switch (cType) {
        case D_COMP_POINT:
            return &mAttributeList;
        case D_COMP_PER_VTX:
            return &mPVtxAttributeList;
        case D_COMP_FACE:
            return &mFaceAttributeList;
        case D_COMP_EDGE:
        default:
            return nullptr;
    }
}

DStringList DMeshInternalData::getAllCompAttrName(const DComponentType &cType) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes ? GetAllNameAttributeList(*attributes) : DStringList();
}

DStringList DMeshInternalData::getAllCompAttrInfo(const DComponentType &cType) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes ? GetAllAttributeListInfo(*attributes) : DStringList();
}

bool DMeshInternalData::isExistCompAttr(const DComponentType &cType, const std::string& name) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes && IsExistInAttributeList(*attributes, name);
}

bool DMeshInternalData::addCompAttr(const DComponentType &cType, const std::string& name, const DPAttributeData& attr) {
    DPAttrList* attributes = AttributeList(cType);
    if (!attributes || name.empty()) return false;
    try {
        AddAttributeList(*attributes, name, attr);
        return IsExistInAttributeList(*attributes, name);
    } catch (...) {
        return false;
    }
}

bool DMeshInternalData::removeCompAttr(const DComponentType &cType, const std::string& name) {
    DPAttrList* attributes = AttributeList(cType);
    if (!attributes) return false;
    RemoveFromAttributeList(*attributes, name);
    return true;
}

bool DMeshInternalData::removeCompAttrAll(const DComponentType &cType) {
    DPAttrList* attributes = AttributeList(cType);
    if (!attributes) return false;
    attributes->clear();
    return true;
}

DPAttributeData::ConstPtr DMeshInternalData::getCompAttr(const DComponentType &cType, const std::string& name) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes ? ExtractAttributeFromAttributeList(*attributes, name)
                      : DPAttributeData::ConstPtr();
}

DPAttributeData::Ptr DMeshInternalData::getCompAttr(const DComponentType &cType, const std::string& name) {
    DPAttrList* attributes = AttributeList(cType);
    return attributes ? GetAttrPtr(*attributes, name)
                      : DPAttributeData::Ptr();
}


std::string DMeshInternalData::getCompAttrTypeName(const DComponentType &cType,const std::string& name) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes ? GetAttributeTypeName(*attributes, name) : DString();
}

size_t DMeshInternalData::numberOfCompAttr(const DComponentType &cType) const {
    const DPAttrList *attrList = AttributeListConst(cType);
    return attrList ? attrList->size() : 0U;
}



// -----------------------------------------------------------------------------
// Component groups
const DPGrpList *DMeshInternalData::GroupListConst(const DComponentType &cType) const {
    switch (cType) {
        case D_COMP_POINT:
            return &mGrooupList;
        case D_COMP_EDGE:
            return &mEdgeGroupList;
        case D_COMP_FACE:
            return &mFaceGroupList;
        default:
            return nullptr;
            
    }
}
DPGrpList *DMeshInternalData::GroupList(const DComponentType &cType) {
    switch (cType) {
        case D_COMP_POINT:
            return &mGrooupList;
        case D_COMP_EDGE:
            return &mEdgeGroupList;
        case D_COMP_FACE:
            return &mFaceGroupList;
        default:
            return nullptr;
            
    }
}


bool DMeshInternalData::isExistCompGrp(const DComponentType &cType, const std::string& name) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups && IsExistInGroupList(*groups, name);
}

bool DMeshInternalData::addCompGrp(const DComponentType &cType, const std::string& name, const DPGroupData& attr) {
    DPGrpList* groups = GroupList(cType);
    if (!groups || name.empty()) return false;
    try {
        addGroupList(*groups, name, attr);
        return IsExistInGroupList(*groups, name);
    } catch (...) {
        return false;
    }
}

bool DMeshInternalData::removeCompGrp(const DComponentType &cType, const std::string& name) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    removeFromGroup(*groups, name);
    return true;
}

bool DMeshInternalData::combineCompGrp(const DComponentType& cType, const std::string& name,
                                      std::size_t count, const DPIdList& selection,
                                      int action, bool invert) {
    if ((cType == D_COMP_POINT && count != numberOfPoints()) ||
        (cType == D_COMP_FACE && count != static_cast<std::size_t>(numFace()))) return false;
    try {
        return dommy::group::publishCombinedComponentGroup(
            GroupList(cType), name, count, selection, action, invert);
    } catch (...) {
        return false;
    }
}

bool DMeshInternalData::removeCompGrpAll(const DComponentType &cType) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    groups->clear();
    return true;
}

DPGroupData::ConstPtr DMeshInternalData::getCompGrp(const DComponentType &cType, const std::string& name) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups ? ExtractGroupFromGroupList(*groups, name)
                  : DPGroupData::ConstPtr();
}

size_t DMeshInternalData::numberOfCompGroups(const DComponentType &cType) const {
    const DPGrpList *grpList = GroupListConst(cType);
    return grpList ? grpList->size() : 0U;
}

bool DMeshInternalData::deleteCompGroupFromIds(const DComponentType &cType, const DPIdList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    DeleteFromGroupUseIds(*groups, ids);
    return true;
}

bool DMeshInternalData::sortCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    SortFromGroupUseIds(*groups, ids);
    return true;
}

bool DMeshInternalData::remapCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    RemapFromGroupUseIds(*groups, ids);
    return true;
}







DStringList DMeshInternalData::getAllCompGroupName(const DComponentType &cType) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups ? GetAllNameInGroupList(*groups) : DStringList();
}

DStringList DMeshInternalData::getAllCompGroupInfo(const DComponentType &cType) const {
    DStringList groupInfos;
    const DPGrpList* groups = GroupListConst(cType);
    if (groups) GetAllGroupListInfo(*groups, groupInfos);
    return groupInfos;
}




// -----------------------------------------------------------------------------
// DMeshData

const MTypeId DMeshData::id(ID_DM_DATA);
const MString DMeshData::typeName("DMeshData");

void* DMeshData::creator()
{
    return ::dommy::maya::createObject("DMeshData", [&]() {
        return new DMeshData();
    });
}


DMeshData::DMeshData() : MPxData()
{
	meshItemArray = DMeshList();
	mBBox = MBoundingBox();
	mBBoxDirty = false;
}

DMeshData::~DMeshData() = default;


void DMeshData::copy(const MPxData& other)
{
    const DMeshData* source = dynamic_cast<const DMeshData*>(&other);
    if (!source || other.typeId() != DMeshData::id ||
        copyFrom(*source) != MS::kSuccess) {
        MGlobal::displayError(
            "DMeshData::copy failed without changing the destination.");
    }
}

MStatus DMeshData::copyFrom(const DMeshData& rhs) try
{
    if (!rhs.hasValidMeshTopology()) return MS::kInvalidParameter;
    if (this == &rhs) return MS::kSuccess;

    dommy::mesh::MeshMergePlan plan;
    if (!dommy::mesh::appendMeshRangePlan(rhs.meshItemArray.begin(), rhs.meshItemArray.end(), plan) ||
        !dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;
    if (!dommy::mesh::validateRepeatedMeshMetadata(rhs, 1U, plan)) return MS::kInvalidParameter;

    DMeshData candidate;
    candidate.meshItemArray.reserve(rhs.meshItemArray.size());
    for (ConstDMItem mesh : rhs.meshItemArray) {
        candidate.meshItemArray.push_back(mesh->copyNewMesh());
    }
    CopyAttributeToAttribute(candidate.mAttributeList, rhs.mAttributeList);
    CopyGroupToGroup(candidate.mGroupList, rhs.mGroupList);
    if (!candidate.hasValidMeshTopology() ||
        !dommy::mesh::validateRepeatedMeshMetadata(candidate, 1U, plan)) return MS::kFailure;
    candidate.updateBBox();

    meshItemArray.swap(candidate.meshItemArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

MStatus DMeshData::copyRepeatedFrom(const DMeshData& source, std::size_t copies,
                                   const MeshCopyOperation& operation) try
{
    if (!source.hasValidMeshTopology()) return MS::kInvalidParameter;
    dommy::mesh::MeshMergePlan sourcePlan, outputPlan;
    if (!dommy::mesh::appendMeshRangePlan(source.beginMeshes(), source.endMeshes(), sourcePlan) ||
        !dommy::mesh::repeatMeshMergePlan(sourcePlan, copies, outputPlan) ||
        !dommy::mesh::validMeshOperationStorage(outputPlan) ||
        !dommy::mesh::validateRepeatedMeshMetadata(source, copies, outputPlan)) return MS::kInvalidParameter;

    DMeshData candidate;
    candidate.meshItemArray.reserve(outputPlan.meshes);
    // Avoid empty copy-count loops while retaining zero-row schemas/defaults.
    if (!source.meshItemArray.empty()) {
        for (std::size_t copyIndex = 0U; copyIndex < copies; ++copyIndex) {
            std::size_t meshIndex = 0U;
            for (ConstDMItem mesh : source.meshItemArray) {
                DMItem copied = mesh->copyNewMesh();
                if (!copied) return MS::kFailure;
                if (copyIndex != 0U && operation)
                    DOMMY_RETURN_IF_FAILURE(operation(*copied, copyIndex, meshIndex));
                candidate.meshItemArray.push_back(copied);
                ++meshIndex;
            }
        }
    }
    CopyAttributeToAttribute(candidate.mAttributeList, source.mAttributeList);
    CopyGroupToGroup(candidate.mGroupList, source.mGroupList);
    if (!source.meshItemArray.empty()) {
        for (std::size_t copyIndex = 1U; copyIndex < copies; ++copyIndex) {
            for (const auto& field : candidate.mAttributeList)
                if (!appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first))) return MS::kFailure;
            for (const auto& group : candidate.mGroupList)
                group.second->appendOtherGrp(source.mGroupList.at(group.first));
        }
    }
    if (!candidate.hasValidMeshTopology()) return MS::kFailure;
    // The callback is intended for placement/baking, but never trust it to
    // preserve admitted counts or metadata. Recheck the retained candidate.
    dommy::mesh::MeshMergePlan finalPlan;
    if (!dommy::mesh::appendMeshRangePlan(candidate.beginMeshes(), candidate.endMeshes(), finalPlan) ||
        !dommy::mesh::validMeshOperationStorage(finalPlan) ||
        !dommy::mesh::validateRepeatedMeshMetadata(candidate, 1U, finalPlan)) return MS::kInvalidParameter;
    candidate.updateBBox();
    meshItemArray.swap(candidate.meshItemArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

MTypeId DMeshData::typeId() const
{
	return DMeshData::id;
}


MString DMeshData::name() const
{
	return DMeshData::typeName;
}




MStatus DMeshData::readASCII(const MArgList&, unsigned&)
{
	return MS::kFailure;
}


MStatus DMeshData::writeASCII(ostream&)
{
	return MS::kFailure;
}

MStatus DMeshData::readBinary(istream&, unsigned)
{
	return MS::kFailure;
}


MStatus DMeshData::writeBinary(ostream&)
{
	return MS::kFailure;
}


bool DMeshData::getCheckedTotalVertex(size_t &totalVtx) const {
	totalVtx = 0;
    
    for(const auto &msh: meshItemArray) {
        std::size_t updatedTotal = 0;
        if (!msh || !msh->hasValidMeshData() || !dommy::memory::checkedAdd(
                totalVtx, msh->numberOfPoints(), updatedTotal)) {
            totalVtx = 0;
            return false;
        }
        totalVtx = updatedTotal;
    }
    return true;
}

void DMeshData::getTotalVertex(size_t &totalVtx) const {
	getCheckedTotalVertex(totalVtx);
}

size_t DMeshData::numTotalVertex() const {
    size_t totalVtx = 0;
    getCheckedTotalVertex(totalVtx);
    return totalVtx;
}

bool DMeshData::getCheckedTotalFace(size_t &totalFace) const {
	totalFace = 0;
	for(const auto &msh: meshItemArray) {
		std::size_t updatedTotal = 0U;
		if (!msh || !msh->hasValidMeshData() ||
			!dommy::memory::checkedAdd(
				totalFace, static_cast<std::size_t>(msh->numFace()),
				updatedTotal)) {
			totalFace = 0U;
			return false;
		}
		totalFace = updatedTotal;
	}
	return true;
}

void DMeshData::getTotalFace(size_t &totalFace) const {
	getCheckedTotalFace(totalFace);
}

bool DMeshData::getCheckedTotalConnect(size_t &totalConnect) const {
	totalConnect = 0;
	for(const auto &msh: meshItemArray) {
		std::size_t updatedTotal = 0U;
		if (!msh || !msh->hasValidMeshData() ||
			!dommy::memory::checkedAdd(
				totalConnect,
					msh->numberOfConnections(),
				updatedTotal)) {
			totalConnect = 0U;
			return false;
		}
		totalConnect = updatedTotal;
	}
	return true;
}

void DMeshData::getTotalConnect(size_t &totalConnect) const {
	getCheckedTotalConnect(totalConnect);
}



void DMeshData::getTotalVertexForDisplay(size_t &totalvtx) const {
    getCheckedTotalVertexForDisplay(totalvtx);
}

bool DMeshData::getCheckedTotalVertexForDisplay(size_t &totalvtx) const {
    totalvtx = 0;
	
	for(const auto &msh: meshItemArray) {
		if (!msh || !msh->hasValidMeshData()) {
			totalvtx = 0U;
			return false;
		}
		MIntArray faces = msh->getFaceList();
		
		for (size_t i=0; i<faces.length(); ++i) {
			if (faces[i] < 3) {
				totalvtx = 0;
				return false;
			}
			std::size_t triangleVertexCount = 0;
			std::size_t updatedTotal = 0;
			if (!dommy::memory::checkedMultiply(
					static_cast<std::size_t>(faces[i] - 2), std::size_t(3),
					triangleVertexCount) ||
				!dommy::memory::checkedAdd(totalvtx, triangleVertexCount, updatedTotal)) {
				totalvtx = 0;
				return false;
			}
			totalvtx = updatedTotal;
		}
	}
	return true;
}

bool DMeshData::hasValidMeshTopology() const {
    if (meshItemArray.size() >
        dommy::generation::kMaxGeneratedMeshCount) {
        return false;
    }
    std::size_t totalVertices = 0U;
    std::size_t totalFaces = 0U;
    std::size_t totalConnections = 0U;
    // Validate each primitive once and accumulate all bounded totals together.
    // Independent public count queries still validate their own inputs.
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh || !mesh->hasValidMeshData() ||
            !dommy::generation::checkedSum(totalVertices, mesh->numberOfPoints(),
                dommy::generation::kMaxGeneratedPointCount, totalVertices) ||
            !dommy::generation::checkedSum(totalFaces, static_cast<std::size_t>(mesh->numFace()),
                dommy::generation::kMaxGeneratedFaceCount, totalFaces) ||
            !dommy::generation::checkedSum(totalConnections, mesh->numberOfConnections(),
                dommy::generation::kMaxWorkItemCount, totalConnections)) return false;
    }
	dommy::attribute::ValidationBudget budget(meshItemArray.size());
	if (!dommy::attribute::validateAttributeList(
			mAttributeList, meshItemArray.size(), budget)) {
		return false;
	}
	for (ConstDPrimGrpListIter group = mGroupList.begin();
		 group != mGroupList.end(); ++group) {
		if (!group->second ||
			group->second->getGroupIds().size() != meshItemArray.size() ||
			!dommy::generation::checkedSum(
				budget.workItems,
				meshItemArray.size(),
				dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
			return false;
		}
	}
	return true;
}




const DMeshInternalData& DMeshData::getMesh(size_t index) const
{
	ConstDMItem mesh = meshItemArray.at(index);
	if (!mesh) throw std::runtime_error("DMeshData contains a null mesh");
	return *mesh;
}

DMItem DMeshData::getMeshPtr(size_t index) {
	markBBoxDirty();
	DMItem mesh = meshItemArray.at(index);
	if (!mesh) throw std::runtime_error("DMeshData contains a null mesh");
    return mesh;
}

ConstDMItem DMeshData::getMeshPtrConst(size_t index) const {
	ConstDMItem mesh = meshItemArray.at(index);
	if (!mesh) throw std::runtime_error("DMeshData contains a null mesh");
	return mesh;
}

bool DMeshData::appendMeshes(const MeshSourceList& meshes, const DPIndexList* sourceIndices) try {
    if (sourceIndices && sourceIndices->size() != meshes.size()) return false;
    if (meshes.empty()) return true;
    if (!hasValidMeshTopology()) return false;

    dommy::mesh::MeshMergePlan plan;
    for (const DMeshInternalData* mesh : meshes)
        if (!mesh || !mesh->hasValidMeshData()) return false;
    if (!dommy::mesh::appendMeshRangePlan(meshItemArray.begin(), meshItemArray.end(), plan) ||
        !dommy::mesh::appendMeshRangePlan(meshes.begin(), meshes.end(), plan)) return false;

    // Count admission alone does not bound the copied geometry workspace.
    // Reject before copying primitives or growing transactional metadata.
    if (!dommy::mesh::validMeshOperationStorage(plan)) return false;

    dommy::attribute::ValidationBudget budget(plan.workItems);
    std::size_t groupWork = 0U;
    if (!dommy::mesh::addMeshMetadataCost(meshItemArray.begin(), meshItemArray.end(), budget) ||
        !dommy::mesh::addMeshMetadataCost(meshes.begin(), meshes.end(), budget) ||
        !dommy::attribute::validateAttributeExpansion(mAttributeList, meshItemArray.size(),
            meshes.size(), sourceIndices, budget) ||
        !dommy::generation::checkedProduct(mGroupList.size(), plan.meshes,
            dommy::generation::kMaxWorkItemCount, groupWork) ||
        !dommy::generation::checkedSum(budget.workItems, groupWork,
            dommy::generation::kMaxWorkItemCount, budget.workItems)) return false;
    for (const auto& group : mGroupList)
        if (!dommy::generation::checkedSum(budget.stringBytes, group.first.size(),
                dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes)) return false;

    DMeshData candidate;
    candidate.meshItemArray = meshItemArray;
    candidate.meshItemArray.reserve(plan.meshes);
    for (const DMeshInternalData* mesh : meshes)
        candidate.meshItemArray.push_back(mesh->copyNewMesh());
    CopyAttributeToAttribute(candidate.mAttributeList, mAttributeList);
    CopyGroupToGroup(candidate.mGroupList, mGroupList);
    if (sourceIndices) {
        for (auto& attribute : candidate.mAttributeList)
            for (std::size_t index : *sourceIndices)
                attribute.second->incrementIndexElement(index);
        for (auto& group : candidate.mGroupList)
            for (std::size_t index : *sourceIndices)
                group.second->expandOtherGrpFromIndex(index);
    } else {
        for (auto& attribute : candidate.mAttributeList)
            attribute.second->incrementDefaultValue(meshes.size());
        ExpandGroupListSize(candidate.mGroupList, meshes.size());
    }
    if (!candidate.hasValidMeshTopology() ||
        !dommy::mesh::validateRepeatedMeshMetadata(candidate, 1U, plan)) return false;
    candidate.updateBBox();
    meshItemArray.swap(candidate.meshItemArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return true;
} catch (...) {
    return false;
}

bool DMeshData::insert(const DMeshInternalData& data) {
    try {
        const MeshSourceList meshes(1U, &data);
        return appendMeshes(meshes);
    } catch (...) {
        return false;
    }
}

bool DMeshData::insert(const DMItem& mesh) {
    try {
        const MeshSourceList meshes(1U, mesh.get());
        return appendMeshes(meshes);
    } catch (...) {
        return false;
    }
}

bool DMeshData::insert(const ConstDMItem& mesh) {
    try {
        const MeshSourceList meshes(1U, mesh.get());
        return appendMeshes(meshes);
    } catch (...) {
        return false;
    }
}

bool DMeshData::insert(const DMeshList& rhs) {
    try {
        MeshSourceList meshes;
        meshes.reserve(rhs.size());
        for (const DMItem& mesh : rhs) {
            meshes.push_back(mesh.get());
        }
        return appendMeshes(meshes);
    } catch (...) {
        return false;
    }
}

bool DMeshData::expand(const DMItem& data, size_t itemIndex) {
    try {
        return expand(DMeshList(1U, data), DPIndexList(1U, itemIndex));
    } catch (...) {
        return false;
    }
}

bool DMeshData::expand(const DMeshList& meshes, const DPIndexList& sourceIndices) {
    try {
        MeshSourceList sources;
        sources.reserve(meshes.size());
        for (const DMItem& mesh : meshes) sources.push_back(mesh.get());
        return appendMeshes(sources, &sourceIndices);
    } catch (...) {
        return false;
    }
}



bool DMeshData::deleteFromIds(const DPIdList& ids) {
    if (ids.size() != numberOfMesh() ||
        !hasValidMeshTopology()) {
        return false;
    }

    try {
        DMeshData candidate;
        candidate.meshItemArray.reserve(meshItemArray.size());
        for (std::size_t index = 0U;
             index < meshItemArray.size(); ++index) {
            if (!ids[index]) {
                candidate.meshItemArray.push_back(
                    meshItemArray[index]);
            }
        }
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGroupList, mGroupList);
        if (!DDeleteAttributeFromIds(
                candidate.mAttributeList, ids)) {
            return false;
        }
        DeleteFromGroupUseIds(candidate.mGroupList, ids);
        candidate.updateBBox();
        if (!candidate.hasValidMeshTopology()) return false;

        meshItemArray.swap(candidate.meshItemArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGroupList.swap(candidate.mGroupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

// -----------------------------------------------------------------------------
// Merge
MStatus DMeshData::copyMergedFrom(const DMeshData& base, const DMeshData& data) try
{
    if (!base.hasValidMeshTopology() || !data.hasValidMeshTopology())
        return MS::kInvalidParameter;

    // Empty geometry must not strip the other input's metadata or UVs.
    // copyFrom also handles destination/source aliasing without extra copies.
    if (data.meshItemArray.empty()) return copyFrom(base);
    if (base.meshItemArray.empty()) return copyFrom(data);

    dommy::mesh::MeshMergePlan mergedPlan;
    if (!dommy::mesh::appendMeshRangePlan(base.meshItemArray.begin(), base.meshItemArray.end(), mergedPlan) ||
        !dommy::mesh::appendMeshRangePlan(data.meshItemArray.begin(), data.meshItemArray.end(), mergedPlan) ||
        !dommy::mesh::validMeshOperationStorage(mergedPlan)) {
        return MS::kInvalidParameter;
    }

    std::size_t mergedMeshCount = mergedPlan.meshes;
    dommy::attribute::ValidationBudget budget(mergedPlan.workItems);
    if (!dommy::mesh::addMeshMetadataCost(base.beginMeshes(), base.endMeshes(), budget) ||
        !dommy::mesh::addMeshMetadataCost(data.beginMeshes(), data.endMeshes(), budget))
        return MS::kInvalidParameter;
    if (!dommy::attribute::validateAttributeMerge(
            base.mAttributeList, base.meshItemArray.size(),
            data.mAttributeList, data.meshItemArray.size(), budget))
        return MS::kInvalidParameter;
    for (const auto& group : base.mGroupList) {
        if (data.mGroupList.find(group.first) != data.mGroupList.end() &&
            (!dommy::generation::checkedSum(budget.workItems, mergedMeshCount,
                dommy::generation::kMaxWorkItemCount, budget.workItems) ||
             !dommy::generation::checkedSum(budget.stringBytes, group.first.size(),
                dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes)))
            return MS::kInvalidParameter;
    }

    struct ComponentSchema {
        std::map<std::string, DP_ATTR_DATA_TYPE> attributes;
        std::set<std::string> pointGroups, faceGroups, edgeGroups;
    };
    const auto collectSchema = [](const DMeshData& source) {
        ComponentSchema schema;
        const auto collectGroups = [](ConstDPGrpIter begin, ConstDPGrpIter end,
                                      std::set<std::string>& names) {
            for (; begin != end; ++begin) names.insert(begin->first);
        };
        for (ConstDMItem mesh : source.meshItemArray) {
            for (auto field = mesh->beginAttr(); field != mesh->endAttr(); ++field)
                // emplace deliberately keeps the first-present type.
                schema.attributes.emplace(field->first, field->second->getAttrType());
            collectGroups(mesh->beginGrp(), mesh->endGrp(), schema.pointGroups);
            collectGroups(mesh->beginFaceGrp(), mesh->endFaceGrp(), schema.faceGroups);
            collectGroups(mesh->beginEdgeGrp(), mesh->endEdgeGrp(), schema.edgeGroups);
        }
        return schema;
    };
    const ComponentSchema baseSchema = collectSchema(base), targetSchema = collectSchema(data);
    const auto commonNames = [](const std::set<std::string>& source,
                                const std::set<std::string>& target) {
        std::set<std::string> common;
        std::set_intersection(source.begin(), source.end(), target.begin(), target.end(),
                              std::inserter(common, common.end()));
        return common;
    };
    std::set<std::string> pointAttributes;
    for (const auto& field : baseSchema.attributes) {
        const auto other = targetSchema.attributes.find(field.first);
        if (other != targetSchema.attributes.end() && field.second == other->second)
            pointAttributes.insert(field.first);
    }
    // Preserve the historical per-collection union of names, intersected across
    // inputs. A field need not exist on every primitive in either collection.
    const auto pointGroups = commonNames(baseSchema.pointGroups, targetSchema.pointGroups);
    const auto faceGroups = commonNames(baseSchema.faceGroups, targetSchema.faceGroups);
    const auto edgeGroups = commonNames(baseSchema.edgeGroups, targetSchema.edgeGroups);
    const bool removeUV = base.hasUVs() != data.hasUVs();

    const auto filterComponents = [&](DMeshInternalData& mesh) {
        for (auto field = mesh.beginAttr(); field != mesh.endAttr();) {
            const std::string name = (field++)->first;
            if (pointAttributes.find(name) == pointAttributes.end()) mesh.removeAttr(name);
        }
        const auto filterGroups = [&](DPGrpIter begin, DPGrpIter end,
                                      const std::set<std::string>& keep, DComponentType component) {
            for (auto group = begin; group != end;) {
                const std::string name = (group++)->first;
                if (keep.find(name) == keep.end()) mesh.removeCompGrp(component, name);
            }
        };
        filterGroups(mesh.beginGrp(), mesh.endGrp(), pointGroups, D_COMP_POINT);
        filterGroups(mesh.beginFaceGrp(), mesh.endFaceGrp(), faceGroups, D_COMP_FACE);
        filterGroups(mesh.beginEdgeGrp(), mesh.endEdgeGrp(), edgeGroups, D_COMP_EDGE);
        // Face and corner attributes, including normals, retain their schemas.
        if (removeUV) mesh.clearUVs();
    };

    DMeshData candidate;
    candidate.meshItemArray.reserve(mergedMeshCount);
    for (const DMeshData* source : {&base, &data}) {
        for (ConstDMItem mesh : source->meshItemArray) {
            DMItem copied = mesh->copyNewMesh();
            if (!copied) return MS::kFailure;
            filterComponents(*copied);
            candidate.meshItemArray.push_back(copied);
        }
    }

    // Clone only retained primitive fields; shared_ptr copies in these scoped
    // schema views do not copy payloads or transfer ownership from either input.
    DPrimAttrList retainedAttributes;
    for (const auto& field : base.mAttributeList)
        if (data.mAttributeList.find(field.first) != data.mAttributeList.end())
            retainedAttributes.emplace(field.first, field.second);
    CopyAttributeToAttribute(candidate.mAttributeList, retainedAttributes);
    for (const auto& field : candidate.mAttributeList)
        if (!appendOtherDPAttribute(field.second, data.mAttributeList.at(field.first)))
            return MS::kFailure;
    if (retainedAttributes.size() != base.mAttributeList.size() ||
        retainedAttributes.size() != data.mAttributeList.size())
        MGlobal::displayWarning("Mesh merge retains only primitive attributes shared by both inputs.");

    DPrimGrpList retainedGroups;
    for (const auto& group : base.mGroupList)
        if (data.mGroupList.find(group.first) != data.mGroupList.end())
            retainedGroups.emplace(group.first, group.second);
    CopyGroupToGroup(candidate.mGroupList, retainedGroups);
    MergeGroupToGroup(candidate.mGroupList, data.mGroupList);
    if (retainedGroups.size() != base.mGroupList.size())
        MGlobal::displayWarning("Mesh merge removed primitive groups absent from the target input.");

    if (!candidate.hasValidMeshTopology() ||
        !dommy::mesh::validateRepeatedMeshMetadata(candidate, 1U, mergedPlan)) return MS::kFailure;
    candidate.updateBBox();
    meshItemArray.swap(candidate.meshItemArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
}
catch (const std::exception& error) {
    MGlobal::displayError(MString("DMeshData::copyMergedFrom failed without changing the destination: ") + error.what());
    return MS::kFailure;
}
catch (...) {
    MGlobal::displayError("DMeshData::copyMergedFrom failed without changing the destination.");
    return MS::kFailure;
}

bool DMeshData::mergeDMeshData(const DMeshData& data)
{
    return static_cast<bool>(copyMergedFrom(*this, data));
}



void DMeshData::clear() {
	meshItemArray.clear();
	meshItemArray.shrink_to_fit();
	mAttributeList.clear();
	mGroupList.clear();
	mBBox = MBoundingBox();
	mBBoxDirty = false;
}


// -----------------------------------------------------------------------------
// UV data
bool DMeshData::hasUVs() const {
	if (meshItemArray.empty()) return false;
    for (ConstDMItem mesh : meshItemArray) {
		if (!mesh || !mesh->hasValidUVTopology() || !mesh->hasUV()) {
			return false;
		}
	}
	return true;
}


// -----------------------------------------------------------------------------
// Bounding box
MBoundingBox DMeshData::calculateBoundingBox() const
{
	MBoundingBox bounds;
		
    for (ConstDMItem mesh : meshItemArray) {
		if (mesh && mesh->hasValidMeshData()) {
			bounds.expand(mesh->getBBox());
		}
	}
    return bounds;
}

void DMeshData::updateBBox()
{
	mBBox = calculateBoundingBox();
	mBBoxDirty = false;
}


MBoundingBox DMeshData::getBoundingBox() const
{
	return mBBoxDirty ? calculateBoundingBox() : mBBox;
}

// -----------------------------------------------------------------------------
// Centers

void DMeshData::getPrimitiveCenterList(DPointList &centerList) const
{
    DPointList centers;
    centers.reserve(numberOfMesh());
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh || !mesh->hasValidMeshData()) {
            centerList.clear();
            return;
        }
        centers.push_back(mesh->getCenter());
    }
    centerList.swap(centers);
}

void DMeshData::getPrimitiveCenterList(MVectorArray &centerList) const
{
    if (!hasValidMeshTopology())
        throw std::invalid_argument("Invalid mesh data for primitive centers");
    const MStatus status = dommy::maya::buildCheckedArray(meshItemArray.size(),
        [&](unsigned int index, MVector& value) -> MStatus {
            value = meshItemArray[index]->getCenterDouble();
            return MS::kSuccess;
        }, centerList);
    if (!status) throw std::runtime_error("Unable to construct mesh center array");
}

// -----------------------------------------------------------------------------
// Attributes

bool DMeshData::isExistAttr(const std::string& name) const {
    
    return IsExistInAttributeList(mAttributeList, name);
	
}


void DMeshData::addAttr(const std::string& name, const DPAttributeData& attr)
{
    AddAttributeList(mAttributeList, name, attr);
	
}


void DMeshData::removeAttr(const std::string& name) {
    RemoveFromAttributeList(mAttributeList, name);
	
}


DPAttributeData::ConstPtr DMeshData::getAttr(const std::string& name) const {
	
    return ExtractAttributeFromAttributeList(mAttributeList, name);
	
	
}

DPAttributeData::Ptr DMeshData::getAttrPtr(
        const std::string& name) {
    return GetAttrPtr(mAttributeList, name);
}

DPAttributeData::ConstPtr DMeshData::getAttrPtr(
        const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
}
std::string DMeshData::getAttributeType(const std::string& name) const {
    return GetAttributeTypeName(mAttributeList, name);
}



// Add attributes
template<typename T>
void DMeshData::addDPAttributeValues(
        const std::string& name, const std::vector<T>& values) {
    (void)name;
    (void)values;
    static_assert(
        !std::is_same<T, T>::value,
        "DMeshData::addDPAttributeValues does not support this type");
}

template<>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<bool> &values) {
    if (values.size() == numberOfMesh()) {
         addAttr(name, DPBoolAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<int32_t> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPIntAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<uint32_t> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPUInt32Attr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<int64_t> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPInt64Attr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<float> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPFloatAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<double> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPDoubleAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V2i> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec2iAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V2f> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec2fAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V2d> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec2dAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V3i> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec3iAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V3f> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec3fAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<Imath::V3d> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPVec3dAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<std::string> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPStringAttr(values));
    }
}

template <>
void DMeshData::addDPAttributeValues(const std::string& name, const std::vector<MMatrix> &values) {
    if (values.size() == numberOfMesh()) {
        addAttr(name, DPMat4Attr(values));
    }
}





//set
void DMeshData::setDPAttributeValues(const std::string& name, const DScalarDataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            replaced = replaceConvertedAttributeValues<int32_t>(
                *this, name, values);
            break;
        case DP_ATTR_UINT32:
            replaced = replaceConvertedAttributeValues<uint32_t>(
                *this, name, values);
            break;
        case DP_ATTR_INT64:
            replaced = replaceConvertedAttributeValues<int64_t>(
                *this, name, values);
            break;
        case DP_ATTR_FLOAT:
            replaced = replaceConvertedAttributeValues<float>(
                *this, name, values);
            break;
        case DP_ATTR_DOUBLE:
            replaced = replaceConvertedAttributeValues<double>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues requires a matching scalar attribute");
    }
}


void DMeshData::setDPAttributeValues(const std::string& name, const DVec2DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC2I:
            replaced = replaceConvertedAttributeValues<Imath::V2i>(
                *this, name, values);
            break;
        case DP_ATTR_VEC2F:
            replaced = replaceConvertedAttributeValues<Imath::V2f>(
                *this, name, values);
            break;
        case DP_ATTR_VEC2D:
            replaced = replaceConvertedAttributeValues<Imath::V2d>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues requires a matching vec2 attribute");
    }
}


void DMeshData::setDPAttributeValues(const std::string& name, const DVec3DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC3I:
            replaced = replaceConvertedAttributeValues<Imath::V3i>(
                *this, name, values);
            break;
        case DP_ATTR_VEC3F:
            replaced = replaceConvertedAttributeValues<Imath::V3f>(
                *this, name, values);
            break;
        case DP_ATTR_VEC3D:
            replaced = replaceConvertedAttributeValues<Imath::V3d>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DMeshData::setDPAttributeValues requires a matching vec3 attribute");
    }
}




size_t DMeshData::numberOfAttributes() const {
	
	return mAttributeList.size();
}


void DMeshData::extendAttributeFromCount(const size_t &count) {
    //attribute add default value
    
    for (auto aIt=beginAttr(); aIt!=endAttr(); ++aIt) {
        aIt->second->incrementDefaultValue( count );
    }
}

void DMeshData::extendAttributeFromIndex(const size_t &index) {
    for (auto aIt=beginAttr(); aIt!=endAttr(); ++aIt) {
        aIt->second->incrementIndexElement(index);
    }
}


// point attribute method
size_t DMeshData::numberOfPointAttribute(bool detail) const {
	DStringList attributeList;

	for (const auto& mesh : meshItemArray) {
		if (!mesh) {
			continue;
		}
		IntersectDStringList(attributeList, mesh->getAllAttributeName());
		if (!detail) {
			break;
		}
	}

	return attributeList.size();
}

DStringList DMeshData::getPointAttributeInfo(bool detail) const {
	DStringList attributeInfo;

	for (const auto& mesh : meshItemArray) {
		if (!mesh) {
			continue;
		}
		IntersectDStringList(attributeInfo, mesh->getAllAttributeInfo());
		if (!detail) {
			break;
		}
	}

	return attributeInfo;
}

bool DMeshData::isExistPointAttr(const std::string& name) const {
	for (const auto& mesh : meshItemArray) {
		if (mesh && mesh->isExistAttr(name)) {
			return true;
		}
	}
	
	return false;
}

DString DMeshData::getPointAttributeTypeStr(const std::string& name) const {
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh) {
            continue;
        }
        const DPAttributeData::ConstPtr attribute = mesh->getAttr(name);
        if (attribute) {
            return attribute->typeName();
        }
	}
		
	return "";
}


DP_ATTR_DATA_TYPE DMeshData::getPointAttributeType(const std::string& name) const {
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh) {
            continue;
        }
        const DPAttributeData::ConstPtr attribute = mesh->getAttr(name);
        if (attribute) {
            return attribute->getAttrType();
        }
	}
		
	return DP_ATTR_INVALID;
}

DStringList DMeshData::getPrimAttributeList() const {
    DStringList attributeNames;
    for(auto itr:mAttributeList) {
        attributeNames.push_back(itr.first);
    }
    return attributeNames;
}

DStringList DMeshData::getPointAttributeList() const {
    DStringList allPointAttributeList;
    std::set<std::string> seen;
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh) {
            return DStringList();
        }
        for (auto field = mesh->beginAttr(); field != mesh->endAttr(); ++field)
            if (seen.insert(field->first).second) allPointAttributeList.push_back(field->first);
    }
    return allPointAttributeList;
}




// Component Attribute method

bool DMeshData::isExistCompAttr(const DComponentType &cType, const std::string& name) const {
    return !meshItemArray.empty() && meshItemArray.front() &&
           meshItemArray.front()->isExistCompAttr(cType, name);
}

size_t DMeshData::numberOfCompAttr(const DComponentType &cType) const {
    DStringList attrList;
    if (meshItemArray.empty() || !meshItemArray.front()) return 0U;

    DStringList attrNames =
        meshItemArray.front()->getAllCompAttrName(cType);
    IntersectDStringList(attrList, attrNames);
    
    return attrList.size();
}

DStringList DMeshData::getCompAttrInfo(const DComponentType &cType) const {
    DStringList attrInfo;
    if (meshItemArray.empty() || !meshItemArray.front()) return attrInfo;

    DStringList infos =
        meshItemArray.front()->getAllCompAttrInfo(cType);
    IntersectDStringList(attrInfo, infos);
    
    return attrInfo;
}

DStringList DMeshData::getAllCompAttrName(const DComponentType &cType) const {
    
    DStringList allCompAttrList;
    
    for (auto mIt:meshItemArray) {
        if (!mIt) continue;
        //check point attribute
        
        DStringList attrList = mIt->getAllCompAttrName(cType);
        IntersectDStringList(allCompAttrList, attrList);
        
    }
    
    return allCompAttrList;
    
}

DStringList DMeshData::getAllAttributeList() const {
    
    DStringList allCompAttrList;
    
    DStringList pattrList = getPrimAttributeList();
    
    IntersectDStringList(allCompAttrList, pattrList);
    
    for (auto mIt:meshItemArray) {
        if (!mIt) continue;
        
        DStringList attrList = mIt->getAllCompAttrName(D_COMP_POINT);
        IntersectDStringList(allCompAttrList, attrList);
        
        attrList = mIt->getAllCompAttrName(D_COMP_FACE);
        IntersectDStringList(allCompAttrList, attrList);
        
        attrList = mIt->getAllCompAttrName(D_COMP_PER_VTX);
        IntersectDStringList(allCompAttrList, attrList);
        
    }
    
    return allCompAttrList;
    
}


DComponentType DMeshData::getAttributeComponentType(const std::string& name) const {
    
    if (numberOfMesh()==0)
        return D_COMP_COUNT;
    
    
    //primitive
    if(isContainDStringList(getPrimAttributeList(), name)) {
        return D_COMP_PRIM;
    }
    
    //Component
    DMItem mitem = meshItemArray[0];
    
    if(isContainDStringList(mitem->getAllCompAttrName(D_COMP_POINT), name)) {
        return D_COMP_POINT;
    }

    if(isContainDStringList(mitem->getAllCompAttrName(D_COMP_FACE), name)) {
        return D_COMP_FACE;
    }
    
    if(isContainDStringList(mitem->getAllCompAttrName(D_COMP_PER_VTX), name)) {
        return D_COMP_PER_VTX;
    }
    
    return D_COMP_COUNT;
}



// group method
bool DMeshData::isExistGrp(const std::string& name) const {
    
    return IsExistInGroupList(mGroupList, name);
}

void DMeshData::addGrp(const std::string& name, const DPGroupData& attr) {
    
    addGroupList(mGroupList, name, attr);
	
}

void DMeshData::removeGrp(const std::string& name) {
    
    removeFromGroup(mGroupList, name);
	
}

DPGroupData::ConstPtr DMeshData::getGrp(const std::string& name) const {
    
    return ExtractGroupFromGroupList(mGroupList, name);
	
}

DPGroupData::Ptr DMeshData::getGrpPtr(const std::string& name) {
    auto iter = mGroupList.find(name);
    return (iter == mGroupList.end() ? DPGroupData::Ptr() : iter->second);
}

DPGroupData::ConstPtr DMeshData::getGrpPtr(
        const std::string& name) const {
    auto iter = mGroupList.find(name);
    return (iter == mGroupList.end()
        ? DPGroupData::ConstPtr() : iter->second);
}
size_t DMeshData::numberOfGroups() const {
	return mGroupList.size();
}


void DMeshData::deleteGroupFromIds(const DPIdList &ids) {
	
    DeleteFromGroupUseIds(mGroupList, ids);
	
}

void DMeshData::sortGroupFromIds(const DPIndexList &ids) {
	
    SortFromGroupUseIds(mGroupList, ids);
	
	
}

void DMeshData::extendGroupFromCount(const size_t &count) {
    
    ExpandGroupListSize(mGroupList, count);
    
}

void DMeshData::extendGroupFromIndex(const size_t &index) {
    for (auto iter = mGroupList.begin(); iter!=mGroupList.end(); iter++) {
        iter->second->expandOtherGrpFromIndex(index);
    }
}

bool DMeshData::isExistCompGrp(const DComponentType &cType, const std::string& name) const {
	if (meshItemArray.empty() || !meshItemArray.front()) {
		return false;
	}
	return meshItemArray.front()->isExistCompGrp(cType, name);
}



size_t DMeshData::numberOfCompGroup(const DComponentType &cType) const {
    DStringList groupList;
	if (meshItemArray.empty() || !meshItemArray.front()) {
		return 0U;
	}

	IntersectDStringList(
		groupList, meshItemArray.front()->getAllCompGroupName(cType));

    
    return groupList.size();
}





DStringList DMeshData::getCompGroupInfo(const DComponentType &cType) const {
    DStringList groupInfo;
	if (meshItemArray.empty() || !meshItemArray.front()) {
		return groupInfo;
	}

    IntersectDStringList(
		groupInfo, meshItemArray.front()->getAllCompGroupInfo(cType));

    return groupInfo;
    
}

DStringList DMeshData::getAllCompGroupName(const DComponentType &cType) const {
    DStringList allCompGrpList;
    std::set<std::string> seen;
    for (ConstDMItem mesh : meshItemArray) {
        if (!mesh) continue;
        const DStringList groups = mesh->getAllCompGroupName(cType);
        for (const auto& name : groups)
            if (seen.insert(name).second) allCompGrpList.push_back(name);
    }
    return allCompGrpList;
}


/**
 Return primitive group name string list.
 
 @param
 @return primitive group name list.
 */
DStringList DMeshData::getPrimGroupList() const {
    DStringList groupNames;
    for(auto iter : mGroupList) {
        groupNames.push_back(iter.first);
    }
    return groupNames;
}

/**
 Return All group name string list.
 
 @param
 @return all group name list.
 */
DStringList DMeshData::getAllGroupList() const {
    
    DStringList allCompGrpList;
    
    DStringList pgrpList = getPrimGroupList();
    
    IntersectDStringList(allCompGrpList, pgrpList);
    
    for (auto mIt:meshItemArray) {
        if (!mIt) continue;
        
        DStringList grpList = mIt->getAllCompGroupName(D_COMP_POINT);
        IntersectDStringList(allCompGrpList, grpList);
        
        grpList = mIt->getAllCompGroupName(D_COMP_EDGE);
        IntersectDStringList(allCompGrpList, grpList);
        
        grpList = mIt->getAllCompGroupName(D_COMP_FACE);
        IntersectDStringList(allCompGrpList, grpList);
        
    }
    
    return allCompGrpList;
}


/**
 Return component type that contain group list for given name
 
 @param group name.
 @return component type.
 */
DComponentType DMeshData::getGroupComponentType(const std::string& name) const {
    if (numberOfMesh()==0)
        return D_COMP_COUNT;
    
    
    //primitive
    if(isContainDStringList(getPrimGroupList(), name)) {
        return D_COMP_PRIM;
    }
    
    //Component
    DMItem mitem = meshItemArray[0];
    
    if(isContainDStringList(mitem->getAllCompGroupName(D_COMP_POINT), name)) {
        return D_COMP_POINT;
    }
    
    if(isContainDStringList(mitem->getAllCompGroupName(D_COMP_EDGE), name)) {
        return D_COMP_EDGE;
    }
    
    if(isContainDStringList(mitem->getAllCompGroupName(D_COMP_FACE), name)) {
        return D_COMP_FACE;
    }
    
    
    return D_COMP_COUNT;
}

//
//  DPolygonUtil.hpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//

#pragma once


#include "DMayaHeaders.h"
#include "common/utility/DMayaCheckedArray.h"

#include <cstdint>
#include <vector>
#include <list>
#include <map>

class TriangleIndices {
	public :
	int v1;
	int v2;
	int v3;
	
	TriangleIndices(int v1, int v2, int v3) {
		this->v1 = v1;
		this->v2 = v2;
		this->v3 = v3;
	}
};


class DMeshSphere {
public :
	explicit DMeshSphere(int frequency);
	
	MObject meshObj;
	
private:
	
	int addVertex(MFloatPoint p);
	int getMiddlePoint(int p1, int p2);
	
	std::map<std::uint64_t, int> middlePointIndexCache;
	
	dommy::maya::CheckedArray<MFloatPointArray> baseVertexArray;
	dommy::maya::CheckedArray<MIntArray> basePolyCountArray;
	dommy::maya::CheckedArray<MIntArray> basePolyConnectArray;
};


// Mesh data must already share a coordinate system. The selection is committed
// only on success; target meshes are caller-owned staging objects and may have
// been modified by a failed native collapse/Boolean operation.
MStatus DPolyIsIntersectMeshCheck(MObject srcMeshObj,
							   std::vector<MObject> &targetObjects,
							   std::vector<bool> &isCheckedArray,
							   bool isCheckCell=false,
							   double tolerance=0.01);


class meshPrimData
{
public:
	meshPrimData() = default;
	
    MStatus updatePoints();
    // Geometry-only copy into a distinct staging object. Source topology must
    // already be valid. Discard the destination after any failed native copy.
	MStatus duplicateTransform(
		const MTransformationMatrix& mat, meshPrimData& other) const;
    // Maya's API takes non-const references to these input arrays. Callers own
    // the arrays; no implicit, statusless copies are made at this boundary.
    MStatus setNormalPerVtxPerFace(MVectorArray& normalArray,
        MIntArray& normalfaceIds, MIntArray& normalfvtxIds);
    
    
    MObject m_obj;
    bool hasExplicitNormals = false;
    
	MPointArray m_pts;
	MIntArray m_faces;
	MIntArray m_connects;
	
	//group
	std::vector<bool> surfaceGroup;
	std::vector<bool> interiorGroup;
	std::vector<bool> seamGroup;
	std::vector<bool> seamPointGroup;
};


// Read into caller-owned staging storage. Native array-copy failure can leave
// a partial destination; every caller must discard it when status is failure.
// m_obj is not retained here: callers set the owning mesh-data object on success.
MStatus getPrimDataFromMesh(MFnMesh& srcMesh, meshPrimData& primData);

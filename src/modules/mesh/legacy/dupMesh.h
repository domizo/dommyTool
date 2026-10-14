#ifndef DUPMESH_H
#define DUPMESH_H

#include  <maya/MFloatPointArray.h>
#include  <maya/MIntArray.h>
#include  <maya/MFloatVectorArray.h>
#include  <maya/MFloatArray.h>
#include <memory>
#include "common/utility/DMeshMergePlan.h"


#include "DMeshDataHeaders.h"



class dupMesh {
public:
	// Scalar-only admission, shared by the constructor and batch callers.
	struct ImportPlan {
		dommy::mesh::MeshMergePlan geometry;
		std::size_t normals = 0U, edges = 0U, extraBytes = 0U;
	};
	static MStatus preflight(MObject object, ImportPlan& output);
	bool matches(const ImportPlan& plan) const;
	
	explicit dupMesh(MObject obj);
	explicit dupMesh(const DMItem& src);
	~dupMesh() = default;
	
	
	
	bool isValid() const { return storage_->valid; }
	unsigned int getTotalVertexNum() const { return storage_->totalVertexNum; }
	unsigned int getTotalPolygonNum() const { return storage_->totalPolygonNum; }
	unsigned int getTotalConnectionNum() const { return storage_->polyConnectList.length(); }
	
	const MFloatPointArray& getVertexArray() const { return storage_->vertexArray; }
	const MIntArray& getPolyCountsList() const { return storage_->polyCountsList; }
	const MIntArray& getPolyConnectList() const { return storage_->polyConnectList; }
	
	const MFloatVectorArray& getNormalArray() const { return storage_->normalArray; }
	const MIntArray& getNormIdsArray() const { return storage_->normIdsArray; }
	unsigned int getNormalNum() const { return storage_->totalNormal; }
	
	const MFloatArray& getUCoords() const { return storage_->uCoords; }
	const MFloatArray& getVCoords() const { return storage_->vCoords; }
	const MIntArray& getUVCountsArray() const { return storage_->uvCountsArray; }
	const MIntArray& getUVIdsArray() const { return storage_->uvIdsArray; }
	unsigned int getTotalUVNum() const { return storage_->uCoords.length(); }
	bool hasUVData() const {
		return storage_->uCoords.length() > 0U &&
			storage_->uCoords.length() == storage_->vCoords.length() &&
			storage_->uvCountsArray.length() == storage_->totalPolygonNum &&
			storage_->uvIdsArray.length() == storage_->polyConnectList.length();
	}
	
	unsigned int getTotalEdgeNum() const { return storage_->totalEdgeNum; }
	bool getEdgeMod() const { return false; }
	bool getEdgeAllSmooth() const { return storage_->allSameSmooth; }
	const MIntArray& getEdgeArray() const { return storage_->edgeArray; }
	int getSmoothType() const { return storage_->smoothType; }
	
private:
	// Immutable after construction. Copying a dupMesh shares its validated
	// snapshot, so vector growth cannot silently fail a Maya-array copy.
	struct Storage {
		bool valid = false;
		unsigned int totalVertexNum = 0U;
		unsigned int totalPolygonNum = 0U;
		unsigned int totalNormal = 0U;
		MFloatPointArray vertexArray;
		MIntArray polyCountsList, polyConnectList;
		MFloatVectorArray normalArray;
		MIntArray normIdsArray, normCountsArray;
		MFloatArray uCoords, vCoords;
		MIntArray uvCountsArray, uvIdsArray;
		unsigned int totalEdgeNum = 0U;
		MIntArray edgeArray;
		int smoothType = 0;
		bool allSameSmooth = true;
	};
	std::shared_ptr<Storage> storage_ = std::make_shared<Storage>();
};

#endif

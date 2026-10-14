//
//  DOpenVDBConvert.hpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//

#pragma once




#include <maya/MPxNode.h>

#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

#include "DPolygonUtil.h"

#include <list>
#include <vector>
namespace dommy { namespace vdb { struct MeshingBudget; } }

class DOpenVDBConvert final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	template <class GridType>
	MStatus referenceMeshing(const std::list<openvdb::GridBase::ConstPtr>& grids,
						  openvdb::tools::VolumeToMesh& mesher,
						  bool computeNormals,
						  MDataBlock& data,
						  std::vector<meshPrimData> &primDataList,
                          dommy::vdb::MeshingBudget& budget);
	
	static MObject aVdbInput;
	
	static MObject aVdbAllGridNames;
	static MObject aVdbSelectedGridNames;
	
	static MObject aReferenceMesh;
	
	
	static MObject aIsovalue;
	static MObject aAdaptivity;
	
	static MObject aComputeNormals;
	static MObject aIsCheckCorner;
	static MObject aConerAngle;
	static MObject aConcernAllPartsNormal;
    static MObject aIsCheckPartsDistance;
    static MObject aCheckTolerance;
    
    
	static MObject aInternalAdaptivity;
	static MObject aTransferAttributes;
	static MObject aSharpenFeatures;
	static MObject aEdgeTolerance;
	
	
	//color set
	static MObject aExportColorSet;
	
	//metadata
	static MObject aExportMetadata;
	
	//group name
	static MObject aSurfaceGroupName;
	static MObject aInteriorGroupName;
	static MObject aSeamGroupName;
	static MObject aSeamPointGroupName;
	
	static MObject outDM;
	
};

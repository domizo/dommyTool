//
//  DOpenVDBResample.hpp
//  dommyTool
//
//  Created by yuzo on 4/20/16.
//
//

#ifndef DOpenVDBResample_h
#define DOpenVDBResample_h




#include <maya/MPxNode.h>


class DOpenVDBResample final : public MPxNode
{
public:
	enum {
		MODE_PARMS = 0,
		MODE_REF_GRID,
		MODE_VOXEL_SIZE,
		MODE_VOXEL_SCALE
	};
	

	DOpenVDBResample() {}
	~DOpenVDBResample() override = default;
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
    
    static MObject aVdbSelectedGridNames;
	static MObject aVdbInput;
    
    static MObject referenceName;
    static MObject referenceVdbInput;
    static MObject referenceMatrix;
    
	static MObject aVdbOutput;
	
	
	static MObject interpolationMode;
	static MObject defineMode;
	static MObject voxelSize;
	static MObject voxelScale;
    static MObject transformVec;
	static MObject isRebuild;
	static MObject isPrune;
	static MObject tolerance;
	
	
};



#endif /* DOpenVDBResample_h */

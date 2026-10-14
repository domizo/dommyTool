//
//  DOpenVDBSamplingDP.hpp
//  dommyTool
//
//  Created by yuzo on 12/6/16.
//
//

#ifndef DOpenVDBSamplingDP_hpp
#define DOpenVDBSamplingDP_hpp


#include <maya/MPxNode.h>


class DOpenVDBSamplingDP final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aVoxelSize;
	
	static MObject aVdbOutput;
	
	
};

#endif /* DOpenVDBSamplingDP_hpp */

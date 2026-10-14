//
//  DReadOpenVDB.hpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#ifndef DReadOpenVDB_h
#define DReadOpenVDB_h

#include <maya/MPxNode.h>

class DReadOpenVDB final : public MPxNode
{
public:
	DReadOpenVDB() {}
	~DReadOpenVDB() override {}
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	static const MTypeId typeId;
	static  void *creator();
	static  MStatus initialize();
	
	
	static MObject offsetTime;
	static MObject aTime;
	
	static MObject aVdbFilePath;
	static MObject aVdbOutput;
	
};



#endif /* DReadOpenVDB_h */

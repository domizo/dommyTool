//
//  DWriteOpenVDB.hpp
//  dommyTool
//
//  Created by yuzo on 4/17/16.
//
//

#ifndef DWriteOpenVDB_h
#define DWriteOpenVDB_h



#include <string>
#include <set>


#include <maya/MPxNode.h>

class DWriteOpenVDB final : public MPxNode
{
public:
	
	DWriteOpenVDB() {}
	~DWriteOpenVDB() override {}
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	SchedulingType schedulingType() const override { return kGloballySerialize; }
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
    
    
    static MObject aVdbInput;
	
	static MObject aVdbFilePath;
	static MObject aWriteFile;
    static MObject aFrameNumbering;
	static MObject aCompression;
    static MObject aInputTime;
    
    static MObject aNodeInfo;
    
	static MObject aVdbOutput;
	
private:
	
	typedef std::set<std::string> StringSet;
	void reportFloatPrecisionConflicts(const StringSet& conflicts);
	
	
	
};





#endif /* DWriteOpenVDB_h */

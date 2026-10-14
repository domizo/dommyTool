//
//  DPWriteToAi.hpp
//  dommyTool
//
//  Created by yuzo on 11/20/16.
//
//

#ifndef DPWriteToAi_h
#define DPWriteToAi_h

#include <maya/MPxNode.h>

#include <string>

#include "DPData.h"

class DPWriteToAi final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	SchedulingType schedulingType() const override { return kGloballySerialize; }
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	
	MStatus exportDPointToArnold(
		MDataBlock& data,
		const DPData* inputDPData,
		const std::string& filename);
	
	
	
	//attribute
	static MObject inDP;
	
	static MObject aFilePath;
	static MObject aExportMode;
	static MObject aExportLog;
	
	
	static MObject aRenderType;
	static MObject aConstantRadius;
	static MObject aRadiusAttributeName;
	
	static MObject aOutputAttributes;
	
	
	static MObject outDP;
	
};


#endif /* DPWriteToAi_h */

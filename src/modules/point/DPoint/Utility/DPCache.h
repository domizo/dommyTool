//
//  DPCache.hpp
//  dommyTool
//
//  Created by yuzo on 5/26/17.
//
//




#pragma once

#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

//DPointのキャッシュを作成
class DPCache final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	SchedulingType schedulingType() const override { return kGloballySerialize; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	static MObject inDP;
	
	static MObject aEnableCache;
	static MObject aCreateMissingCache;
	static MObject aCachePath;
	
	
	static MObject aAttributeNames;
	
	
	static MObject aCurrentTime;
	
	static MObject outDP;
	
};

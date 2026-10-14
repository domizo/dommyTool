//
//  DPAttributeTransfer.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/20.
//
//

//DPointのアトリビュートをターゲットDPointの距離を元に移植


#pragma once



#include <maya/MPxNode.h>


class DPAttributeTransfer final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDP;
	
	static MObject aTargetDP;
	
	
	static MObject aAttributeNames;
	static MObject aDistance;
	
	static MObject aMaxSampleing;
	
	static MObject aTransferRamp;
	static MObject aValueRange;
	static MObject aScale;
	
	
	static MObject outDP;
	
	
private:
	
};

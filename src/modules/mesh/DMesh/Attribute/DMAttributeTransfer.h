//
//  DMAttributeTransfer.hpp
//  dommyTool
//
//  Created by yuzo on 5/11/17.
//
//

//DMeshのアトリビュートをDPointの距離を元に作成、編集


#pragma once



#include <maya/MPxNode.h>


class DMAttributeTransfer final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	static MObject aTargetDP;
	
	
	static MObject aAttributeNames;
	static MObject aDistance;
	
	static MObject aTransferRamp;
	static MObject aValueRange;
	static MObject aScale;
	
	
	static MObject outDM;
	
	
private:
	
};

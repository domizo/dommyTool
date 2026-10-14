//
//  DCCopy.hpp
//  dommyTool
//
//  Created by yuzo on 1/27/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//入力したカーブ、サーフェイス、DCurve,DPointを元にカーブを複製します
class DCCopy final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
//	virtual void postConstructor();
	static const MTypeId typeId;
	
public:
	
	static MObject inCopyObject;
	
	static MObject inDP;
	static MObject inDC;
	static MObject inObject;
	
	static MObject inReferenceObject;
	
	static MObject aCountU;
	static MObject aCountV;
	
	static MObject aAimVector;
	static MObject aNormalVector;
	
	static MObject aDPNormalAttributeName;
	static MObject aDPTangentAttributeName;
	
	static MObject aConnectType;
	static MObject aConnectUVSwitch;
    
	static MObject aTangentMode;
	static MObject aTangentVector;
	
	
	static MObject aUniformScale;
	static MObject aScale;
	
    static MObject aSeed;
    static MObject aRandomScale;
	static MObject aIsRandScaleAbsolute;
	static MObject aRandomRotate;
	static MObject aIsRandRotateabsolute;
    
	static MObject outDC;
	
private:
	
};

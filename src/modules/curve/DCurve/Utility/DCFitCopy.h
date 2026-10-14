//
//  DCFitCopy.hpp
//  dommyTool
//
//  Created by yuzo on 3/5/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//入力したカーブ、サーフェイスを元にカーブを複製します。
//変形時に元となるオブジェクトに追従します。
class DCFitCopy final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	static MObject inCopyObject;
	
	
	static MObject inObject;
	
	static MObject inReferenceObject;
	
	static MObject aCountU;
	static MObject aCountV;
	
	static MObject aPadding;
	static MObject aRandomPadding;
	
	
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

//
//  DCMeshCopy.hpp
//  dommyTool
//
//  Created by yuzo on 3/7/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//入力したカーブ、サーフェイスを元にカーブを複製します。
//変形時に元となるオブジェクトに追従します。
class DCMeshCopy final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	static MObject inCopyObject;
	
	static MObject inMesh;
	
	static MObject inReferenceObject;
	
	static MObject aCount;
	
	static MObject aPadding;
	static MObject aRandomPadding;
	
	static MObject aHeightScale;
    static MObject aRandomHeight;
    
	static MObject aDirectionMode;
	
	
	
	static MObject aSeed;
	
	
	static MObject outDC;
	
private:
	
};





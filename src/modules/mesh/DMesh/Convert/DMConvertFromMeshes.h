//
//  DMConvertFromMeshes.hpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//



#pragma once


#include <maya/MPxNode.h>

//mayaのメッシュオブジェクトをDMeshに変換
class DMConvertFromMeshes final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inMeshes;
    static MObject aInMatrices;
    
    
    static MObject aConvertType;
    
    static MObject aAddFaceNormalAttribute;
	static MObject aNormalAttributeName;
    
    
    static MObject aCurrentTime;
	
	static MObject outDM;
	
private:
	
};

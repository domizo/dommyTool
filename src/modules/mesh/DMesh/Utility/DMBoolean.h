//
//  DMBoolean.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/24.
//
//





#pragma once



#include <maya/MPxNode.h>

//DMesh同士をブーリアンする
class DMBoolean final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    static MObject targetDM;
    
    static MObject aOperation;
	
	static MObject aMethodType;
    
    static MObject outDM;
    
    
private:
    
};

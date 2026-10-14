//
//  DCGroupDelete.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/10.
//




#pragma once



#include <maya/MPxNode.h>


//DCurveのグループを削除
class DCGroupDelete final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static MTypeId typeId;
    
    
public:
    
    
    static MObject inDC;
    
    static MObject aIsEnable;
    static MObject aPrimitiveGroupName;
    static MObject aPointGroupName;
    
    static MObject outDC;
    
    
private:
    
};


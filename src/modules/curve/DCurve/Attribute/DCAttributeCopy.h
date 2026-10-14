//
//  DCAttributeCopy.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/30.
//




#pragma once



#include <maya/MPxNode.h>

///DCurveのアトリビュートをDPoint等から複製
class DCAttributeCopy final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aTargetDP;
    
    static MObject aAttributeNames;
    
    static MObject outDC;
    
    
private:
    
};

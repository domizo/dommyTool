//
//  DCBend.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/17.
//



#pragma once


#include <maya/MPxNode.h>

///DCurveを特定のVectorを軸にベンドする
class DCBend final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aBend;
    
    static MObject aBendVectorType;
    static MObject aRotateVector;
    static MObject aRotateVectorAttribute;
    
    
    static MObject aReverse;
    
    static MObject aWeightAttriute;
    static MObject aWeight;
    
    static MObject outDC;
    
private:
    
};

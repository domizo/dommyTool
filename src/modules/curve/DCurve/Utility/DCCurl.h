//
//  DCCurl.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/18.
//

#pragma once


#include <maya/MPxNode.h>

///DCurveにカールをかける
class DCCurl final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    
    static MObject aRadius;
    static MObject aRadiusAttribute;
    
    static MObject aArcStep;
    static MObject aArcStepAttribute;
    
    static MObject aSpiralOffset;
    static MObject aSpiralOffsetAttribute;
    
    static MObject aWeightAttriute;
    static MObject aWeight;
    
    static MObject outDC;
    
private:
    
};


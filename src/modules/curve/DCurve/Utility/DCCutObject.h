//
//  DCCutObject.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/18.
//

#pragma once


#include <maya/MPxNode.h>

///DCurveをpolygonでカットする
class DCCutObject final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    
    static MObject aInObject;
    
    static MObject aSampling;
    
    static MObject aFitCutPoint;
    
    static MObject outDC;
    
    
    
private:
    
};

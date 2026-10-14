//
//  DCClumping.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/24.
//

#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>


#include "DType.h"

//DCurveを束でまとめる
class DCClumping final : public MPxNode
{
public:
    void postConstructor() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    static MObject inDC;
    
    static MObject targetDC;
    static MObject aIndexAttribute;
    
    static MObject aClumpRamp;
    static MObject aWeight;
    
    static MObject outDC;
};

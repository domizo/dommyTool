//
//  DPMathFunc.hpp
//  dommyTool
//
//  Created by yuzo on 2018/01/26.
//


#pragma once

#include <maya/MPxNode.h>

class DPMathFunc final : public MPxNode
{
public:

    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    static MObject inDD;
    
    static MObject aFunc;
    
    static MObject inSourceDataArray;
    static MObject aSourceAttribute;
    
    static MObject aIsOutAttribute;
    static MObject aIsDeleteOriginal;
    static MObject aOutAttribute;
    
    static MObject outDD;
    static MObject outDataArray;
    
    
};

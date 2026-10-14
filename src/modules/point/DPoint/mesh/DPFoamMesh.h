//
//  DPFoamMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/23.
//
//

#pragma once

#include <maya/MPxNode.h>


class DPFoamMesh final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    static MObject inDP;
    
    static MObject aRadius;
    static MObject aRadiusAttribute;
    
    //for ball
    static MObject aFrequency;
    static MObject aRadiusOffset;
    static MObject aRadiusResultOffset;
    
    
    //for deform
    static MObject aSelfDeform;
    
    static MObject outDM;
};

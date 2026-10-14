//
//  DPAttributeBlend.hpp
//  dommyTool
//
//  Created by yuzo on 2016/11/24.
//
//

#pragma once



#include <maya/MPxNode.h>




class DPAttributeBlend final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDP;
    
    static MObject aSourceAttribute;
    static MObject aTargetAttribute;
    static MObject aWeight;
    static MObject aWeightAttribute;
    
    
    static MObject aOutputAttribute;
    static MObject aIsDeleteOriginal;
    
    static MObject outDP;
    
};


//
//  DPAttrSmooth.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/29.
//
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>



class DPAttrSmooth final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDD;
    
    
    static MObject aSmoothAttribute;
    static MObject aPointRadius;
    static MObject aSearchRadius;
	static MObject aSearchGridSize;
    static MObject aIterate;
    static MObject aMaxSampleCount;
    
    
    
    static MObject outDD;
    
};

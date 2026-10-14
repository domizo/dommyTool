//
//  DPBlend.hpp
//  dommyTool
//
//  Created by yuzo on 2016/07/08.
//
//

#pragma once


#include <maya/MPxNode.h>



/*DPointの*/
class DPBlend final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDP;
    
    
    static MObject aBlendArray;
    static MObject aTargetDP;
    static MObject aBlend;
    static MObject aBlendAttribute;
    
    static MObject aWeight;
    
    static MObject outDP;
    
};



//
//  DMExtrude.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/24.
//
//

#pragma once

#include <maya/MPxNode.h>


///DMeshのフェイスを押し出し
class DMExtrude final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDM;
    
    
    static MObject aThickNess;
    static MObject aOffset;
    
    static MObject aExtrudeTogether;
    static MObject aExtrusionCount;
    
    
    static MObject aFaceGroup;
    
    static MObject outDM;
    
};

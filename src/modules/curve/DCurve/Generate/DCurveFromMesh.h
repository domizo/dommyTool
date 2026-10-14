//
//  DCurveFromMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/05.
//
//

#pragma once



#include <maya/MPxNode.h>


class DCurveFromMesh final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    static MObject inMesh;
    
    static MObject outDC;
    
    
    
};

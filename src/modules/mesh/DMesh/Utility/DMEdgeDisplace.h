//
//  DMEdgeDisplace.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/24.
//


#pragma once

#include <maya/MPxNode.h>


///DMeshのエッジを変形
class DMEdgeDisplace final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    
    static MObject inDM;
    
    static MObject aDisplace;
    
    
    static MObject aDisplaceAttribute;
    static MObject aVectorAttribute;
    static MObject aFaceGroup;
    static MObject aIsConcernCorner;
    static MObject aCheckMinLength;
    static MObject outDM;
    
};

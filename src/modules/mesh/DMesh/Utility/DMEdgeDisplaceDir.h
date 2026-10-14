//
//  DMEdgeDisplaceDir.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/31.
//




#pragma once



#include <maya/MPxNode.h>




//
class DMEdgeDisplaceDir final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    static MObject aInObject;
    
    static MObject aAttributeName;
    
    static MObject aMinRatio;
    
    static MObject outDM;
    
    
private:
    
    
    
};

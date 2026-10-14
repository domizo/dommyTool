//
//  DMDelete.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/02.
//
//

//DMeshのprimitive, faceの削除


#pragma once

#include <maya/MPxNode.h>


class DMDelete final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aDeleteType;
    static MObject aDeleteGroups;
    
    static MObject aPattern;
    
    static MObject outDM;
    
    
private:
    
};

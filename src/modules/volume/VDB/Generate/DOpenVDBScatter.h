//
//  DOpenVDBScatter.hpp
//  dommyTool
//
//  Created by yuzo on 2016/12/15.
//
//

#pragma once


#include <maya/MPxNode.h>


class DOpenVDBScatter final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    static MObject aSeed;
    static MObject aSpread;
    static MObject aMode;
    static MObject aCount;
    static MObject aDensity;
    static MObject aIsLocal;
    static MObject aPerVoxel;
    
    static MObject aInterior;
    
    static MObject outDP;
    
    
};

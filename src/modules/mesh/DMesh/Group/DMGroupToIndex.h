//
//  DMGroupToIndex.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/22.
//

#pragma once

#include <maya/MPxNode.h>

///DMeshのGroupのインデックスをIntDataArrayとして出力
class DMGroupToIndex final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static MTypeId typeId;
    
public:
    
    static MObject inDM;
    
    static MObject aGroupName;
    
    
    static MObject outDataArray;
    
};

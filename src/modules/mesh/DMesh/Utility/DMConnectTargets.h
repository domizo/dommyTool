//
//  DMConnectTargets.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/29.
//

#pragma once

#include <maya/MPxNode.h>

enum {
    DM_C_TAR_DPOINT= 0,
    DM_C_TAR_DCURVE,
    DM_C_TAR_DMESH,
    DM_CONNECT_TARGET_TYPE_COUNT
};typedef short DM_CONNECT_TARGET_TYPE;


///DMeshと他のDDataとの関係性を生成
class DMConnectTargets final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDM;
    
    static MObject aType;
    
    static MObject aSearchRadius;
    
    static MObject aTargetDP;
    static MObject aTargetDC;
    static MObject aTargetDM;
    
    static MObject aSourceComponentType;
    static MObject aTargetComponentType;
    
    
    static MObject aMaxSampleCount;
    static MObject aMaxConnections;
    static MObject aOffsetFromCentroid;
    
    
    
    static MObject outDC;
    
};

//
//  DMConnetedPieces.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/19.
//

#pragma once

#include <maya/MPxNode.h>

enum {
    DM_C_POINT_PIECE= 0,
    DM_C_PRIM_CENTER_PIECE,
    DM_C_ALL_POINT,
    DM_CONNECT_PIECE_TYPE_COUNT
};typedef short DM_CONNECT_PIECE_TYPE;
///DMeshのItem同士の距離を元に接続の関係性を生成
class DMConnetedPieces final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDM;
    
    static MObject aType;
    static MObject aSearchRadius;
    static MObject aMaxSampleCount;
    static MObject aMaxConnections;
    static MObject aOffsetFromCentroid;
    
    static MObject outDC;
    
};

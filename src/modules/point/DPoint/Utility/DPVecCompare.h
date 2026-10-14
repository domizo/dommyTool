//
//  DPCompare.hpp
//  dommyTool
//
//  Created by yuzo on 3/2/16.
//
//

#pragma once

enum {
    DP_V_COMP_NO_OP = 0,
    DP_V_COMP_MAG_MIN,
    DP_V_COMP_MAG_MAX,
    DP_V_COMP_LUM_MIN,
    DP_V_COMP_LUM_MAX,
    DP_VEC_COMPARE_TYPE_COUNT
};typedef int DP_VEC_COMPARE_TYPE;


#include <maya/MPxNode.h>

class DPVecCompare final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    static MObject compareType;
    
    static MObject inputArray1;
    static MObject inputArray2;
    
    static MObject value;
    static MObject valueX;
    static MObject valueY;
    static MObject valueZ;
    
    
    static MObject resultArray;
    
};


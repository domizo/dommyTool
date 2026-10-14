//
//  DPSpiralField.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/03.
//
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>


#include "DMayaHeaders.h"

struct SpiralParams
{
    SpiralParams()
    : m_radius_ramp()
    , m_rad_min(0.0f)
    , m_rad_max(0.0f)
    , m_is_max_dist(false)
    , m_max_distance(0.0f)
    , m_center_ramp()
    , m_center_min(0.0f)
    , m_center_max(0.0f)
    , m_center_dist_ramp()
    , m_center_dist_min(0.0f)
    , m_center_dist_max(0.0f)
    , m_up_ramp()
    , m_up_min(0.0f)
    , m_up_max(0.0f)
    , m_up_dist_ramp()
    , m_up_dist_min(0.0f)
    , m_up_dist_max(0.0f)
    , m_curveObject(MObject::kNullObj)
    , m_invert(false)
    , m_normalize(false)
    , m_magnitude(0.0f)
    { }
    
    
    
    MRampAttribute m_radius_ramp;
    float m_rad_min;
    float m_rad_max;
    
    
    
    bool m_is_max_dist;
    float m_max_distance;
    
    MRampAttribute m_center_ramp;
    float m_center_min;
    float m_center_max;
    MRampAttribute m_center_dist_ramp;
    float m_center_dist_min;
    float m_center_dist_max;
    
    MRampAttribute m_up_ramp;
    float m_up_min;
    float m_up_max;
    MRampAttribute m_up_dist_ramp;
    float m_up_dist_min;
    float m_up_dist_max;

    
    
    
    
    
    MObject m_curveObject;
    
    
    bool m_invert;
    bool m_normalize;
    float m_magnitude;
    
//    std::vector<MObject> crvObjects;
    
    
};




//螺旋状のベクターアトリビュートを生成
class DPSpiralField final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    MStatus evalParms(MDataBlock& data, SpiralParams& parms);
    
    
    
    
    static MObject inDP;
    
    static MObject inObject;
    
    
    static MObject aRadiusRamp;
    static MObject aRadiusRange;
    
    
    static MObject aIsMaxDistance;
    static MObject aMaxDistance;
    
    
    static MObject aCenterStreamRamp;
    static MObject aCenterStreamRange;
    static MObject aCenterDistanceRamp;
    static MObject aCenterDistanceRange;
    static MObject aUpStraemRamp;
    static MObject aUpStraemRange;
    static MObject aUpDistanceRamp;
    static MObject aUpDistanceRange;
    
    static MObject aAttributeName;
    
    static MObject aInvert;
    static MObject aNormalize;
    static MObject aMagnitude;
    
    static MObject outDP;
    
};

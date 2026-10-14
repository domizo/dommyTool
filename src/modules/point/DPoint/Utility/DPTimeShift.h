//
//  DPTimeShift.hpp
//  dommyTool
//
//  Created by yuzo on 2016/07/03.
//
//

#pragma once


#include <maya/MPxNode.h>
#include "DMayaHeaders.h"



enum {
    DP_TIME_BIRTH_SAME_INDEX = 0,
    DP_TIME_BIRTH_SAME_TIME,
    DPTimeBirthType_COUNT
};typedef short DPTimeBirthType;

enum {
    DP_TIME_LEN = 0,
    DP_TIME_LEN_ATTRIBUTE,
    DP_TIME_LEN_DATA,
    DP_TIME_LEN_RAMP,
    DP_TIME_LEN_FRAME,
    DPTimeLengthType_COUNT
};typedef short DPTimeLengthType;

enum {
    DP_TIME_ATTR_NORMALIZE = 0,
    DP_TIME_ATTR_IGNORE_OFFSET,
    DP_TIME_ATTR_RAW,
    DPTimeAttributeType_COUNT
};typedef short DPTimeAttributeType;


///Ray methodで使用するパラメータークラス
struct DPTimeShiftParams
{
    DPTimeShiftParams()
    : m_birth_type(DP_TIME_BIRTH_SAME_INDEX)
    , m_length_type(DP_TIME_LEN)
	, m_time(0.0)
	, m_time_offset(0.0f)
	, m_time_length(0.0f)
	, m_time_sample(1)
	, m_time_rand(0.0f)
	, m_seed(0)
	, m_is_time_attr(false)
    , m_time_attr_type(DP_TIME_ATTR_NORMALIZE)
	, m_is_parent_id_attr(false)
    { }
    
    DPTimeBirthType m_birth_type;
    
    DPTimeLengthType m_length_type;
    
    MTime m_time;
    float m_time_offset;
    float m_time_length;
    long m_time_sample;
    float m_time_rand;
    long m_seed;
    MRampAttribute m_time_ramp;
    bool m_is_time_attr;
    
    DPTimeAttributeType m_time_attr_type;
    
    bool m_is_parent_id_attr;
    
    std::string m_time_offset_name;
    std::string m_time_length_name;
    std::string m_time_attr_name;
    std::string m_parent_id_attr_name;
    
};




class DPTimeShift final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    MStatus evalParams(MDataBlock& data, DPTimeShiftParams& params);
    
    
    static MObject inDP;
    static MObject inObject;
    
    static MObject aPointOrderType;
    
    static MObject aCurrentTime;
    static MObject aTimeOffset;
    static MObject aTimeLength;
    static MObject aTimeSampling;
    
    static MObject aTimeLengthType;
    
    
    static MObject aSeed;
    static MObject aTimeLengthRandom;
    
    
    static MObject aTimeOffsetAttrName;
    static MObject aTimeLengthAttrName;
    
    
    static MObject inTimeOffsetData;
    static MObject inTimeLengthData;
	
	static MObject aTimeRamp;
	
	
	
	//Attribute
	static MObject aIsAddTimeAttr;
	static MObject aTimeAttrValueType;
	static MObject aTimeAttrName;
	
    static MObject aIsAddParentIdAttr;
    static MObject aParentIdAttrName;
    
    static MObject outDP;
    
};

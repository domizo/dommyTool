//
//  DPAffect.hpp
//  dommyTool
//
//  Created by yuzo on 11/24/16.
//
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


enum {
    AFFECT_POINT = 0,
    AFFECT_AXIS_LINE,
    AFFECT_OBJECT,
    AFFECT_DPOINT,
    AFFECT_TYPE_COUNT
};typedef short AffectType;


struct AffectParams
{
    AffectParams()
    : m_attr_class(static_cast<DP_ATTRIBUTE_CLASS_TYPE>(0))
    , m_type(AFFECT_POINT)
    , m_distanceMin(0.0)
    , m_distanceMax(5.0)
    , m_slideDistance(0.0)
    , sourcePt(DPoint(0,0,0))
    , m_axis_source(MVector())
    , m_axis_target(MVector())
    , m_useAxisDistance(true)
    , m_object()
    , m_objectType(MFnData::kInvalid)
    , m_targetPts(NULL)
    , m_tolerance(0.1)
    , m_isCheckInner(false)
    , m_innerCheckAngle(MVector())
    , meshResolution(0)
    , m_innerValue(0.0f)
    , m_ramp_attr()
    , m_scale(1.0f)
    , valueRangeMin(0.0f)
    , valueRangeMax(1.0f)
    , m_axisSourceVec(MPoint())
    , m_axisTargetVec(MPoint())
    { }
	
	DP_ATTRIBUTE_CLASS_TYPE m_attr_class;
	
    AffectType m_type;
    float m_distanceMin;
    float m_distanceMax;
    float m_slideDistance;
    
    DPoint sourcePt;
    MVector m_axis_source;
    MVector m_axis_target;
    bool m_useAxisDistance;
    
    
    
    MObject m_object;
    MFnData::Type m_objectType;
    const DPointList *m_targetPts;
    
    double m_tolerance;
    bool m_isCheckInner;
    MVector m_innerCheckAngle;
    long meshResolution;
    
    float m_innerValue;
    
    MRampAttribute m_ramp_attr;
    float m_scale;
    float valueRangeMin;
    float valueRangeMax;
    
    //for axis
    MPoint m_axisSourceVec;
    MPoint m_axisTargetVec;
    DScalarDataList m_maxDistArray;
    DScalarDataList m_slideArray;
    DScalarDataList m_scaleArray;
};




class DPAffect final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
    
    
    MStatus evalAffectParms(MDataBlock& data, AffectParams& parms);
    
    
	
	
	static MObject inDD;
	
	static MObject aAttributeClass;
	
	static MObject aAffectType;
	
	static MObject aDistance;
    static MObject aSlideDistance;
	
	
	
	
    //Point
    static MObject aSourcePoint;
	
	
	//Axis
	static MObject aAxisSource;
	static MObject aAxisSourceMatrix;
	static MObject aAxisTarget;
	static MObject aAxisTargetMatrix;
	static MObject aUseAxisDistance;
	
	
	
	
	//Object
	static MObject aInObject;
	static MObject aTolerance;
	static MObject aIsCheckInner;
    static MObject aInnerCheckAngle;
    static MObject aMeshResolution;
	static MObject aInnerValue;
	
	
    //TargetDP
    static MObject aTargetDP;
    static MObject aDistanceAttribute;
    static MObject aSlideAttribute;
	static MObject aScaleAttribute;
	
	static MObject aAffectRamp;
    static MObject aValueRange;
    static MObject aScale;
    static MObject aOutAttribute;
    
	static MObject outDD;
	
};

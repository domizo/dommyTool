//
//  DPMagnet.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/26.
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


struct MagneteParams
{
	MagneteParams()
	: m_type(0)
	, m_magnitude(1.0)
	, isClamp(false)
	, mMinClamp(0.0)
	, mMaxClamp(1.0)
	, m_weight_attr("")
    , m_global_weight(1.0f)
	, m_targetPt()
	, m_targetObject(MObject::kNullObj)
    , m_targetObjectType(MFnData::kInvalid)
	, m_targetPts(NULL)
	, m_dcdata(NULL)
	{ }
	
	short m_type;
	float m_magnitude;
	
	bool isClamp;
	float mMinClamp;
	float mMaxClamp;
    
    std::string m_weight_attr;
    float m_global_weight;
	
	DPoint m_targetPt;
	MObject m_targetObject;
    MFnData::Type m_targetObjectType;
	const DPointList *m_targetPts;
	
	
	const DCurveData *m_dcdata;
	std::vector<MObject> crvObjects;
};


/*特定の距離にあるポイントを引きつけます*/
class DPMagnet final : public MPxNode
{
public:
	
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
	
	
	
	MStatus evalMagnetParms(MDataBlock& data, MagneteParams& parms);
	
	
	
	
    static MObject inDD;
    
    static MObject aMagnitude;
    
    static MObject aMagnetType;
	
	static MObject aIsClamp;
	static MObject aClampRange;
	
    
    static MObject aTargetPoint;
    
    static MObject inTargetObject;
    static MObject inTargetDP;
	static MObject inTargetDC;
	
	
	
	static MObject aWeightAttributeName;
	static MObject aWeight;
	
	
	
	
     
    static MObject outDD;
    
};

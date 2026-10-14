//
//  DPAvoid.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/29.
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"



struct AvoidParams
{
    AvoidParams()
    : m_radius(5.0f)
    , m_iterate(1)
    , m_type(0)
    , m_targetPt()
    , m_object()
    , m_objectType(MFnData::kInvalid)
    , m_targetPts(NULL)
    , m_dcdata(NULL)
    { }
    
    
    float m_radius;
    int m_iterate;
    short m_type;
    DPoint m_targetPt;
    MObject m_object;
    MFnData::Type m_objectType;
    const DPointList *m_targetPts;
    const DCurveData *m_dcdata;
    
    std::vector<MObject> crvObjects;
    
    DScalarDataList radiusList;
};



//特定の距離にあるポイントをs指定の距離をとります。
class DPAvoid final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    MStatus evalMagnetParms(MDataBlock& data, AvoidParams& parms);
    
    
    
    
    static MObject inDD;
    
    static MObject aRadius;
    static MObject aRadiusAtribute;

    static MObject aIterate;
    
    static MObject aAvoidType;
    static MObject aTargetPoint;
    static MObject inTargetObject;
    static MObject inTargetDP;
    static MObject inTargetDC;
    
    
    
    static MObject aWeightAttributeName;
    static MObject aWeight;
    
    
    
    
    
    static MObject outDD;
    
};


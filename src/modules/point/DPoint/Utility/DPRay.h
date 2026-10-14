//
//  DPRay.hpp
//  dommyTool
//
//  Created by yuzo on 2016/07/07.
//
//

#pragma once



#include <maya/MPxNode.h>
#include "DMayaHeaders.h"

#include "DPRayUtility.h"


class DPRay final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    MStatus evalRayParms(MDataBlock& data, DPRayParams& parms);
    
    
    static MObject inDD;
    
    
    static MObject inObject;
    
    
    static MObject aMethod;
    
    static MObject aRayDirection;
    
    static MObject aIsTransform;
    static MObject aIsFarthest;
//    static MObject aPointIntersectNormal;
//    static MObject aPointIntersectDistance;
    
    static MObject aIsReverseRay;
    
    static MObject aRayTolerance;
    static MObject aScale;
    static MObject aLift;
//    static MObject aBias;
    
    static MObject aIsMaxDistance;
    static MObject aMaxDistance;
    
    static MObject aIsCreateRayGroup;
    static MObject aRayGroupName;
    
//    static MObject aHitGridSize;
   
    
    static MObject aVectorAttribute;
    static MObject aFaceGroup;//for DMesh
    
    
    static MObject outDD;
    
};



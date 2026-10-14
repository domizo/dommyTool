//
//  DMWireGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//

#pragma once

#include <maya/MPxNode.h>


///DCurveからwire形状のDMeshを生成
class DMWireGen final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDC;
    
    
    static MObject inTangentCurve;
    
    
    static MObject aProfileMode;
    static MObject aProfileCurve;
    
    static MObject aCircleRadius;
    static MObject aCircleSampling;
    
    
    static MObject aSampling;
    static MObject aCurveLength;
    static MObject aCurveOffset;
    
    static MObject aTangentMode;
    static MObject aTangentVector;
    
    static MObject aTwistRamp;
    static MObject aWidthRamp;
    
    static MObject aGlobalRotate;
    static MObject aGlobalScale;
    
    static MObject aExpression;
    
    
    static MObject aUVTranslate;
    static MObject aUVScale;
    
    
    
    
    static MObject outDM;
    
};

//
//  DCurveFromCurve.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/04.
//
//




#pragma once


#include <maya/MPxNode.h>

//カーブから複数のカーブを生成
class DCurveFromCurve final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    void postConstructor() override;
    
    static const MTypeId typeId;
    
    
    
    static MObject inCurve;
    static MObject inDC;
    
    static MObject aProfileDP;
    static MObject aInTangentCurve;
	
    static MObject aSamplingType;
    
	static MObject curveLength;
    static MObject sampling;
    
    static MObject aCurveDivide;
    
    
	static MObject offset;
	static MObject aDegree;
    static MObject aIsMultiEndKnot;
	
	static MObject twist;
    static MObject tubeWidth;
    
    static MObject aTwistScale;
    static MObject aTubeScale;
	
	static MObject aSeed;
	static MObject aUseRandomRotation;
	static MObject aRandRotation;
    static MObject aUseRandomRadius;
	static MObject aRandRadius;
	
	
	static MObject aUseCachedParam;
	
    static MObject outDC;
    
	
};

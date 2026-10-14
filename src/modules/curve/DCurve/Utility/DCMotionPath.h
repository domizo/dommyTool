//
//  DCMotionPath.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/03.
//
//





#pragma once


#include <maya/MPxNode.h>

//DCurveにそったDPointを生成します
class DCMotionPath final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aType;
    static MObject aPathPosition;
    static MObject aPositionAttribute;
	
	static MObject aOffset;
	static MObject aOffsetAttribute;
	static MObject aSpeed;
	static MObject aSpeedAttribute;
	
    static MObject aWrapMode;
    
    static MObject aTangentMode;
    static MObject aTangentVector;
    
    static MObject aIsExportNormal;
    static MObject aNormalAttribute;
    
    static MObject aIsExportTangent;
    static MObject aTangentAttribute;
	
	static MObject aIsExportParam;
	static MObject aParamAttribute;
	
    static MObject outDP;
    
private:
    
};

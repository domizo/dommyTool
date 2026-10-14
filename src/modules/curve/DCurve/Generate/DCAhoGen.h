//
//  DCAhoGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/01.
//
//



#pragma once


#include <maya/MPxNode.h>

//カーブ、から縮毛を作成
class DCAhoGen final : public MPxNode
{
public:
	void postConstructor() override;
	
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;

    static  void *creator();
    static  MStatus initialize();
	
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aDistributeType;
    static MObject aCount;
    static MObject aLength;
    static MObject aSegment;
	static MObject aAngleType;
    static MObject aAngleSpread;
	static MObject aNormal;
	
    static MObject aSeed;
	
	static MObject aLengthRandom;
	
	
//	static MObject aBend;
	
	
	static MObject aIsStruggle;
	static MObject aStruggleRange;
	static MObject aStruggleStrength;
	static MObject aStruggleRamp;
	
    static MObject outDC;
    
private:
    
};


//
//  DCFurGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/12.
//
//

//DPointからDCurveを生成

#pragma once


#include <maya/MPxNode.h>


class DCFurGen final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    
    
    
    static MObject inDP;
	
	static MObject aSeed;
	
	static MObject aThinning;
//	static MObject aThinningAttribute;
	
    static MObject aLength;
    static MObject aLengthRandom;
    static MObject aLengthAttribute;
    
    static MObject aSegment;
    
    static MObject aNormalAttribute;
	
	
	static MObject aBend;
	
	
	static MObject aIsStruggle;
	static MObject aStruggleRange;
	static MObject aStruggleStrength;
	static MObject aStruggleRamp;
	
	
	static MObject aOffset;
    static MObject aScale;
    
    
    static MObject aTransferAttribute;
    
    
    static MObject outDC;
    
    
    
};


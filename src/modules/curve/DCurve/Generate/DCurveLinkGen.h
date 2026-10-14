//
//  DCurveConnectGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/01/22.
//
//





#pragma once

#include <maya/MPxNode.h>

//カーブ、サーフェイスの面の情報から複数のカーブを生成
class DCurveLinkGen final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject aCount;
    static MObject aSegment;
    static MObject aDegree;
    
    
    
    
    static MObject aLinkList;
    static MObject aInObject;
	
	static MObject aMode;
	
	static MObject aWrapMode;
	
	static MObject aStepSizeU;
    static MObject aStepSizeV;
    static MObject aColumnNum;
	
    static MObject aRangeU;
    static MObject aRangeV;
    static MObject aOffsetU;
    static MObject aOffsetV;
	
	static MObject aIsRandom;
	static MObject aSeed;
	static MObject aRandom;
   
    
    
    
    static MObject outDC;
    
private:
    
};


//
//  DCurveFromPfx.h
//  dommyTool
//
//  Created by yuzo on 2017/02/05.
//
//



#pragma once


#include <maya/MPxNode.h>
#include <maya/MRenderLineArray.h>


//ペインントエフェクトからDCurveを生成
class DCurveFromPfx final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    
    
    
    static const MTypeId typeId;
    
    
    static MObject inPfx;
    static MObject aEvaluationTime;
//    static MObject aPfxNodeName;
    
    static MObject aDegree;
    
    
    static MObject aIsMainLines;
    static MObject aIsLeafLines;
    static MObject aIsFlowerLines;
    
    static MObject aIsExportTwist;
    static MObject aIsExportWidth;
    static MObject aIsExportFlat;
    static MObject aIsExportParameter;
    static MObject aIsExportColor;
    static MObject aIsExportIncandence;
    static MObject aIsExportTransparency;
    
    
    
    static MObject outDC;
    
    
    
};

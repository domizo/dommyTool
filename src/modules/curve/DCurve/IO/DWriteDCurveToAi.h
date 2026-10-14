//
//  DWriteDCurveToAi.hpp
//  dommyTool
//
//  Created by yuzo on 2017/01/21.
//
//




#pragma once

#include <maya/MPxNode.h>

class DWriteDCurveToAi final : public MPxNode
{
public:
    
    DWriteDCurveToAi() = default;
    ~DWriteDCurveToAi() override = default;
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kGloballySerialize; }
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDC;
    
    static MObject aFilePath;
    static MObject aExportMode;
    static MObject aExportLog;
    
    static MObject aIsBinary;
	
	static MObject aOutNodeName;
	
	static MObject aRenderMode;
    static MObject basisMode;
    static MObject isAddEdgeCV;
	static MObject aCurveWidthType;
    static MObject aConstantWidth;
    static MObject aWidthAttributeName;
    static MObject aMinPixelWidth;
	
	static MObject aOrientType;
	static MObject aOrientAttributeName;
    
    static MObject aExportCurveAttribute;
    static MObject aExportPointAttribute;
    
    static MObject outDC;
    
};

//
//  LeafGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/06.
//

#pragma once




#include <maya/MPxNode.h>

class LeafGen final : public MPxNode
{
public:
    
    
    void postConstructor() override;
    
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static    void*        creator();
    static    MStatus        initialize();
    static    MTypeId        typeId;
    
    
    static MObject aLeafLength;
    static MObject aLeafWidth;
    static MObject aLeafDepth;
    
    static MObject aLengthSampling;
    static MObject aWidthSampling;
    
    static MObject aUseWidthBias;
    
    static MObject aLeftRamp;
    static MObject aIsSymmetry;
    static MObject aRightRamp;
    
    
    static MObject aDepthWidthRamp;
    static MObject aDepthLengthRamp;
    
    
    
    static MObject aIsNoiseEdge;
    static MObject aSeed;
    static MObject aNoiseAmplitude;
    static MObject aNoiseFrequency;
    static MObject aNoiseWidthRamp;
    static MObject aNoiseLengthRamp;
    
    
    
    static MObject aBend;
    static MObject aBendRamp;
    
    
    
    static MObject outMesh;
    
};

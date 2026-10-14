//
//  DBubbleCrystal.h
//  dommyTool
//
//  Created by yuzo on 2015/02/10.
//
//

#ifndef __dommyTool__DBubbleCrystal__
#define __dommyTool__DBubbleCrystal__

#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxNode.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>


class DBubbleCrystal final : public MPxNode {
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    //bubble shape
    static MObject side;
    static MObject radius;
    
    
    static MObject divide;
    static MObject midDivide;
    
    
    static MObject growth;
    
    
    //randam
    static MObject seed;
    
    static MObject arcNoiseAmplitude;
    static MObject arcNoiseFrequency;
    static MObject arcNoiseSmooth;
    static MObject arcNoiseSpace;
    
    
    static MObject heightNoiseAmplitude;
    static MObject heightNoiseFrequency;
    static MObject heightNoiseSmooth;
    static MObject heightNoiseSpace;
    static MObject heightNoisePower;
    
    //output
    static MObject outPutMesh;
    
    
private:
    
    
};
#endif /* defined(__dommyTool__DBubbleCrystal__) */

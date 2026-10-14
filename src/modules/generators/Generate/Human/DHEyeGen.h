//
//  DHEyeGen.hpp
//  dommyTool
//
//  Created by yuzo on 5/7/16.
//
//

#ifndef DHEyeGen_h
#define DHEyeGen_h

#include <maya/MPxNode.h>

class DHEyeGen final : public MPxNode
{
public:
	
	DHEyeGen() {}
	~DHEyeGen() override {}
	
    void postConstructor() override;
    
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static	void*		creator();
	static	MStatus		initialize();
	static	MTypeId		typeId;
	static	MObject		divideU;
	static	MObject		divideV;
	static	MObject     radius;
    static  MObject     radiusRamp;
	static	MObject     innerRadius;
		
	static	MObject		lenzGap;
	static	MObject		lenzRamp;
	static	MObject     lenzsOffset;
	
	static	MObject     irisSize;
    static	MObject     irisDivide;
    static	MObject     irisRamp;
    static	MObject     irisOffset;

	static	MObject     pupilSize;
    static	MObject     pupilDepth;
    static  MObject     pupilMotion;
    
    static	MObject     isCapPupil;
    
    
    static  MObject      uvPadding;
    
	
	static	MObject		outLenzMesh;
	static	MObject		outIrisMesh;
	
};


#endif /* DHEyeGen_h */

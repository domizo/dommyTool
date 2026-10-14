//
//  DMotionTrailCurveNode.h
//  dommyTool
//
//  Created by yuzo on 3/10/15.
//
//

#ifndef __dommyTool__DMotionTrailCurveNode__
#define __dommyTool__DMotionTrailCurveNode__



#include <maya/MPxNode.h>


enum {
	DT_TRAIL_OBJECT = 0,
	DT_TRAIL_POINT,
	DT_TRAIL_POINT_RANDOM,
	DT_TRAIL_TYPE_COUNT
};typedef int DT_TRAIL_TYPE;

class DMotionTrailCurveNode final : public MPxNode
{
public:
	DMotionTrailCurveNode();
	~DMotionTrailCurveNode() override;
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static	void*		creator();
	static	MStatus		initialize();
	static	MTypeId		typeId;
	
	
	static MObject inputObject;
	static MObject transformMatrix;
	
	static MObject trailType;
	static MObject seed;
	static MObject randomCount;
	
	
	static	 MObject trailLength;
	static	 MObject timeSampling;
	static	 MObject trailLengthRandom;
	
	
	static  MObject time;
	
	static	 MObject outputObjects;
	
	
};



#endif /* defined(__dommyTool__DMotionTrailCurveNode__) */

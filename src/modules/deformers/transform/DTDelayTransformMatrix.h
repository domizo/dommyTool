//
//  DTDelayTransformMatrix.h
//  dommyTool
//
//  Created by yuzo on 3/27/15.
//
//

#ifndef __dommyTool__DTDelayTransformMatrix__
#define __dommyTool__DTDelayTransformMatrix__

#include <vector>

#include <maya/MPxNode.h>

class DTDelayTransformMatrix final : public MPxNode
{
public:
	DTDelayTransformMatrix();
	~DTDelayTransformMatrix() override;
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static	void*		creator();
	static	MStatus		initialize();
	static	MTypeId		typeId;
	
	static MObject transformMatrix;
	
	static MObject delay;
	
	static  MObject time;
	
	static	 MObject outputTransformMatrix;
	
	
};




#endif /* defined(__dommyTool__DTDelayTransformMatrix__) */

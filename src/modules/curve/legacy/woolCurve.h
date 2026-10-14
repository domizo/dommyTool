#ifndef WOOLCURVE_H
#define WOOLCURVE_H

#include <maya/MPxNode.h>



class woolCurve final : public MPxNode
{
public:
	woolCurve() = default;
	~woolCurve() override = default;
		
	MStatus		compute( const MPlug& plug, MDataBlock& data ) override;
		
	static	void*		creator();
	static	MStatus		initialize();
		
	static	MObject		startRadius;
	static	MObject		endRadius;
	static	MObject		round;
	static	MObject		sections;
	static	MObject		rotNoise;
	static	MObject		axisNoise;
	
	static	MObject		zTrans;
	
	
	static	MObject		outputCurve;
		
	static	MTypeId		typeId;
};

#endif

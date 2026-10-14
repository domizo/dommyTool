//
//  DShapeSequencer.hpp
//  dommyTool
//
//  Created by yuzo on 4/29/16.
//
//

#ifndef DShapeSequencer_h
#define DShapeSequencer_h

#include <maya/MPxNode.h>





class DShapeSequencer final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inputObjects;
	
	static MObject seaquence;
	
	
	static MObject outputObject;
};



#endif /* DShapeSequencer_h */

//
//  DStretchData.hpp
//  dommyTool
//
//  Created by yuzo on 6/5/16.
//
//

#ifndef DStretchData_h
#define DStretchData_h

#include <maya/MPxNode.h>

class DStretchData final : public MPxNode
{
public:
	
	DStretchData() = default;
	~DStretchData() override = default;
	void postConstructor() override;
	
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;
	SchedulingType schedulingType() const override { return kUntrusted; }
	
	static	void*		creator();
	static	MStatus		initialize();
	static const MTypeId typeId;
	
	
	static MObject inMesh;
	static MObject relaxMesh;
	
	static MObject aIsStretch;
	static MObject aCalcType;
	static MObject aStretchMap;
	static MObject aCompressMap;
	
	
	static MObject aSampleRadius;
	
	static MObject aIsVector;
	
	static MObject smIterate;
	
	static MObject outMesh;
	
	
};




#endif /* DStretchData_h */

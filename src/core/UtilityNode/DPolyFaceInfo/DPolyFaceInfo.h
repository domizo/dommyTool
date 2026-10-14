//
//  DPolyFaceInfo.hpp
//  dommyTool
//
//  Created by yuzo on 6/1/16.
//
//

#ifndef DPolyFaceInfo_h
#define DPolyFaceInfo_h

#include <maya/MPxNode.h>

class DPolyFaceInfo final : public MPxNode
{
public:
	DPolyFaceInfo() = default;
	~DPolyFaceInfo() override = default;
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static	void*		creator();
	static	MStatus		initialize();
	static const MTypeId typeId;
	
	
	static	MObject		inMesh;
	
	static	MObject		faceIndex;
	
	
	static	MObject		outTranslate;
	static	MObject		outRotate;
	
	
};


#endif /* DPolyFaceInfo_h */

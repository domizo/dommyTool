//
//  DCurvatureData.hpp
//  dommyTool
//
//  Created by yuzo on 6/3/16.
//
//

#ifndef DCurvatureData_h
#define DCurvatureData_h

#include <maya/MPxNode.h>

class DCurvatureData final : public MPxNode
{
public:
	DCurvatureData() = default;
	~DCurvatureData() override = default;
	void postConstructor() override;
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static	void*		creator();
	static	MStatus		initialize();
	static const MTypeId typeId;
	
	
	static MObject inMesh;
	
    static MObject aCalcType;
    static MObject aCurveMap;
	static MObject smIterate;
	static MObject aIsAdjustRange;
    
    
	static MObject outMesh;
	
	
};


#endif /* DCurvatureData_h */

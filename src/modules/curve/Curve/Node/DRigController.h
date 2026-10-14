//
//  DRigController.hpp
//  dommyTool
//
//  Created by yuzo on 2/22/16.
//
//

#ifndef __dommyTool__DRigController__
#define __dommyTool__DRigController__

#include <maya/MPxNode.h>
#include <maya/MPointArray.h>
#include "DType.h"
enum {
	D_CTRL_SQUARE = 0,
	D_CTRL_CIRCLE,
	D_CTRL_CUBE,
	D_CTRL_SPHERE,
	D_CTRL_PYRAMID,
	D_CTRL_S_ARROW,
	D_CTRL_TWIN_ARROW,
	D_CTRL_CROSS_ARROW,
    D_CTRL_PIN,
	D_CONTROLLER_SHAPE_TYPE_COUNT
};typedef int D_CONTROLLER_SHAPE_TYPE;





class DRigController final : public MPxNode
{
public:
	DRigController();
	~DRigController() override;
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static	void*		creator();
	static	MStatus		initialize();
	static	MTypeId		typeId;
	
	
	void addDivision(MPointArray &cvs, MVector sp, MVector ep, long div);
	bool taperDeform(MPointArray &cvs, double highBound, double lowBound, double curvature, D_AXIS_TYPE axis);
	bool bendDeform(MPointArray &cvs, double highBound, double lowBound, double curvature, D_AXIS_TYPE axis);
    bool crossBendDeform(MPointArray &cvs, double highBound, double lowBound, double curvature);
    
	static MObject shapeType;
    static MObject localScale;
//	static MObject localSize;
	static MObject division;
	
	static MObject offset;
	static MObject offsetX;
	static MObject offsetY;
	static MObject offsetZ;
    
    static MObject localRotate;
    static MObject localRotateX;
    static MObject localRotateY;
    static MObject localRotateZ;
	
	static MObject keepCenter;
	
	
	static MObject ratio;
	static MObject dirAxis;
	static MObject taperAxis;
	static MObject taperAmount;
	static MObject bendAmount;
	static MObject bendAxis;
//	static MObject bendBound;
	
	static MObject arrowReverse;
	static MObject arrowRatio;
	static MObject arrowWidth;
	
	
	
	static MObject outputCurve;
	
	
};




#endif /* DRigController */

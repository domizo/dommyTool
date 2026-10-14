//
//  DPRotation.hpp
//  dommyTool
//
//  Created by yuzo on 11/21/16.
//
//

#pragma once



#include <maya/MPxNode.h>



class DPRotation final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aRotateType;
	
	//input
	static MObject aForwardVector;
	static MObject aForwardVectorAttribute;
    
    static MObject aIsUseUpVector;
    static MObject aUpVector;
	static MObject aUpVectorAttribute;
	
	static MObject aRotationMatrix;
	
	//space
	static MObject aSpaceForwardAxis;
	static MObject aSpaceUpAxis;
	
    
	
	static MObject aRotateSpace;
	static MObject aRotateOrder;
	//rotate
	static MObject aRotate;
	static MObject aRotateAttribute;
	
	//output
	static MObject aOutputType;
	static MObject aOutFowardAttribute;
	static MObject aOutUpAttribute;
	static MObject aOutMatrixAttribute;
	
	
	
	//time range
	static MObject aGlobalOffset;
	static MObject aOffsetType;
	static MObject aOffsetAttribute;
	static MObject aInOffsetArray;
	
	static MObject aStretchType;
	static MObject aStretchAttribute;
	static MObject aInStretchArray;
	
	
	
	
	
	static MObject outDP;
	
};


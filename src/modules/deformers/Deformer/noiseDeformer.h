//
//  noiseDeformer.h
//  dommyTool
//
//  Created by yuzo on 11/25/12.
//
//

#ifndef __dommyTool__noiseDeformer__
#define __dommyTool__noiseDeformer__

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxDeformerNode.h>


enum {
	DT_DROPOFF_CONSTANT = 0,
	DT_DROPOFF_LINEAR,
	DT_DROPOFF_QUADRATIC,
	DT_DROPOFF_TYPE_COUNT
};typedef int DT_DROPOFF_TYPE;


class noiseDeformer final : public MPxDeformerNode {
public:
	static  void *creator();
	static  MStatus initialize();
	
	MStatus deform( MDataBlock &block,
					MItGeometry &iter,
					const MMatrix &mat,
					unsigned int multiIndex ) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static MTypeId typeId;
	static MString typeName;
	
private:
	// Attributes
	
	static MObject seed;
	
	static MObject amplitude;
	static MObject ampOffset;
	static MObject absOn;
	
	static MObject frequency;
	
	
	static MObject smooth;//alpha
	static MObject space;//beta
	
//	static MObject noiseSpace;
	static MObject enableTrasform;
	
	static  MObject centerX;
	static  MObject centerY;
	static  MObject centerZ;
	static  MObject center;
	
	static  MObject offsetX;
	static  MObject offsetY;
	static  MObject offsetZ;
	static  MObject offset;
	
	static  MObject scaleX;
	static  MObject scaleY;
	static  MObject scaleZ;
	static  MObject scale;
	
	
	static  MObject isDropoff;
	static  MObject dropoffRadius;
	static  MObject dropoffType;
	static  MObject attenuation;
	
	
};

#endif /* defined(__dommyTool__noiseDeformer__) */

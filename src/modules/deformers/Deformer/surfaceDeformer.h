#ifndef SURFACEDEFORMER_H
#define SURFACEDEFORMER_H


#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxDeformerNode.h>


class surfaceDeformer final : public MPxDeformerNode {
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
		static MObject posU;
		static MObject posV;
		
		static MObject offsetN;
		
		static MObject scaleU;
		static MObject scaleV;
		
		static MObject scaleN;
		
		static MObject minU, maxU, minV, maxV;
		
		
		
		static MObject baseSurface;
		
		
};

#endif

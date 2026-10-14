
#ifndef PUSHDEFORMER_H
#define PUSHDEFORMER_H


#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxDeformerNode.h>


class pushDeformer final : public MPxDeformerNode {
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
		static MObject amplitude;
		static MObject ampColor;
		static MObject cameraMatrix;
		
		
};

#endif

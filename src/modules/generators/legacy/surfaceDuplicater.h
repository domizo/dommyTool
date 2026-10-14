#ifndef SURFACEDUPLICATER_H
#define SURFACEDUPLICATER_H



#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxNode.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MVectorArray.h>
#include <maya/MDoubleArray.h>


#include <vector>

#include "dupMesh.h"



class surfaceDuplicater final : public MPxNode
	{
public:
		MStatus compute( const MPlug& plug, MDataBlock& block ) override;
		SchedulingType schedulingType() const override { return kUntrusted; }

		static  void *creator();
		static  MStatus initialize();

		static const MTypeId typeId;

public:
		
		
		static MObject duplicateObjects;
		static MObject typeSeed;
		static MObject sourcePlane;
		static MObject numU;
		static MObject numV;
		
		static MObject stepU;
		static MObject stepV;
		
		static MObject randamU;
		static MObject randamV;
		
		static MObject aimNormal;
		
		static MObject normalSet;
		
		static MObject offsetU;
		static MObject offsetV;
		
		static MObject tiltU;
		static MObject tiltV;
		
		static MObject tiltColor;
		static MObject tiltColorR;
		static MObject tiltColorG;
		static MObject tiltColorB;
		
		static MObject randamRotX;
		static MObject randamRotY;
		static MObject randamRotZ;
		
		
		static MObject scaleX;
		static MObject scaleY;
		static MObject scaleZ;
		
		static MObject randamScaleX;
		static MObject randamScaleY;
		static MObject randamScaleZ;
		
		static MObject baldnessColor;
		static MObject baldnessColorR;
		static MObject baldnessColorG;
		static MObject baldnessColorB;
		
		static MObject globalHeightOffset;
		static MObject heightColor;
		static MObject heightColorR;
		static MObject heightColorG;
		static MObject heightColorB;
		
		static MObject scaleColorX;
		static MObject scaleColorY;
		static MObject scaleColorZ;
		static MObject scaleColor;
		static MObject scaleColorR;
		static MObject scaleColorG;
		static MObject scaleColorB;
		
		static MObject UVsize;
		static MObject minU, maxU, minV, maxV;
        static MObject UVType;
        
        static MObject animMatrix;
        static MObject timeMapColor;
		static MObject timeMapColorR;
		static MObject timeMapColorG;
		static MObject timeMapColorB;
		
		static MObject timeOffsetMapColor;
		static MObject timeOffsetMapColorR;
		static MObject timeOffsetMapColorG;
		static MObject timeOffsetMapColorB;
		
		
		static MObject outPutMesh;
		
		
private:
		static MStatus getColorFromTex(
			MObject texNode, const MFloatArray& colUCoords,
			const MFloatArray& colVCoords, MVectorArray& colorArray,
			MDoubleArray& transArray);
};

#endif

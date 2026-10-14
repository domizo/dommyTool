#ifndef SPACEDUPLICATER_H
#define SPACEDUPLICATER_H



#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MPxNode.h>



class spaceDuplicater final : public MPxNode
		{
		public:
			MStatus compute( const MPlug& plug, MDataBlock& block ) override;
		
		static  void *creator();
		static  MStatus initialize();
		
		static const MTypeId typeId;
		
	public:
		
		
		static MObject duplicateObjects;
		static MObject typeSeed;
		
		static MObject baldnessMesh;
		
		static MObject numX;
		static MObject numY;
		static MObject numZ;
		
		static MObject offsetX;
		static MObject offsetY;
		static MObject offsetZ;
		
		
		static MObject thickness;
		
		
		static MObject outPutMesh;
		
		
		};

#endif

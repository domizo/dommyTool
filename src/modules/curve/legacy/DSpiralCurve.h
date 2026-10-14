#ifndef DSPIRALCURVE_H
#define DSPIRALCURVE_H

#include <maya/MPxNode.h>



class DSpiralCurve final : public MPxNode
	{
	public:
        static  void *creator();
        static  MStatus initialize();
        
//        virtual void postConstructor();
        
        static const MTypeId typeId;
		
		MStatus compute( const MPlug& plug, MDataBlock& data ) override;
		
		static MObject inputCurve;
        
        static MObject inputTangentCurve;
        
        static MObject curveLength;
        static MObject sampling;
        static MObject offset;
        static MObject spiralOffset;

		static MObject radius;
		static MObject arcStep;
		
		
		static MObject outputCurve;
		
		
	};

#endif

//
//  DPolyWire.hpp
//  dommyTool
//
//  Created by yuzo on 9/25/16.
//
//

#ifndef DPolyWire_h
#define DPolyWire_h

#include <maya/MPxNode.h>


class DPolyWire final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	void postConstructor() override;
	static const MTypeId typeId;
	
public:
	
	
	
	static MObject inCurve;
	static MObject inTangentCurve;
	
	
	static MObject profileMode;
	static MObject profileCurve;
	
	static MObject circleRadius;
	static MObject circleSampling;
	
	
	static MObject sampling;
	static MObject curveLength;
	static MObject curveOffset;
	
	static MObject tangentMode;
	static MObject tangentVector;
	
    static MObject twistRamp;
    static MObject widthRamp;
	
    static MObject globalRotate;
	static MObject globalScale;
	
	static MObject aExpression;
	
	
	static MObject aUVTranslate;
	static MObject aUVScale;
	
	
	
	
	static MObject outMesh;
};



#endif /* DPolyWire_hpp */

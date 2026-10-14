//
//  DCurveDuplicator.hpp
//  dommyTool
//
//  Created by yuzo on 2016/09/25.
//
//

#ifndef DCurveDuplicator_h
#define DCurveDuplicator_h

#include <maya/MPxNode.h>


class DCurveDuplicator final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inObjects;
    
    static MObject inCurve;
    static MObject inTangentCurve;
	
	static MObject duplicateMode;
    static MObject count;
	
	static MObject curveLength;
	static MObject curveOffset;
	static MObject curveStep;
	
    static MObject tangentMode;
    static MObject tangentVector;
    
    static MObject offsetTranslate;
    
    
    static MObject globalRotate;
    static MObject globalScale;
    
    
    static MObject outMesh;
};


#endif /* DCurveDuplicator_hpp */

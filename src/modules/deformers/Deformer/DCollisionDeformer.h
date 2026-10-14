//
//  DCollisionDeformer.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/09.
//
//

#ifndef DCollisionDeformer_h
#define DCollisionDeformer_h

#include <maya/MPxDeformerNode.h>


class DCollisionDeformer final : public MPxDeformerNode {
public:
	void postConstructor() override;
	
    static  void *creator();
    static  MStatus initialize();
    
    MStatus deform( MDataBlock &data,
                    MItGeometry &iter,
                    const MMatrix &mat,
                    unsigned int multiIndex ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static MTypeId typeId;
    static MString typeName;
    
private:
    
    static MObject inObject;
    static MObject inObjectMatrix;
    
    static MObject offset;
	
	static MObject calcType;
	
	static MObject isFalloffEffect;
	static MObject distance;
	static MObject amplitude;
    static MObject falloffRamp;
	
	static MObject surfaceAdjustType;
	static MObject smIterate;
	
	
	static MObject detectType;
	static MObject gridSize;
};

#endif /* DCollisionDeformer_h */

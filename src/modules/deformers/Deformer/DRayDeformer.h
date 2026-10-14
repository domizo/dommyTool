//
//  DRayDeformer.hpp
//  dommyTool
//
//  Created by yuzo on 2016/06/02.
//
//

#ifndef DRayDeformer_h
#define DRayDeformer_h


#include <maya/MPxDeformerNode.h>



class DRayDeformer final : public MPxDeformerNode {
public:
    
    
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
    
    
    
    static MObject rayType;
	static MObject distance;
	
    static MObject inObject;
    static MObject inObjectMatrix;
    
};

#endif /* DRayDeformer_h */

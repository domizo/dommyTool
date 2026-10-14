//
//  DMotionDeformer.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/11.
//
//

#ifndef DMotionDeformer_h
#define DMotionDeformer_h

#include <vector>

#include <maya/MPxDeformerNode.h>
#include <maya/MPointArray.h>


class DMotionDeformer final : public MPxDeformerNode {
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
    
    MStatus getPlugTimeValue(std::vector<MPointArray>& dataarray,
                             long sampling,
                             double timeRange,
                             const MTime& currentTime,
                             unsigned int geomIndex,
                             unsigned int expectedVertexCount);
    
private:
    
    
    
    static MObject timeRange;
    static MObject sampling;
    
    static MObject amplitude;
    static MObject amplitudeRamp;
    
    
    
};


#endif /* DMotionDeformer_h */

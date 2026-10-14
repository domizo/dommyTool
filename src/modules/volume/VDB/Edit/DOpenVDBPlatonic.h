//
//  DOpenVDBPlatonic.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/22.
//
//

#ifndef DOpenVDBPlatonic_h
#define DOpenVDBPlatonic_h



#include <maya/MPxNode.h>



class DOpenVDBPlatonic final : public MPxNode
{
public:
    
    
    DOpenVDBPlatonic() {}
    ~DOpenVDBPlatonic() override = default;
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbSelectedGridNames;
    static MObject aVdbInput;
    
    static MObject aVdbOutput;
    
    
    static MObject solidType;
    static MObject gridclass;
    static MObject scalarRadius;
    static MObject center;
    static MObject voxelSize;
    static MObject halfWidth;
    static MObject fogVolume;
    
};





#endif /* DOpenVDBPlatonic_h */

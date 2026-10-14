//
//  DOpenVDBFromParticle.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#ifndef DOpenVDBFromParticle_h
#define DOpenVDBFromParticle_h


#include <maya/MPxNode.h>


class DOpenVDBFromParticle final : public MPxNode
{
public:
    DOpenVDBFromParticle() {}
    ~DOpenVDBFromParticle() override = default;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inputPointArray;
    static MObject radiusArray;
    static MObject velocityArray;
    
    static MObject aVdbOutput;
    
    static MObject levelSet;
    static MObject gridName;
    static MObject fogVolume;
    static MObject fogVolumeGridName;
    static MObject maskVolume;
    static MObject maskVolumeName;
    
    // Group name (Transform reference)
    //group Reference VDB
    
    //writeintoref
    //merge reference vdb
    
    static MObject voxelSize;//Voxel Size
    static MObject worldSpaceUnits;//Use World Space for Band
    static MObject bandWidth;//Half-Band Voxels
    static MObject bandWidthWS;//Half-Band
    
    static MObject dR;//Particle Radius Scale
    static MObject Rmin;//Minimum Radius in Voxels
    
    static MObject prune;//Prune Level Set
    static MObject maskWidth;//Mask Width Scale
    
    static MObject footprint;//Particle Footprint
    
    static MObject dV;//Velocity Multiplier
    static MObject velocitySpacing;//Velocity Spacing
    
    //attribute transfer no implement
    
};



#endif /* DOpenVDBFromParticle_h */

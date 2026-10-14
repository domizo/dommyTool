//
//  DOpenVDBFilterLevelSet.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/20.
//
//

#ifndef DOpenVDBFilterLevelSet_h
#define DOpenVDBFilterLevelSet_h

#include <maya/MPxNode.h>

class DOpenVDBFilterLevelSet final : public MPxNode {
  public:
    DOpenVDBFilterLevelSet() = default;
    ~DOpenVDBFilterLevelSet() override = default;

    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;

    static MObject isMask;
    static MObject maskName;
    static MObject maskVdbInput;

    static MObject aVdbOutput;

    static MObject filterType;
    static MObject iterations;
    static MObject accuracyType;
    static MObject isInvert;
    static MObject minMask;
    static MObject maxMask;
    static MObject isWorldSpaceUnits;
    static MObject stencilWidth;
    static MObject stencilWidthWorld;
    static MObject halfWidth;
    static MObject halfWidthWorld;
    static MObject voxelOffset;
};

#endif /* DOpenVDBFilterLevelSet_h */

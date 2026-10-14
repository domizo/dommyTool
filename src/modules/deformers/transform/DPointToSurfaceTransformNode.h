//
//  pointToSurfaceTransformNode.h
//  dommyTool
//
//  Created by yuzo on 2015/02/09.
//
//

#ifndef __dommyTool__DPointToSurfaceTransformNode__
#define __dommyTool__DPointToSurfaceTransformNode__

#include <maya/MPxNode.h>

class DPointToSurfaceTransformNode final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject inputPositionX;
    static MObject inputPositionY;
    static MObject inputPositionZ;
    static MObject inputPosition;

    static MObject targetDirectionX;
    static MObject targetDirectionY;
    static MObject targetDirectionZ;
    static MObject targetDirection;

    static MObject inputMesh;

    static MObject offset;

    static MObject outputPositionX;
    static MObject outputPositionY;
    static MObject outputPositionZ;
    static MObject outputPosition;

    static MObject outputNormalX;
    static MObject outputNormalY;
    static MObject outputNormalZ;
    static MObject outputNormal;
};

#endif /* defined(__dommyTool__pointToSurfaceTransformNode__) */

#pragma once

#include <maya/MPxEmitterNode.h>

class DPolygonEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    static MObject inMeshObject;
    static MObject isVertexEmit;
    static MObject isEdgeEmit;
    static MObject edgeSampling;
    static MObject isFaceEmit;
    static MObject faceSampling;
};

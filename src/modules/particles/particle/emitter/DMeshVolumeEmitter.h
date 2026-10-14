#pragma once

#include <maya/MPxEmitterNode.h>

class DMeshVolumeEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    static MObject inMeshObject;
    static MObject transformMatrix;
    static MObject isOutKill;
    static MObject density;
    static MObject densityX;
    static MObject densityY;
    static MObject densityZ;
    static MObject thickness;
    static MObject noise;
    static MObject nSeed;
};

#pragma once

#include <maya/MPxEmitterNode.h>

class DEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    // samplingArea is retained for compatibility with archived scenes. The
    // original active triangle-emission path did not use its value.
    static MObject samplingArea;
    static MObject timeSampling;
    static MObject timeSamples;
    static MObject timeNoise;
};

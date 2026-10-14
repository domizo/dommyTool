#pragma once

#include <maya/MPxEmitterNode.h>

// The historical lower-case class and node name are retained so archived Maya
// scenes continue to resolve the same registered node type.
class curveEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    static MObject startPosition;
    static MObject endPosition;
    static MObject samplingArea;
    static MObject curveRadius;
    static MObject radiusNoise;
    static MObject rNoisefrequency;
    static MObject rNoiseScale;
};

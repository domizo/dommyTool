#pragma once

#include <maya/MPxEmitterNode.h>

class DPEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    static MObject inDP;
    static MObject aEmitTiming;
    static MObject aEmitStartEnd;
    static MObject aVelocityType;
    static MObject aVelocityAttribute;
    static MObject aTransferList;
    static MObject aSourceAttribute;
    static MObject aTargetAttribute;
};

#pragma once

#include <maya/MPxFieldNode.h>
#include <maya/MTypeId.h>

class DPFluidVectorField final : public MPxFieldNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId typeId;

private:
    static MObject inDP;
    static MObject aVectorAttribute;
};

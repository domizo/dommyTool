#pragma once

#include <maya/MPxNode.h>


/// Generates Maya integer-array data from a value, range, ramp, or animation.
class DPIntArrayGen final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject generateType;
    static MObject value;
    static MObject valueSize;
    static MObject aSeed;
    static MObject minRange;
    static MObject maxRange;
    static MObject aShiftRange;
    static MObject aValueRamp;
    static MObject aRampRange;
    static MObject aGlobalOffset;
    static MObject aInOffsetArray;
    static MObject aInStretchArray;
    static MObject outDataArray;
};

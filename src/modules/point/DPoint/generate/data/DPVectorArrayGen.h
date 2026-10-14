#pragma once

#include <maya/MPxNode.h>


/// Generates Maya point- or vector-array data.
class DPVectorArrayGen final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject generateType;
    static MObject aValueType;
    static MObject value;
    static MObject valueSize;
    static MObject isNormalize;
    static MObject seed;
    static MObject minRange;
    static MObject maxRange;
    static MObject aGlobalOffset;
    static MObject aInOffsetArray;
    static MObject aInStretchArray;
    static MObject outDataArray;
};

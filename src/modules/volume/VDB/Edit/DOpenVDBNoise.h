// Historical procedural volume-noise node. Implementation details stay local.
#pragma once

#include <maya/MPxNode.h>

class DOpenVDBNoise final : public MPxNode {
  public:
    DOpenVDBNoise() = default;
    ~DOpenVDBNoise() override = default;
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbSelectedGridNames;
    static MObject aVdbInput;
    static MObject maskMode;
    static MObject maskName;
    static MObject maskThreshold;
    static MObject maskFallOff;
    static MObject maskVdbInput;
    static MObject aVdbOutput;
    static MObject amplitude;
    static MObject frequency;
    static MObject octaves;
    static MObject lacunarity;
    static MObject gain;
    static MObject roughness;
    static MObject surfaceOffset;
    static MObject noiseOffset;
    static MObject noiseMode;
    static MObject aTime;
};

#pragma once

#include <maya/MPxEmitterNode.h>

enum DRadialEmitType
{
    D_RADIAL_EMIT_SOURCE = 0,
    D_RADIAL_EMIT_DIRECTION = 1
};

class DRadialGeometryEmitter final : public MPxEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId id;

private:
    static MObject inputGeometry;
    static MObject transformMatrix;

    static MObject emitType;
    static MObject emitSpeedMultiply;
    static MObject emitSpeedRandom;
    static MObject emitPositionBias;
    static MObject emitSourcePosition;
    static MObject emitSourcePositionX;
    static MObject emitSourcePositionY;
    static MObject emitSourcePositionZ;
    static MObject emitDirection;
    static MObject emitDirectionX;
    static MObject emitDirectionY;
    static MObject emitDirectionZ;

    static MObject isNurbsSampling;
    static MObject sampling;

    static MObject isVertexEmit;
    static MObject isEdgeEmit;
    static MObject edgeSampling;
    static MObject isFaceEmit;
    static MObject faceSampling;

    static MObject nSeed;
    static MObject nFrequency;
    static MObject directionNoise;
    static MObject directionNoiseX;
    static MObject directionNoiseY;
    static MObject directionNoiseZ;
    static MObject noiseScale;
    static MObject noiseScaleX;
    static MObject noiseScaleY;
    static MObject noiseScaleZ;
    static MObject noisePositionOffset;
    static MObject noisePositionOffsetX;
    static MObject noisePositionOffsetY;
    static MObject noisePositionOffsetZ;
};

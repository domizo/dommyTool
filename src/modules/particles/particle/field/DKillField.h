#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxFieldNode.h>
#include <maya/MTypeId.h>

enum DKillType
{
    D_KILL_BBOX = 0,
    D_KILL_MESH,
    D_KILL_SPACERANGE,
    D_KILL_ATTRIBUTE,
    D_KILL_PP_DISTANCE,
    D_KILL_EXPRESSION,
    D_KILL_TYPE_COUNT
};
typedef int D_KILL_TYPE;

enum DKillAttributeType
{
    D_ATTR_1D = 0,
    D_ATTR_2D,
    D_ATTR_3D,
    D_ATTR_VEC_MAGNITUDE,
    D_ATTR_VEC_ANGLE,
    D_ATTR_TYPE_COUNT
};
typedef int D_ATTR_TYPE;

enum DKillConditionType
{
    D_CONDITION_EQUAL = 0,
    D_CONDITION_NOT_EQUAL,
    D_CONDITION_GREATER_THAN,
    D_CONDITION_GREATER_OR_EQUAL,
    D_CONDITION_LESS_THAN,
    D_CONDITION_LESS_OR_EQUAL,
    D_CONDITION_TYPE_COUNT
};
typedef int D_CONDITION_TYPE;

class DKillField final : public MPxFieldNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    // compute() traverses connections and mutates the connected particle
    // system's lifespanPP array, so evaluations of this node must never overlap.
    SchedulingType schedulingType() const override
    {
        return kGloballySerialize;
    }

    static MTypeId id;

    static MObject killType;
    static MObject transformMatrix;
    static MObject inMeshObject;

    static MObject minKillSpaceRange;
    static MObject minKillSpaceRangeX;
    static MObject minKillSpaceRangeY;
    static MObject minKillSpaceRangeZ;

    static MObject maxKillSpaceRange;
    static MObject maxKillSpaceRangeX;
    static MObject maxKillSpaceRangeY;
    static MObject maxKillSpaceRangeZ;

    static MObject killAttributeName;
    static MObject killAttributeType;
    static MObject killAttributeValue;
    static MObject killAttributeValueX;
    static MObject killAttributeValueY;
    static MObject killAttributeValueZ;
    static MObject killCondition;
    static MObject killAngleValue;

    static MObject killDistance;
    static MObject killExpression;
};

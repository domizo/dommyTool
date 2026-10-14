#pragma once
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class DOpenVDBAdvectPoints final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject inDP;
    static MObject aPointGroup;

    static MObject aVdbVelocityInput;
    static MObject aVdbVelocityGridNames;

    static MObject aVdbClosestPointInput;
    static MObject aVdbClosestPointGridNames;

    static MObject aOperation;
    static MObject aIntegration;

    static MObject aIterations;
    static MObject aTimestep;
    static MObject aSubsteps;

    static MObject aOutputStreamlines;

    static MObject outDP;
    static MObject outDC;
};

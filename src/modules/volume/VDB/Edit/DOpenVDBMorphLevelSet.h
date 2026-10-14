#pragma once

#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class DOpenVDBMorphLevelSet final : public MPxNode {
  public:
    MStatus compute(const MPlug&, MDataBlock&) override;
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    static MObject aTargetVdbInput;
    static MObject aTargetVdbSelectedGridNames;
    static MObject aMaskName;
    static MObject aInvertMask;
    static MObject aMinMask;
    static MObject aMaxMask;
    static MObject aMaskVdbInput;
    static MObject aTimestep;
    static MObject aAdvectSpatial;
    static MObject aAdvectTemporal;
    static MObject aSteps;
    static MObject aRenormSpatial;
    static MObject aRenormTemporal;
    static MObject aVdbOutput;
};

#pragma once

#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class DOpenVDBAdvect final : public MPxNode {
  public:
    MStatus compute(const MPlug&, MDataBlock&) override;
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    static MObject aVdbVelocityInput;
    static MObject aVdbVelocityGridNames;
    static MObject aRespectGridClass;
    static MObject aTimestep;
    static MObject aSubsteps;
    static MObject aAdvectionScheme;
    static MObject aLimiterScheme;
    static MObject aSpatialScheme;
    static MObject aTemporalScheme;
    static MObject aRenormalizationSteps;
    static MObject aSpatialRenormalization;
    static MObject aTemporalRenormalization;
    static MObject aVdbOutput;
};

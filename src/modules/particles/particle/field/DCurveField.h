#pragma once

#include <maya/MDataBlock.h>
#include <maya/MDoubleArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxFieldNode.h>
#include <maya/MTypeId.h>
#include <maya/MVectorArray.h>

class DCurveField final : public MPxFieldNode {
  public:
    static void* creator();
    static MStatus initialize();

    void postConstructor() override;
    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    // Ramp and NURBS function-set access stays serial until it has been
    // validated with Maya's parallel evaluation manager.
    SchedulingType schedulingType() const override {
        return kSerial;
    }

    static MObject inputTargetCurve;
    static MObject tangentAffectDistance;
    static MObject areaRadius;
    static MObject aRadiusRamp;
    static MObject aDamping;
    static MObject aAttraction;
    static MObject aForwardScale;
    static MObject aToCurveScale;
    static MObject aForwardRamp;
    static MObject aToCurveRamp;
    static MObject aIsAffectTip;
    static MObject aInnerMethod;
    static MObject aBlend;
    static MObject aBlendRamp;

    static MTypeId id;

  private:
    MStatus calculateForces(MDataBlock& block, const MVectorArray& points,
                            const MVectorArray& velocities, const MDoubleArray& masses,
                            MVectorArray& outputForce);
};

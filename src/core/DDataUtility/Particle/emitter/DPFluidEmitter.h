//
//  DPFluidEmitter.h
//  dommyTool
//
//  Created by yuzo on 12/1/16.
//

#pragma once

#include <maya/MPxFluidEmitterNode.h>

namespace dommy {
namespace fluid {
struct GridPlan;
}
}

class DPFluidEmitter final : public MPxFluidEmitterNode
{
public:
    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    MStatus fluidEmitter(const MObject& fluid,
                         const MMatrix& worldMatrix,
                         int plugIndex) override;
    SchedulingType schedulingType() const override { return kSerial; }

    static MTypeId typeId;

private:
    struct EmissionValues;

    MStatus emitFromPoints(MFnFluid& fluid,
                           const MMatrix& fluidWorldMatrix,
                           MDataBlock& data,
                           double deltaTime,
                           double conversion,
                           double dropoff,
                           bool writeFalloff,
                           const dommy::fluid::GridPlan& gridPlan,
                           const EmissionValues& emission);
    MStatus emitFromGridAttributes(
        MFnFluid& fluid,
        MDataBlock& data,
        const dommy::fluid::GridPlan& gridPlan,
        const EmissionValues& emission);

    static MObject inDP;
    static MObject aDPEmitType;
    static MObject aDensityAttribute;
    static MObject aHeatAttribute;
    static MObject aFuelAttribute;
    static MObject aColorAttribute;
};

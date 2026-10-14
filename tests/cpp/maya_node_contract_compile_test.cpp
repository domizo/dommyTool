#include "DDataUtility/Utility/DPMerge.h"
#include "DDataUtility/Particle/DPEmitter.h"
#include "DDataUtility/Particle/emitter/DPFluidEmitter.h"
#include "DDataUtility/Particle/field/DPFluidNoiseField.h"
#include "DDataUtility/Particle/field/DPFluidVectorField.h"
#include "DDataUtility/Particle/field/DPGoalField.h"
#include "particle/emitter/DMeshVolumeEmitter.h"
#include "particle/emitter/DPolygonEmitter.h"
#include "particle/emitter/DRadialGeometryEmitter.h"
#include "particle/field/DKillField.h"
#include "DCurve/IO/DWriteDCurveToAi.h"
#include "VDB/Convert/DOpenVDBToMesh.h"
#include "VDB/IO/DWriteOpenVDB.h"
#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DEvaluationContext.h"

#include <type_traits>

static_assert(std::is_base_of<MPxNode, DPMerge>::value,
              "DPMerge must remain a Maya dependency node");
static_assert(std::is_base_of<MPxEmitterNode, DPEmitter>::value,
              "DPEmitter must remain a Maya particle emitter node");
static_assert(std::is_base_of<MPxFluidEmitterNode, DPFluidEmitter>::value,
              "DPFluidEmitter must remain a Maya fluid emitter node");
static_assert(std::is_base_of<MPxFieldNode, DPFluidVectorField>::value,
              "DPFluidVectorField must remain a Maya field node");
static_assert(std::is_base_of<MPxFieldNode, DPFluidNoiseField>::value,
              "DPFluidNoiseField must remain a Maya field node");
static_assert(std::is_base_of<MPxFieldNode, DPGoalField>::value,
              "DPGoalField must remain a Maya field node");
static_assert(std::is_base_of<MPxEmitterNode, DMeshVolumeEmitter>::value,
              "DMeshVolumeEmitter must remain a Maya particle emitter node");
static_assert(std::is_base_of<MPxEmitterNode, DPolygonEmitter>::value,
              "DPolygonEmitter must remain a Maya particle emitter node");
static_assert(std::is_base_of<MPxEmitterNode, DRadialGeometryEmitter>::value,
              "DRadialGeometryEmitter must remain a Maya particle emitter node");
static_assert(std::is_base_of<MPxFieldNode, DKillField>::value,
              "DKillField must remain a Maya field node");
static_assert(std::is_base_of<MPxNode, DOpenVDBToMesh>::value,
              "DOpenVDBToMesh must remain a Maya dependency node");
static_assert(std::is_base_of<MPxNode, DWriteDCurveToAi>::value,
              "DWriteDCurveToAi must remain a Maya dependency node");
static_assert(std::is_base_of<MPxNode, DWriteOpenVDB>::value,
              "DWriteOpenVDB must remain a Maya dependency node");

namespace {

class SchedulingApiProbe final : public MPxNode {
  public:
    MStatus compute(const MPlug&, MDataBlock&) override {
        return MS::kUnknownParameter;
    }

    SchedulingType schedulingType() const override {
        return kUntrusted;
    }
};

} // namespace

MStatus instantiateParticleAttributeApi(const MFnParticleSystem& particle) {
    return dommy::ensureParticleRgbAttributes(particle);
}

MStatus instantiateEvaluationContextApi(MDataBlock& data, MTime& time) {
    return dommy::maya::getEvaluationTime(data, time);
}

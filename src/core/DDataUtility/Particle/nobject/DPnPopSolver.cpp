//
//  DPnPopSolver.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/19.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPnPopSolver.h"
#include "common/utility/DStatusOperation.h"
#include "common/object/DNucleusCollisionUtils.h"
#include "common/object/DNucleusParticleControls.h"
#include "common/object/DNucleusSolverControls.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaPluginDataOutput.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DStringOp.h"
#include "common/utility/DParticleFieldMath.h"

#include <cmath>
#include <limits>


#if defined(DOMMY_ENABLE_DEBUG_LOGS) && DOMMY_ENABLE_DEBUG_LOGS
const static bool DP_NPOP_SOLVER_DEBUG = true;
#else
const static bool DP_NPOP_SOLVER_DEBUG = false;
#endif


const MTypeId DPnPopSolver::typeId( ID_DP_NPOP_SOLVER );



MObject DPnPopSolver::inDP;


MObject DPnPopSolver::aStartTime;
MObject DPnPopSolver::aCurrentTime;
//    static MObject prevDP;
//    static MObject nextDP;


//particle attribute
MObject DPnPopSolver::aVelocityAttribute;

MObject DPnPopSolver::aRadius;
MObject DPnPopSolver::aRadiusAttribute;

MObject DPnPopSolver::aInverseMass;
MObject DPnPopSolver::aMassAttribute;


MObject DPnPopSolver::aBounce;
MObject DPnPopSolver::aFriction;
MObject DPnPopSolver::aDamping;

//liquid Attribute
MObject DPnPopSolver::aLiquidSimulation;
MObject DPnPopSolver::aIncompressibility;
MObject DPnPopSolver::aRestDensity;
MObject DPnPopSolver::aLiquidRadiusScale;
MObject DPnPopSolver::aViscosity;
MObject DPnPopSolver::aSurfaceTension;

//Collision
MObject DPnPopSolver::aMaxIterations;
MObject DPnPopSolver::aMaxSelfCollisionIterations;

MObject DPnPopSolver::aSelfCollisionSoftness;
MObject DPnPopSolver::aDragAndLift;
MObject DPnPopSolver::aCollide;
MObject DPnPopSolver::aSelfCollide;
MObject DPnPopSolver::aSelfCollideWidth;


//Solver
MObject DPnPopSolver::aEnable;
MObject DPnPopSolver::aSubsteps;
MObject DPnPopSolver::aSolverIterations;

MObject DPnPopSolver::aDisableGravity;
MObject DPnPopSolver::aGravity;
MObject DPnPopSolver::aGravityDirection;

MObject DPnPopSolver::aAirDensity;
MObject DPnPopSolver::aWindSpeed;
MObject DPnPopSolver::aWindDirection;
MObject DPnPopSolver::aWindNoiseIntensity;





//Force
MObject DPnPopSolver::aEnableForce;

MObject DPnPopSolver::aForceAttribute;
MObject DPnPopSolver::aForceEnvelope;
MObject DPnPopSolver::aIsRealForce;


//collision object
MObject DPnPopSolver::aInCollideObject;

//Pop
MObject DPnPopSolver::inPOP;

MObject DPnPopSolver::outDP;



DPnPopSolver::DPnPopSolver()
    : m_particleInitialized(false)
{
}

void *DPnPopSolver::creator()
{
    return ::dommy::maya::createObject("DPnPopSolver", [&]() {
        return new DPnPopSolver();
    });
}

#pragma mark -
#pragma mark solver method

MStatus DPnPopSolver::ensureNParticle()
{
    if (m_particleInitialized) return MS::kSuccess;
    const MStatus status = fNObject.createNParticle();
    if (status) m_particleInitialized = true;
    return status;
}

MStatus DPnPopSolver::initDPParticle(const DPData& dpoint, MDataBlock& data) {
    const std::size_t pointCount = dpoint.numberOfPoints();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount) {
        MGlobal::displayError("DPnPopSolver: point count exceeds the generation budget.");
        return MS::kInvalidParameter;
    }

    MFloatPointArray positions;
    DOMMY_RETURN_IF_FAILURE(dommy::geometry::copyPointsToMayaArray(
        *dpoint.getPositionPtrConst(), positions));
    if (positions.length() != pointCount) return MS::kFailure;

    MFloatPointArray velocities;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(pointCount,
        [](unsigned int, MFloatPoint& value) -> MStatus {
            value = MFloatPoint(0.0f, 0.0f, 0.0f);
            return MS::kSuccess;
        }, velocities));
    MStatus status;
    const MDataHandle velocityHandle = data.inputValue(aVelocityAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string velocityName = velocityHandle.asString().asChar();
    if (dpoint.isExistAttr(velocityName)) {
        DOMMY_RETURN_IF_FAILURE(dommy::attribute::copyVector3MayaArray<Imath::V3f>(
            dpoint.getAttr(velocityName), pointCount,
            [](const Imath::V3f& value) { return MFloatPoint(value[0], value[1], value[2], 0.0f); },
            velocities));
    }
    if (velocities.length() != pointCount) return MS::kFailure;
    for (unsigned int index = 0U; index < velocities.length(); ++index) {
        if (!dommy::maya::isFinitePoint(velocities[index])) return MS::kInvalidParameter;
    }

    DScalarDataList radiusArray, massArray;
    // Preserve the empty-input policy: radius/mass are neither read nor set.
    if (pointCount != 0U) {
        const MDataHandle radiusHandle = data.inputValue(aRadius, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float radius = radiusHandle.asFloat();
        const MDataHandle radiusNameHandle = data.inputValue(aRadiusAttribute, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const std::string radiusName = radiusNameHandle.asString().asChar();
        radiusArray.assign(pointCount, radius);
        if (dpoint.isExistAttr(radiusName)) {
            if (!dommy::attribute::convertScalarAttribute(
                    dpoint.getAttr(radiusName), pointCount, radiusArray)) return MS::kInvalidParameter;
        }

        const MDataHandle massHandle = data.inputValue(aInverseMass, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float mass = massHandle.asFloat();
        const MDataHandle massNameHandle = data.inputValue(aMassAttribute, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const std::string massName = massNameHandle.asString().asChar();
        massArray.assign(pointCount, mass);
        if (dpoint.isExistAttr(massName)) {
            if (!dommy::attribute::convertScalarAttribute(
                    dpoint.getAttr(massName), pointCount, massArray)) return MS::kInvalidParameter;
        }
        // Validate the effective values, retaining attribute-over-default priority.
        for (const DScalarDataList* values : {&radiusArray, &massArray}) {
            if (values->size() != pointCount) return MS::kFailure;
            for (float value : *values) {
                if (!std::isfinite(value) || value < 0.0f) return MS::kInvalidParameter;
            }
        }
    }
    dommy::maya::ParticleControls controls;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readParticleControls<DPnPopSolver>(data, controls));

    // Complete all local allocation, input reads and validation before changing
    // Nucleus. A failed native setter is propagated, but cannot be rolled back.
    DOMMY_RETURN_IF_FAILURE(fNObject.setTopology(static_cast<int>(pointCount)));
    DOMMY_RETURN_IF_FAILURE(fNObject.setPositions(positions, true));
    DOMMY_RETURN_IF_FAILURE(fNObject.setVelocities(velocities));
    if (pointCount != 0U) {
        DOMMY_RETURN_IF_FAILURE(fNObject.setThickness(radiusArray.data()));
        DOMMY_RETURN_IF_FAILURE(fNObject.setInverseMass(massArray.data()));
    }
    return dommy::maya::applyParticleControls(fNObject, controls);
}

MStatus DPnPopSolver::updateCommonDPParticle(MDataBlock& data) {
    dommy::maya::ParticleControls controls;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readParticleControls<DPnPopSolver>(data, controls));
    return dommy::maya::applyParticleControls(fNObject, controls);
}


#pragma mark -
#pragma mark collision method

MStatus DPnPopSolver::syncCollisions(MDataBlock& data)
{
    return dommy::maya::syncNucleusRigidCollisions(
        data, aInCollideObject, solver, colObjList);
}
#pragma mark field method

MStatus DPnPopSolver::applyFields(double deltaTime)
{
    if (!dommy::field::isValidFieldTimeStep(deltaTime)) {
        MGlobal::displayError("DPnPopSolver field time step must be finite and positive.");
        return MS::kFailure;
    }

    unsigned int numPoint = 0;
    MStatus status = fNObject.getNumVertices(numPoint);
    if (status != MS::kSuccess) return status;
    if(DP_NPOP_SOLVER_DEBUG) DOMMY_MAYA_DEBUG("DPnPopSolver", "numPoint:" <<  numPoint);

    if (numPoint == 0U) return MS::kSuccess;
    if (numPoint > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;

    status = fNObject.getPositions(m_positions);
    if (status != MS::kSuccess) return status;

    MFloatPointArray velocities;
    status = fNObject.getVelocities(velocities);
    if (status != MS::kSuccess) return status;

    MFloatArray inverseMasses;
    status = fNObject.getInverseMass(inverseMasses);
    if (status != MS::kSuccess) return status;

    if (!dommy::field::hasMatchingParticleFieldCounts(
            numPoint, m_positions.length(), velocities.length(),
            inverseMasses.length())) {
        MGlobal::displayError(
            "DPnPopSolver field inputs do not match the particle count.");
        return MS::kFailure;
    }

    MPointArray positions;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(numPoint,
        [&](unsigned int index, MPoint& value) -> MStatus {
            const MFloatPoint& position = m_positions[index];
            if (!dommy::maya::isFinitePoint(position)) return MS::kFailure;
            // Keep the historical Cartesian XYZ interpretation, ignoring w.
            value = MPoint(position.x, position.y, position.z);
            return MS::kSuccess;
        }, positions));
    MVectorArray velocityVectors;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(numPoint,
        [&](unsigned int index, MVector& value) -> MStatus {
            const MFloatPoint& velocity = velocities[index];
            if (!dommy::maya::isFinitePoint(velocity)) return MS::kFailure;
            value = MVector(velocity.x, velocity.y, velocity.z);
            return MS::kSuccess;
        }, velocityVectors));
    MDoubleArray masses;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(numPoint,
        [&](unsigned int index, double& value) -> MStatus {
            return dommy::field::fieldMassFromInverseMass(inverseMasses[index], value)
                ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
        }, masses));

    MVectorArray totalForce;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(numPoint,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        }, totalForce));
    MStatus iteratorStatus;
    MItDag iterator(MItDag::kDepthFirst, MFn::kField, &iteratorStatus);
    if (iteratorStatus != MS::kSuccess) return iteratorStatus;

    bool hasField = false;
    for (;;) {
        const bool done = iterator.isDone(&iteratorStatus);
        if (iteratorStatus != MS::kSuccess) return iteratorStatus;
        if (done) break;

        MObject fieldObject = iterator.currentItem(&iteratorStatus);
        if (iteratorStatus != MS::kSuccess) return iteratorStatus;
        MFnField field(fieldObject, &iteratorStatus);
        if (iteratorStatus != MS::kSuccess) return iteratorStatus;

        MVectorArray fieldForce;
        status = field.getForceAtPoint(
            positions, velocityVectors, masses, fieldForce, deltaTime);
        if (status != MS::kSuccess) return status;
        if (fieldForce.length() != numPoint) {
            MGlobal::displayError(
                "A Maya field returned a force count that does not match the particles.");
            return MS::kFailure;
        }

        for (unsigned int index = 0; index < numPoint; ++index) {
            const MVector& accumulated = totalForce[index];
            const MVector& additional = fieldForce[index];
            MVector sum;
            if (!dommy::field::checkedAccumulateFieldForce(
                    accumulated.x, additional.x, sum.x) ||
                !dommy::field::checkedAccumulateFieldForce(
                    accumulated.y, additional.y, sum.y) ||
                !dommy::field::checkedAccumulateFieldForce(
                    accumulated.z, additional.z, sum.z)) {
                MGlobal::displayError(
                    "Maya field forces must contain only finite values.");
                return MS::kFailure;
            }
            status = totalForce.set(sum, index);
            if (status != MS::kSuccess) return status;
        }
        hasField = true;

        iteratorStatus = iterator.next();
        if (iteratorStatus != MS::kSuccess) return iteratorStatus;
    }

    if (!hasField) return MS::kSuccess;

    MFloatPointArray newVelocities;
    const double maximumFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(numPoint,
        [&](unsigned int index, MFloatPoint& value) -> MStatus {
            const MFloatPoint& current = velocities[index];
            const MVector& force = totalForce[index];
            const double inverseMass = static_cast<double>(inverseMasses[index]);
            double updated[3] = {0.0, 0.0, 0.0};
            if (!dommy::field::integrateFieldVelocity(
                    static_cast<double>(current.x), force.x, inverseMass,
                    deltaTime, updated[0]) ||
                !dommy::field::integrateFieldVelocity(
                    static_cast<double>(current.y), force.y, inverseMass,
                    deltaTime, updated[1]) ||
                !dommy::field::integrateFieldVelocity(
                    static_cast<double>(current.z), force.z, inverseMass,
                    deltaTime, updated[2]) ||
                std::fabs(updated[0]) > maximumFloat ||
                std::fabs(updated[1]) > maximumFloat ||
                std::fabs(updated[2]) > maximumFloat) {
                MGlobal::displayError(
                    "DPnPopSolver field integration produced an invalid velocity.");
                return MS::kFailure;
            }

            value = MFloatPoint(static_cast<float>(updated[0]),
                static_cast<float>(updated[1]), static_cast<float>(updated[2]),
                current.w);
            return MS::kSuccess;
        }, newVelocities));

    return fNObject.setVelocities(newVelocities);
}


#pragma mark -
#pragma mark solver method


MStatus DPnPopSolver::initSolver(MDataBlock& data) {
    if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "initSolver:");
    
    MStatus status;
    const MDataHandle startTimeHandle = data.inputValue(aStartTime, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    float startTime = 0.0f;
    if (!dommy::numeric::checkedFloatCast(
            startTimeHandle.asTime().as(MTime::kSeconds), startTime)) {
        return MS::kInvalidParameter;
    }

    solver = MnSolver();
    DOMMY_RETURN_IF_FAILURE(solver.createNSolver());
    status = solver.setStartTime(startTime);
    if (status != MS::kSuccess) {
        return status;
    }

    colObjList.clear();
    status = solver.addNObject(&fNObject);
    if (status != MS::kSuccess) {
        return status;
    }
    return syncCollisions(data);
}


MStatus DPnPopSolver::calcSolver(MDataBlock& data, float solverTime) {
    return dommy::maya::solveNucleus<DPnPopSolver>(data, solver, solverTime);
}
#pragma mark -
#pragma mark main compute



MStatus DPnPopSolver::compute( const MPlug& plug, MDataBlock& data ) try
{
    if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "Calling DPnPopSolver::compute \n");
    if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "Plug: " << plug.name().asChar());
    
    if (plug != outDP) return MS::kUnknownParameter;

    MStatus stat = ensureNParticle();
    if (!stat) {
        MGlobal::displayError(
            "DPnPopSolver: failed to create the Nucleus particle.");
        return stat;
    }
        
        
        //create output
        MFnPluginData pluginData;
        const MObject outputObject = pluginData.create(DPData::id, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (outputObject.isNull()) return MS::kFailure;
        
        DPData* const outputDPData(dommy::maya::pluginDataPointer<DPData>(pluginData, &stat));
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        if (outputDPData) {
            
            const MDataHandle enabledHandle = data.inputValue(aEnable, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool enabled = enabledHandle.asBool();
            
            
            if (enabled) {
                
                //Get time
                const MDataHandle currentTimeHandle = data.inputValue(aCurrentTime, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MTime time = currentTimeHandle.asTime();
                const MDataHandle startTimeHandle = data.inputValue(aStartTime, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MTime startTime = startTimeHandle.asTime();
                if (!std::isfinite(startTime.value()) ||
                    !std::isfinite(startTime.as(MTime::kSeconds))) {
                    return MS::kInvalidParameter;
                }
                float solveTime = 0.0f;
                if (!dommy::numeric::checkedFloatCast(time.as(MTime::kSeconds), solveTime)) {
                    return MS::kInvalidParameter;
                }
                if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "time:" << time.value());
                if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "solveTime:" << solveTime);
                
                
                
                
                
                if(time <= startTime) {
                    if(DP_NPOP_SOLVER_DEBUG)DOMMY_MAYA_DEBUG("DPnPopSolver", "first frame, init the simulation");
                    
#pragma mark init
                    
                    
                    
                    
                    // Only an empty, unconnected input takes the legacy
                    // common-controls path. Read/type/topology failures must
                    // not masquerade as an absent optional input.
                    MDataHandle inputHandle = data.inputValue(inDP, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const DPData* inputDPData = nullptr;
                    if (!inputHandle.data().isNull()) {
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
                            inputHandle, DPData::id, inputDPData));
                        if (!inputDPData->hasValidPointTopology()) return MS::kInvalidParameter;
                    } else {
                        const MPlug inputPlug(thisMObject(), inDP);
                        const bool connected = inputPlug.isConnected(&stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        if (connected) return MS::kFailure;
                    }
                    
                    if (inputDPData) {
                        //Init NParticle
                        if(DP_NPOP_SOLVER_DEBUG) DOMMY_MAYA_DEBUG("DPnPopSolver", "Init NParticle");
                        stat = initDPParticle(*inputDPData, data);
                    } else {
                        stat = updateCommonDPParticle(data);
                    }
                    if (stat != MS::kSuccess) return stat;
                    
                    
                    
                    
                    if(DP_NPOP_SOLVER_DEBUG) DOMMY_MAYA_DEBUG("DPnPopSolver", "Init Solver");
                    //solver set
                    stat = initSolver(data);
                    if (stat != MS::kSuccess) return stat;

                    //calc first frame
                    stat = calcSolver(data, solveTime);
                    if (stat != MS::kSuccess) return stat;
                    
                } else {
                    
                    
                    // Keep the solver's non-owning collider pointers
                    // synchronized with this evaluation's data handles.
                    stat = syncCollisions(data);
                    if (stat != MS::kSuccess) return stat;
                    
                    
#pragma mark calculate particle
                    
                    if(DP_NPOP_SOLVER_DEBUG) DOMMY_MAYA_DEBUG("DPnPopSolver", "solve time:" <<  solveTime);
                    
                    // MFnField expects an increment in seconds, not the
                    // absolute solve time. This stateful node is evaluated one
                    // UI frame at a time, matching the Nucleus workflow.
                    const double fieldDeltaTime =
                        MTime(1.0, MTime::uiUnit()).as(MTime::kSeconds);
                    stat = applyFields(fieldDeltaTime);
                    if (stat != MS::kSuccess) return stat;
                    
                    //calc curent frame
                    stat = calcSolver(data, solveTime);
                    if (stat != MS::kSuccess) return stat;
                } //simlation
                
            }//check enable solver
            
            
            
            
            
            unsigned int numPoint=0;
            stat = fNObject.getNumVertices(numPoint);
            if (stat != MS::kSuccess) return stat;
            if(DP_NPOP_SOLVER_DEBUG) DOMMY_MAYA_DEBUG("DPnPopSolver", "numPoint:" <<  numPoint);
            
            if (numPoint>0) {
                
               
                
                stat = fNObject.getPositions(m_positions);
                if (stat != MS::kSuccess) return stat;
                if (m_positions.length() != numPoint) {
                    return MS::kFailure;
                }
                
                
                
//                MFloatPointArray velocities;
//                fNObject.getVelocities(velocities);
                
                outputDPData->setPosition(m_positions);
//                MFloatArray radius;
//                fNObject.getThickness(radius);
//
//                MFloatArray inverseMass;
//                fNObject.getInverseMass(inverseMass);
                
//                outputDPData->updateBBox();
            }
            
            
            outputDPData->updateBBox();
            
            MDataHandle output = data.outputValue(outDP, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishPluginDataObject(
                    output, outputObject, *outputDPData));
            return data.setClean(plug);
            
        }//check output dpoint

    return MS::kFailure;
}
DOMMY_COMPUTE_CATCH("DPnPopSolver")




MStatus DPnPopSolver::initialize() try {
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
#pragma mark time
    MFnUnitAttribute uniAttr;
    aStartTime = uniAttr.create( "StartTime", "stm" , MFnUnitAttribute::kTime,  1.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartTime));
    
    
    aCurrentTime = uniAttr.create( "CurrentTime", "ctm" , MFnUnitAttribute::kTime,  0.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));
    
    
    
#pragma mark particle attribute
    MFnStringData fnStringData;
    const MObject aVelocityAttributeDefault = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityAttribute = tAttr.create("VelocityAttribute", "vattr", MFnData::kString, aVelocityAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttribute));
    
    
    //attribute
    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.05f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    const MObject aRadiusAttributeDefault = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "radattr", MFnData::kString, aRadiusAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
    
    
    aInverseMass = nAttr.create( "InverseMass", "invm", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInverseMass));
    
    const MObject aMassAttributeDefault = fnStringData.create("mass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMassAttribute = tAttr.create("MassAttribute", "msattr", MFnData::kString, aMassAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassAttribute));
    
    
    
    aBounce = nAttr.create( "Bounce", "bnc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBounce));
    
    aFriction = nAttr.create( "Friction", "frc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFriction));
    
    aDamping = nAttr.create( "Damping", "dmp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));
    
    
    
#pragma mark liquid attribute
    
    
    aLiquidSimulation = nAttr.create( "LiquidSimulation", "liqsims", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLiquidSimulation));
    
    aIncompressibility = nAttr.create( "Incompressibility", "incmpb", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIncompressibility));
    
    aRestDensity = nAttr.create( "RestDensity", "resden", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestDensity));
    
    aLiquidRadiusScale = nAttr.create( "LiquidRadiusScale", "liqradscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLiquidRadiusScale));
    
    aViscosity = nAttr.create( "Viscosity", "visco", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aViscosity));
    
    aSurfaceTension = nAttr.create( "SurfaceTension", "srften", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceTension));
    
    
    
    
    
    
    
#pragma mark solver attribute
    
    aEnable = nAttr.create( "Enable", "ebl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnable));
    
    aSubsteps = nAttr.create( "Substeps", "sbstp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));
    
    aSolverIterations = nAttr.create( "SolverIterations", "slvmxiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolverIterations));
    
    
    aDisableGravity = nAttr.create( "DisableGravity", "disg", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisableGravity));
    
    aGravity = nAttr.create( "Gravity", "grvy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(9.8f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravity));
    
    aGravityDirection = nAttr.create( "GravityDirection", "grvydir", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, -1.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravityDirection));
    
    aAirDensity = nAttr.create( "AirDensity", "adsy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAirDensity));
    
    aWindSpeed = nAttr.create( "WindSpeed", "wspd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindSpeed));
    
    aWindDirection = nAttr.create( "WindDirection", "wdspd", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 1.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindDirection));
    
    aWindNoiseIntensity = nAttr.create( "WindNoiseIntensity", "wit", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindNoiseIntensity));
    
    
    
    
    
    
#pragma mark force attribute
    
    aEnableForce = nAttr.create( "EnableForce", "eblfrc", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnableForce));
    
    
    const MObject aForceAttributeDefault = fnStringData.create("force", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aForceAttribute = tAttr.create("ForceAttribute", "frcattr", MFnData::kString, aForceAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForceAttribute));
    
    
    aForceEnvelope = nAttr.create( "ForceEnvelope", "frcenv", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForceEnvelope));
    
    
    aIsRealForce = nAttr.create( "IsRealForce", "isrlfrc", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRealForce));
    
#pragma mark collide attribute
    
    
    aCollide = nAttr.create( "Collide", "coll", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollide));
    
    aSelfCollide = nAttr.create( "SelfCollide", "slfcoll", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollide));
    
    aSelfCollideWidth = nAttr.create( "SelfCollideWidth", "slfcollw", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollideWidth));
    
    
    aMaxIterations = nAttr.create( "MaxIterations", "maxiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxIterations));
    
    aMaxSelfCollisionIterations = nAttr.create( "MaxSelfCollisionIterations", "maxslfiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSelfCollisionIterations));
    
    aSelfCollisionSoftness = nAttr.create( "SelfCollisionSoftness", "slfcollsft", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollisionSoftness));
    
    aDragAndLift = nAttr.create( "DragAndLift", "drgalft", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.010));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDragAndLift));
    
    
    aInCollideObject = tAttr.create("InCollideObject", "incolobj", MFnData::kNObject, MObject::kNullObj, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInCollideObject));
    
    
#pragma mark Pop attribute
    
    
    MFnMessageAttribute fnMsgAttr;
    inPOP = fnMsgAttr.create("inPOP", "inpop", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inPOP));
    
#pragma mark output attribute
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    const MObject simulationInputs[] = {
        inDP,
        aStartTime,
        aCurrentTime,
        aVelocityAttribute,
        aRadius,
        aRadiusAttribute,
        aInverseMass,
        aMassAttribute,
        aBounce,
        aFriction,
        aDamping,
        aLiquidSimulation,
        aIncompressibility,
        aRestDensity,
        aLiquidRadiusScale,
        aViscosity,
        aSurfaceTension,
        aMaxIterations,
        aMaxSelfCollisionIterations,
        aSelfCollisionSoftness,
        aDragAndLift,
        aCollide,
        aSelfCollide,
        aSelfCollideWidth,
        aEnable,
        aSubsteps,
        aSolverIterations,
        aDisableGravity,
        aGravity,
        aGravityDirection,
        aAirDensity,
        aWindSpeed,
        aWindDirection,
        aWindNoiseIntensity,
        aEnableForce,
        aForceAttribute,
        aForceEnvelope,
        aIsRealForce,
        aInCollideObject,
    };
    for (const MObject& input : simulationInputs) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outDP));
    }
    
    
    
    
    
    
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPnPopSolver", "initialize", std::current_exception());
}

//
//  DMbtSoft.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/10.
//

#include "DMbtSoft.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "SoftBody/DSoftBodyPreparation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"
#include "DDataOp.h"
#include "DNodeOperation.h"

//bullet
#include "DBulletDataHeaders.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"





#include "mathUtils.h"
#include "commonBulletRigidBody.h"
#include "DBulletGeometryPreparation.h"

#include "db_collision_base_shapes.h"
#include "db_collision_mesh_shapes.h"


#define DM_BULLET_SOFT_DEBUG DOMMY_ENABLE_DEBUG_LOGS


const MTypeId DMbtSoft::typeId( ID_DM_BT_SOFT );

MObject DMbtSoft::inDM;

MObject DMbtSoft::aMass;
MObject DMbtSoft::aVolumeMass;

MObject DMbtSoft::aRestLengthScale;
MObject DMbtSoft::aRestitution;
MObject DMbtSoft::aFriction;
MObject DMbtSoft::aRollingFriction;
MObject DMbtSoft::aSpinningFriction;

MObject DMbtSoft::aStiffness;
MObject DMbtSoft::aAngularStiffness;
MObject DMbtSoft::aVolumeStiffness;

MObject DMbtSoft::aIsGenerateBendConstraint;
MObject DMbtSoft::aIsCreateCluster;

MObject DMbtSoft::aAeromodel;
MObject DMbtSoft::aVelocityCorrectionFactor;
MObject DMbtSoft::aDynamicFrictionCoeff;
MObject DMbtSoft::aDamping;
MObject DMbtSoft::aDrag;
MObject DMbtSoft::aLift;
MObject DMbtSoft::aPressure;
MObject DMbtSoft::aVolumeConversation;
MObject DMbtSoft::aPoseMatch;

MObject DMbtSoft::aRigidContactHardness;
MObject DMbtSoft::aKineticContactHardness;
MObject DMbtSoft::aSoftContactHardness;
MObject DMbtSoft::aAnchorContactHardness;

MObject DMbtSoft::aRigidImpulseSplit;
MObject DMbtSoft::aKinetictImpulseSplit;
MObject DMbtSoft::aSoftImpulseSplit;

MObject DMbtSoft::aSetPoseFrame;
MObject DMbtSoft::aSetPoseVolume;
MObject DMbtSoft::aIsRandomizeConstraints;

MObject DMbtSoft::aBendConstraintDistance;
MObject DMbtSoft::aNumClusters;

//MObject DMbtRigid::aRestitution;//反発
//MObject DMbtRigid::aFriction;//摩擦
//MObject DMbtRigid::aRollingFriction;//回転摩擦
//MObject DMbtRigid::aLinearDamping;//減衰
//MObject DMbtRigid::aAngularDamping;//回転減衰

//MObject DMbtSoft::aInitialPosition;
//MObject DMbtSoft::aInitialRotation;
//MObject DMbtSoft::aInitialVelocity;
//MObject DMbtSoft::aInitialSpin;

//MObject DMbtSoft::aExternalForce;
//MObject DMbtSoft::aExternalTorque;


MObject DMbtSoft::aCollisionMargin;
MObject DMbtSoft::aPositionIterations;
MObject DMbtSoft::aVelocityIterations;
MObject DMbtSoft::aDriftIterations;
MObject DMbtSoft::aClusterIterations;

MObject DMbtSoft::aMaxVolume;

MObject DMbtSoft::aCollideRigidBody;
MObject DMbtSoft::aCollideSoftBody;
MObject DMbtSoft::aCollideSelf;
MObject DMbtSoft::aScale;

MObject DMbtSoft::outDM;

DMbtSoft::DMbtSoft()
    : preSoftBodyCount(0)
{
}

DMbtSoft::~DMbtSoft()
{
    releaseSoftBodies();
}


void *DMbtSoft::creator() {
    return ::dommy::maya::createObject("DMbtSoft", [&]() {
        return new DMbtSoft();
    });
}



#pragma mark -
#pragma mark create softbody method

void DMbtSoft::releaseSoftBodies() noexcept
{
    btSoftRigidDynamicsWorld* const world =
        static_cast<btSoftRigidDynamicsWorld*>(
            ResolveBulletWorld(m_worldReference));
    for(size_t i = 0; i < softBodyShapes.size(); ++i) {
        db_softbody_base* const softBody = softBodyShapes[i];
        if(!softBody) {
            continue;
        }
        if(world && softBody->body()) {
            world->removeSoftBody(softBody->body());
        }
        delete softBody;
    }
    softBodyShapes.clear();
    preSoftBodyCount = 0;
    m_nativeWork = 0;
    m_inputTopology.clear();
    m_worldReference.reset();
}

namespace {

MStatus readSoftBodyTopology(const DMeshData& input,
    std::vector<DBulletSoftTopology>& result)
{
    if (input.numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount)
        return MS::kInvalidParameter;
    std::vector<DBulletSoftTopology> candidate;
    candidate.reserve(input.numberOfMesh());
    std::size_t points = 0, faces = 0, connections = 0;
    for (std::size_t i = 0; i < input.numberOfMesh(); ++i) {
        const auto mesh = input.getMeshPtrConst(i);
        if (!mesh || mesh->numberOfPoints() < 3 || mesh->numFace() <= 0 ||
            !dommy::generation::checkedSum(points, mesh->numberOfPoints(),
                dommy::generation::kMaxGeneratedPointCount, points) ||
            !dommy::generation::checkedSum(faces, mesh->numFace(),
                dommy::generation::kMaxGeneratedFaceCount, faces) ||
            !dommy::generation::checkedSum(connections, mesh->numberOfConnections(),
                dommy::generation::kMaxWorkItemCount, connections) ||
            !mesh->hasValidPolygonTopology()) return MS::kInvalidParameter;
        MIntArray faceArray, connectionArray;
        DOMMY_RETURN_IF_FAILURE(mesh->copyTopology(faceArray, connectionArray));
        DBulletSoftTopology topology;
        topology.points = mesh->numberOfPoints();
        topology.faces.reserve(faceArray.length());
        topology.connections.reserve(connectionArray.length());
        for (unsigned j = 0; j < faceArray.length(); ++j) topology.faces.push_back(faceArray[j]);
        for (unsigned j = 0; j < connectionArray.length(); ++j) topology.connections.push_back(connectionArray[j]);
        candidate.push_back(std::move(topology));
    }
    result.swap(candidate);
    return MS::kSuccess;
}

bool areValidSoftBodyParameters(const btSoftParams& params)
{
    const double scalarValues[] = {
        params.m_mass, params.m_volumeMass, params.m_stiffness,
        params.m_angular_stiffness, params.m_volume_stiffness,
        params.m_rest_length_scale, params.m_restitution,
        params.m_friction, params.m_rolling_friction,
        params.m_spinning_friction, params.m_VelocityCorrectionFactor,
        params.m_dynamicFrictionCoeff, params.m_Damping, params.m_Drag,
        params.m_Lift, params.m_Pressure, params.m_VolumeConversation,
        params.m_PoseMatch, params.m_RigidContactHardness,
        params.m_KineticContactHardness, params.m_SoftContactHardness,
        params.m_AnchorContactHardness, params.m_rigid_impulse_split,
        params.m_kinetict_impulse_split, params.m_soft_impulse_split,
        params.m_collisionMargin, params.m_max_volume, params.m_scale
    };
    for (double value : scalarValues) {
        float converted;
        if (!dommy::numeric::checkedFloatCast(value, converted) ||
            (value != 0 && !std::isnormal(converted))) {
            return false;
        }
    }

    const double unitValues[] = {
        params.m_stiffness, params.m_angular_stiffness, params.m_volume_stiffness,
        params.m_restitution, params.m_dynamicFrictionCoeff, params.m_Damping,
        params.m_PoseMatch, params.m_RigidContactHardness, params.m_KineticContactHardness,
        params.m_SoftContactHardness, params.m_AnchorContactHardness,
        params.m_rigid_impulse_split, params.m_kinetict_impulse_split, params.m_soft_impulse_split
    };
    for (double value : unitValues) if (value < 0 || value > 1) return false;
    const int iterations[] = {params.m_positionIterations, params.m_velocityIterations,
        params.m_driftIterations, params.m_clusterIterations};
    for (int count : iterations) if (count < 0 || count > 1024) return false;

    return dommy::bullet::isSafeInverseOperand(params.m_mass) && params.m_volumeMass >= 0.0f &&
        params.m_friction >= 0 && params.m_rolling_friction >= 0 && params.m_spinning_friction >= 0 &&
        params.m_Drag >= 0 && params.m_Lift >= 0 && params.m_VolumeConversation >= 0 &&
        params.m_VelocityCorrectionFactor >= 0 && params.m_max_volume > 0 &&
        params.m_aeromodel >= 0 && params.m_aeromodel <= 6 &&
        params.m_rest_length_scale > 0.0 &&
        dommy::bullet::isSafeSoftBodyCollisionMargin(params.m_collisionMargin) && params.m_scale > 0.0f &&
        params.m_positionIterations >= 0 &&
        params.m_velocityIterations >= 0 &&
        params.m_driftIterations >= 0 &&
        params.m_clusterIterations >= 0 &&
        (!params.m_is_bend_constraint ||
         params.m_bendConstraintDistance > 0) &&
        (!params.m_is_cluster || params.m_numClusters > 0);
}

MStatus softBodyWorkBudget(const std::vector<DBulletSoftTopology>& topology,
    std::size_t first, const btSoftParams& params, std::size_t& total)
{
    if (first > topology.size() || !areValidSoftBodyParameters(params)) return MS::kInvalidParameter;
    std::size_t candidate = total;
    const int iterations[] = {params.m_positionIterations, params.m_velocityIterations,
        params.m_driftIterations, params.m_clusterIterations};
    for (std::size_t i = first; i < topology.size(); ++i) {
        std::size_t triangles = 0, work;
        for (int face : topology[i].faces) {
            if (face < 3 || !dommy::generation::checkedSum(triangles, std::size_t(face - 2),
                dommy::bullet::kMaxSoftBodyFaces, triangles)) return MS::kInvalidParameter;
        }
        if (!dommy::bullet::softBodyNativeWorkBudget(topology[i].points, triangles,
                params.m_is_bend_constraint ? params.m_bendConstraintDistance : 0,
                params.m_is_cluster ? params.m_numClusters : 0, iterations, work) ||
            !dommy::generation::checkedSum(candidate, work,
                dommy::bullet::kMaxSoftBodyWork, candidate)) return MS::kInvalidParameter;
    }
    total = candidate;
    return MS::kSuccess;
}

std::unique_ptr<db_softbody_base> createSoftBodyShape(
    btSoftBodyWorldInfo* m_worldInfo,
    const btSoftParams* sParam,
    const DMeshInternalData& mesh)
{
    if (!m_worldInfo || !sParam || !areValidSoftBodyParameters(*sParam) ||
        mesh.numberOfPoints() < 3U || mesh.numFace() == 0U) {
        throw std::invalid_argument(
            "Soft-body input mesh or parameters are invalid.");
    }
    
    MStatus stat;
    MFnMesh meshFn;
    MObject newObj = MFnMeshData().create(&stat);
    if (stat != MS::kSuccess || newObj.isNull())
        throw std::runtime_error("Maya failed to allocate the temporary soft-body mesh.");
    
    meshFn.create(mesh.numberOfPoints(), mesh.numFace(), mesh.getMPosition(),
                  mesh.getFaceList(), mesh.getConnectList(), newObj, &stat);
    if (stat != MS::kSuccess) {
        throw std::runtime_error(
            "Maya failed to construct the temporary soft-body mesh.");
    }
    
    std::vector<int> triVertIndices;
    std::vector<float> triVertCoords;
    
    MFloatPointArray ptArray;
    stat = meshFn.getPoints(ptArray);
    if (stat != MS::kSuccess || ptArray.length() != mesh.numberOfPoints() ||
        ptArray.length() > dommy::bullet::kMaxSoftBodyVertices) {
        throw std::runtime_error(
            "Maya failed to read soft-body vertex positions.");
    }
    
    // append vertex locations (x, y, z) into "flattened array"
    for(size_t i = 0; i < ptArray.length(); i++)
    {
        MFloatPoint pt;
        pt = ptArray[i];
        pt.cartesianize();
        triVertCoords.push_back(pt.x);
        triVertCoords.push_back(pt.y);
        triVertCoords.push_back(pt.z);
        
    }
    
    MIntArray tCounts;
    MIntArray tVerts;
    stat = meshFn.getTriangles(tCounts, tVerts);
    if (stat != MS::kSuccess || tVerts.length() == 0U ||
        tVerts.length() % 3U != 0U) {
        throw std::invalid_argument(
            "Soft-body mesh does not contain complete triangles.");
    }
    triVertIndices.resize(tVerts.length());
    for(unsigned int i = 0; i < tVerts.length(); ++i)
    {
        triVertIndices[i] = tVerts[i];
    }

    const int iterations[] = {sParam->m_positionIterations, sParam->m_velocityIterations,
        sParam->m_driftIterations, sParam->m_clusterIterations};
    std::size_t work;
    if (!dommy::bullet::softBodyNativeWorkBudget(triVertCoords.size() / 3,
            triVertIndices.size() / 3,
            sParam->m_is_bend_constraint ? sParam->m_bendConstraintDistance : 0,
            sParam->m_is_cluster ? sParam->m_numClusters : 0, iterations, work))
        throw std::invalid_argument("Soft-body native work budget exceeded.");

    // Validate each geometry state before Bullet normalizes face normals.
    // Keep the historical transform contract: node uniform scale, then mesh TR.
    btTransform xform;
    MVector unusedScale;
    if (ReadBulletGeometryTransform(mesh.getTransformMatrix(), xform, unusedScale) != MS::kSuccess)
        throw std::invalid_argument("Soft-body mesh transform is invalid.");
    std::vector<double> weights;
    if (!dommy::bullet::softBodyMassWeights(triVertCoords, triVertIndices, weights))
        throw std::invalid_argument("Soft-body mesh has degenerate faces or unused vertices.");
    std::vector<float> finalCoords(triVertCoords);
    for (float& coordinate : finalCoords) coordinate *= sParam->m_scale;
    if (!dommy::bullet::softBodyMassWeights(finalCoords, triVertIndices, weights))
        throw std::invalid_argument("Soft-body scale produces invalid triangles.");
    for (std::size_t i = 0; i < finalCoords.size(); i += 3) {
        const btVector3 position = xform * btVector3(finalCoords[i], finalCoords[i+1], finalCoords[i+2]);
        for (int axis = 0; axis < 3; ++axis) finalCoords[i+axis] = position[axis];
    }
    std::vector<float> masses;
    const bool useClusters = sParam->m_is_cluster && sParam->m_mass > 0;
    if (!dommy::bullet::softBodyMassWeights(finalCoords, triVertIndices, weights) ||
        !dommy::bullet::prepareSoftBodyMass(sParam->m_mass, weights, masses) ||
        (useClusters && !dommy::bullet::softBodyClusterMassRange(finalCoords, triVertIndices, masses)))
        throw std::invalid_argument("Soft-body transformed geometry or mass is outside the safe range.");
    
    if(DM_BULLET_SOFT_DEBUG) DOMMY_MAYA_DEBUG("DMbtSoft", "create soft body:");
    std::unique_ptr<db_softbody_base> sb(new db_softbody_base(
        *m_worldInfo, triVertCoords, triVertIndices));

    {
        //set soft body charactor
        
        btSoftBody::Material *pm = sb->body()->appendMaterial();
        
        pm->m_flags -= btSoftBody::fMaterial::DebugDraw;
        
        //generateBendingConstraintsの固さ設定 0~1
        pm->m_kLST = sParam->m_stiffness;// Linear stiffness coefficient [0,1] のび
        pm->m_kAST = sParam->m_angular_stiffness;// Area/Angular stiffness coefficient [0,1] ねじれ
        pm->m_kVST = sParam->m_volume_stiffness;// Volume stiffness coefficient [0,1] 体積
        
        
        
        sb->body()->setRestitution(sParam->m_restitution);
        sb->body()->setFriction(sParam->m_friction);
        sb->body()->setRollingFriction(sParam->m_rolling_friction);
        sb->body()->setSpinningFriction(sParam->m_spinning_friction);
//        sb->body()->setContactStiffnessAndDamping(0.0,0.0);
        //        sb->body()->setHitFraction(1.0);
        
//        sb->body()->setInterpolationLinearVelocity(btVector3 &linvel);
//        sb->body()->setInterpolationAngularVelocity(btVector3 &angvel);
        
        
        
        
        
        
        //近くにある頂点同士を拘束、表面を固くする効果
        if (sParam->m_is_bend_constraint) {
            sb->body()->generateBendingConstraints(sParam->m_bendConstraintDistance,pm);
        }
        
        
        
        
        switch (sParam->m_aeromodel) {
            case 1:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::V_TwoSided;
                break;
            case 2:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::V_TwoSidedLiftDrag;
                break;
            case 3:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::V_OneSided;
                break;
            case 4:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::F_TwoSided;
                break;
            case 5:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::F_TwoSidedLiftDrag;
                break;
            case 6:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::F_OneSided;
                break;
            case 0:
            default:
                sb->body()->m_cfg.aeromodel = btSoftBody::eAeroModel::V_Point;
                break;
        }
        
        sb->body()->m_cfg.kVCF = sParam->m_VelocityCorrectionFactor;
        sb->body()->m_cfg.kDP = sParam->m_Damping;
        sb->body()->m_cfg.kDG = sParam->m_Drag;
        sb->body()->m_cfg.kLF = sParam->m_Lift;
        sb->body()->m_cfg.kPR = sParam->m_Pressure;
        sb->body()->m_cfg.kVC = sParam->m_VolumeConversation;
        sb->body()->m_cfg.kDF = sParam->m_dynamicFrictionCoeff;
        sb->body()->m_cfg.kMT = sParam->m_PoseMatch;
        sb->body()->m_cfg.kCHR = sParam->m_RigidContactHardness;
        sb->body()->m_cfg.kKHR = sParam->m_KineticContactHardness;
        sb->body()->m_cfg.kSHR = sParam->m_SoftContactHardness;
        sb->body()->m_cfg.kAHR = sParam->m_AnchorContactHardness;
        
        
        // enable cluster collisions with other soft bodies and self
        
        sb->body()->m_cfg.piterations = sParam->m_positionIterations;
        sb->body()->m_cfg.viterations = sParam->m_velocityIterations;
        sb->body()->m_cfg.diterations = sParam->m_driftIterations;
        sb->body()->m_cfg.citerations = sParam->m_clusterIterations;
        
        sb->body()->m_cfg.maxvolume = sParam->m_max_volume;
        
        
        
        if(useClusters){
            
            sb->body()->m_cfg.collisions = (sParam->m_collide_rigid?btSoftBody::fCollision::CL_RS:0)
            +(sParam->m_collide_soft?btSoftBody::fCollision::CL_SS:0)
            +(sParam->m_collide_self?btSoftBody::fCollision::CL_SELF:0);
            
            
            sb->body()->m_cfg.kSRHR_CL = sParam->m_RigidContactHardness;
            sb->body()->m_cfg.kSKHR_CL = sParam->m_KineticContactHardness;
            sb->body()->m_cfg.kSSHR_CL = sParam->m_SoftContactHardness;
            
            sb->body()->m_cfg.kSR_SPLT_CL = sParam->m_rigid_impulse_split;
            sb->body()->m_cfg.kSK_SPLT_CL = sParam->m_kinetict_impulse_split;
            sb->body()->m_cfg.kSS_SPLT_CL = sParam->m_soft_impulse_split;
            
        } else {
            
            
            sb->body()->m_cfg.collisions = (sParam->m_collide_rigid?btSoftBody::fCollision::SDF_RS:0)
            +(sParam->m_collide_soft?btSoftBody::fCollision::VF_SS:0);
        }
        
        
        sb->body()->getCollisionShape()->setMargin(sParam->m_collisionMargin);
        sb->body()->scale(btVector3(sParam->m_scale,sParam->m_scale,sParam->m_scale));
        sb->body()->transform(xform);
        for (std::size_t i = 0; i < masses.size(); ++i)
            sb->body()->setMass(static_cast<int>(i), masses[i]);
        if (!dommy::bullet::isSafeInverseOperand(sb->body()->getTotalMass()))
            throw std::invalid_argument("Soft-body total mass is outside the safe range.");
        
        
        
        //ランダムに選んだ頂点間を拘束、全体の形状を保つ
        if(sParam->m_isRandomizeConstraints)
            sb->body()->randomizeConstraints();
        
        
        if (sParam->m_mass > 0 && (sParam->m_set_pose_volume ||
                sParam->m_Pressure != 0 || sParam->m_VolumeConversation > 0)) {
            const float volume = std::abs(sb->body()->getVolume());
            if (volume == 0 || !dommy::bullet::isSafeInverseOperand(volume))
                throw std::invalid_argument("Soft-body volume forces require a finite nonzero volume.");
        }
        sb->initialize_pose(sParam->m_set_pose_volume, sParam->m_set_pose_frame);

        if (useClusters) {
            sb->body()->generateClusters(sParam->m_numClusters,
                dommy::bullet::kMaxSoftClusterGenerationIterations);
            if (!dommy::bullet::validSoftBodyClusters(*sb->body()))
                throw std::invalid_argument("Soft-body cluster initialization produced invalid inertia.");
        }
        
        
        /*
         eAeroModel::_ aeromodel;// Aerodynamic model (default: V_Point)
         btScalar kVCF;                   // Velocities correction factor (Baumgarte)//速度補正係数（Baumgarte）
         btScalar kDP;                    // Damping coefficient [0,1] //ダンピング係数
         btScalar kDG;                    // Drag coefficient [0,+inf] //抗力係数
         btScalar kLF;                    // Lift coefficient [0,+inf] //揚力係数
         btScalar kPR;                    // Pressure coefficient [-inf,+inf]//圧力係数
         btScalar kVC;                    // Volume conversation coefficient [0,+inf]//体積維持
         btScalar kDF;                    // Dynamic friction coefficient [0,1]//動摩擦
         btScalar kMT;                    // Pose matching coefficient [0,1]  //setPoseの効果 0~1
         btScalar kCHR;                   // Rigid contacts hardness [0,1] //Rigidに対する硬さ
         btScalar kKHR;                   // Kinetic contacts hardness [0,1]　 //Kineticに対する硬さ
         btScalar kSHR;                   // Soft contacts hardness [0,1]　//Softに対する硬さ
         btScalar kAHR;                   // Anchors hardness [0,1] //Anchorsに対する硬さ
         btScalar kSRHR_CL;               // Soft vs rigid hardness [0,1] (cluster only)
         btScalar kSKHR_CL;               // Soft vs kinetic hardness [0,1] (cluster only)
         btScalar kSSHR_CL;               // Soft vs soft hardness [0,1] (cluster only)
         btScalar kSR_SPLT_CL;    // Soft vs rigid impulse split [0,1] (cluster only)
         btScalar kSK_SPLT_CL;    // Soft vs rigid impulse split [0,1] (cluster only)
         btScalar kSS_SPLT_CL;    // Soft vs rigid impulse split [0,1] (cluster only)
         btScalar maxvolume;              // Maximum volume ratio for pose
         btScalar timescale;              // Time scale
         int viterations;    // Velocities solver iterations
         int piterations;    // Positions solver iterations
         int diterations;    // Drift solver iterations
         int citerations;    // Cluster solver iterations
         int collisions;             // Collisions flags
         tVSolverArray m_vsequence;    // Velocity solvers sequence
         tPSolverArray m_psequence;    // Position solvers sequence
         tPSolverArray m_dsequence;    // Drift solvers sequence
         */
        
        
        // Complete Bullet 2.88's deferred predictMotion initialization now.
        // Otherwise its first updateConstants() overwrites RestLengthScale.
        // Face-tree initialization must accompany clearing m_bUpdateRtCst.
        sb->body()->updateConstants();
        for (int i = 0; i < sb->body()->m_links.size(); ++i) {
            const btSoftBody::Link& link = sb->body()->m_links[i];
            // Use the same scalar rounding as setRestLengthScale(), not a
            // double approximation at the float overflow/underflow boundary.
            const btScalar length = link.m_rl * btScalar(sParam->m_rest_length_scale);
            const btScalar squared = length * length;
            if (!std::isfinite(link.m_c0) || !std::isnormal(squared))
                throw std::invalid_argument("Soft-body link constants are outside the safe range.");
        }
        sb->body()->m_fdbvt.clear();
        if (sb->body()->m_cfg.collisions & btSoftBody::fCollision::VF_SS)
            sb->body()->initializeFaceTree();
        sb->body()->m_bUpdateRtCst = false;
        sb->body()->setRestLengthScale(sParam->m_rest_length_scale);
        
        
        //info
        if (DM_BULLET_SOFT_DEBUG) {
            DOMMY_MAYA_DEBUG("DMbtSoft", "clusterCount" << sb->body()->clusterCount());
            DOMMY_MAYA_DEBUG("DMbtSoft", "Volume" << sb->body()->getVolume());
        }
        
        
        
        
        
    }

    return sb;
}


void addSoftBody(
    btSoftRigidDynamicsWorld& world,
    std::vector<db_softbody_base*>& softBodies,
    std::unique_ptr<db_softbody_base> softBody)
{
    if (!softBody || !softBody->body()) {
        throw std::invalid_argument("Cannot add an empty Bullet soft body.");
    }

    softBodies.reserve(softBodies.size() + 1U);
    db_softbody_base* const rawSoftBody = softBody.get();
    world.addSoftBody(rawSoftBody->body());
    try {
        softBodies.push_back(rawSoftBody);
    } catch (...) {
        world.removeSoftBody(rawSoftBody->body());
        throw;
    }
    softBody.release();
}

} // namespace

#pragma mark -
#pragma mark parameter

MStatus DMbtSoft::evalParms(btSoftParams& parms) {
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMass), parms.m_mass));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aVolumeMass), parms.m_volumeMass));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRestLengthScale), parms.m_rest_length_scale));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRestitution), parms.m_restitution));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aFriction), parms.m_friction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRollingFriction), parms.m_rolling_friction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSpinningFriction), parms.m_spinning_friction));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aStiffness), parms.m_stiffness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aAngularStiffness), parms.m_angular_stiffness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aVolumeStiffness), parms.m_volume_stiffness));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsGenerateBendConstraint), parms.m_is_bend_constraint));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsCreateCluster), parms.m_is_cluster));
    
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aAeromodel), parms.m_aeromodel));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aVelocityCorrectionFactor), parms.m_VelocityCorrectionFactor));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDynamicFrictionCoeff), parms.m_dynamicFrictionCoeff));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDamping), parms.m_Damping));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDrag), parms.m_Drag));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLift), parms.m_Lift));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aPressure), parms.m_Pressure));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aVolumeConversation), parms.m_VolumeConversation));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aPoseMatch), parms.m_PoseMatch));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRigidContactHardness), parms.m_RigidContactHardness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aKineticContactHardness), parms.m_KineticContactHardness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSoftContactHardness), parms.m_SoftContactHardness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aAnchorContactHardness), parms.m_AnchorContactHardness));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRigidImpulseSplit), parms.m_rigid_impulse_split));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aKinetictImpulseSplit), parms.m_kinetict_impulse_split));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSoftImpulseSplit), parms.m_soft_impulse_split));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSetPoseFrame), parms.m_set_pose_frame));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSetPoseVolume), parms.m_set_pose_volume));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsRandomizeConstraints), parms.m_isRandomizeConstraints));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBendConstraintDistance), parms.m_bendConstraintDistance));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aNumClusters), parms.m_numClusters));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionMargin), parms.m_collisionMargin));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aPositionIterations), parms.m_positionIterations));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aVelocityIterations), parms.m_velocityIterations));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDriftIterations), parms.m_driftIterations));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aClusterIterations), parms.m_clusterIterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMaxVolume), parms.m_max_volume));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollideRigidBody), parms.m_collide_rigid));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollideSoftBody), parms.m_collide_soft));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollideSelf), parms.m_collide_self));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aScale), parms.m_scale));
    
    
    return MS::kSuccess;
}


#pragma mark -
#pragma mark init


MStatus DMbtSoft::resetSoftBodies(
    btSoftRigidDynamicsWorld* m_dynamicsWorld,
    btSoftBodyWorldInfo* m_worldInfo,
    const DBulletWorldReferencePtr& worldReference) {
    if (!m_dynamicsWorld || !m_worldInfo || !worldReference ||
        worldReference->get() != m_dynamicsWorld) {
        return MS::kInvalidParameter;
    }
    
    
    
    MPlug dmPlug(thisMObject(), inDM);
    dommy::maya::ScopedPlugDataHandle dMeshHndOwner;
    MDataHandle& dMeshHnd = dMeshHndOwner.handle();
    DOMMY_RETURN_IF_FAILURE(dMeshHndOwner.read(dmPlug));
    
    
    
    const DMeshData* const dMeshes(getInputDMfromHandle(dMeshHnd));
    if (!dMeshes) return MS::kFailure;

    btSoftParams sParam;
    DOMMY_RETURN_IF_FAILURE(evalParms(sParam));
    if (!areValidSoftBodyParameters(sParam)) return MS::kInvalidParameter;

    std::vector<DBulletSoftTopology> topology;
    DOMMY_RETURN_IF_FAILURE(readSoftBodyTopology(*dMeshes, topology));
    std::size_t work = 0;
    DOMMY_RETURN_IF_FAILURE(softBodyWorkBudget(topology, 0, sParam, work));
    // A failed input/parameter read must not discard the previous bodies.
    releaseSoftBodies();
    m_worldReference = worldReference;
    
    if (dMeshes && dMeshes->numberOfMesh()) {
        
        MFnDependencyNode nodeFn(thisMObject());
        
        
        size_t mIndex = 0;
        for (auto mIt=dMeshes->beginPrims(); mIt!=dMeshes->endPrims(); ++mIt) {
            if (!*mIt) {
                ++mIndex;
                continue;
            }
            
            std::unique_ptr<db_softbody_base> sb = createSoftBodyShape(
                m_worldInfo, &sParam, **mIt);

            if (sb) {
                //add user custom data
                std::unique_ptr<customDBData> userData(new customDBData());
                userData->nodeUuid = nodeFn.uuid();
                userData->prefixName = nodeFn.name().asChar();
                userData->index = mIndex;
                sb->set_user_data(userData.release());
                
                //                sb->body()->setActivationState(DISABLE_DEACTIVATION);
                addSoftBody(
                    *m_dynamicsWorld, softBodyShapes, std::move(sb));
                
                
                if(DM_BULLET_SOFT_DEBUG) DOMMY_MAYA_DEBUG("DMbtSoft", "add soft body:");
                
            }//check softbody data
            
            
            mIndex++;
        }//loop mesh
        
        
    }//check dmesh
    
    m_inputTopology.swap(topology);
    m_nativeWork = work;
    preSoftBodyCount = m_inputTopology.size();
    return MS::kSuccess;
}







MStatus DMbtSoft::updateSoftBodies(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo) {
    if (!m_dynamicsWorld || !m_worldInfo || ResolveBulletWorld(m_worldReference) !=
        m_dynamicsWorld) {
        return MS::kInvalidParameter;
    }
    
    MPlug dmPlug(thisMObject(), inDM);
    dommy::maya::ScopedPlugDataHandle dMeshHndOwner;
    MDataHandle& dMeshHnd = dMeshHndOwner.handle();
    DOMMY_RETURN_IF_FAILURE(dMeshHndOwner.read(dmPlug));
    
    const DMeshData* const dMeshes(getInputDMfromHandle(dMeshHnd));
    if (!dMeshes) return MS::kFailure;
    
    std::vector<DBulletSoftTopology> topology;
    DOMMY_RETURN_IF_FAILURE(readSoftBodyTopology(*dMeshes, topology));
    if (topology.empty()) {
        const auto reference = m_worldReference;
        releaseSoftBodies();
        m_worldReference = reference;
        return MS::kSuccess;
    }
    if (topology.size() < m_inputTopology.size() ||
        !std::equal(m_inputTopology.begin(), m_inputTopology.end(), topology.begin())) {
        EchoWarning(name().asChar(), "Soft-body topology changed; return to StartTime.");
        return MS::kFailure;
    }
    std::size_t work = m_nativeWork;
    if(topology.size()>preSoftBodyCount) {
        
        btSoftParams sParam;
        DOMMY_RETURN_IF_FAILURE(evalParms(sParam));
        if (!areValidSoftBodyParameters(sParam)) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(softBodyWorkBudget(topology, preSoftBodyCount, sParam, work));
        
        MFnDependencyNode nodeFn(thisMObject());
        
        
        for (size_t i=preSoftBodyCount; i<dMeshes->numberOfMesh(); ++i) {
            
            ConstDMItem mIt = dMeshes->getMeshPtrConst(i);
            if (!mIt) {
                continue;
            }
            
            std::unique_ptr<db_softbody_base> sb = createSoftBodyShape(
                m_worldInfo, &sParam, *mIt);
            
            if (sb) {
                //add user custom data
                std::unique_ptr<customDBData> userData(new customDBData());
                userData->nodeUuid = nodeFn.uuid();
                userData->prefixName = nodeFn.name().asChar();
                userData->index = i;
                sb->set_user_data(userData.release());
                
                //                sb->body()->setActivationState(DISABLE_DEACTIVATION);
                addSoftBody(
                    *m_dynamicsWorld, softBodyShapes, std::move(sb));
                
                if(DM_BULLET_SOFT_DEBUG) DOMMY_MAYA_DEBUG("DMbtSoft", "add soft body:");
                
            }//check softbody data
            
            
        }//loop mesh
        
    }
    m_inputTopology.swap(topology);
    m_nativeWork = work;
    preSoftBodyCount = m_inputTopology.size();
    return MS::kSuccess;
}


#pragma mark -
#pragma mark compute

MStatus DMbtSoft::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle inputHandle = data.inputValue(inDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DMeshData* input = getInputDMfromHandle(inputHandle);
    if (!input || !input->hasValidMeshTopology()) return MS::kInvalidParameter;
    MObject outputObject;
    DMeshData* outputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject(
        DMeshData::id, outputObject, outputData));
    if (!outputData) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*input));
    MDataHandle output = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
        output, outputObject, *outputData));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMbtSoft")




MStatus DMbtSoft::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
#pragma mark rigid body attribute
    aMass = nAttr.create("Mass", "ma", MFnNumericData::kFloat, 100, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMass));
    
    aVolumeMass = nAttr.create("VolumeMass", "volma", MFnNumericData::kFloat, 100, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumeMass));
    
    
    
    aRestLengthScale = nAttr.create("RestLengthScale", "rstlenscl", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestLengthScale));
    
    aRestitution = nAttr.create("Restitution", "rst", MFnNumericData::kDouble, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestitution));
    
    aFriction = nAttr.create("Friction", "fc", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFriction));
    
    aRollingFriction = nAttr.create("RollingFriction", "rfc", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRollingFriction));
    
    
    aSpinningFriction = nAttr.create("SpinningFriction", "sfc", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpinningFriction));
    
    
    
    
    
    aStiffness = nAttr.create("Stiffness", "stf", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStiffness));
    
    aAngularStiffness = nAttr.create("AngularStiffness", "aglstf", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngularStiffness));
    
    aVolumeStiffness = nAttr.create("VolumeStiffness", "volstf", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumeStiffness));
    
    
    aIsGenerateBendConstraint = nAttr.create("IsGenerateBendConstraint", "isgenbndcnst", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsGenerateBendConstraint));
    
    aIsCreateCluster = nAttr.create("IsCreateCluster", "iscrtclt", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCreateCluster));
    
    
    
    MFnEnumAttribute aeroEnumFn;
    aAeromodel = aeroEnumFn.create( "Aeromodel", "aerom", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "V_Point", 0 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "V_TwoSided", 1 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "V_TwoSidedLiftDrag", 2 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "V_OneSided", 3 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "F_TwoSided", 4 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "F_TwoSidedLiftDrag", 5 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "F_OneSided", 6 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAeromodel));
    
    
    
    
    aVelocityCorrectionFactor = nAttr.create("VelocityCorrectionFactor", "velcf", MFnNumericData::kDouble, 1.0f, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityCorrectionFactor));
    
    aDynamicFrictionCoeff = nAttr.create("DynamicFrictionCoeff", "dfc", MFnNumericData::kDouble, 0.2f, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDynamicFrictionCoeff));
    
    
    aDamping = nAttr.create("Damping", "dmp", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));
    
    aDrag = nAttr.create("Drag", "drg", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDrag));
    
    aLift = nAttr.create("Lift", "lft", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLift));
    
    
    aPressure = nAttr.create("Pressure", "prsr", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPressure));
    
    
    aVolumeConversation = nAttr.create("VolumeConversation", "volcnv", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumeConversation));
    
    aPoseMatch = nAttr.create("PoseMatch", "pmtch", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPoseMatch));
    
    
    
    aRigidContactHardness = nAttr.create("RigidContactHardness", "rgdcnthd", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRigidContactHardness));
    
    aKineticContactHardness = nAttr.create("KineticContactHardness", "knctcnthd", MFnNumericData::kDouble, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKineticContactHardness));
    
    aSoftContactHardness = nAttr.create("SoftContactHardness", "sftcnthd", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSoftContactHardness));
    
    aAnchorContactHardness = nAttr.create("AnchorContactHardness", "achcnthd", MFnNumericData::kDouble, 0.7, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAnchorContactHardness));
    
    
    aRigidImpulseSplit = nAttr.create("RigidImpulseSplit", "rimpsplt", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRigidImpulseSplit));
    
    aKinetictImpulseSplit = nAttr.create("KinetictImpulseSplit", "knctimpsplt", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKinetictImpulseSplit));
    
    aSoftImpulseSplit = nAttr.create("SoftImpulseSplit", "sftimpsplt", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSoftImpulseSplit));
    
    
    
    
    
    
    aSetPoseFrame = nAttr.create("SetPoseFrame", "setpsfrm", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSetPoseFrame));
    
    aSetPoseVolume = nAttr.create("SetPoseVolume", "setpsvol", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSetPoseVolume));
    
    
    
    
    aIsRandomizeConstraints = nAttr.create("IsRandomizeConstraints", "isrndcont", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandomizeConstraints));
    
    
    
    
    aBendConstraintDistance = nAttr.create("BendConstraintDistance", "bndcnstdist", MFnNumericData::kInt, 2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBendConstraintDistance));
    
    // collision clusters attribute
    aNumClusters = nAttr.create("NumClusters", "nclust", MFnNumericData::kInt, 100, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNumClusters));
    
    
    
    
    
    
#pragma mark collision shape attribute
    
    
    aCollisionMargin = nAttr.create("CollisionMargin", "colm", MFnNumericData::kFloat, 1.75 * 0.04, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionMargin));
    
    aPositionIterations = nAttr.create("PositionIterations", "pitr", MFnNumericData::kInt, 3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPositionIterations));
    
    aVelocityIterations = nAttr.create("VelocityIterations", "vitr", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityIterations));
    
    aDriftIterations = nAttr.create("DriftIterations", "ditr", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDriftIterations));
    
    aClusterIterations = nAttr.create("ClusterIterations", "citr", MFnNumericData::kInt, 4, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClusterIterations));
    
    aMaxVolume = nAttr.create("MaxVolume", "mxvol", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxVolume));
    
    
    
    aCollideRigidBody = nAttr.create("CollideRigidBody", "colrb", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideRigidBody));
    
    aCollideSoftBody = nAttr.create("CollideSoftBody", "colsb", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideSoftBody));
    
    aCollideSelf = nAttr.create("CollideSelf", "colself", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideSelf));
    
    aScale = nAttr.create("Scale", "scl", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));
    
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMass, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVolumeMass, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestLengthScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestitution, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFriction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRollingFriction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpinningFriction, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStiffness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularStiffness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVolumeStiffness, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsGenerateBendConstraint, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCreateCluster, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAeromodel, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityCorrectionFactor, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDynamicFrictionCoeff, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDamping, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDrag, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLift, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPressure, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVolumeConversation, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPoseMatch, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRigidContactHardness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aKineticContactHardness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSoftContactHardness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAnchorContactHardness, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRigidImpulseSplit, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aKinetictImpulseSplit, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSoftImpulseSplit, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSetPoseFrame, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSetPoseVolume, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsRandomizeConstraints, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBendConstraintDistance, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNumClusters, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionMargin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPositionIterations, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityIterations, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDriftIterations, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aClusterIterations, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMaxVolume, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollideRigidBody, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollideSoftBody, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollideSelf, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDM ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMbtSoft", "initialize", std::current_exception());
}

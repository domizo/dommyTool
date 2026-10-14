//
//  DPSelfCollision.cpp
//  dommyTool
//
//  Created by yuzo on 2017/04/05.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSelfCollision.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "DPointOp.h"
#include "common/utility/DDirectionMath.h"
#include "DPoint/Common/DPointForceOutput.h"

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <atomic>
#include <cmath>



#pragma mark Utilities

namespace {

    // Preserve the historical mass weighting and accumulation order. Only the
    // numerical representation changes: compare lengths, not float squares.
    class DPSelfCollideOp
    {
    public:
        DPSelfCollideOp(const DPointList& points,
                       const DScalarDataList& radii,
                       const DScalarDataList& masses,
                       const std::vector<Imath::V3d>& velocities,
                       DVec3DataList& output,
                       const IDsListArray& neighbors,
                       std::atomic<bool>& succeeded)
            : m_damping(0.5f), m_avoidPower(0.0f),
              m_pts(points), m_rad(radii), m_mass(masses),
              m_vels(velocities), m_output(output), m_neighbors(neighbors),
              m_succeeded(succeeded)
        {
        }

        void operator()(const tbb::blocked_range<size_t>& range) const
        {
            const size_t count = m_pts.size();
            if (m_rad.size() != count || m_mass.size() != count ||
                m_vels.size() != count || m_output.size() != count ||
                m_neighbors.size() != count || range.end() > count ||
                !std::isfinite(m_damping) || !std::isfinite(m_avoidPower)) {
                m_succeeded.store(false, std::memory_order_relaxed);
                return;
            }
            for (size_t i = range.begin(); i != range.end(); ++i) {
                if (!m_succeeded.load(std::memory_order_relaxed)) return;
                Imath::V3d totalVelocity(m_vels[i]);
                const double currentMass = m_mass[i];
                if (!std::isfinite(currentMass) || currentMass < 0.0 ||
                    !std::isfinite(m_rad[i]) || m_rad[i] < 0.0f) {
                    m_succeeded.store(false, std::memory_order_relaxed);
                    return;
                }
                for (uint32_t neighbor : m_neighbors[i]) {
                    if (neighbor >= count) {
                        m_succeeded.store(false, std::memory_order_relaxed);
                        return;
                    }
                    dommy::direction::Delta delta;
                    const double neighborMass = m_mass[neighbor];
                    const double radius = static_cast<double>(m_rad[i]) + m_rad[neighbor];
                    if (!dommy::direction::difference(m_pts[neighbor], m_pts[i], delta) ||
                        !std::isfinite(m_rad[neighbor]) || m_rad[neighbor] < 0.0f ||
                        !std::isfinite(neighborMass) || neighborMass < 0.0) {
                        m_succeeded.store(false, std::memory_order_relaxed);
                        return;
                    }
                    // Coincident centers have no unique normal. Two zero mass
                    // weights have no defined impulse; retain both legacy no-ops.
                    const double massSum = currentMass + neighborMass;
                    if (delta.length == 0.0 || delta.length > radius || massSum == 0.0)
                        continue;
                    const Imath::V3d normal(delta.values[0] / delta.length,
                                           delta.values[1] / delta.length,
                                           delta.values[2] / delta.length);
                    const Imath::V3d relativeVelocity = m_vels[i] - m_vels[neighbor];
                    const double normalSpeed = relativeVelocity.dot(normal);
                    if (!std::isfinite(normalSpeed)) {
                        m_succeeded.store(false, std::memory_order_relaxed);
                        return;
                    }
                    totalVelocity += normal * ((currentMass / massSum) * m_avoidPower);
                    if (normalSpeed > 0.0) continue;
                    // Form the bounded mass ratio before scaling the impulse.
                    const double impulse = -normalSpeed *
                        ((1.0 + static_cast<double>(m_damping)) * (neighborMass / massSum));
                    totalVelocity += normal * impulse;
                }
                DVec3Data candidate;
                if (!dommy::direction::assignFinite(totalVelocity, candidate)) {
                    m_succeeded.store(false, std::memory_order_relaxed);
                    return;
                }
                m_output[i] = candidate;
            }
        }

        float m_damping;
        float m_avoidPower;
    private:
        const DPointList& m_pts;
        const DScalarDataList& m_rad;
        const DScalarDataList& m_mass;
        const std::vector<Imath::V3d>& m_vels;
        DVec3DataList& m_output;
        const IDsListArray& m_neighbors;
        std::atomic<bool>& m_succeeded;
    };

} // namespace

const MTypeId DPSelfCollision::typeId( ID_DP_SELF_COLLISION );


MObject DPSelfCollision::inDP;

MObject DPSelfCollision::aSearchRadius;

MObject DPSelfCollision::aRadius;
MObject DPSelfCollision::aRadiusAttribute;


MObject DPSelfCollision::aUseMass;
MObject DPSelfCollision::aMassAttribute;

MObject DPSelfCollision::aDamping;
MObject DPSelfCollision::aAvoidPower;

MObject DPSelfCollision::outDP;



void *DPSelfCollision::creator() {
    return ::dommy::maya::createObject("DPSelfCollision", [&]() {
        return new DPSelfCollision();
    });
}







MStatus DPSelfCollision::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle handle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    handle = data.inputValue(aSearchRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float searchRadius = handle.asFloat();
    handle = data.inputValue(aRadiusAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string radiusName = handle.asString().asChar();
    handle = data.inputValue(aRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float radius = handle.asFloat();
    handle = data.inputValue(aUseMass, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useMass = handle.asBool();
    handle = data.inputValue(aMassAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string massName = handle.asString().asChar();
    handle = data.inputValue(aAvoidPower, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float avoidPower = handle.asFloat();
    handle = data.inputValue(aDamping, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float damping = handle.asFloat();

    const size_t count = input->numberOfPoints();
    const DPointList* positions = input->getPositionPtrConst();
    size_t neighborCapacity = 0U;
    if (!positions || positions->size() != count ||
        !std::isfinite(searchRadius) || searchRadius < 0.0f ||
        (count != 0U && searchRadius == 0.0f) ||
        !std::isfinite(radius) || radius < 0.0f ||
        !std::isfinite(avoidPower) || !std::isfinite(damping) ||
        !dommy::generation::checkedProduct(count, count,
            dommy::generation::kMaxWorkItemCount, neighborCapacity) ||
        !dommy::attribute::validArrayConversionBudget(count,
            2U * sizeof(float) + 2U * sizeof(Imath::V3d) +
            sizeof(DVec3Data) + sizeof(IDs))) return MS::kInvalidParameter;

    // vel takes precedence over v; a missing velocity field is not created.
    const std::string velocityName = input->isExistAttr("vel") ? "vel" :
        (input->isExistAttr("v") ? "v" : "");
    DVec3DataList finalVelocities;
    if (count != 0U) {
        DScalarDataList radii, masses;
        if (!dommy::point::readPointForceScalars(input->getAttr(radiusName), count, radius, radii) ||
            !dommy::point::readPointForceScalars(
                useMass ? input->getAttr(massName) : DPAttributeData::ConstPtr(),
                count, 1.0f, masses)) return MS::kInvalidParameter;
        std::vector<Imath::V3d> velocities;
        if (!velocityName.empty()) {
            if (!dommy::attribute::convertVector3Attribute(
                    input->getAttr(velocityName), count, velocities))
                return MS::kInvalidParameter;
        } else {
            velocities.assign(count, Imath::V3d(0.0, 0.0, 0.0));
        }
        for (size_t i = 0U; i < count; ++i) {
            dommy::direction::Delta validated;
            if (!dommy::direction::difference((*positions)[i], (*positions)[i], validated) ||
                radii[i] < 0.0f || masses[i] < 0.0f)
                return MS::kInvalidParameter;
        }
        IDsListArray neighbors(count);
        DPointSearchKDTreeRadiusGen search(*positions, -1, searchRadius, neighbors);
        search();
        finalVelocities.resize(count);
        std::atomic<bool> succeeded(true);
        DPSelfCollideOp operation(*positions, radii, masses, velocities,
                                 finalVelocities, neighbors, succeeded);
        operation.m_damping = damping;
        operation.m_avoidPower = avoidPower;
        tbb::parallel_for(tbb::blocked_range<size_t>(0U, count, 10000U), operation);
        if (!succeeded.load(std::memory_order_relaxed)) return MS::kInvalidParameter;
    }

    status = dommy::maya::setPluginDataOutput<DPData>(data, outDP, DPData::id,
        [&](DPData& output) {
            output.setDPData(*input);
            if (count != 0U && !velocityName.empty())
                output.setDPAttributeValues(velocityName, finalVelocities);
            return MS::kSuccess;
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPSelfCollision")

MStatus DPSelfCollision::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    MFnGenericAttribute outAttr;
    
    
    // ----------------------- INPUTS --------------------------
    
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    aSearchRadius = nAttr.create( "SearchRadius", "srhrad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchRadius));
    
    const MObject aRadiusAttributeDefault = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aRadiusAttribute = tAttr.create("RadiusAttribute", "rattr", MFnData::kString, aRadiusAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
    
    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.01 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    
    aUseMass = nAttr.create( "UseMass", "uma", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseMass));
    
    const MObject aMassAttributeDefault = fnStringData.create("mass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aMassAttribute = tAttr.create("MassAttribute", "mattr", MFnData::kString, aMassAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassAttribute));
    
    aDamping = nAttr.create( "Damping", "dmp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.5 ));
    
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));
    
    
    aAvoidPower = nAttr.create( "AvoidPower", "avdpwr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAvoidPower));
    
    
    
    
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSearchRadius, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusAttribute, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadius, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseMass, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMassAttribute, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDamping, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAvoidPower, outDP ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPSelfCollision", "initialize", std::current_exception());
}

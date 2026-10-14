// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Advect_Points.cc

//
//  DOpenVDBAdvectPoints.cpp
//  dommyTool
//
//  Created by yuzo on 1/11/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBAdvectPoints.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBTransportPolicy.h"
#include "DMayaHeaders.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include <openvdb/tools/PointAdvect.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <memory>
#include <stdexcept>
#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include <cmath>
#include <limits>

const MTypeId DOpenVDBAdvectPoints::typeId(ID_VDB_DOPENVDB_POINT_ADVECT);

MObject DOpenVDBAdvectPoints::inDP;
MObject DOpenVDBAdvectPoints::aPointGroup;

MObject DOpenVDBAdvectPoints::aVdbVelocityInput;
MObject DOpenVDBAdvectPoints::aVdbVelocityGridNames;

MObject DOpenVDBAdvectPoints::aVdbClosestPointInput;
MObject DOpenVDBAdvectPoints::aVdbClosestPointGridNames;

MObject DOpenVDBAdvectPoints::aOperation;
MObject DOpenVDBAdvectPoints::aIntegration;

MObject DOpenVDBAdvectPoints::aIterations;
MObject DOpenVDBAdvectPoints::aTimestep;
MObject DOpenVDBAdvectPoints::aSubsteps;

MObject DOpenVDBAdvectPoints::aOutputStreamlines;

MObject DOpenVDBAdvectPoints::outDP;
MObject DOpenVDBAdvectPoints::outDC;

void* DOpenVDBAdvectPoints::creator() {
    return ::dommy::maya::createObject("DOpenVDBAdvectPoints", [&]() {
        return new DOpenVDBAdvectPoints();
    });
}

namespace {
enum PropagationType {
    PROPAGATION_TYPE_UNKNOWN = -1,
    PROPAGATION_TYPE_ADVECTION = 0,
    PROPAGATION_TYPE_PROJECTION,
    PROPAGATION_TYPE_CONSTRAINED_ADVECTION
};

enum { NUM_PROPAGATION_TYPES = PROPAGATION_TYPE_CONSTRAINED_ADVECTION + 1 };

std::string propagationTypeToMenuName(PropagationType pt) {
    std::string ret;
    switch (pt) {
    case PROPAGATION_TYPE_ADVECTION:
        ret = "Advection";
        break;
    case PROPAGATION_TYPE_PROJECTION:
        ret = "Projection";
        break;
    case PROPAGATION_TYPE_CONSTRAINED_ADVECTION:
        ret = "Constrained Advection";
        break;
    case PROPAGATION_TYPE_UNKNOWN:
        ret = "Unknown";
        break;
    }
    return ret;
}
// Add new items to the *end* of this list, and update NUM_INTEGRATION_TYPES.
enum IntegrationType {
    INTEGRATION_TYPE_UNKNOWN = -1,
    INTEGRATION_TYPE_FWD_EULER = 0,
    INTEGRATION_TYPE_RK_2ND,
    INTEGRATION_TYPE_RK_3RD,
    INTEGRATION_TYPE_RK_4TH
};

enum { NUM_INTEGRATION_TYPES = INTEGRATION_TYPE_RK_4TH + 1 };

std::string integrationTypeToMenuName(IntegrationType it) {
    std::string ret;
    switch (it) {
    case INTEGRATION_TYPE_FWD_EULER:
        ret = "Forward Euler";
        break;
    case INTEGRATION_TYPE_RK_2ND:
        ret = "Second-Order Runge-Kutta";
        break;
    case INTEGRATION_TYPE_RK_3RD:
        ret = "Third-Order Runge-Kutta";
        break;
    case INTEGRATION_TYPE_RK_4TH:
        ret = "Fourth-Order Runge-Kutta";
        break;
    case INTEGRATION_TYPE_UNKNOWN:
        ret = "Unknown";
        break;
    }
    return ret;
}
struct AdvectionPointParms {
    GridCPtr mVelVdb, mCptVdb;
    PropagationType mPropagationType = PROPAGATION_TYPE_ADVECTION;
    IntegrationType mIntegrationType = INTEGRATION_TYPE_FWD_EULER;
    double mTimeStep = 1.0;
    int mIterations = 1, mSteps = 1;
    bool mStaggered = false, mStreamlines = false;
    long double motionRadius = 0;
};

MStatus readPointGrid(MDataBlock& data, const MObject& input, const MObject& selection,
                      GridCPtr& grid) {
    MStatus status;
    const MDataHandle handle = data.inputValue(input, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DOpenVDBData* payload = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(handle, DOpenVDBData::id, payload));
    if (!payload || !payload->hasValidData())
        return MS::kFailure;
    const MDataHandle names = data.inputValue(selection, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    GridCPtrVec grids;
    if (!getSelectedGrids(grids, names.asString().asChar(), *payload) || grids.empty() ||
        !grids.front())
        return MS::kInvalidParameter;
    const DVdbGridType type = getVdbGridType(*grids.front());
    if (type != DVdbGridType::kVec3f && type != DVdbGridType::kVec3d)
        return MS::kInvalidParameter;
    if (grids.size() > 1U)
        MGlobal::displayWarning(
            "More than one point-advection grid matched; the first grid is used.");
    grid = grids.front();
    return MS::kSuccess;
}

MStatus readPointAdvectionSettings(MDataBlock& data, AdvectionPointParms& parms) {
    using Node = DOpenVDBAdvectPoints;
    MStatus status;
    const MDataHandle operation = data.inputValue(Node::aOperation, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short propagation = operation.asShort();
    if (propagation < PROPAGATION_TYPE_ADVECTION ||
        propagation > PROPAGATION_TYPE_CONSTRAINED_ADVECTION)
        return MS::kInvalidParameter;
    parms.mPropagationType = static_cast<PropagationType>(propagation);
    // Read only the ports used by this mode, preserving disconnected-port behavior.
    if (parms.mPropagationType != PROPAGATION_TYPE_PROJECTION) {
        DOMMY_RETURN_IF_FAILURE(readPointGrid(data, Node::aVdbVelocityInput,
                                              Node::aVdbVelocityGridNames, parms.mVelVdb));
        parms.mStaggered = parms.mVelVdb->getGridClass() == openvdb::GRID_STAGGERED;
        const MDataHandle time = data.inputValue(Node::aTimestep, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        parms.mTimeStep = time.asDouble();
        const MDataHandle steps = data.inputValue(Node::aSubsteps, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        parms.mSteps = steps.asInt();
        if (!std::isfinite(parms.mTimeStep) || parms.mSteps <= 0)
            return MS::kInvalidParameter;
        parms.mTimeStep /= static_cast<double>(parms.mSteps);
        const MDataHandle lines = data.inputValue(Node::aOutputStreamlines, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        parms.mStreamlines = lines.asBool();
        const MDataHandle integration = data.inputValue(Node::aIntegration, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const short order = integration.asShort();
        if (order < INTEGRATION_TYPE_FWD_EULER || order > INTEGRATION_TYPE_RK_4TH)
            return MS::kInvalidParameter;
        parms.mIntegrationType = static_cast<IntegrationType>(order);
    }
    if (parms.mPropagationType != PROPAGATION_TYPE_ADVECTION) {
        DOMMY_RETURN_IF_FAILURE(readPointGrid(data, Node::aVdbClosestPointInput,
                                              Node::aVdbClosestPointGridNames, parms.mCptVdb));
        const MDataHandle iterations = data.inputValue(Node::aIterations, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        parms.mIterations = iterations.asInt();
        if (parms.mIterations <= 0)
            return MS::kInvalidParameter;
    }
    if (parms.mVelVdb && parms.mCptVdb &&
        getVdbGridType(*parms.mVelVdb) != getVdbGridType(*parms.mCptVdb))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}

bool validateAdvectionBudget(const AdvectionPointParms& parms, const DPData& input,
                             std::size_t& work) {
    using namespace dommy::generation;
    const std::size_t count = input.numberOfPoints();
    std::size_t steps = 0, iterations = 0, samples = 0, perPoint = 0;
    if (!checkedCount(parms.mSteps, kMaxCurveSampleCount, steps) || steps == 0 ||
        !checkedCount(parms.mIterations, kMaxCurveSampleCount, iterations) || iterations == 0)
        return false;
    if (parms.mPropagationType == PROPAGATION_TYPE_PROJECTION)
        perPoint = iterations;
    else {
        perPoint = static_cast<std::size_t>(parms.mIntegrationType) + 1U;
        if (parms.mPropagationType == PROPAGATION_TYPE_CONSTRAINED_ADVECTION &&
            !checkedSum(perPoint, iterations, kMaxWorkItemCount, perPoint))
            return false;
        if (!checkedProduct(perPoint, steps, kMaxWorkItemCount, perPoint))
            return false;
    }
    if (!checkedProduct(count, perPoint, kMaxWorkItemCount, work))
        return false;
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(input, sizeof(DPoint), workspace);
    workspace.add(count, sizeof(DPoint)); // Staged positions, in addition to retained metadata.
    if (parms.mStreamlines) {
        if (count > kMaxGeneratedCurveCount ||
            !checkedSum(steps, 1U, kMaxCurveSampleCount, samples) ||
            !checkedProduct(count, samples, kMaxGeneratedPointCount, samples))
            return false;
        // The shared four-copy allowance covers retained CVs, knots and batch copies.
        // This is a host-payload admission budget, not a native allocator peak guarantee.
        workspace.add(samples, sizeof(MPoint) + sizeof(double) + sizeof(DPoint));
        workspace.add(count, sizeof(MPointArray) + sizeof(DCItem) + sizeof(DCurveInternalData) +
                                 2U * sizeof(double)); // Up to two extra cubic knots per curve.
    }
    return true;
}

template <typename GridT, typename VectorT>
bool pointSampleDomain(const GridT& grid, const VectorT& point, long double radius) {
    using Element = typename GridT::ValueType::ValueType;
    // Affine maps have their extrema at corners. The radius bounds every native
    // RK stage (including RK3's 2*v1-v0), with padding for Box/Staggered samplers.
    const int corners = radius == 0 ? 1 : 8;
    for (int corner = 0; corner < corners; ++corner) {
        openvdb::Vec3R world;
        for (int axis = 0; axis < 3; ++axis) {
            Element value = 0;
            if (!dommy::numeric::checkedNumericCast(static_cast<long double>(point[axis]) +
                                                        ((corner & (1 << axis)) ? radius : -radius),
                                                    value))
                return false;
            world[axis] = value;
        }
        const auto index = grid.transform().worldToIndex(world);
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::vdb::filterCoordinate(index[axis], 4))
                return false;
    }
    return true;
}

template <typename GridT>
bool preflightPointGrid(const GridT& grid, bool velocity, AdvectionPointParms& parms,
                        const DPointList& points, std::size_t& work) {
    using Element = typename GridT::ValueType::ValueType;
    if (!dommy::vdb::validTransportTransform(grid))
        return false;
    if (velocity) {
        long double speed = 0;
        Element timestep = 0, radius = 0;
        if (!dommy::numeric::checkedNumericCast(parms.mTimeStep, timestep) ||
            !dommy::vdb::transportValues(grid, work, speed) ||
            !dommy::numeric::checkedNumericCast(
                std::fabs(static_cast<long double>(timestep)) * speed * 4, radius))
            return false;
        parms.motionRadius = radius;
    } else {
        // CPT values are world positions, not velocities: do not apply a speed/squared-length
        // limit.
        dommy::vdb::ValuePreflight finite(work, 2);
        finite(grid);
        if (!finite.valid)
            return false;
        const long double limit = std::numeric_limits<Element>::max() / 64.0L;
        const auto bounded = [limit](const typename GridT::ValueType& value) {
            for (int axis = 0; axis < 3; ++axis)
                if (std::fabs(static_cast<long double>(value[axis])) > limit)
                    return false;
            return true;
        };
        if (!bounded(grid.background()))
            return false;
        for (auto it = grid.cbeginValueAll(); it; ++it)
            if (!bounded(*it))
                return false;
    }
    for (const auto& point : points)
        if (!pointSampleDomain(grid, point, parms.motionRadius))
            return false;
    return true;
}

bool preflightPointGrid(const Grid& grid, bool velocity, AdvectionPointParms& parms,
                        const DPointList& points, std::size_t& work) {
    if (grid.isType<openvdb::Vec3SGrid>())
        return preflightPointGrid(vdbGridCast<openvdb::Vec3SGrid>(grid), velocity, parms, points,
                                  work);
    if (grid.isType<openvdb::Vec3DGrid>())
        return preflightPointGrid(vdbGridCast<openvdb::Vec3DGrid>(grid), velocity, parms, points,
                                  work);
    return false;
}

template <typename VectorT> DPoint checkedPoint(const VectorT& point) {
    DPoint result;
    for (int axis = 0; axis < 3; ++axis)
        if (!dommy::numeric::checkedNumericCast(point[axis], result[axis]))
            throw std::domain_error("Point advection produced an unrepresentable DPoint");
    return result;
}

template <typename GridT, int Order, bool Staggered> class PointPropagationOp {
    using Vector = typename GridT::ValueType;
    using Element = typename Vector::ValueType;
    using Integrator = openvdb::tools::VelocityIntegrator<GridT, Staggered>;
    using ProjectorType = openvdb::tools::ClosestPointProjector<GridT>;

  public:
    PointPropagationOp(const GridT& grid, const AdvectionPointParms& parms, DPointList& points,
                       int steps)
        : grid_(grid), parms_(parms), points_(points), steps_(steps) {}
    void operator()(const tbb::blocked_range<std::size_t>& range) const {
        const bool projection = parms_.mPropagationType == PROPAGATION_TYPE_PROJECTION;
        const GridT* cpt = parms_.mCptVdb ? &vdbGridCast<GridT>(*parms_.mCptVdb) : nullptr;
        std::unique_ptr<Integrator> integrator;
        std::unique_ptr<ProjectorType> projector;
        if (!projection)
            integrator.reset(new Integrator(grid_));
        // One native projection at a time allows validation before EVERY sample.
        if (cpt)
            projector.reset(new ProjectorType(*cpt, 1));
        Element timestep = 0;
        if (!dommy::numeric::checkedNumericCast(parms_.mTimeStep, timestep))
            throw std::domain_error("Point-advection timestep is not representable");
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
            Vector world;
            for (int axis = 0; axis < 3; ++axis)
                if (!dommy::numeric::checkedNumericCast(points_[i][axis], world[axis]))
                    throw std::domain_error("Point-advection input is not representable");
            for (int step = 0; step < steps_; ++step) {
                if (integrator) {
                    if (!pointSampleDomain(grid_, world, parms_.motionRadius))
                        throw std::domain_error(
                            "Point trajectory exceeds the velocity sampling domain");
                    integrator->template rungeKutta<Order, Vector>(timestep, world);
                    (void)checkedPoint(world);
                }
                if (projector)
                    for (int iteration = 0; iteration < parms_.mIterations; ++iteration) {
                        if (!pointSampleDomain(*cpt, world, 0))
                            throw std::domain_error(
                                "Point trajectory exceeds the projection sampling domain");
                        projector->projectToConstraintSurface(world);
                        (void)checkedPoint(world);
                    }
            }
            // Retain native precision across non-streamline substeps. Only the
            // DPoint/streamline boundary rounds to float, as in the original node.
            points_[i] = checkedPoint(world);
        }
    }

  private:
    const GridT& grid_;
    const AdvectionPointParms& parms_;
    DPointList& points_;
    int steps_;
};

MStatus appendLineNodes(std::vector<MPointArray>& curves, const DPointList& points) {
    if (curves.size() != points.size())
        return MS::kFailure;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        DOMMY_RETURN_IF_FAILURE(curves[i].append(MPoint(p[0], p[1], p[2])));
    }
    return MS::kSuccess;
}

MStatus buildStreamlines(const std::vector<MPointArray>& curves, DCurveData& output) {
    DCurveOutputBatch batch;
    for (const auto& points : curves) {
        if (points.length() < 2U)
            return MS::kFailure;
        const int degree = points.length() < 4U ? 1 : 3;
        MDoubleArray knots;
        GenerateDCurveKnots(points.length(), degree, knots);
        if (!batch.append(DCItem(new DCurveInternalData(points, knots, degree))))
            return MS::kFailure;
    }
    return batch.publishTo(output) ? MS::kSuccess : MS::kFailure;
}

template <typename GridT, int Order, bool Staggered>
MStatus propagatePoints(const GridT& grid, const AdvectionPointParms& parms, DPointList& points,
                        DCurveData& output) {
    if (points.empty())
        return MS::kSuccess;
    const tbb::blocked_range<std::size_t> range(0U, points.size(), 10000U);
    if (!parms.mStreamlines) {
        tbb::parallel_for(
            range, PointPropagationOp<GridT, Order, Staggered>(grid, parms, points, parms.mSteps));
        return MS::kSuccess;
    }
    // Maya arrays/curve objects are touched only by the calling thread.
    std::vector<MPointArray> curves(points.size());
    DOMMY_RETURN_IF_FAILURE(appendLineNodes(curves, points));
    for (int step = 0; step < parms.mSteps; ++step) {
        tbb::parallel_for(range,
                          PointPropagationOp<GridT, Order, Staggered>(grid, parms, points, 1));
        DOMMY_RETURN_IF_FAILURE(appendLineNodes(curves, points));
    }
    return buildStreamlines(curves, output);
}

template <typename GridT, bool Staggered>
MStatus propagatePoints(const GridT& grid, const AdvectionPointParms& parms, DPointList& points,
                        DCurveData& output) {
    switch (parms.mIntegrationType) {
    case INTEGRATION_TYPE_FWD_EULER:
        return propagatePoints<GridT, 1, Staggered>(grid, parms, points, output);
    case INTEGRATION_TYPE_RK_2ND:
        return propagatePoints<GridT, 2, Staggered>(grid, parms, points, output);
    case INTEGRATION_TYPE_RK_3RD:
        return propagatePoints<GridT, 3, Staggered>(grid, parms, points, output);
    case INTEGRATION_TYPE_RK_4TH:
        return propagatePoints<GridT, 4, Staggered>(grid, parms, points, output);
    case INTEGRATION_TYPE_UNKNOWN:
        return MS::kInvalidParameter;
    }
    return MS::kInvalidParameter;
}

template <typename GridT>
MStatus propagatePoints(const GridT& grid, const AdvectionPointParms& parms, DPointList& points,
                        DCurveData& output) {
    return parms.mStaggered ? propagatePoints<GridT, true>(grid, parms, points, output)
                            : propagatePoints<GridT, false>(grid, parms, points, output);
}
} // namespace

MStatus DOpenVDBAdvectPoints::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP && plug != outDC)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* inputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, inputDPData));
    if (!inputDPData)
        return MS::kFailure;
    AdvectionPointParms parms;
    DOMMY_RETURN_IF_FAILURE(readPointAdvectionSettings(data, parms));
    std::size_t work = 0;
    if (!validateAdvectionBudget(parms, *inputDPData, work))
        return MS::kInvalidParameter;
    const DPointList* inputPoints = inputDPData->getPositionPtrConst();
    if (!inputPoints || inputPoints->size() != inputDPData->numberOfPoints())
        return MS::kFailure;
    if ((parms.mVelVdb && !preflightPointGrid(*parms.mVelVdb, true, parms, *inputPoints, work)) ||
        (parms.mCptVdb && !preflightPointGrid(*parms.mCptVdb, false, parms, *inputPoints, work)))
        return MS::kInvalidParameter;

    // All admission checks precede deep copies and host output allocations.
    MObject pointDataObject, curveDataObject;
    DPData* outputDPData = nullptr;
    DCurveData* outputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginDataObject<DPData>(DPData::id, pointDataObject, outputDPData));
    if (!outputDPData)
        return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, curveDataObject, outputCurves));
    if (!outputCurves)
        return MS::kFailure;
    outputDPData->setDPData(*inputDPData);
    DPointList stagedPoints(*inputPoints);
    const Grid& grid = parms.mVelVdb ? *parms.mVelVdb : *parms.mCptVdb;
    if (grid.isType<openvdb::Vec3SGrid>())
        status = propagatePoints(vdbGridCast<openvdb::Vec3SGrid>(grid), parms, stagedPoints,
                                 *outputCurves);
    else if (grid.isType<openvdb::Vec3DGrid>())
        status = propagatePoints(vdbGridCast<openvdb::Vec3DGrid>(grid), parms, stagedPoints,
                                 *outputCurves);
    else
        return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(status);
    if (!outputDPData->replacePositions(stagedPoints))
        return MS::kFailure;
    outputDPData->updateBBox();
    if (!outputDPData->hasValidPointTopology() || !outputCurves->hasValidCurveTopology())
        return MS::kFailure;
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(plug == outDP ? outDP : outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (plug == outDP)
        status = dommy::maya::publishPluginDataObject(output, pointDataObject, *outputDPData);
    else
        status = dommy::maya::publishPluginDataObject(output, curveDataObject, *outputCurves);
    DOMMY_RETURN_IF_FAILURE(status);
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBAdvectPoints")

MStatus DOpenVDBAdvectPoints::initialize() try {
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnStringData strData;
    // Retained for scene compatibility; group-filtered advection was never
    // implemented by this node.
    const MObject groupDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPointGroup = tAttr.create("PointGroup", "pgrp", MFnData::kString, groupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointGroup));

    // input velocity vdb
    aVdbVelocityInput =
        tAttr.create("VdbVelocityInput", "vdbvelinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbVelocityInput));

    const MObject velocityNamesDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbVelocityGridNames =
        tAttr.create("VelocityGridNames", "vgrids", MFnData::kString, velocityNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbVelocityGridNames));

    // input closest point vdb
    aVdbClosestPointInput = tAttr.create("VdbClosestPointInput", "vdbclptinput", DOpenVDBData::id,
                                         MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbClosestPointInput));

    const MObject closestPointNamesDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbClosestPointGridNames = tAttr.create("ClosestPointGridNames", "clgrids", MFnData::kString,
                                             closestPointNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbClosestPointGridNames));

    MFnEnumAttribute opAttr;
    aOperation = opAttr.create("Operation", "op", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < NUM_PROPAGATION_TYPES; ++i) {
        const PropagationType pt = static_cast<PropagationType>(i);
        DOMMY_RETURN_IF_FAILURE(opAttr.addField(propagationTypeToMenuName(pt).c_str(), pt));
    }
    DOMMY_RETURN_IF_FAILURE(opAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOperation));

    MFnEnumAttribute inteAttr;
    aIntegration = inteAttr.create("Integration", "inte", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < NUM_INTEGRATION_TYPES; ++i) {
        const IntegrationType it = static_cast<IntegrationType>(i);
        DOMMY_RETURN_IF_FAILURE(inteAttr.addField(integrationTypeToMenuName(it).c_str(), it));
    }
    DOMMY_RETURN_IF_FAILURE(inteAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIntegration));

    aIterations = nAttr.create("Iterations", "iter", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterations));

    aTimestep = nAttr.create("Timestep", "ts", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimestep));

    aSubsteps = nAttr.create("Substeps", "sst", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));

    aOutputStreamlines =
        nAttr.create("OutputStreamlines", "outsl", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputStreamlines));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    // attribute dependencies
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointGroup, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityInput, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityGridNames, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbClosestPointInput, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbClosestPointGridNames, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOperation, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIntegration, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterations, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimestep, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSubsteps, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputStreamlines, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointGroup, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityInput, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityGridNames, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbClosestPointInput, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbClosestPointGridNames, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOperation, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIntegration, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterations, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimestep, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSubsteps, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputStreamlines, outDC));

    return MS::kSuccess;
}

catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBAdvectPoints", "initialize",
                                                  std::current_exception());
}

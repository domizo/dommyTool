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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Remove_Divergence.cc

//
//  DOpenVDBRemoveDivergence.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/27.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBRemoveDivergence.h"
#include "../Common/DVDBComputeGuard.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include <openvdb/math/ConjGradient.h> // for JacobiPreconditioner
#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/LevelSetUtil.h> // for tools::sdfInteriorMask()
#include <openvdb/tools/PoissonSolver.h>
#include <openvdb/tools/Prune.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <cmath>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>

////////////////////////////////////////

namespace {
    using ColliderMaskGrid = openvdb::BoolGrid;
    using ColliderBBox = openvdb::BBoxd;
    using Coord = openvdb::Coord;

    enum ColliderType { CT_NONE, CT_BBOX, CT_STATIC, CT_DYNAMIC };

    const int DEFAULT_MAX_ITERATIONS = 10000;
    const double DEFAULT_MAX_ERROR = 1.0e-20;
}

namespace {

    struct SolverParms {
        SolverParms()
        : invertCollider(false)
        , colliderType(CT_NONE)
        , iterations(1)
        , absoluteError(-1.0)
        , outputState(openvdb::math::pcg::terminationDefaults<double>())
        , interrupter(nullptr)
        {}

        GridPtr velocityGrid;
        GridCPtr colliderGrid;
        GridPtr pressureGrid;
        GridCPtr domainMaskGrid;
        ColliderBBox colliderBBox;
        openvdb::CoordBBox colliderIndexBBox;
        bool invertCollider;
        ColliderType colliderType;
        int iterations;
        double absoluteError;
        openvdb::math::pcg::State outputState;
        openvdb::util::NullInterrupter* interrupter;
    };

    // Independent of Maya's soft slider maximum. The aggregate budget below
    // also charges the worst-case iterations across every selected grid.
    const int kMaxDivergenceIterations = DEFAULT_MAX_ITERATIONS;
    struct DivergenceSettings {
        int iterations = 50;
        double tolerance = -1;
        bool outputPressure = false, useCollider = false, invertCollider = false;
        short colliderType = 0;
        std::string colliderSelection;
    };
    MStatus readDivergenceSettings(MDataBlock& data, DivergenceSettings& settings) {
        MStatus status;
        const auto useIterations = data.inputValue(DOpenVDBRemoveDivergence::aUseiterations, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.iterations = DEFAULT_MAX_ITERATIONS;
        if (useIterations.asBool()) {
            const auto iterations = data.inputValue(DOpenVDBRemoveDivergence::aIterations, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.iterations = iterations.asInt();
        }
        if (settings.iterations < 1 || settings.iterations > kMaxDivergenceIterations)
            return MS::kInvalidParameter;
        const auto useTolerance = data.inputValue(DOpenVDBRemoveDivergence::aUsetolerance, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (useTolerance.asBool()) {
            const auto tolerance = data.inputValue(DOpenVDBRemoveDivergence::aTolerance, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.tolerance = tolerance.asFloat();
            if (!std::isfinite(settings.tolerance) || settings.tolerance < 0)
                return MS::kInvalidParameter;
        }
        const auto pressure = data.inputValue(DOpenVDBRemoveDivergence::aPressure, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.outputPressure = pressure.asBool();
        const auto useCollider = data.inputValue(DOpenVDBRemoveDivergence::aUsecollider, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.useCollider = useCollider.asBool();
        if (settings.useCollider) {
            const auto colliderType = data.inputValue(DOpenVDBRemoveDivergence::aCollidertype, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.colliderType = colliderType.asShort();
            if (settings.colliderType < 0 || settings.colliderType > 2)
                return MS::kInvalidParameter;
            const auto invert = data.inputValue(DOpenVDBRemoveDivergence::aInvertCollider, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.invertCollider = invert.asBool();
            const auto names = data.inputValue(DOpenVDBRemoveDivergence::aCollideVdbName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.colliderSelection = names.asString().asChar();
        }
        return MS::kSuccess;
    }

    struct DivergenceInspect {
        dommy::vdb::TreeEditBudget& budget;
        bool narrow;
        bool valid = false;
        dommy::math::Bounds3d bounds;
        DivergenceInspect(dommy::vdb::TreeEditBudget& b, bool singlePrecision)
            : budget(b), narrow(singlePrecision) {}
        template<typename GridT> void operator()(const GridT& grid) {
            using Value = typename GridT::ValueType;
            dommy::vdb::ValuePreflight values(budget.work, 2);
            values(grid);
            if (!values.valid ||
                !dommy::vdb::validTransportTransform(grid) ||
                !dommy::vdb::validDerivativeGridMap(grid.transform())) return;
            long double magnitude = dommy::vdb::TransportMagnitude<Value>::get(grid.background());
            for (auto it = grid.cbeginValueAll(); it; ++it)
                magnitude = std::max(magnitude, dommy::vdb::TransportMagnitude<Value>::get(*it));
            // Leave headroom for derivatives, pressure-vector products and
            // dynamic-collider conversion into the velocity grid's precision.
            const long double maximum = narrow ? std::numeric_limits<float>::max()
                                                : std::numeric_limits<double>::max();
            if (magnitude > std::sqrt(std::sqrt(maximum)) / 1024) return;
            for (auto it = grid.cbeginValueAll(); it; ++it) {
                if (it.isTileValue() && !it.isValueOn() &&
                    dommy::vdb::ValueComponents<Value>::matchesBackground(*it, grid.background(), Value(0)))
                    continue;
                openvdb::CoordBBox box;
                it.getBoundingBox(box);
                if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
                    !bounds.add(box.max()[0], box.max()[1], box.max()[2])) return;
            }
            if (!bounds.empty()) {
                openvdb::CoordBBox region;
                if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(bounds), region)) return;
            }
            valid = true;
        }
    };
    template<typename GridT>
    bool divergenceTreeBudget(const openvdb::CoordBBox& region, dommy::vdb::TreeEditBudget& budget) {
        using Root = typename GridT::TreeType::RootNodeType::ChildNodeType;
        return dommy::vdb::TreeNodeBudget<Root>::add(region, budget);
    }
    struct DivergenceSolveBudget {
        const openvdb::CoordBBox& region;
        dommy::vdb::TreeEditBudget& budget;
        int iterations;
        bool valid = false;
        DivergenceSolveBudget(const openvdb::CoordBBox& r, dommy::vdb::TreeEditBudget& b, int n)
            : region(r), budget(b), iterations(n) {}
        template<typename GridT> void operator()(const GridT&) {
            using Scalar = typename GridT::template ValueConverter<typename GridT::ValueType::ValueType>::Type;
            std::size_t voxels = 0;
            // 5.2 uses a seven-entry double matrix, Int32 index tree, PCG
            // vectors and Jacobi diagonal. 512 bytes/voxel is a conservative
            // admission allowance, not an allocator or wall-clock guarantee.
            valid = dommy::vdb::treeBlockCount(region, 1, voxels) &&
                budget.addBytes(voxels, 512) && budget.addWork(voxels, 64U * (std::size_t(iterations) + 4U)) &&
                divergenceTreeBudget<GridT>(region, budget) &&
                divergenceTreeBudget<GridT>(region, budget) &&
                divergenceTreeBudget<Scalar>(region, budget) &&
                divergenceTreeBudget<Scalar>(region, budget) &&
                divergenceTreeBudget<openvdb::Int32Grid>(region, budget) &&
                divergenceTreeBudget<ColliderMaskGrid>(region, budget) &&
                divergenceTreeBudget<ColliderMaskGrid>(region, budget) &&
                divergenceTreeBudget<ColliderMaskGrid>(region, budget);
        }
    };
    bool finiteDivergenceGrid(const GridCPtr& grid) {
        if (!grid) return false;
        std::size_t work = 0;
        dommy::vdb::ValuePreflight values(work, 1);
        return processVdbTopologyGrid(getVdbGridType(*grid), *grid, values) && values.valid;
    }
    struct DivergencePlan {
        bool solve = false;
        openvdb::CoordBBox colliderBox;
    };
    bool divergenceColliderBox(const Grid& velocity, const ColliderBBox& world,
                              openvdb::CoordBBox& index, openvdb::BBoxd& mapped) {
        if (!dommy::vdb::validGridBox(world)) return false;
        openvdb::Vec3d low, high;
        openvdb::math::calculateBounds(velocity.transform(), world.min(), world.max(), low, high);
        mapped = openvdb::BBoxd(low, high);
        // Preserve 5.2 worldToIndexNodeCentered's floor(x + 0.5) rounding,
        // but reject unsafe casts before invoking the native tree routines.
        for (int axis = 0; axis < 3; ++axis) { low[axis] += .5; high[axis] += .5; }
        return dommy::vdb::indexGridBox(openvdb::BBoxd(low, high), index);
    }

    ////////////////////////////////////////

    /// @brief Functor to extract an interior mask from a level set grid
    /// of arbitrary floating-point type
    struct LevelSetMaskOp
    {
        template<typename GridType>
        void operator()(const GridType& grid) { outputGrid = openvdb::tools::sdfInteriorMask(grid); }

        GridPtr outputGrid;
    };

    /// @brief Functor to extract a topology mask from a grid of arbitrary type
    struct ColliderMaskOp
    {
        template<typename GridType>
        void operator()(const GridType& grid)
        {
            if (mask) {
                mask->topologyUnion(grid);
                mask->setTransform(grid.transform().copy());
            }
        }

        ColliderMaskGrid::Ptr mask;
    };

    ////////////////////////////////////////

    /// @brief Generic grid accessor
    /// @details This just wraps a const accessor to a collider grid, but
    /// it changes the behavior of the copy constructor for thread safety.
    template<typename GridType>
    class GridConstAccessor
    {
    public:
        using ValueType = typename GridType::ValueType;

        explicit GridConstAccessor(const SolverParms& parms):
        mAcc(static_cast<const GridType&>(*parms.colliderGrid).getConstAccessor())
        {}
        explicit GridConstAccessor(const GridType& grid): mAcc(grid.getConstAccessor()) {}

        // When copying, create a new, empty accessor, to avoid a data race
        // with the existing accessor, which might be updating on another thread.
        GridConstAccessor(const GridConstAccessor& other): mAcc(other.mAcc.tree()) {}

        bool isValueOn(const Coord& ijk) const { return mAcc.isValueOn(ijk); }
        const ValueType& getValue(const Coord& ijk) const { return mAcc.getValue(ijk); }
        bool probeValue(const Coord& ijk, ValueType& val) const { return mAcc.probeValue(ijk, val); }

    private:
        GridConstAccessor& operator=(const GridConstAccessor&);

        typename GridType::ConstAccessor mAcc;
    }; // class GridConstAccessor

    using ColliderMaskAccessor = GridConstAccessor<ColliderMaskGrid>;

    /// @brief Bounding box accessor
    class BBoxConstAccessor
    {
    public:
        using ValueType = double;

        explicit BBoxConstAccessor(const SolverParms& parms):
        mBBox(parms.colliderIndexBBox) {}
        BBoxConstAccessor(const BBoxConstAccessor& other): mBBox(other.mBBox) {}

        // Voxels outside the bounding box are solid, i.e., active.
        bool isValueOn(const Coord& ijk) const { return !mBBox.isInside(ijk); }
        ValueType getValue(const Coord&) const { return ValueType(0); }
        bool probeValue(const Coord& ijk, ValueType& v) const { v=ValueType(0); return isValueOn(ijk); }

    private:
        BBoxConstAccessor& operator=(const BBoxConstAccessor&);

        const openvdb::CoordBBox mBBox;
    }; // class BBoxConstAccessor

    ////////////////////////////////////////

    template<typename TreeType> struct PressureLeafPair {
        typename TreeType::LeafNodeType* velocity;
        const typename TreeType::LeafNodeType* gradient;
    };

    template<typename TreeType>
    std::vector<PressureLeafPair<TreeType>> pressureLeafPairs(
        TreeType& velocity, const TreeType& gradient)
    {
        using Leaf = typename TreeType::LeafNodeType;
        std::vector<Leaf*> leaves;
        velocity.getNodes(leaves);
        std::vector<PressureLeafPair<TreeType>> pairs;
        pairs.reserve(leaves.size());
        for (Leaf* leaf : leaves) {
            if (!leaf) throw std::runtime_error("Null velocity leaf");
            // getNodes includes wholly inactive leaves, whereas pruneInactive
            // removes them from the gradient. Never zip the two arrays by index.
            if (leaf->isInactive()) continue;
            const Leaf* pressure = gradient.probeConstLeaf(leaf->origin());
            if (!pressure || pressure->origin() != leaf->origin())
                throw std::runtime_error("Pressure gradient leaf is missing or misaligned");
            pairs.push_back({leaf, pressure});
        }
        return pairs;
    }

    /// @brief Functor to compute pressure projection over prevalidated leaf pairs.
    template<typename TreeType>
    struct PressureProjectionOp
    {
        using LeafNodeType = typename TreeType::LeafNodeType;
        using ValueType = typename TreeType::ValueType;

        PressureProjectionOp(SolverParms& parms,
                             const std::vector<PressureLeafPair<TreeType>>& pairs, bool staggered)
        : mPairs(pairs)
        , mVoxelSize(parms.velocityGrid->transform().voxelSize()[0])
        , mStaggered(staggered)
        {
        }

        void operator()(const tbb::blocked_range<size_t>& range) const
        {
            using ElementType = typename ValueType::value_type;

            // Account for voxel size here, instead of in the Poisson solve.
            ElementType scale;
            if (!dommy::numeric::checkedNumericCast(
                    (mStaggered ? 1.0L : 4.0L) * mVoxelSize * mVoxelSize, scale) || scale <= 0)
                throw std::overflow_error("Pressure projection scale is not representable");

            for (size_t n = range.begin(), N = range.end(); n < N; ++n) {
                const auto& pair = mPairs.at(n);
                LeafNodeType& velocityNode = *pair.velocity;
                ValueType* velocityData = velocityNode.buffer().data();
                const ValueType* gradientOfPressureData = pair.gradient->buffer().data();

                for (typename LeafNodeType::ValueOnIter it = velocityNode.beginValueOn(); it; ++it) {
                    const openvdb::Index pos = it.pos();
                    ValueType projected;
                    for (int axis = 0; axis < 3; ++axis) {
                        const long double value = static_cast<long double>(velocityData[pos][axis]) -
                            static_cast<long double>(scale) * gradientOfPressureData[pos][axis];
                        if (!dommy::numeric::checkedNumericCast(value, projected[axis]))
                            throw std::overflow_error("Pressure projection result is not representable");
                    }
                    velocityData[pos] = projected;
                }
            }
        }

        const std::vector<PressureLeafPair<TreeType>>& mPairs;
        const double mVoxelSize;
        const bool mStaggered;
    }; // class PressureProjectionOp

    ////////////////////////////////////////

    /// @brief Functor for use with Tree::modifyValue() to set a single element
    /// of a vector-valued voxel
    template<typename VectorType>
    struct SetVecElemOp
    {
        using ValueType = typename VectorType::ValueType;

        SetVecElemOp(int axis_, ValueType value_): axis(axis_), value(value_) {}
        void operator()(VectorType& v) const { v[axis] = value; }

        const int axis;
        const ValueType value;
    };

    /// @brief Functor to correct the velocities of voxels adjacent to solid obstacles
    template<typename VelocityGridType>
    class CorrectCollisionVelocityOp
    {
    public:
        using VectorType = typename VelocityGridType::ValueType;
        using VectorElementType = typename VectorType::ValueType;
        using MaskGridType = typename VelocityGridType::template ValueConverter<bool>::Type;
        using MaskTreeType = typename MaskGridType::TreeType;

        explicit CorrectCollisionVelocityOp(SolverParms& parms): mParms(&parms)
        {
            const MaskGridType& domainMaskGrid =
            static_cast<const MaskGridType&>(*mParms->domainMaskGrid);
            typename MaskTreeType::Ptr interiorMask(
                                                    new MaskTreeType(domainMaskGrid.tree(), /*background=*/false, openvdb::TopologyCopy()));
            mBorderMask.reset(new MaskTreeType(*interiorMask));
            openvdb::tools::erodeVoxels(*interiorMask, /*iterations=*/1, openvdb::tools::NN_FACE);
            mBorderMask->topologyDifference(*interiorMask);
        }

        template<typename ColliderGridType>
        void operator()(const ColliderGridType&)
        {
            GridConstAccessor<ColliderGridType> collider(
                                                         static_cast<const ColliderGridType&>(*mParms->colliderGrid));
            correctVelocity(collider);
        }

        template<typename ColliderAccessorType>
        void correctVelocity(const ColliderAccessorType& collider)
        {
            using ColliderValueType = typename ColliderAccessorType::ValueType;

            VelocityGridType& velocityGrid = static_cast<VelocityGridType&>(*mParms->velocityGrid);

            typename VelocityGridType::Accessor velocity = velocityGrid.getAccessor();

            const bool invert = mParms->invertCollider;

            switch (mParms->colliderType) {
                case CT_NONE:
                    break;

                case CT_BBOX:
                case CT_STATIC:
                    // For each border voxel of the velocity grid...
                    /// @todo parallelize
                    for (typename MaskTreeType::ValueOnCIter it = mBorderMask->cbeginValueOn(); it; ++it) {
                        const Coord ijk = it.getCoord();

                        // If the neighbor in a certain direction is a stationary obstacle,
                        // set the border voxel's velocity in that direction to zero.

                        // x direction
                        if ((collider.isValueOn(ijk.offsetBy(-1, 0, 0)) != invert)
                            || (collider.isValueOn(ijk.offsetBy(1, 0, 0)) != invert))
                        {
                            velocity.modifyValue(ijk, SetVecElemOp<VectorType>(0, 0));
                        }
                        // y direction
                        if ((collider.isValueOn(ijk.offsetBy(0, -1, 0)) != invert)
                            || (collider.isValueOn(ijk.offsetBy(0, 1, 0)) != invert))
                        {
                            velocity.modifyValue(ijk, SetVecElemOp<VectorType>(1, 0));
                        }
                        // z direction
                        if ((collider.isValueOn(ijk.offsetBy(0, 0, -1)) != invert)
                            || (collider.isValueOn(ijk.offsetBy(0, 0, 1)) != invert))
                        {
                            velocity.modifyValue(ijk, SetVecElemOp<VectorType>(2, 0));
                        }
                    }
                    break;

                case CT_DYNAMIC:
                    // For each border voxel of the velocity grid...
                    /// @todo parallelize
                    for (typename MaskTreeType::ValueOnCIter it = mBorderMask->cbeginValueOn(); it; ++it) {
                        const Coord ijk = it.getCoord();

                        ColliderValueType colliderVal;

                        // If the neighbor in a certain direction is a moving obstacle,
                        // set the border voxel's velocity in that direction to the
                        // obstacle's velocity in that direction.
                        for (int axis = 0; axis <= 2; ++axis) { // 0:x, 1:y, 2:z
                            Coord neighbor = ijk;
                            neighbor[axis] -= 1;
                            if (collider.probeValue(neighbor, colliderVal) != invert) {
                                // Copy or create a Vec3 from the collider value and extract one of
                                // its components.
                                // (Since the collider is dynamic, ColliderValueType must be a Vec3 type,
                                // but this code has to compile for all ColliderGridTypes.)
                                VectorElementType colliderVelocity = VectorType(colliderVal)[axis];
                                velocity.modifyValue(ijk,
                                                     SetVecElemOp<VectorType>(axis, colliderVelocity));
                            } else {
                                neighbor = ijk;
                                neighbor[axis] += 1;
                                if (collider.probeValue(neighbor, colliderVal) != invert) {
                                    VectorElementType colliderVelocity = VectorType(colliderVal)[axis];
                                    velocity.modifyValue(ijk,
                                                         SetVecElemOp<VectorType>(axis, colliderVelocity));
                                }
                            }
                        }
                    }
                    break;
            } // switch (mParms->colliderType)
        }

    private:
        SolverParms* mParms;
        typename MaskTreeType::Ptr mBorderMask;
    }; // class CorrectCollisionVelocityOp

    ////////////////////////////////////////

    // Boundary condition functors

    /// @brief Functor specifying boundary conditions for the Poisson solver
    /// when exterior voxels may be either solid (and possibly in motion) or empty
    template<typename VelocityGridType, typename ColliderAccessorType>
    class ColliderBoundaryOp
    {
    public:
        using VectorType = typename VelocityGridType::ValueType;

        explicit ColliderBoundaryOp(const SolverParms& parms)
        : mVelocity(static_cast<VelocityGridType&>(*parms.velocityGrid).getConstAccessor())
        , mCollider(parms)
        , mInvert(parms.invertCollider)
        , mDynamic(parms.colliderType == CT_DYNAMIC)
        , mInvVoxelSize(0.5 / (parms.velocityGrid->voxelSize()[0])) // assumes uniform voxels
        {}

        ColliderBoundaryOp(const ColliderBoundaryOp& other)
        // Give this op new, empty accessors, to avoid data races with
        // the other op's accessors, which might be updating on another thread.
        : mVelocity(other.mVelocity.tree())
        , mCollider(other.mCollider)
        , mInvert(other.mInvert)
        , mDynamic(other.mDynamic)
        , mInvVoxelSize(other.mInvVoxelSize)
        {}

        void operator()(const Coord& ijk, const Coord& ijkNeighbor, double& rhs, double& diag) const
        {
            // Voxels outside both the velocity field and the collider
            // are considered to be empty (unless the collider is inverted).
            // Voxels outside the velocity field and inside the collider
            // are considered to be solid.
            if (mCollider.isValueOn(ijkNeighbor) == mInvert) {
                // The exterior neighbor is empty (i.e., zero), so just adjust the center weight.
                diag -= 1;
            } else {
                const VectorType& velocity = mVelocity.getValue(ijkNeighbor);
                double delta = 0.0;
                if (mDynamic) { // exterior neighbor is a solid obstacle with nonzero velocity
                    const openvdb::Vec3d colliderVelocity(mCollider.getValue(ijkNeighbor));
                    if (ijkNeighbor[0] < ijk[0]) { delta +=  velocity[0] - colliderVelocity[0]; }
                    if (ijkNeighbor[0] > ijk[0]) { delta -= (velocity[0] - colliderVelocity[0]); }
                    if (ijkNeighbor[1] < ijk[1]) { delta +=  velocity[1] - colliderVelocity[1]; }
                    if (ijkNeighbor[1] > ijk[1]) { delta -= (velocity[1] - colliderVelocity[1]); }
                    if (ijkNeighbor[2] < ijk[2]) { delta +=  velocity[2] - colliderVelocity[2]; }
                    if (ijkNeighbor[2] > ijk[2]) { delta -= (velocity[2] - colliderVelocity[2]); }
                } else { // exterior neighbor is a stationary solid obstacle
                    if (ijkNeighbor[0] < ijk[0]) { delta += velocity[0]; }
                    if (ijkNeighbor[0] > ijk[0]) { delta -= velocity[0]; }
                    if (ijkNeighbor[1] < ijk[1]) { delta += velocity[1]; }
                    if (ijkNeighbor[1] > ijk[1]) { delta -= velocity[1]; }
                    if (ijkNeighbor[2] < ijk[2]) { delta += velocity[2]; }
                    if (ijkNeighbor[2] > ijk[2]) { delta -= velocity[2]; }
                }
                rhs += delta * mInvVoxelSize;
                // Note: no adjustment to the center weight (diag).
            }
        }

    private:
        // Disable assignment (due to const members).
        ColliderBoundaryOp& operator=(const ColliderBoundaryOp&);

        typename VelocityGridType::ConstAccessor mVelocity; // accessor to the velocity grid
        ColliderAccessorType mCollider; // accessor to the collider
        const bool mInvert; // invert the collider?
        const bool mDynamic; // is the collider moving?
        const double mInvVoxelSize;
    }; // class ColliderBoundaryOp

    ////////////////////////////////////////

    /// @brief Main solver routine
    template<typename VectorGridType, typename ColliderGridType, typename BoundaryOpType>
    inline bool
    removeDivergenceWithColliderGrid(SolverParms& parms, const BoundaryOpType& boundaryOp)
    {
        using VectorTreeType = typename VectorGridType::TreeType;
        using VectorType = typename VectorGridType::ValueType;
        using VectorElementType = typename VectorType::ValueType;

        using ScalarGrid = typename VectorGridType::template ValueConverter<VectorElementType>::Type;
        using ScalarTree = typename ScalarGrid::TreeType;

        using MaskGridType = typename VectorGridType::template ValueConverter<bool>::Type;

        VectorGridType& velocityGrid = static_cast<VectorGridType&>(*parms.velocityGrid);

        const bool staggered = ((velocityGrid.getGridClass() == openvdb::GRID_STAGGERED)
                                && (openvdb::VecTraits<VectorType>::Size == 3));

        // Compute the divergence of the incoming velocity field.
        /// @todo Consider neighboring collider velocities at border voxels?
        openvdb::tools::Divergence<VectorGridType> divergenceOp(velocityGrid);
        typename ScalarGrid::ConstPtr divGrid = divergenceOp.process();
        if (!finiteDivergenceGrid(divGrid))
            throw std::runtime_error("Divergence produced invalid values");

        parms.outputState = openvdb::math::pcg::terminationDefaults<VectorElementType>();
        parms.outputState.iterations = parms.iterations;
        parms.outputState.absoluteError = (parms.absoluteError >= 0.0 ?
                                           parms.absoluteError : DEFAULT_MAX_ERROR);
        parms.outputState.relativeError = 0.0;

        using PCT = openvdb::math::pcg::JacobiPreconditioner<openvdb::tools::poisson::LaplacianMatrix>;

        // Solve for pressure using Poisson's equation.
        typename ScalarTree::Ptr pressure;
        if (parms.colliderType == CT_NONE) {
            pressure = openvdb::tools::poisson::solveWithBoundaryConditionsAndPreconditioner<PCT>(
                                                                                                  divGrid->tree(), boundaryOp, parms.outputState, *parms.interrupter, staggered);
        } else {
            // Create a domain mask by clipping the velocity grid's topology against the collider's.
            // Pressure will be computed only where the domain mask is active.
            MaskGridType* domainMaskGrid = new MaskGridType(*divGrid); // match input grid's topology
            parms.domainMaskGrid.reset(domainMaskGrid);
            if (parms.colliderType == CT_BBOX) {
                if (parms.invertCollider) {
                    // Solve for pressure only outside the bounding box.
                    domainMaskGrid->fill(parms.colliderIndexBBox, false, false);
                } else {
                    // Solve for pressure only inside the bounding box.
                    domainMaskGrid->clipGrid(parms.colliderBBox);
                }
            } else {
                const ColliderGridType& colliderGrid =
                static_cast<const ColliderGridType&>(*parms.colliderGrid);
                if (parms.invertCollider) {
                    // Solve for pressure only inside the collider.
                    domainMaskGrid->topologyIntersection(colliderGrid);
                } else {
                    // Solve for pressure only outside the collider.
                    domainMaskGrid->topologyDifference(colliderGrid);
                }
            }
            pressure = openvdb::tools::poisson::solveWithBoundaryConditionsAndPreconditioner<PCT>(
                                                                                                  divGrid->tree(), domainMaskGrid->tree(), boundaryOp, parms.outputState,
                                                                                                  *parms.interrupter, staggered);
        }

        // A failed solve preserves the incoming grid; do not run projection on
        // an unconverged or absent pressure tree.
        if (!parms.outputState.success) return false;
        if (!pressure) throw std::runtime_error("Pressure solver returned no tree");

        // Store the computed pressure grid.
        parms.pressureGrid = ScalarGrid::create(pressure);
        if (!finiteDivergenceGrid(parms.pressureGrid))
            throw std::runtime_error("Pressure solver produced invalid values");
        parms.pressureGrid->setTransform(velocityGrid.transform().copy());
        {
            std::string name = parms.velocityGrid->getName();
            if (!name.empty()) name += "_";
            name += "pressure";
            parms.pressureGrid->setName(name);
        }

        // Compute the gradient of the pressure.
        openvdb::tools::Gradient<ScalarGrid> gradientOp(static_cast<ScalarGrid&>(*parms.pressureGrid));
        typename VectorGridType::Ptr gradientOfPressure = gradientOp.process();
        if (!finiteDivergenceGrid(gradientOfPressure))
            throw std::runtime_error("Pressure gradient produced invalid values");

        // Compute pressure projection in parallel over leaf nodes.
        {
            // Pressure (and therefore the gradient of the pressure) is computed only where
            // the domain mask is active, but the gradient and velocity grid topologies must match
            // so that pressure projection can be computed in parallel over leaf nodes (see below).
            velocityGrid.tree().voxelizeActiveTiles();
            gradientOfPressure->topologyUnion(velocityGrid);
            gradientOfPressure->topologyIntersection(velocityGrid);
            openvdb::tools::pruneInactive(gradientOfPressure->tree());

            const auto pairs = pressureLeafPairs(velocityGrid.tree(), gradientOfPressure->tree());
            tbb::parallel_for(tbb::blocked_range<size_t>(0, pairs.size()),
                              PressureProjectionOp<VectorTreeType>(parms, pairs, staggered));
        }

        if (parms.colliderType != CT_NONE) {
            // When obstacles are present, the Poisson solve returns a divergence-free
            // velocity field in the interior of the input grid, but border voxels
            // need to be adjusted manually to match neighboring collider velocities.
            CorrectCollisionVelocityOp<VectorGridType> op(parms);
            if (parms.colliderType == CT_BBOX) {
                op.correctVelocity(BBoxConstAccessor(parms));
            } else {
                processVdbTopologyGrid(
                                              getVdbGridType(*parms.colliderGrid), *parms.colliderGrid, op);
            }
        }

        return parms.outputState.success;
    }

    /// @brief Main solver routine in the case of no collider or a bounding box collider
    template<typename VectorGridType, typename BoundaryOpType>
    inline bool
    removeDivergence(SolverParms& parms, const BoundaryOpType& boundaryOp)
    {
        return removeDivergenceWithColliderGrid<VectorGridType, VectorGridType>(parms, boundaryOp);
    }

    /// @brief Functor to invoke the solver with a collider velocity grid of arbitrary vector type
    template<typename VelocityGridType>
    struct ColliderDispatchOp
    {
        SolverParms* parms;
        bool success;

        explicit ColliderDispatchOp(SolverParms& parms_): parms(&parms_) , success(false) {}

        template<typename ColliderGridType>
        void operator()(const ColliderGridType&)
        {
            using ColliderAccessorType = GridConstAccessor<ColliderGridType>;
            ColliderBoundaryOp<VelocityGridType, ColliderAccessorType> boundaryOp(*parms);
            success = removeDivergenceWithColliderGrid<VelocityGridType, ColliderGridType>(
                                                                                           *parms, boundaryOp);
        }
    }; // struct ColliderDispatchOp

    /// @brief Invoke the solver for collider inputs of various types (or no collider).
    template<typename VelocityGridType>
    inline bool
    processGrid(SolverParms& parms)
    {
        bool success = false;
        switch (parms.colliderType) {
            case CT_NONE:
                // No collider
                success = removeDivergence<VelocityGridType>(
                                                             parms, openvdb::tools::poisson::DirichletBoundaryOp<double>());
                break;
            case CT_BBOX:
                // If collider geometry was supplied, the faces of its bounding box
                // define solid obstacles.
                success = removeDivergence<VelocityGridType>(parms,
                                                             ColliderBoundaryOp<VelocityGridType, BBoxConstAccessor>(parms));
                break;
            case CT_STATIC:
                // If a static collider grid was supplied, its active voxels define solid obstacles.
                success = removeDivergenceWithColliderGrid<VelocityGridType, ColliderMaskGrid>(
                                                                                               parms, ColliderBoundaryOp<VelocityGridType, ColliderMaskAccessor>(parms));
                break;
            case CT_DYNAMIC:
            {
                // If a dynamic collider grid was supplied, its active values define
                // the velocities of solid obstacles.
                ColliderDispatchOp<VelocityGridType> op(parms);
                success = processVdbVectorGrid(
                                                    getVdbGridType(*parms.colliderGrid), *parms.colliderGrid, op);
                if (success) success = op.success;
                break;
            }
        }
        return success;
    }

            joined.append(names[index]);
        }
        return "VDB" + (((names.size() == 1) ? " " : "s ") + joined);
    }

} // unnamed namespace

const MTypeId DOpenVDBRemoveDivergence::typeId( ID_VDB_REMOVE_DIVERGENCE );

MObject DOpenVDBRemoveDivergence::aVdbInput;
MObject DOpenVDBRemoveDivergence::aVdbSelectedGridNames;

MObject DOpenVDBRemoveDivergence::aUseiterations;
MObject DOpenVDBRemoveDivergence::aIterations;
MObject DOpenVDBRemoveDivergence::aUsetolerance;
MObject DOpenVDBRemoveDivergence::aTolerance;
MObject DOpenVDBRemoveDivergence::aUsecollider;
MObject DOpenVDBRemoveDivergence::aCollidertype;
MObject DOpenVDBRemoveDivergence::aCollideVdb;
MObject DOpenVDBRemoveDivergence::aCollideVdbName;
MObject DOpenVDBRemoveDivergence::aInvertCollider;
MObject DOpenVDBRemoveDivergence::aPressure;

MObject DOpenVDBRemoveDivergence::aVdbOutput;

void* DOpenVDBRemoveDivergence::creator()
{
    return ::dommy::maya::createObject("DOpenVDBRemoveDivergence", [&]() {
        return new DOpenVDBRemoveDivergence();
    });
}

MStatus DOpenVDBRemoveDivergence::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) return MS::kUnknownParameter;
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, inputVdb));
    MStatus status;
    const auto names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selectGroupName = names.asString().asChar();
    GridCPtrVec grids;
    getSelectedGrids(grids, selectGroupName, *inputVdb);

    DivergenceSettings settings;
    SolverParms parms;
    dommy::vdb::TreeEditBudget budget;
    std::vector<DivergencePlan> plans(grids.size());
    std::size_t outputCount = inputVdb->numberOfGrids();
    GridCPtr collider;
    bool levelSetCollider = false;
    if (!grids.empty()) {
        DOMMY_RETURN_IF_FAILURE(readDivergenceSettings(data, settings));
        parms.iterations = settings.iterations;
        parms.absoluteError = settings.tolerance;
        parms.invertCollider = settings.invertCollider;
        bool narrow = false;
        for (const auto& grid : grids)
            narrow = narrow || getVdbGridType(*grid) == DVdbGridType::kVec3f;
        if (settings.useCollider) {
            const DOpenVDBData* colVdb = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aCollideVdb, data, colVdb));
            GridCPtrVec colliders;
            getSelectedGrids(colliders, settings.colliderSelection, *colVdb);
            if (colliders.empty()) return MS::kInvalidParameter;
            if (colliders.size() > 1)
                MGlobal::displayWarning("Multiple collider grids selected; only the first will be used.");
            collider = colliders.front();
            DivergenceInspect inspect(budget, narrow);
            if (!budget.addBytes(2, collider->memUsage()) ||
                !processVdbTopologyGrid(getVdbGridType(*collider), *collider, inspect) || !inspect.valid)
                return MS::kInvalidParameter;
            if (settings.colliderType == 0) {
                if (!getActiveVoxelWorldBounds(*collider, parms.colliderBBox) ||
                    !dommy::vdb::validGridBox(parms.colliderBBox))
                    return MS::kInvalidParameter;
                parms.colliderType = CT_BBOX;
            } else if (!collider->empty() || collider->getGridClass() == openvdb::GRID_LEVEL_SET) {
                levelSetCollider = collider->getGridClass() == openvdb::GRID_LEVEL_SET;
                const auto type = getVdbGridType(*collider);
                if (levelSetCollider && type != DVdbGridType::kFloat && type != DVdbGridType::kDouble)
                    return MS::kInvalidParameter;
                if (levelSetCollider) {
                    dommy::vdb::PositiveMaskBackground background;
                    if (!processVdbRealGrid(type, *collider, background) || !background.valid)
                        return MS::kInvalidParameter;
                }
                parms.colliderType = !levelSetCollider && settings.colliderType == 2 &&
                    getVdbGridTupleSize(type) == 3 ? CT_DYNAMIC : CT_STATIC;
                if (parms.colliderType == CT_STATIC && !inspect.bounds.empty()) {
                    openvdb::CoordBBox region;
                    if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(inspect.bounds), region) ||
                        !divergenceTreeBudget<ColliderMaskGrid>(region, budget) ||
                        !divergenceTreeBudget<ColliderMaskGrid>(region, budget))
                        return MS::kInvalidParameter;
                }
            } else {
                collider.reset(); // Historical empty-collider behavior: no obstacle.
            }
        }

        // Plan the complete batch before making any native copy, mask or solve.
        for (std::size_t i = 0; i < grids.size(); ++i) {
            const auto& grid = *grids[i];
            if (!budget.addBytes(1, grid.memUsage())) return MS::kInvalidParameter;
            const auto type = getVdbGridType(grid);
            if (type != DVdbGridType::kVec3f && type != DVdbGridType::kVec3d) continue;
            DivergenceInspect inspect(budget, type == DVdbGridType::kVec3f);
            if (!processVdbRealVectorGrid(type, grid, inspect) || !inspect.valid)
                return MS::kInvalidParameter;
            if (!grid.transform().hasUniformScale()) {
                MGlobal::displayWarning("Remove Divergence requires uniform voxels; input passed through.");
                continue;
            }
            if (collider && parms.colliderType != CT_BBOX &&
                collider->transform() != grid.transform()) {
                MGlobal::displayWarning("Velocity and collider transforms differ; input passed through.");
                continue;
            }
            if (grid.empty() || inspect.bounds.empty()) continue;
            openvdb::BBoxd bounds = dommy::vdb::gridBounds(inspect.bounds);
            if (parms.colliderType == CT_BBOX) {
                openvdb::BBoxd mapped;
                if (!divergenceColliderBox(grid, parms.colliderBBox, plans[i].colliderBox, mapped))
                    return MS::kInvalidParameter;
                // Inverted fill can materialize inactive mask tiles outside the
                // velocity domain; include that region in the admission estimate.
                bounds.expand(mapped);
            }
            bounds.expand(2); // native stencils and boundary-neighbor coordinates
            openvdb::CoordBBox region;
            if (!dommy::vdb::indexGridBox(bounds, region)) return MS::kInvalidParameter;
            DivergenceSolveBudget solveBudget(region, budget, settings.iterations);
            if (!processVdbRealVectorGrid(type, grid, solveBudget) || !solveBudget.valid)
                return MS::kInvalidParameter;
            plans[i].solve = true;
            if (settings.outputPressure && !dommy::generation::checkedSum(
                    outputCount, 1, dommy::vdb::kMaxVdbGridCount, outputCount))
                return MS::kInvalidParameter;
        }
    }

    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb) return MS::kFailure;
    GridCPtrVec selected;
    getSelectedGrids(selected, selectGroupName, *inputVdb, *outputVdb);
    if (grids.empty())
        MGlobal::displayWarning("No grids match Remove Divergence; input passed through.");

    bool willSolve = false;
    for (const auto& plan : plans) willSolve = willSolve || plan.solve;
    if (willSolve && collider && parms.colliderType != CT_BBOX) {
        parms.colliderGrid = collider;
        if (levelSetCollider) {
            LevelSetMaskOp op;
            if (!processVdbRealGrid(getVdbGridType(*collider), *collider, op) || !op.outputGrid)
                return MS::kFailure;
            parms.colliderGrid = op.outputGrid;
        }
        // An inactive level-set interior can still generate an active mask.
        // Test emptiness only after that conversion, as in the legacy path.
        if (parms.colliderGrid->empty()) {
            parms.colliderGrid.reset();
            parms.colliderType = CT_NONE;
        } else if (parms.colliderType == CT_STATIC) {
            ColliderMaskOp op;
            op.mask = ColliderMaskGrid::create();
            if (!op.mask || !processVdbTopologyGrid(getVdbGridType(*parms.colliderGrid),
                                                   *parms.colliderGrid, op))
                return MS::kFailure;
            parms.colliderGrid = op.mask;
        }
    }
    openvdb::util::NullInterrupter interrupter;
    parms.interrupter = &interrupter;
    for (std::size_t i = 0; i < grids.size(); ++i) {
        const auto& grid = *grids[i];
        if (!plans[i].solve) {
            outputVdb->insert(grid);
            continue;
        }
        parms.velocityGrid = grid.deepCopyGrid();
        parms.pressureGrid.reset();
        parms.domainMaskGrid.reset();
        parms.colliderIndexBBox = plans[i].colliderBox;
        const auto type = getVdbGridType(grid);
        if (!parms.velocityGrid || parms.velocityGrid.get() == grids[i].get() ||
            getVdbGridType(*parms.velocityGrid) != type)
            return MS::kFailure;
        const bool success = type == DVdbGridType::kVec3f
            ? processGrid<openvdb::Vec3SGrid>(parms) : processGrid<openvdb::Vec3DGrid>(parms);
        if (!success) {
            MGlobal::displayWarning("Remove Divergence solver did not converge; input passed through.");
            outputVdb->insert(grid);
            continue;
        }
        const auto pressureType = type == DVdbGridType::kVec3f ? DVdbGridType::kFloat : DVdbGridType::kDouble;
        if (!finiteDivergenceGrid(parms.velocityGrid) || !finiteDivergenceGrid(parms.pressureGrid) ||
            getVdbGridType(*parms.velocityGrid) != type ||
            getVdbGridType(*parms.pressureGrid) != pressureType)
            return MS::kFailure;
        outputVdb->insert(parms.velocityGrid);
        if (settings.outputPressure) outputVdb->insert(parms.pressureGrid);
    }
    if (!outputVdb->hasValidData()) return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBRemoveDivergence")

MStatus DOpenVDBRemoveDivergence::initialize() try
{
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    //input vdb
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;
    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    aUseiterations = nAttr.create("Useiterations", "useitr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseiterations));

    aIterations = nAttr.create("Iterations", "itr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(50));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterations));

    aUsetolerance = nAttr.create("Usetolerance", "usetol", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUsetolerance));

    aTolerance = nAttr.create("Tolerance", "tol", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(openvdb::math::Delta<float>::value()));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    aUsecollider = nAttr.create("Usecollider", "usecol", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUsecollider));

    MFnEnumAttribute coltypeAttr;
    aCollidertype = coltypeAttr.create("Collidertype", "colty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.addField("Bounding Box", 0));
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.addField("Static VDB", 1));
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.addField("Dynamic VDB", 2));
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollidertype));

    aCollideVdb = tAttr.create("CollideVdb", "colvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideVdb));
    const MObject colliderDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCollideVdbName = tAttr.create("CollideVdbName", "colvdbnm", MFnData::kString, colliderDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideVdbName));

    aInvertCollider = nAttr.create("InvertCollider", "invcol", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertCollider));

    aPressure = nAttr.create("Pressure", "prs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPressure));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseiterations, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterations, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUsetolerance, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUsecollider, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollidertype, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollideVdb, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollideVdbName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInvertCollider, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPressure, aVdbOutput));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBRemoveDivergence", "initialize",
                                                  std::current_exception());
}

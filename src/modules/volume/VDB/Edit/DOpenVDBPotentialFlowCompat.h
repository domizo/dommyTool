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

// Adapted from OpenVDB 5.2 PotentialFlow.h (Todd Keeler and Dan Bailey).
// Local changes, 2026-09-20: own voxel width, check boundary arithmetic and
// null results. Keep the upstream Poisson implementation and staggered solve.
// https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb/tools/PotentialFlow.h
#pragma once

#include "common/utility/DNumericValidation.h"
#include <openvdb/tools/PoissonSolver.h>
#include <cmath>
#include <stdexcept>

namespace dommy {
namespace vdb {
template<typename VectorGrid> class PotentialBoundary {
public:
    PotentialBoundary(const VectorGrid& velocity, double voxelWidth)
        : velocity_(velocity), voxelWidth_(voxelWidth) {
        if (!std::isfinite(voxelWidth_) || voxelWidth_ <= 0)
            throw std::invalid_argument("Invalid Potential Flow voxel width");
    }
    void operator()(const openvdb::Coord& point, const openvdb::Coord& neighbor,
                    double& source, double& diagonal) const {
        const auto accessor = velocity_.getConstAccessor();
        if (accessor.isValueOn(point)) {
            const auto velocity = accessor.getValue(point);
            // Widen BEFORE subtracting coordinates, and preserve component order.
            for (int axis = 0; axis < 3; ++axis) {
                const long double offset = static_cast<long double>(point[axis]) - neighbor[axis];
                const long double value = source + voxelWidth_ * offset * velocity[axis];
                if (!numeric::checkedNumericCast(value, source))
                    throw std::overflow_error("Potential Flow boundary overflow");
            }
        } else {
            if (!numeric::checkedNumericCast(static_cast<long double>(diagonal) - 1, diagonal))
                throw std::overflow_error("Potential Flow diagonal overflow");
        }
    }
private:
    const VectorGrid& velocity_;
    const double voxelWidth_; // Grid::voxelSize() returns a temporary Vec3.
};

template<typename VectorGrid, typename MaskGrid>
typename VectorGrid::template ValueConverter<typename VectorGrid::ValueType::value_type>::Type::Ptr
safeScalarPotential(const MaskGrid& domain, const VectorGrid& neumann,
                    openvdb::math::pcg::State& state) {
    using Scalar = typename VectorGrid::ValueType::value_type;
    using ScalarGrid = typename VectorGrid::template ValueConverter<Scalar>::Type;
    using ScalarTree = typename ScalarGrid::TreeType;
    ScalarTree rhs(domain.tree(), Scalar(0), openvdb::TopologyCopy());
    rhs.voxelizeActiveTiles();
    PotentialBoundary<VectorGrid> boundary(neumann, domain.voxelSize()[0]);
    openvdb::util::NullInterrupter interrupter;
    const auto tree = openvdb::tools::poisson::solveWithBoundaryConditions(
        rhs, boundary, state, interrupter, true);
    if (!tree) throw std::runtime_error("Potential Flow returned no potential tree");
    auto result = ScalarGrid::create(tree);
    if (!result) throw std::runtime_error("Potential Flow returned no potential grid");
    result->setTransform(domain.transform().copy());
    return result;
}
} // namespace vdb
} // namespace dommy


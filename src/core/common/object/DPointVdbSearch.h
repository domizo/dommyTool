#pragma once

#include "DPointOp.h"
#include "DOpenVDBDataHeaders.h"

#include <openvdb/tools/ParticleAtlas.h>
#include <openvdb/tools/PointIndexGrid.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

typedef openvdb::tools::ParticleAtlas<>::Iterator ParticleAtlasIterator;
typedef openvdb::tools::ParticleAtlas<> ParticleAtlas;

class DPointVdbParticleList {
public:
    using PosType = openvdb::Vec3R;
    using ScalarType = PosType::value_type;

    DPointVdbParticleList(
        const std::vector<PosType>& positions,
        const std::vector<double>& radii)
        : positions_(&positions), radii_(&radii)
    {
        if (positions.size() != radii.size()) {
            throw std::invalid_argument(
                "OpenVDB search positions and radii must have equal lengths");
        }
        for (std::size_t index = 0U; index < positions.size(); ++index) {
            const PosType& position = positions[index];
            if (!std::isfinite(position.x()) ||
                !std::isfinite(position.y()) ||
                !std::isfinite(position.z()) ||
                !std::isfinite(radii[index]) || radii[index] < 0.0) {
                throw std::invalid_argument(
                    "OpenVDB search particles must be finite with non-negative radii");
            }
        }
    }

    std::size_t size() const { return positions_->size(); }

    void getPos(std::size_t index, PosType& position) const {
        position = (*positions_)[index];
    }

    void getRadius(std::size_t index, ScalarType& radius) const {
        radius = static_cast<ScalarType>((*radii_)[index]);
    }

private:
    const std::vector<PosType>* positions_;
    const std::vector<double>* radii_;
};

class PointSearchOp {
public:
    PointSearchOp(const ParticleAtlas& atlas,
                  const DPointVdbParticleList& particles,
                  std::size_t maxSampling,
                  double searchRadius,
                  IDsListArray& idListArray,
                  const std::vector<openvdb::Vec3R>& points)
        : atlas_(&atlas),
          particles_(&particles),
          maxSampling_(maxSampling),
          searchRadius_(searchRadius),
          idListArray_(&idListArray),
          points_(&points)
    {
    }

    void operator()(const tbb::blocked_range<std::size_t>& range) const
    {
        const std::size_t end = std::min(
            range.end(), std::min(points_->size(), idListArray_->size()));
        for (std::size_t index = range.begin(); index < end; ++index) {
            ParticleAtlasIterator iterator(*atlas_);
            iterator.updateFromLevel(0);
            iterator.worldSpaceSearchAndUpdate(
                (*points_)[index], searchRadius_, *particles_);

            IDs indices;
            for (; iterator; ++iterator) {
                if (index == static_cast<std::size_t>(*iterator)) continue;
                indices.push_back(static_cast<std::uint32_t>(*iterator));
                if (indices.size() >= maxSampling_) {
                    break;
                }
            }
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            (*idListArray_)[index].swap(indices);
        }
    }

private:
    const ParticleAtlas* atlas_;
    const DPointVdbParticleList* particles_;
    std::size_t maxSampling_;
    double searchRadius_;
    IDsListArray* idListArray_;
    const std::vector<openvdb::Vec3R>* points_;
};

} // namespace

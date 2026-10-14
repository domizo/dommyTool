#pragma once

#include <cstddef>
#include <limits>

namespace dommy {
namespace arnold {

// Describes the relationship between a DCurve's source points and Arnold's
// control-vertex and varying data arrays. Keeping these counts and index
// mappings together prevents the two arrays from silently drifting apart.
class CurveLayout {
  public:
    CurveLayout()
        : sourcePointCount_(0), controlVertexCount_(0), varyingCount_(0), cubic_(false),
          addEdgeControlVertices_(false), valid_(false) {}

    static CurveLayout create(std::size_t sourcePointCount, bool cubic,
                              bool addEdgeControlVertices) {
        CurveLayout layout;
        layout.sourcePointCount_ = sourcePointCount;
        layout.cubic_ = cubic;
        layout.addEdgeControlVertices_ = cubic && addEdgeControlVertices;

        if (sourcePointCount == 0)
            return layout;

        if (!cubic) {
            layout.controlVertexCount_ = sourcePointCount;
            layout.varyingCount_ = sourcePointCount;
            layout.valid_ = true;
            return layout;
        }

        if (addEdgeControlVertices) {
            if (sourcePointCount > std::numeric_limits<std::size_t>::max() - 4)
                return layout;
            layout.controlVertexCount_ = sourcePointCount + 4;
            layout.varyingCount_ = sourcePointCount + 2;
            layout.valid_ = true;
            return layout;
        }

        // Cubic Arnold curves without duplicated edge CVs need at least four
        // source points. Their varying data omits the two unsupported ends.
        if (sourcePointCount < 4)
            return layout;

        layout.controlVertexCount_ = sourcePointCount;
        layout.varyingCount_ = sourcePointCount - 2;
        layout.valid_ = true;
        return layout;
    }

    bool valid() const { return valid_; }
    std::size_t sourcePointCount() const { return sourcePointCount_; }
    std::size_t controlVertexCount() const { return controlVertexCount_; }
    std::size_t varyingCount() const { return varyingCount_; }

    std::size_t controlVertexSourceIndex(std::size_t outputIndex) const {
        if (!valid_ || outputIndex >= controlVertexCount_)
            return sourcePointCount_;
        if (!addEdgeControlVertices_)
            return outputIndex;
        if (outputIndex < 2)
            return 0;
        if (outputIndex >= sourcePointCount_ + 2)
            return sourcePointCount_ - 1;
        return outputIndex - 2;
    }

    std::size_t varyingSourceIndex(std::size_t outputIndex) const {
        if (!valid_ || outputIndex >= varyingCount_)
            return sourcePointCount_;
        if (!cubic_)
            return outputIndex;
        if (!addEdgeControlVertices_)
            return outputIndex + 1;
        if (outputIndex == 0)
            return 0;
        if (outputIndex + 1 == varyingCount_)
            return sourcePointCount_ - 1;
        return outputIndex - 1;
    }

  private:
    std::size_t sourcePointCount_;
    std::size_t controlVertexCount_;
    std::size_t varyingCount_;
    bool cubic_;
    bool addEdgeControlVertices_;
    bool valid_;
};

inline bool checkedAdd(std::size_t value, std::size_t& total) {
    if (value > std::numeric_limits<std::size_t>::max() - total)
        return false;
    total += value;
    return true;
}

} // namespace arnold
} // namespace dommy

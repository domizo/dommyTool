#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "DCheckedSize.h"

namespace dommy {
namespace geometry {

inline bool checkedAccumulatePolylineIndices(std::size_t pointCount,
                                             std::size_t& indexCount) {
    if (pointCount < 2) {
        return true;
    }

    std::size_t additionalIndices = 0;
    std::size_t updatedIndexCount = 0;
    if (!memory::checkedMultiply(pointCount - 1, std::size_t(2), additionalIndices) ||
        !memory::checkedAdd(indexCount, additionalIndices, updatedIndexCount)) {
        return false;
    }

    indexCount = updatedIndexCount;
    return true;
}

inline bool checkedTupleScalarCount(std::size_t elementCount,
                                    std::size_t componentCount,
                                    std::size_t& scalarCount) {
    return componentCount != 0 &&
           memory::checkedMultiply(elementCount, componentCount, scalarCount);
}

template <typename T>
inline bool checkedAssignTupleBuffer(std::vector<T>& buffer,
                                     std::size_t elementCount,
                                     std::size_t componentCount,
                                     const T& value = T()) {
    std::size_t scalarCount = 0;
    if (!checkedTupleScalarCount(elementCount, componentCount, scalarCount)) {
        return false;
    }
    buffer.assign(scalarCount, value);
    return true;
}

template <typename T>
inline bool checkedAppendBuffer(std::vector<T>& destination,
                                const std::vector<T>& source) {
    std::size_t combinedSize = 0;
    if (!memory::checkedAdd(destination.size(), source.size(), combinedSize) ||
        combinedSize > destination.max_size()) {
        return false;
    }
    destination.reserve(combinedSize);
    destination.insert(destination.end(), source.begin(), source.end());
    return true;
}

// Keeps direct access to a geometry's owned position array out of node code.
// Algorithms may edit the detached lists, then atomically publish each list
// through the geometry's validated replacePositions() boundary.
template <typename Geometry>
class StagedPositionEdits final {
public:
    typedef typename std::decay<
        decltype(std::declval<const Geometry&>().getPositionConst())>::type
        PointList;

    bool reserve(std::size_t count)
    {
        try {
            mGeometries.reserve(count);
            mPositions.reserve(count);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool add(Geometry& geometry)
    {
        try {
            mPositions.push_back(geometry.getPositionConst());
            try {
                mGeometries.push_back(&geometry);
            } catch (...) {
                mPositions.pop_back();
                throw;
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    std::size_t size() const
    {
        return mPositions.size();
    }

    PointList& positions(std::size_t index)
    {
        return mPositions.at(index);
    }

    const PointList& positions(std::size_t index) const
    {
        return mPositions.at(index);
    }

    bool mutablePositionPointers(std::vector<PointList*>& pointers)
    {
        try {
            pointers.clear();
            pointers.reserve(mPositions.size());
            for (typename std::vector<PointList>::iterator position =
                     mPositions.begin();
                 position != mPositions.end(); ++position) {
                pointers.push_back(&*position);
            }
            return true;
        } catch (...) {
            pointers.clear();
            return false;
        }
    }

    bool commit()
    {
        if (mGeometries.size() != mPositions.size()) {
            return false;
        }
        for (std::size_t index = 0U; index < mPositions.size(); ++index) {
            if (!mGeometries[index] ||
                !mGeometries[index]->replacePositions(mPositions[index])) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<Geometry*> mGeometries;
    std::vector<PointList> mPositions;
};

} // namespace geometry
} // namespace dommy

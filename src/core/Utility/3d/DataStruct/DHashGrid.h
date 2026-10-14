// Spatial hash grid used by the historical point relaxation nodes.
#ifndef DHashGrid_h
#define DHashGrid_h

#include <cstddef>

#include "DType.h"
#include "DUtility.h"
#include "common/utility/DBoundedRadiusSearch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

struct DHashSort {
    uint hash;
    uint value;
};

inline bool LessHash(const DHashSort& left, const DHashSort& right)
{
    return left.hash < right.hash;
}

inline bool DHashGridPositionIsFinite(const DTools::Vec3& position)
{
    return std::isfinite(position[0]) && std::isfinite(position[1]) &&
           std::isfinite(position[2]);
}

struct DNeigh {
    int Idx;
    DReal Dist;
    DReal Dist2;
};

class DHashGrid {
public:
    struct DCell {
        std::vector<uint> hSortedIndex;
        std::vector<uint> hGridParticleHash;
        std::vector<uint> hCellStart;
        std::vector<uint> hCellEnd;
        uint uNumCells;
    };

    explicit DHashGrid(int dim, std::size_t workLimit =
                          dommy::generation::kMaxWorkItemCount)
        : m_iDim(dim), m_uObjectCapacity(0U), m_indexedCount(0U),
          m_hasIndex(false), m_workBudget(workLimit)
    {
        Clear();
    }

    DHashGrid(const DHashGrid&) = delete;
    DHashGrid& operator=(const DHashGrid&) = delete;

    // False means invalid input or an unsupported grid, not an empty query.
    // Allocation/work-budget exceptions are contained by the owning compute.
    // One budget covers this entire object's lifetime, including every rebuild.
    bool Setup(DTools::Vec3 minimum, DTools::Vec3 maximum, DReal width, int count);
    // Let an owning node admit the exact grid capacities together with its
    // other staging storage, before any grid allocation. The legacy overload
    // keeps its existing count/cell/work limits and has no extra admission.
    template <typename StorageAdmission>
    bool Setup(DTools::Vec3 minimum, DTools::Vec3 maximum, DReal width, int count,
               StorageAdmission admitStorage);
    bool SetObjectToCell(const DReal* positions, uint count);

    // Success replaces results; failure preserves them without partial output.
    // GetNN uses an inclusive radius and retains self/coincident points.
    bool GetNN(DTools::Vec3 position, const DReal* positions, uint count,
               std::vector<DNeigh>& neighbors, DReal radius = -1.0);
    bool GetNN_Direct(DTools::Vec3 position, const DReal* positions, uint count,
                      std::vector<DNeigh>& neighbors, DReal radius = -1.0);
    uint CalGridHash(int x, int y, int z) const;
    uint CalGridHash(DTools::Vec3 position) const;

private:
    void Clear();
    void getNeighborsInCell(DTools::Vec3 position, const DReal* positions,
                            uint hash, std::vector<DNeigh>& neighbors,
                            DReal squaredRadius);

    DCell m_hCellData;
    int m_iGridSize[3];
    DReal m_fCellWidth[3];
    int m_iDim;
    DTools::Vec3 m_v3EnvMin;
    uint m_uObjectCapacity;
    uint m_indexedCount;
    bool m_hasIndex;
    dommy::spatial::RadiusSearchBudget m_workBudget;
};

inline void DHashGrid::Clear()
{
    m_hCellData.hSortedIndex.clear();
    m_hCellData.hGridParticleHash.clear();
    m_hCellData.hCellStart.clear();
    m_hCellData.hCellEnd.clear();
    m_hCellData.uNumCells = 0U;
    m_iGridSize[0] = m_iGridSize[1] = m_iGridSize[2] = 0;
    m_fCellWidth[0] = m_fCellWidth[1] = m_fCellWidth[2] = 0.0;
    m_uObjectCapacity = m_indexedCount = 0U;
    m_hasIndex = false;
}

inline bool DHashGrid::Setup(DTools::Vec3 minimum, DTools::Vec3 maximum,
                             DReal width, int count)
{
    return Setup(minimum, maximum, width, count,
                 [](std::size_t, std::size_t) { return true; });
}

template <typename StorageAdmission>
inline bool DHashGrid::Setup(DTools::Vec3 minimum, DTools::Vec3 maximum,
                             DReal width, int count, StorageAdmission admitStorage)
{
    Clear();
    std::size_t checkedCount = 0U, scalarCount = 0U;
    if (m_iDim < 3 || !std::isfinite(width) || width < D_EPS ||
        !DHashGridPositionIsFinite(minimum) ||
        !DHashGridPositionIsFinite(maximum) ||
        !dommy::generation::checkedCount(count,
            dommy::generation::kMaxGeneratedPointCount, checkedCount) ||
        !dommy::generation::checkedProduct(checkedCount,
            static_cast<std::size_t>(m_iDim),
            dommy::generation::kMaxWorkItemCount, scalarCount)) return false;

    const DTools::Vec3 extent = maximum - minimum;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(extent[axis]) || extent[axis] < 0.0) return false;
    }
    const DReal maxAxis = DFunc::Max3(extent);
    DReal cellWidth = width;
    if (maxAxis >= D_EPS) {
        const double ratio = static_cast<double>(maxAxis) / width;
        const int exponent = static_cast<int>(std::max(0.0, std::min(30.0,
            std::floor(std::log2(ratio) + 0.5))));
        cellWidth = maxAxis / (1U << exponent);
        if (!std::isfinite(cellWidth) || cellWidth < D_EPS) return false;
    }

    int dimensions[3] = {1, 1, 1};
    std::size_t cells = 1U;
    const std::size_t maxCells = std::size_t(1U) << 22;
    for (int axis = 0; axis < 3; ++axis) {
        if (extent[axis] >= D_EPS) {
            const double ratio = static_cast<double>(extent[axis]) / cellWidth;
            const int exponent = static_cast<int>(std::max(0.0, std::min(30.0,
                std::floor(std::log2(ratio) + 0.5))));
            dimensions[axis] = static_cast<int>(1U << exponent);
        }
        if (!dommy::generation::checkedProduct(cells,
                static_cast<std::size_t>(dimensions[axis]), maxCells, cells)) {
            return false;
        }
    }

    if (!admitStorage(checkedCount, cells)) return false;
    m_workBudget.consume(2U * checkedCount + 2U * cells);
    // Commit usable dimensions only after all storage has been allocated.
    DCell candidate{};
    candidate.hSortedIndex.assign(checkedCount, 0U);
    candidate.hGridParticleHash.assign(checkedCount, 0U);
    candidate.hCellStart.assign(cells, std::numeric_limits<uint>::max());
    candidate.hCellEnd.assign(cells, std::numeric_limits<uint>::max());
    m_hCellData.hSortedIndex.swap(candidate.hSortedIndex);
    m_hCellData.hGridParticleHash.swap(candidate.hGridParticleHash);
    m_hCellData.hCellStart.swap(candidate.hCellStart);
    m_hCellData.hCellEnd.swap(candidate.hCellEnd);
    m_hCellData.uNumCells = static_cast<uint>(cells);
    m_uObjectCapacity = static_cast<uint>(count);
    m_v3EnvMin = minimum;
    for (int axis = 0; axis < 3; ++axis) {
        m_iGridSize[axis] = dimensions[axis];
        m_fCellWidth[axis] = cellWidth;
    }
    return true;
}

inline bool DHashGrid::SetObjectToCell(const DReal* positions, uint count)
{
    m_hasIndex = false;
    m_indexedCount = 0U;
    if (m_hCellData.uNumCells == 0U || count > m_uObjectCapacity ||
        (count > 0U && !positions)) return false;

    std::size_t sortLevels = 1U;
    for (std::size_t n = count; n > 1U; n /= 2U) ++sortLevels;
    // Account for cell clearing, position checks, sorting, and index writes.
    m_workBudget.consume(2U * m_hCellData.uNumCells +
        static_cast<std::size_t>(count) * (2U * sortLevels + 6U));
    const uint empty = std::numeric_limits<uint>::max();
    std::fill(m_hCellData.hCellStart.begin(), m_hCellData.hCellStart.end(), empty);
    std::fill(m_hCellData.hCellEnd.begin(), m_hCellData.hCellEnd.end(), empty);
    std::vector<DHashSort> sorted(count);
    for (uint i = 0U; i < count; ++i) {
        const std::size_t offset = static_cast<std::size_t>(m_iDim) * i;
        const DTools::Vec3 point(positions[offset], positions[offset + 1U],
                                positions[offset + 2U]);
        if (!DHashGridPositionIsFinite(point)) return false;
        sorted[i].hash = CalGridHash(point);
        sorted[i].value = i;
    }
    std::sort(sorted.begin(), sorted.end(), LessHash);
    for (uint i = 0U; i < count; ++i) {
        const uint hash = sorted[i].hash;
        m_hCellData.hSortedIndex[i] = sorted[i].value;
        m_hCellData.hGridParticleHash[i] = hash;
        if (i == 0U || hash != sorted[i - 1U].hash) {
            m_hCellData.hCellStart[hash] = i;
            if (i > 0U) m_hCellData.hCellEnd[sorted[i - 1U].hash] = i;
        }
        if (i + 1U == count) m_hCellData.hCellEnd[hash] = count;
    }
    m_indexedCount = count;
    m_hasIndex = true;
    return true;
}

inline bool DHashGrid::GetNN_Direct(DTools::Vec3 position, const DReal* positions,
                                   uint count, std::vector<DNeigh>& neighbors,
                                   DReal radius)
{
    std::size_t scalarCount = 0U;
    if (m_iDim < 3 || (count > 0U && !positions) ||
        count > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::generation::checkedProduct(count,
            static_cast<std::size_t>(m_iDim),
            dommy::generation::kMaxWorkItemCount, scalarCount) ||
        !DHashGridPositionIsFinite(position) || !std::isfinite(radius) ||
        radius < 0.0 || radius > std::sqrt(std::numeric_limits<DReal>::max())) {
        return false;
    }
    m_workBudget.consume(1U + 2U * static_cast<std::size_t>(count));
    const DReal squaredRadius = radius * radius;
    std::vector<DNeigh> result;
    for (uint i = 0U; i < count; ++i) {
        const std::size_t offset = static_cast<std::size_t>(m_iDim) * i;
        const DTools::Vec3 point(positions[offset], positions[offset + 1U],
                                positions[offset + 2U]);
        if (!DHashGridPositionIsFinite(point)) return false;
        DNeigh neighbor;
        neighbor.Dist2 = norm2(position - point);
        // Retain the direct-search API's historical zero-distance exclusion.
        if (std::isfinite(neighbor.Dist2) && neighbor.Dist2 <= squaredRadius &&
            neighbor.Dist2 > D_FEQ_EPS) {
            neighbor.Idx = static_cast<int>(i);
            neighbor.Dist = std::sqrt(neighbor.Dist2);
            result.push_back(neighbor);
        }
    }
    neighbors.swap(result);
    return true;
}

inline bool DHashGrid::GetNN(DTools::Vec3 position, const DReal* positions,
                            uint count, std::vector<DNeigh>& neighbors,
                            DReal radius)
{
    if (!m_hasIndex || count != m_indexedCount ||
        (count > 0U && !positions) || !DHashGridPositionIsFinite(position)) {
        return false;
    }
    if (!std::isfinite(radius)) return false;
    if (radius < 0.0) radius = m_fCellWidth[0];
    if (radius > std::sqrt(std::numeric_limits<DReal>::max())) return false;
    m_workBudget.consume(1U);
    std::vector<DNeigh> result;
    if (count == 0U) {
        neighbors.swap(result);
        return true;
    }

    int center[3];
    for (int axis = 0; axis < 3; ++axis) {
        const double coordinate = (static_cast<double>(position[axis]) -
            m_v3EnvMin[axis]) / m_fCellWidth[axis];
        center[axis] = static_cast<int>(std::max(0.0, std::min(
            static_cast<double>(m_iGridSize[axis] - 1), coordinate)));
    }
    const int maxDimension = std::max(m_iGridSize[0],
        std::max(m_iGridSize[1], m_iGridSize[2]));
    const double radiusInCells = std::ceil(
        static_cast<double>(radius) / m_fCellWidth[0]) + 1.0;
    const int extent = static_cast<int>(std::min(
        static_cast<double>(maxDimension), radiusInCells));
    int minimum[3], maximum[3];
    for (int axis = 0; axis < 3; ++axis) {
        minimum[axis] = std::max(0, center[axis] - extent);
        maximum[axis] = std::min(m_iGridSize[axis] - 1, center[axis] + extent);
    }
    for (int z = minimum[2]; z <= maximum[2]; ++z) {
        for (int y = minimum[1]; y <= maximum[1]; ++y) {
            for (int x = minimum[0]; x <= maximum[0]; ++x) {
                m_workBudget.consume(1U); // Includes empty-cell traversal.
                getNeighborsInCell(position, positions, CalGridHash(x, y, z),
                                   result, radius * radius);
            }
        }
    }
    neighbors.swap(result);
    return true;
}

inline void DHashGrid::getNeighborsInCell(DTools::Vec3 position,
    const DReal* positions, uint hash, std::vector<DNeigh>& neighbors,
    DReal squaredRadius)
{
    const uint begin = m_hCellData.hCellStart[hash];
    if (begin == std::numeric_limits<uint>::max()) return;
    const uint end = m_hCellData.hCellEnd[hash];
    for (uint i = begin; i < end; ++i) {
        m_workBudget.consume(2U); // Distance test plus caller's neighbor work.
        const uint index = m_hCellData.hSortedIndex[i];
        const std::size_t offset = static_cast<std::size_t>(m_iDim) * index;
        const DTools::Vec3 point(positions[offset], positions[offset + 1U],
                                positions[offset + 2U]);
        if (!DHashGridPositionIsFinite(point)) {
            throw std::invalid_argument("Hash-grid positions changed to non-finite values");
        }
        DNeigh neighbor;
        neighbor.Dist2 = norm2(position - point);
        if (std::isfinite(neighbor.Dist2) && neighbor.Dist2 <= squaredRadius) {
            neighbor.Idx = static_cast<int>(index);
            neighbor.Dist = std::sqrt(neighbor.Dist2);
            neighbors.push_back(neighbor);
        }
    }
}

inline uint DHashGrid::CalGridHash(int x, int y, int z) const
{
    if (m_hCellData.uNumCells == 0U) return 0U;
    x = std::max(0, std::min(m_iGridSize[0] - 1, x));
    y = std::max(0, std::min(m_iGridSize[1] - 1, y));
    z = std::max(0, std::min(m_iGridSize[2] - 1, z));
    return static_cast<uint>(z * m_iGridSize[1] * m_iGridSize[0] +
                             y * m_iGridSize[0] + x);
}

inline uint DHashGrid::CalGridHash(DTools::Vec3 position) const
{
    if (m_hCellData.uNumCells == 0U || !DHashGridPositionIsFinite(position)) return 0U;
    int cell[3];
    for (int axis = 0; axis < 3; ++axis) {
        const double coordinate = (static_cast<double>(position[axis]) -
            m_v3EnvMin[axis]) / m_fCellWidth[axis];
        cell[axis] = static_cast<int>(std::max(0.0, std::min(
            static_cast<double>(m_iGridSize[axis] - 1), coordinate)));
    }
    return CalGridHash(cell[0], cell[1], cell[2]);
}

#endif // DHashGrid_h

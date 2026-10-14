#pragma once

// Explicit OpenVDB metadata/transport seams. The formatter and pre-publication
// snippets are generated from production; no native grid conversion runs here.
#include <array>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include <maya/MString.h>

namespace vdb_info_fixture {
namespace openvdb {
using Coord = std::array<int, 3>;
struct GridBase {
    std::string name;
    const std::string& getName() const { return name; }
    const char* type() const { return "Tree_float_fixture"; }
    const char* valueType() const { return "float"; }
    std::size_t activeVoxelCount() const { return 1234U; }
    std::size_t memUsage() const { return 4096U; }
    std::array<double, 3> voxelSize() const { return {{1.0, 1.0, 1.0}}; }
    Coord evalActiveVoxelDim() const { return {{3, 4, 5}}; }
};
}
using GridCPtrVec = std::vector<std::shared_ptr<const openvdb::GridBase>>;
struct DOpenVDBData {
    GridCPtrVec grids;
    std::size_t numberOfGrids() const { return grids.size(); }
};
inline void getGrids(GridCPtrVec& result, const DOpenVDBData& data, const std::string& selection)
{
    (void)selection;
    result = data.grids;
}
struct Count {
    std::size_t count;
    std::size_t size() const { return count; }
};
struct Timer { double elapsedTime() const { return 1.25; } };
inline void printGridInfo(std::ostream& text, const DOpenVDBData& data)
{
    for (const auto& grid : data.grids) if (grid) text << "grid:" << grid->getName() << '\n';
}
#include "vdb_info_methods.inc"
} // namespace vdb_info_fixture

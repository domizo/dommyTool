#pragma once
#include "field_api_doubles.h"
#include "common/utility/DNumericValidation.h"
#include <cstdint>
#include <stdexcept>

// OpenVDB transform/tree/sampler and plug-in payload are explicit seams. These
// tests execute admission, cardinality, host statuses and publication, not VDB
// interpolation, native allocation, Maya ownership or dependency evaluation.
double indexScale = 1.0;
MVector indexOffset;
MVector sampledValue(1, 2, 3);
int samples = 0;
bool throwTransform = false, throwSampler = false, activeSamples = true;
bool validPayload = true, gridsExist = true;
std::string selectedNames;
namespace openvdb {
struct Vec3d {
    double values[3];
    explicit Vec3d(double value = 0) : values{value, value, value} {}
    Vec3d(double x, double y, double z) : values{x, y, z} {}
    double operator[](unsigned int axis) const { return values[axis]; }
};
namespace math {
struct Transform {
    Vec3d worldToIndex(Vec3d point) const {
        if (throwTransform) throw std::runtime_error("transform failure");
        return Vec3d(point[0] * indexScale + indexOffset.x,
                     point[1] * indexScale + indexOffset.y,
                     point[2] * indexScale + indexOffset.z);
    }
};
}
struct Tree {};
struct GridBase {
    const math::Transform& transform() const { static math::Transform value; return value; }
};
struct Vec3SGrid : GridBase {
    using ValueType = Vec3d;
    using TreeType = Tree;
    const Tree& tree() const { static Tree value; return value; }
};
struct Vec3DGrid : Vec3SGrid {};
namespace tools {
struct BoxSampler {
    template <typename TreeT>
    static bool sample(const TreeT&, const Vec3d& point, Vec3d& value) {
        ++samples;
        for (unsigned int axis = 0; axis < 3; ++axis)
            assert(std::isfinite(point[axis]) && point[axis] >= -2147483648.0 &&
                   point[axis] < 2147483647.0);
        if (throwSampler) throw std::runtime_error("sampler failure");
        value = Vec3d(sampledValue.x, sampledValue.y, sampledValue.z);
        return activeSamples;
    }
};
}
}
struct DOpenVDBData {
    static const int id = 1003;
    bool hasValidData() const { return validPayload; }
};
namespace dommy { namespace maya {
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DOpenVDBData*& result) {
    handle.check();
    const MStatus status = operation("vdb.payload");
    result = nullptr;
    if (status == MS::kSuccess) { static DOpenVDBData data; result = &data; }
    return status;
}
} }
enum class DVdbGridType { kVec3f = 6, kVec3d = 7, kOther = 100 };
DVdbGridType gridType = DVdbGridType::kVec3f;
DVdbGridType getVdbGridType(const openvdb::GridBase&) { return gridType; }
using GridCPtrVec = std::vector<const openvdb::GridBase*>;
void getGrids(GridCPtrVec& grids, const DOpenVDBData&, const std::string& names) {
    selectedNames = names;
    if (!gridsExist) return;
    static openvdb::Vec3SGrid floats;
    static openvdb::Vec3DGrid doubles;
    grids.push_back(gridType == DVdbGridType::kVec3d ?
                    static_cast<const openvdb::GridBase*>(&doubles) : &floats);
}
struct DOpenVDBField {
    enum { aVdbInput = 1001, aVdbSelectedGridNames = 1002 };
    MStatus compute(const MPlug&, MDataBlock&);
    MStatus applyNoMaxDist(MDataBlock&, const MVectorArray&, const MVectorArray&,
                           const MDoubleArray&, MVectorArray&);
};
#define DOMMY_VDB_COMPUTE_CATCH(name) catch (...) { return MS::kFailure; }

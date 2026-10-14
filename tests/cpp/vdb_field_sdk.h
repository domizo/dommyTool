#pragma once
// Real Maya declarations and helpers; OpenVDB grid/tree APIs are declarations
// only. This checks initializer/compute boundaries, not a native VDB build.
#include "modules/volume/VDB/Dynamics/DOpenVDBField.h"
#include <maya/MArrayDataHandle.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaFieldData.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include <cstdint>
#include <limits>
#include <string>
#include <memory>
#include <vector>

class DOpenVDBData : public MPxData {
public:
    static const MTypeId id;
    bool hasValidData() const;
};
namespace openvdb {
struct Vec3d {
    explicit Vec3d(double);
    Vec3d(double, double, double);
    double operator[](unsigned int) const;
};
namespace math { struct Transform { Vec3d worldToIndex(const Vec3d&) const; }; }
struct Tree {};
struct GridBase {
    using ConstPtr = std::shared_ptr<const GridBase>;
    const math::Transform& transform() const;
};
struct Vec3SGrid : GridBase {
    using ValueType = Vec3d;
    using TreeType = Tree;
    const Tree& tree() const;
};
struct Vec3DGrid : Vec3SGrid {};
namespace tools {
struct BoxSampler {
    template <typename TreeT> static bool sample(const TreeT&, const Vec3d&, Vec3d&);
};
}
}
enum class DVdbGridType { kVec3f = 6, kVec3d = 7 };
using GridCPtrVec = std::vector<openvdb::GridBase::ConstPtr>;
DVdbGridType getVdbGridType(const openvdb::GridBase&);
void getGrids(GridCPtrVec&, const DOpenVDBData&, const std::string&);
#define McheckErr(status, message) DOMMY_RETURN_IF_FAILURE(status)
#define DOMMY_VDB_COMPUTE_CATCH(name) DOMMY_COMPUTE_CATCH(name)

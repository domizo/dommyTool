#pragma once

// Explicit boundaries, not an OpenVDB sparse-tree/kernel implementation.
// Actual production compute/apply functions are used; kernels are recording doubles.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>

static int publications, gridCopies, invalidReads;
static bool nullCopy, invalidNativeOutput, wrongCopy;
static int fillCalls;
static bool meshConnected = true, meshValid = true;
static bool nullOutput = false, invalidOutputPayload = false;
static int lateFillFailure = -1;

inline void nativeCall(const char* name) {
    if (!operation(name))
        throw std::runtime_error("injected native boundary failure");
}
namespace boost {
struct bad_lexical_cast : std::exception {};
template <typename T> T lexical_cast(const std::string& text) {
    std::istringstream stream(text);
    T value;
    if (!(stream >> value) || stream.peek() != std::char_traits<char>::eof())
        throw bad_lexical_cast();
    return value;
}
inline std::string is_any_of(const char* value) {
    return value;
}
inline void split(std::vector<std::string>& out, const std::string& value,
                  const std::string& delimiters) {
    out.clear();
    std::size_t begin = 0U;
    for (;;) {
        const auto end = value.find_first_of(delimiters, begin);
        out.push_back(value.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1U;
    }
}
} // namespace boost

namespace openvdb {
namespace tools {}
namespace math {
template <typename T> struct Vec3 {
    using ValueType = T;
    T v[3];
    explicit Vec3(T x = T()) : v{x, x, x} {}
    Vec3(T x, T y, T z) : v{x, y, z} {}
    T operator[](int i) const {
        return v[i];
    }
    T& operator[](int i) {
        return v[i];
    }
    bool operator==(const Vec3& b) const {
        return v[0] == b[0] && v[1] == b[1] && v[2] == b[2];
    }
};
} // namespace math
using Vec3R = math::Vec3<double>;
using Int32 = std::int32_t;
struct Coord {
    int value[3];
    Coord(int x = 0, int y = 0, int z = 0) : value{x, y, z} {}
    int operator[](int axis) const {
        return value[axis];
    }
};
struct CoordBBox {
    Coord low, high;
    void reset(Coord a, Coord b) {
        low = a;
        high = b;
    }
    Coord min() const {
        return low;
    }
    Coord max() const {
        return high;
    }
};
namespace math {

struct Transform {
    bool linear = true;
    double inverse[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3R inverseShift;
    bool isLinear() const {
        return linear;
    }
    Vec3R worldToIndex(const Vec3R& p) const {
        nativeCall("map.worldToIndex");
        Vec3R result = inverseShift;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                result[row] += inverse[row][col] * p[col];
        return result;
    }
};
// Recording transform boundary; mirrors the eight-corner AABB contract only.
inline void calculateBounds(const Transform& transform, const Vec3R& low, const Vec3R& high,
                            Vec3R& resultLow, Vec3R& resultHigh) {
    resultLow = Vec3R(std::numeric_limits<double>::infinity());
    resultHigh = Vec3R(-std::numeric_limits<double>::infinity());
    for (int corner = 0; corner < 8; ++corner) {
        Vec3R point;
        for (int axis = 0; axis < 3; ++axis)
            point[axis] = (corner & (1 << axis)) ? high[axis] : low[axis];
        point = transform.worldToIndex(point);
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(point[axis])) {
                resultLow = resultHigh = point;
                return;
            }
            resultLow[axis] = std::min(resultLow[axis], point[axis]);
            resultHigh[axis] = std::max(resultHigh[axis], point[axis]);
        }
    }
}
} // namespace math
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() = default;
    std::string name = "source";
    math::Transform xform;
    Vec3R dx{1, 1, 1};
    std::size_t memory = 1024;
    std::size_t memUsage() const {
        nativeCall("grid.memory");
        return memory;
    }

    template <typename T> bool isType() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    virtual Ptr deepCopyGrid() const {
        nativeCall("grid.copy");
        ++gridCopies;
        return nullCopy ? nullptr : std::make_shared<GridBase>(*this);
    }
    const math::Transform& transform() const {
        return xform;
    }
    const std::string& getName() const {
        return name;
    }
    Vec3R voxelSize() const {
        return dx;
    }
};
// Dimensions/entry counts of OpenVDB 5.2 Tree4<T, 5, 4, 3>.
// Deliberately oversized object footprints keep this test conservative;
// these are NOT native layouts or allocators.
template <typename T> struct FillLeaf {
    using ValueType = T;
    enum { DIM = 8, NUM_VALUES = 512, LEVEL = 0 };
    char footprint[256];
};
template <typename T> struct FillLower {
    using ValueType = T;
    using ChildNodeType = FillLeaf<T>;
    enum { DIM = 128, NUM_VALUES = 4096, LEVEL = 1 };
    char footprint[65536];
};
template <typename T> struct FillUpper {
    using ValueType = T;
    using ChildNodeType = FillLower<T>;
    enum { DIM = 4096, NUM_VALUES = 32768, LEVEL = 2 };
    char footprint[524288];
};
template <typename T> struct FillTree {
    struct RootNodeType {
        using ChildNodeType = FillUpper<T>;
    };
};
template <typename T> struct InvalidFillValue {
    static T get() {
        return std::numeric_limits<T>::infinity();
    }
};
template <typename T> struct InvalidFillValue<math::Vec3<T>> {
    static math::Vec3<T> get() {
        return math::Vec3<T>(std::numeric_limits<T>::infinity());
    }
};
template <typename T> struct ScalarGrid : GridBase {
    using ValueType = T;
    using TreeType = FillTree<T>;
    bool filled = false, wasSparse = false, wasActive = false;
    T fillValue = T();
    CoordBBox fillBox;
    void recordFill(const CoordBBox& box, const T& value, bool active, bool sparse) {
        nativeCall("grid.fill");
        if (fillCalls++ == lateFillFailure)
            throw std::runtime_error("late fill failure");
        filled = true;
        wasSparse = sparse;
        wasActive = active;
        fillValue = value;
        fillBox = box;
        values.push_back(invalidNativeOutput ? InvalidFillValue<T>::get() : value);
    }
    void sparseFill(const CoordBBox& box, const T& value, bool active) {
        recordFill(box, value, active, true);
    }
    void denseFill(const CoordBBox& box, const T& value, bool active) {
        recordFill(box, value, active, false);
    }
    using Ptr = std::shared_ptr<ScalarGrid>;
    using ConstPtr = std::shared_ptr<const ScalarGrid>;
    T bg = T(3);
    std::vector<T> values{T(1)};
    T background() const {
        return bg;
    }
    GridBase::Ptr deepCopyGrid() const override {
        nativeCall("grid.copy");
        ++gridCopies;
        if (wrongCopy)
            return std::make_shared<GridBase>();
        return nullCopy ? nullptr : std::make_shared<ScalarGrid>(*this);
    }
    struct Iterator {
        const ScalarGrid* grid;
        std::size_t i;
        explicit operator bool() const {
            return i < grid->values.size();
        }
        Iterator& operator++() {
            ++i;
            return *this;
        }
        T operator*() const {
            return grid->values.at(i);
        }
    };
    Iterator cbeginValueAll() const {
        return {this, 0};
    }
};
using FloatGrid = ScalarGrid<float>;
using DoubleGrid = ScalarGrid<double>;
using BoolGrid = ScalarGrid<bool>;
using Int32Grid = ScalarGrid<int>;
using Int64Grid = ScalarGrid<long long>;
using Vec3SGrid = ScalarGrid<math::Vec3<float>>;
using Vec3DGrid = ScalarGrid<math::Vec3<double>>;
using Vec3IGrid = ScalarGrid<math::Vec3<int>>;
using Vec3fGrid = Vec3SGrid;
using Vec3dGrid = Vec3DGrid;
namespace points {
struct PointDataGrid : GridBase {};
struct PointIndexGrid : GridBase {};
} // namespace points
} // namespace openvdb
using Grid = openvdb::GridBase;
using GridPtr = Grid::Ptr;
using GridCPtr = Grid::ConstPtr;
using GridCRef = const Grid&;
using GridCPtrVec = std::vector<GridCPtr>;

struct MPxData {
    virtual ~MPxData() = default;
};
using MTypeId = int;
struct DOpenVDBData : MPxData {
    static const MTypeId id = 123;
    GridCPtrVec grids;
    bool valid = true;
    bool hasValidData() const {
        return valid && grids.size() <= dommy::vdb::kMaxVdbGridCount &&
               std::all_of(grids.begin(), grids.end(), [](const GridCPtr& p) { return bool(p); });
    }
    std::size_t numberOfGrids() const {
        return grids.size();
    }
    GridCRef grid(std::size_t i) const {
        return *grids.at(i);
    }
    GridCPtr gridPtr(std::size_t i) const {
        return grids.at(i);
    }
    void insert(GridCPtr grid) {
        nativeCall("data.insert");
        grids.push_back(grid);
    }
    void insert(GridCRef grid) {
        nativeCall("data.insert");
        // Non-owning view of test input lifetime; no native shallow-copy claim.
        grids.emplace_back(&grid, [](const Grid*) {});
    }
};
const MTypeId DOpenVDBData::id;
static DOpenVDBData inputData, stagedData, outputData;
using float3 = float[3];
using int3 = int[3];
struct InputValue {
    float3 floats = {0, 0, 0};
    int3 ints = {0, 0, 0};

    int integer = 0;
    std::string text;
    MPxData* payload = nullptr;
    MTypeId type = DOpenVDBData::id;
};
struct MDataHandle {
    InputValue value;
    bool valid;
    void check() const {
        if (!valid) {
            ++invalidReads;
            throw std::runtime_error("invalid handle read");
        }
    }
    const float3& asFloat3() const {
        check();
        return value.floats;
    }
    const int3& asInt3() const {
        check();
        return value.ints;
    }
    short asShort() const {
        check();
        return static_cast<short>(value.integer);
    }
    bool asBool() const {
        check();
        return value.integer != 0;
    }
    MString asString() const {
        check();
        return MString(value.text.c_str());
    }
    MPxData* asPluginData() const {
        check();
        return value.payload;
    }
    MTypeId typeId() const {
        check();
        return value.type;
    }
};
struct MPlug {
    MObject object;
    explicit MPlug(MObject value) : object(value) {}
    MPlug(const MObject&, MObject value) : object(value) {}
    bool isConnected(MStatus* status) const {
        *status = operation("mesh.connected");
        return meshConnected;
    }

    bool operator!=(const MObject& value) const {
        return object.name != value.name;
    }
};
struct MDataBlock {
    std::map<std::string, InputValue> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        return {inputs.at(attribute.name), bool(*status)};
    }
};
struct MFnPluginData {
    MObject object() const {
        return {"staged"};
    }
};
inline MStatus createOpenVDBOutputData(MFnPluginData&, DOpenVDBData*& output) {
    const MStatus status = operation("output.create");
    output = (status && !nullOutput) ? &stagedData : nullptr;
    stagedData.valid = !invalidOutputPayload;
    return status;
}
inline MStatus publishOpenVDBOutput(MDataBlock&, const MObject&, const MPlug&, const MObject&) {
    DOMMY_RETURN_IF_FAILURE(operation("output.handle"));
    DOMMY_RETURN_IF_FAILURE(operation("output.set"));
    outputData = stagedData;
    ++publications;
    return operation("output.clean");
}

struct MPoint {
    double x = 0, y = 0, z = 0;
};
struct MBoundingBox {
    MPoint low, high;
    MPoint min() const {
        return low;
    }
    MPoint max() const {
        return high;
    }
};
static MBoundingBox meshBounds;
// The shared mesh reader is tested separately. This is its checked boundary.
inline MStatus getMeshInformation(const MObject&, MDataBlock&, MBoundingBox& bounds, MObject&) {
    DOMMY_RETURN_IF_FAILURE(operation("mesh.bounds"));
    if (!meshValid)
        return MS::kFailure;
    bounds = meshBounds;
    return MS::kSuccess;
}

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
static bool nullCopy, invalidNativeOutput;
static std::vector<std::string> filterCalls;
static const void* lastMask;
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
enum GridClass { GRID_UNKNOWN, GRID_LEVEL_SET };
struct Vec3R {
    double value[3];
    Vec3R(double x = 0, double y = 0, double z = 0) : value{x, y, z} {}
    double operator[](int axis) const {
        return value[axis];
    }
    double& operator[](int axis) {
        return value[axis];
    }
};
struct Coord {
    int value[3];
    Coord(int x = 0, int y = 0, int z = 0) : value{x, y, z} {}
    int operator[](int axis) const {
        return value[axis];
    }
    Vec3R asVec3d() const {
        return {double(value[0]), double(value[1]), double(value[2])};
    }
};
struct CoordBBox {
    Coord low, high;
    bool vacant = false;
    bool empty() const {
        return vacant;
    }
    Coord min() const {
        return low;
    }
    Coord max() const {
        return high;
    }
};
namespace math {
enum { TVD_RK1, TVD_RK3, FIRST_BIAS, SECOND_BIAS, THIRD_BIAS, WENO5_BIAS, HJWENO5_BIAS };
struct Transform {
    bool linear = true;
    Vec3R shift, inverseShift;
    bool isLinear() const {
        return linear;
    }
    Vec3R indexToWorld(const Vec3R& p) const {
        nativeCall("map.indexToWorld");
        return {p[0] + shift[0], p[1] + shift[1], p[2] + shift[2]};
    }
    Vec3R worldToIndex(const Vec3R& p) const {
        nativeCall("map.worldToIndex");
        return {p[0] + inverseShift[0], p[1] + inverseShift[1], p[2] + inverseShift[2]};
    }
};
} // namespace math
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() = default;
    std::string name = "source";
    GridClass kind = GRID_LEVEL_SET;
    math::Transform xform;
    CoordBBox bounds;
    Vec3R dx{1, 1, 1};
    bool uniform = true;
    std::uint64_t activeCount = 1;
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
    GridClass getGridClass() const {
        return kind;
    }
    bool hasUniformVoxels() const {
        return uniform;
    }
    Vec3R voxelSize() const {
        return dx;
    }
    CoordBBox evalActiveVoxelBoundingBox() const {
        return bounds;
    }
    std::uint64_t activeVoxelCount() const {
        nativeCall("grid.count");
        return activeCount;
    }
};
template <typename T> struct ScalarGrid : GridBase {
    using ValueType = T;
    T bg = T(3);
    std::vector<T> values{T(1)};
    T background() const {
        return bg;
    }
    Ptr deepCopyGrid() const override {
        nativeCall("grid.copy");
        ++gridCopies;
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
namespace tools {
template <typename GridT> struct Filter {
    GridT& grid;
    explicit Filter(GridT& g) : grid(g) {
        nativeCall("filter.create");
    }
    void call(const std::string& op) {
        nativeCall("filter.native");
        filterCalls.push_back(op);
        if (invalidNativeOutput)
            grid.values[0] = std::numeric_limits<typename GridT::ValueType>::infinity();
    }
    void mean(int width, int count) {
        call("mean:" + text(width) + ":" + text(count));
    }
    void gaussian(int width, int count) {
        call("gaussian:" + text(width) + ":" + text(count));
    }
    void median(int width, int count) {
        call("median:" + text(width) + ":" + text(count));
    }
    void offset(typename GridT::ValueType value) {
        call("offset:" + text(value));
        grid.values[0] += value;
    }
};
template <typename GridT, typename MaskT> struct LevelSetFilter : Filter<GridT> {
    using Filter<GridT>::call;
    explicit LevelSetFilter(GridT& g) : Filter<GridT>(g) {}
    void setTemporalScheme(int value) {
        call("temporal:" + text(value));
    }
    void setSpatialScheme(int value) {
        call("spatial:" + text(value));
    }
    void setMaskRange(float low, float high) {
        assert(low < high);
        call("mask:" + text(low) + ":" + text(high));
    }
    void invertMask(bool value) {
        call("invert:" + text(value));
    }
    void setNormCount(int value) {
        call("norm:" + text(value));
    }
    void normalize() {
        call("normalize");
    }
    void prune() {
        call("prune");
    }
    bool resize(int width) {
        call("resize:" + text(width));
        return false;
    }
    void track() {
        call("track");
    }
    void offset(float value, const MaskT* mask) {
        lastMask = mask;
        call("offset:" + text(value));
    }
    void mean(int width, const MaskT* mask) {
        lastMask = mask;
        call("mean:" + text(width));
    }
    void gaussian(int width, const MaskT* mask) {
        lastMask = mask;
        call("gaussian:" + text(width));
    }
    void median(int width, const MaskT* mask) {
        lastMask = mask;
        call("median:" + text(width));
    }
    void meanCurvature(const MaskT* mask) {
        lastMask = mask;
        call("curvature");
    }
    void laplacian(const MaskT* mask) {
        lastMask = mask;
        call("laplacian");
    }
};
} // namespace tools
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
static DOpenVDBData inputData, maskData, stagedData, outputData;
struct InputValue {
    float number = 0.0f;
    int integer = 0;
    std::string text;
    MPxData* payload = nullptr;
    MTypeId type = DOpenVDBData::id;
    bool nonnullObject = false;
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
    float asFloat() const {
        check();
        return value.number;
    }
    bool asBool() const {
        check();
        return value.integer != 0;
    }
    int asInt() const {
        check();
        return value.integer;
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
    MObject data() {
        check();
        return value.nonnullObject ? MObject{"data"} : MObject::kNullObj;
    }
};
struct MPlug {
    MObject object;
    explicit MPlug(MObject value) : object(value) {}
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
    output = status ? &stagedData : nullptr;
    return status;
}
inline MStatus publishOpenVDBOutput(MDataBlock&, const MObject&, const MPlug&, const MObject&) {
    DOMMY_RETURN_IF_FAILURE(operation("output.handle"));
    DOMMY_RETURN_IF_FAILURE(operation("output.set"));
    outputData = stagedData;
    ++publications;
    return operation("output.clean");
}

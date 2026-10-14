#pragma once
#include "math/DBounds.h"

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
static std::map<std::string, bool> socketConnections;
static bool nullOutput = false, invalidOutputPayload = false;
static bool createEmptyGrid = false;
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
enum GridClass { GRID_UNKNOWN, GRID_LEVEL_SET, GRID_FOG_VOLUME };
enum VecType { VEC_INVARIANT, VEC_COVARIANT, VEC_COVARIANT_NORMALIZE };
namespace math {
template <typename T> struct Vec3 {
    using ValueType = T;
    using value_type = T;
    T v[3];
    explicit Vec3(T x = T()) : v{x, x, x} {}
    Vec3(T x, T y, T z) : v{x, y, z} {}
    T operator[](int i) const {
        return v[i];
    }
    T& operator[](int i) {
        return v[i];
    }
    T x() const { return v[0]; }
    T y() const { return v[1]; }
    T z() const { return v[2]; }
    bool operator==(const Vec3& b) const {
        return v[0] == b[0] && v[1] == b[1] && v[2] == b[2];
    }
};
} // namespace math
using Vec3R = math::Vec3<double>;
using Int32 = std::int32_t;
using Vec3d = Vec3R;
struct BBoxd {
    Vec3R low{-1}, high{1};
    BBoxd() = default;
    BBoxd(Vec3R a, Vec3R b) : low(a), high(b) {}
    Vec3R& min() {
        return low;
    }
    const Vec3R& min() const {
        return low;
    }
    Vec3R& max() {
        return high;
    }
    const Vec3R& max() const {
        return high;
    }
    Vec3R extents() const {
        return {high[0] - low[0], high[1] - low[1], high[2] - low[2]};
    }
    void expand(double d) {
        for (int i = 0; i < 3; ++i) {
            low[i] -= d;
            high[i] += d;
        }
    }
    void expand(const BBoxd& b) {
        for (int i = 0; i < 3; ++i) {
            low[i] = std::min(low[i], b.low[i]);
            high[i] = std::max(high[i], b.high[i]);
        }
    }
};

struct Coord {
    int value[3];
    Coord(int x = 0, int y = 0, int z = 0) : value{x, y, z} {}
    int operator[](int axis) const {
        return value[axis];
    }
    bool operator!=(const Coord& other) const {
        return value[0] != other.value[0] || value[1] != other.value[1] || value[2] != other.value[2];
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
    using Ptr = std::shared_ptr<Transform>;
    static Ptr createLinearTransform(double scale = 1) {
        nativeCall("map.create");
        auto result = std::make_shared<Transform>();
        result->preScale(Vec3R(scale));
        return result;
    }
    void preScale(const Vec3R& scale) {
        nativeCall("map.scale");
        uniform = scale[0] == scale[1] && scale[1] == scale[2];
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
            forward[row][col] *= scale[col];
            inverse[row][col] /= scale[row];
        }
        for (int row = 0; row < 3; ++row) inverseShift[row] /= scale[row];
    }
    template<typename Axis> void preRotate(double, Axis) { nativeCall("map.rotate"); }
    template<typename Vector> void postTranslate(const Vector& offset) {
        nativeCall("map.translate");
        for (int row = 0; row < 3; ++row) shift[row] += offset[row];
        for (int row = 0; row < 3; ++row) {
            inverseShift[row] = 0;
            for (int col = 0; col < 3; ++col) inverseShift[row] -= inverse[row][col] * shift[col];
        }
    }
    template<typename Matrix> void preMult(const Matrix& matrix) {
        nativeCall("map.preMult");
        // The resample fixture uses scale/translation matrices only. Rotation
        // decomposition and native affine resampling are separate host gates.
        for (int row = 0; row < 3; ++row) {
            shift[row] += forward[row][row] * matrix.translation[row];
            forward[row][row] *= matrix.scale[row];
            inverse[row][row] /= matrix.scale[row];
            inverseShift[row] = -shift[row] * inverse[row][row];
        }
        uniform = matrix.scale[0] == matrix.scale[1] && matrix.scale[1] == matrix.scale[2];
    }
    bool uniform = true;
    bool hasUniformScale() const { return uniform; }
    Vec3R voxelSize() const {
        return {std::hypot(std::hypot(forward[0][0], forward[1][0]), forward[2][0]),
                std::hypot(std::hypot(forward[0][1], forward[1][1]), forward[2][1]),
                std::hypot(std::hypot(forward[0][2], forward[1][2]), forward[2][2])};
    }
    double voxelVolume() const {
        return std::fabs(forward[0][0] * (forward[1][1]*forward[2][2]-forward[1][2]*forward[2][1]) -
            forward[0][1] * (forward[1][0]*forward[2][2]-forward[1][2]*forward[2][0]) +
            forward[0][2] * (forward[1][0]*forward[2][1]-forward[1][1]*forward[2][0]));
    }
    bool operator!=(const Transform& other) const {
        if (linear != other.linear || uniform != other.uniform || !(shift == other.shift) ||
            !(inverseShift == other.inverseShift)) return true;
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
            if (forward[i][j] != other.forward[i][j] || inverse[i][j] != other.inverse[i][j]) return true;
        return false;
    }
    std::shared_ptr<Transform> copy() const {
        return std::make_shared<Transform>(*this);
    }
    std::shared_ptr<const Transform> baseMap() const {
        return std::shared_ptr<const Transform>(this, [](const Transform*) {});
    }
    bool linear = true;
    double inverse[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3R inverseShift, shift;
    double forward[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3R indexToWorld(const Vec3R& p) const {
        nativeCall("map.indexToWorld");
        Vec3R result = shift;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result[i] += forward[i][j] * p[j];
        return result;
    }
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
using MapBase = Transform;
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
    std::uint64_t activeCount = 1;
    std::uint64_t activeVoxelCount() const { nativeCall("grid.activeCount"); return activeCount; }
    std::map<std::string, double> metadata;
    template<typename T> void insertMeta(const std::string& key, const T& value) {
        nativeCall("grid.metadata"); metadata[key] = value.value;
    }
    GridClass kind = GRID_UNKNOWN;
    VecType vectorType = VEC_INVARIANT;
    VecType getVectorType() const {
        return vectorType;
    }
    void setVectorType(VecType value) {
        vectorType = value;
    }
    void setTransform(std::shared_ptr<math::Transform> value) {
        xform = *value;
    }
    bool vacant = false;
    bool empty() const {
        return vacant;
    }
    GridClass getGridClass() const {
        return kind;
    }
    void setGridClass(GridClass value) { kind = value; }
    bool worldSpace = true;
    bool isInWorldSpace() const { return worldSpace; }
    void setName(const std::string& n) {
        name = n;
    }
    ConstPtr copyGrid() const {
        return deepCopyGrid();
    }

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
    virtual Ptr copyGridWithNewTree() const { return deepCopyGrid(); }
    const math::Transform& transform() const {
        return xform;
    }
    const std::string& getName() const {
        return name;
    }
    Vec3R voxelSize() const {
        return dx;
    }
    bool hasUniformVoxels() const { return xform.hasUniformScale(); }
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
    using Ptr = std::shared_ptr<FillTree>;
    using ValueType = T;
    FillTree() = default;
    T sample = T();
    template<typename Other, typename CopyTag>
    FillTree(const Other&, T, CopyTag) { nativeCall("native.topologyCopy"); }
    void voxelizeActiveTiles() { nativeCall("native.voxelize"); }
    template<typename Other> void topologyIntersection(const Other&) { nativeCall("native.intersection"); }
    template<typename Other> void topologyDifference(const Other&) { nativeCall("native.difference"); }
    template <typename Other> void topologyUnion(const Other&) {
        nativeCall("native.topologyUnion");
    }
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
    template<typename U> struct ValueConverter { using Type = ScalarGrid<U>; };
    template<typename G> void topologyUnion(const G&) { nativeCall("native.topologyUnion"); }
    static std::shared_ptr<ScalarGrid> create(T value = T()) {
        nativeCall("grid.create");
        auto result = std::make_shared<ScalarGrid>();
        result->bg = value;
        if (createEmptyGrid) { result->vacant=true; result->values.clear(); result->activeCount=0; }
        return result;
    }
    static std::shared_ptr<ScalarGrid> create(const typename TreeType::Ptr& tree) {
        nativeCall("grid.createTree");
        if (nullCopy) return nullptr;
        auto result = std::make_shared<ScalarGrid>();
        result->storedTree = *tree;
        result->values = {tree->sample};
        return result;
    }
    struct ConstAccessor {
        const ScalarGrid& grid;
        bool isValueOn(const Coord&) const { return grid.active; }
        T getValue(const Coord&) const { return grid.values.front(); }
    };
    ConstAccessor getConstAccessor() const { return {*this}; }
    TreeType storedTree;
    TreeType& tree() {
        return storedTree;
    }
    const TreeType& tree() const {
        return storedTree;
    }
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
    bool tile = false, active = true;
    std::uint64_t voxelsPerValue = 1;
    CoordBBox storedBox;
    ScalarGrid() {
        storedBox.reset({-4, -4, -4}, {3, 3, 3});
    }
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
    Ptr deepCopy() const { return std::dynamic_pointer_cast<ScalarGrid>(deepCopyGrid()); }
    GridBase::Ptr copyGridWithNewTree() const override {
        nativeCall("grid.emptyCopy");
        auto result = std::make_shared<ScalarGrid>(*this);
        result->values.clear();
        return result;
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
        bool isTileValue() const {
            return grid->tile;
        }
        bool isVoxelValue() const { return !grid->tile; }
        std::uint64_t getVoxelCount() const { return grid->tile ? grid->voxelsPerValue : 1; }
        bool isValueOn() const {
            return grid->active;
        }
        void getBoundingBox(CoordBBox& b) const {
            b = grid->storedBox;
        }
        T operator*() const {
            return grid->values.at(i);
        }
    };
    Iterator cbeginValueAll() const {
        return {this, 0};
    }
    Iterator cbeginValueOn() const { return {this, active ? 0 : values.size()}; }
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
using GridCPtrVecCIter = GridCPtrVec::const_iterator;

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
    std::int64_t wideInteger = 0;
    float number = 0;
    std::string text;
    MPxData* payload = nullptr;
    MTypeId type = DOpenVDBData::id;
};
struct MMatrix;
struct MDataHandle {
    InputValue value;
    bool valid;
    const MMatrix& asMatrix() const;
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
    float asFloat() const {
        check();
        return value.number;
    }
    short asShort() const {
        check();
        return static_cast<short>(value.integer);
    }
    int asInt() const { check(); return value.integer; }
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
        const auto found = socketConnections.find(object.name);
        return found == socketConnections.end() ? meshConnected : found->second;
    }

    bool operator!=(const MObject& value) const {
        return object.name != value.name;
    }
    bool operator==(const MObject& value) const { return object.name == value.name; }
};
struct MDataBlock {
    std::map<std::string, InputValue> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status = nullptr) {
        const MStatus result = operation("input " + attribute.name);
        if (status)
            *status = result;
        return {inputs.at(attribute.name), bool(result)};
    }
};
struct MFnPluginData {
    MObject create(int,MStatus* s){*s=operation("plugin.create");return {"staged"};}
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
    double x = 0, y = 0, z = 0, w = 1;
    MPoint() = default;
    MPoint(double a, double b, double c) : x(a), y(b), z(c) {}
    double operator[](int axis) const { return axis == 0 ? x : axis == 1 ? y : axis == 2 ? z : w; }
};
struct MBoundingBox {
    MPoint low, high;
    MPoint min() const {
        return low;
    }
    MPoint max() const {
        return high;
    }
    MPoint center() const { return {(low.x+high.x)/2, (low.y+high.y)/2, (low.z+high.z)/2}; }
};
static MBoundingBox meshBounds;
// The shared mesh reader is tested separately. This is its checked boundary.
inline MStatus getMeshInformation(const MObject&, MDataBlock&, MBoundingBox& bounds, MObject& object) {
    DOMMY_RETURN_IF_FAILURE(operation("mesh.bounds"));
    if (!meshValid)
        return MS::kFailure;
    bounds = meshBounds;
    object = {"mesh"};
    return MS::kSuccess;
}

inline MString& operator+=(MString& a, const char* b) {
    a = a + b;
    return a;
}
static int clipCalls;
static bool nullClip, wrongClip, aliasClip, badClipValue;
static int failClipAt = -1;
static int maskCalls;
static bool emptyInteriorMask = false;
static openvdb::CoordBBox lastClipBox;
static bool lastInside;
static double receivedCoordinate, receivedVolume;

namespace openvdb {
namespace tools {
template <typename G> GridBase::Ptr clipResult(const G& g, bool inside) {
    nativeCall("native.clip");
    if (clipCalls++ == failClipAt)
        throw std::runtime_error("late clip failure");
    lastInside = inside;
    if (nullClip)
        return nullptr;
    if (wrongClip)
        return std::make_shared<GridBase>();
    if (aliasClip)
        return GridBase::Ptr(const_cast<G*>(&g), [](GridBase*) {});
    auto result = std::make_shared<G>(g);
    if (badClipValue)
        result->values[0] = InvalidFillValue<typename G::ValueType>::get();
    return result;
}
template <typename G> GridBase::Ptr clip(const G& g, const BBoxd& b, bool inside) {
    Vec3R low, high;
    math::calculateBounds(g.transform(), b.min(), b.max(), low, high);
    Int32 l[3], h[3];
    for (int i = 0; i < 3; ++i) {
        l[i] = static_cast<Int32>(std::floor(low[i]));
        h[i] = static_cast<Int32>(std::floor(high[i]));
    }
    lastClipBox.reset({l[0], l[1], l[2]}, {h[0], h[1], h[2]});
    return clipResult(g, inside);
}
template <typename G, typename M> GridBase::Ptr clip(const G& g, const M& m, bool inside) {
    // Recording only: expose sampler-coordinate conversion, not resampling.
    auto p = m.transform().worldToIndex(g.transform().indexToWorld(Vec3d(0)));
    for (int i = 0; i < 3; ++i) {
        volatile Int32 n = static_cast<Int32>(std::floor(p[i]));
        (void)n;
    }
    return clipResult(g, inside);
}
template <typename G> BoolGrid::Ptr sdfInteriorMask(const G& g) {
    nativeCall("native.interiorMask");
    ++maskCalls;
    if (nullCopy)
        return nullptr;
    auto result = std::make_shared<BoolGrid>();
    result->xform = g.xform;
    result->bg = false;
    result->vacant = emptyInteriorMask;
    return result;
}
} // namespace tools
} // namespace openvdb
static openvdb::BBoxd activeWorldBox;
inline bool getActiveVoxelWorldBounds(const Grid&, openvdb::BBoxd& b) {
    nativeCall("active.bounds");
    b = activeWorldBox;
    return true;
}

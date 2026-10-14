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
#include "vdb_transport_enums.h"

static int publications, gridCopies, invalidReads;
static bool nullCopy, invalidNativeOutput, wrongCopy;
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
enum GridClass { GRID_UNKNOWN, GRID_LEVEL_SET, GRID_STAGGERED };
const int LEVEL_SET_HALF_WIDTH = 3;
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
namespace util {
struct NullInterrupter {
    bool wasInterrupted() const {
        return false;
    }
};
} // namespace util
namespace tools {
namespace Scheme {
enum SemiLagrangian { SEMI, MID, RK3, RK4, MAC, BFECC };
enum Limiter { NO_LIMITER, CLAMP, REVERT };
} // namespace Scheme
template <int, bool> struct Sampler {};
struct BoxSampler {};
struct StaggeredBoxSampler {};
template <typename G, typename S> struct DiscreteField {
    explicit DiscreteField(const G&) {}
};
template <typename G> void invalidate(G& grid) {
    grid.values[0] = typename G::ValueType(std::numeric_limits<float>::infinity());
}
template <typename G> struct TransportOperation {
    G& grid;
    explicit TransportOperation(G& g) : grid(g) {
        nativeCall("transport.create");
    }
    void setSpatialScheme(math::BiasedGradientScheme v) {
        nativeCall("transport.spatial");
        assert(v == math::FIRST_BIAS || v == math::HJWENO5_BIAS);
    }
    void setTemporalScheme(math::TemporalIntegrationScheme v) {
        nativeCall("transport.temporal");
        assert(v >= math::TVD_RK1 && v <= math::TVD_RK3);
    }
    void setTrackerSpatialScheme(math::BiasedGradientScheme v) {
        nativeCall("transport.trackerSpatial");
        assert(v == math::FIRST_BIAS || v == math::HJWENO5_BIAS);
    }
    void setTrackerTemporalScheme(math::TemporalIntegrationScheme v) {
        nativeCall("transport.trackerTemporal");
        assert(v >= math::TVD_RK1 && v <= math::TVD_RK3);
    }
    void setNormCount(int n) {
        nativeCall("transport.normCount");
        assert(n >= 0 && n <= 1024);
    }
    void advect(int, float) {
        nativeCall("transport.advect");
        if (invalidNativeOutput)
            invalidate(grid);
    }
};
template <typename G, typename F, typename B> struct LevelSetAdvection : TransportOperation<G> {
    LevelSetAdvection(G& g, const F&, B*) : TransportOperation<G>(g) {}
};
template <typename G, typename B> struct LevelSetMorphing : TransportOperation<G> {
    LevelSetMorphing(G& g, const G&, B*) : TransportOperation<G>(g) {}
    void setAlphaMask(const G& mask) {
        nativeCall("morph.mask");
        lastMask = &mask;
    }
    void setMaskRange(float lo, float hi) {
        nativeCall("morph.maskRange");
        assert(lo < hi);
    }
    void invertMask(bool) {
        nativeCall("morph.invert");
    }
};
template <typename Velocity, bool Staggered, typename B> struct VolumeAdvection {
    VolumeAdvection(const Velocity&, B*) {
        nativeCall("volume.create");
    }
    void setIntegrator(Scheme::SemiLagrangian v) {
        nativeCall("volume.integrator");
        assert(v >= Scheme::SEMI && v <= Scheme::BFECC);
    }
    void setLimiter(Scheme::Limiter v) {
        nativeCall("volume.limiter");
        assert(v >= Scheme::NO_LIMITER && v <= Scheme::REVERT);
    }
    void setSubSteps(int n) {
        nativeCall("volume.substeps");
        assert(n >= 1 && n <= 1024);
    }
    template <typename G, typename S> typename G::Ptr advect(const G& g, float) {
        nativeCall("volume.advect");
        ++gridCopies;
        if (nullCopy)
            return nullptr;
        auto result = std::make_shared<G>(g);
        if (invalidNativeOutput)
            invalidate(*result);
        return result;
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
    double number = 0;
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
    double asDouble() const {
        check();
        return value.number;
    }
    short asShort() const {
        check();
        return static_cast<short>(value.integer);
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
        return (value.nonnullObject || value.payload) ? MObject{"data"} : MObject::kNullObj;
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

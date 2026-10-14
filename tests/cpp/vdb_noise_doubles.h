#pragma once

// Explicit boundaries, not an OpenVDB sparse-tree/kernel implementation.
// Actual production compute/apply functions and actual vendored Perlin are used.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include "perlin.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>

static int publications, gridCopies, invalidReads, sampleCalls, stencilCalls;
static bool nullCopy;
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
struct Coord {
    int value[3];
    Coord(int x = 0, int y = 0, int z = 0) : value{x, y, z} {}
    int operator[](int i) const {
        return value[i];
    }
};
struct Vec3R {
    double value[3];
    Vec3R(double x = 0, double y = 0, double z = 0) : value{x, y, z} {}
    double operator[](int i) const {
        return value[i];
    }
    Vec3R operator+(const Vec3R& b) const {
        return {value[0] + b[0], value[1] + b[1], value[2] + b[2]};
    }
};
namespace math {
struct Transform {
    Vec3R shift, inverseShift;
    Vec3R indexToWorld(const Coord& p) const {
        nativeCall("map.indexToWorld");
        return Vec3R(p[0], p[1], p[2]) + shift;
    }
    Vec3R indexToWorld(const Vec3R& p) const {
        nativeCall("map.indexToWorld");
        return p + shift;
    }
    Vec3R worldToIndex(const Vec3R& p) const {
        nativeCall("map.worldToIndex");
        return p + inverseShift;
    }
};
} // namespace math
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() = default;
    std::string name = "source";
    math::Transform xform;
    std::uint64_t activeCount = 1U;
    Vec3R gradient, closestPoint;
    template <typename T> bool isType() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    virtual Ptr deepCopyGrid() const {
        nativeCall("grid.copy");
        ++gridCopies;
        return std::make_shared<GridBase>(*this);
    }
    const math::Transform& transform() const {
        return xform;
    }
    const std::string& getName() const {
        return name;
    }
    std::uint64_t activeVoxelCount() const {
        nativeCall("grid.count");
        return activeCount;
    }
};
template <typename Value> struct ScalarGrid : GridBase {
    using ValueType = Value;
    struct TreeType {
        using ValueType = Value;
        Value sample = Value(1);
        bool sampleActive = true;
    };
    TreeType values;
    std::vector<Value> active{Value(5)};
    std::vector<Coord> coordinates{Coord()};
    Ptr deepCopyGrid() const override {
        nativeCall("grid.copy");
        ++gridCopies;
        return nullCopy ? nullptr : std::make_shared<ScalarGrid>(*this);
    }
    const TreeType& tree() const {
        return values;
    }
    struct ValueOnIter {
        ScalarGrid* grid;
        std::size_t index;
        explicit operator bool() const {
            return index < grid->active.size();
        }
        ValueOnIter& operator++() {
            ++index;
            return *this;
        }
        Value operator*() const {
            return grid->active.at(index);
        }
        Coord getCoord() const {
            return grid->coordinates.at(index);
        }
        void setValue(Value v) {
            nativeCall("value.set");
            grid->active.at(index) = v;
        }
    };
    ValueOnIter beginValueOn() {
        return {this, 0U};
    }
};
using FloatGrid = ScalarGrid<float>;
using DoubleGrid = ScalarGrid<double>;
namespace math {
enum { CD_2ND };
struct GenericMap {
    explicit GenericMap(const GridBase&) {
        nativeCall("map.create");
    }
};
template <typename GridType> struct SecondOrderDenseStencil {
    const GridType& grid;
    explicit SecondOrderDenseStencil(const GridType& value) : grid(value) {
        nativeCall("stencil.create");
    }
    void moveTo(Coord) {
        nativeCall("stencil.move");
        ++stencilCalls;
    }
    void moveTo(typename GridType::ValueOnIter iter) {
        moveTo(iter.getCoord());
    }
};
template <typename Map, int> struct Gradient {
    template <typename Stencil> static Vec3R result(const Map&, const Stencil& s) {
        nativeCall("gradient");
        return s.grid.gradient;
    }
};
template <typename Map, int> struct CPT {
    template <typename Stencil> static Vec3R result(const Map&, const Stencil& s) {
        nativeCall("cpt");
        return s.grid.closestPoint;
    }
};
} // namespace math
namespace tools {
struct BoxSampler {
    template <typename Tree>
    static bool sample(const Tree& tree, const Vec3R& point, typename Tree::ValueType& result) {
        nativeCall("sample");
        ++sampleCalls;
        // Match the dangerous native boundary: floor -> int, then +1.
        // Sanitizers must catch regressions which omit the coordinate preflight.
        for (int axis = 0; axis < 3; ++axis) {
            volatile int floor = static_cast<int>(std::floor(point[axis]));
            volatile int next = floor + 1;
            (void)next;
        }
        result = tree.sample;
        return tree.sampleActive;
    }
};
} // namespace tools
} // namespace openvdb
using Grid = openvdb::GridBase;
using GridPtr = Grid::Ptr;
using GridCPtr = Grid::ConstPtr;
using GridCRef = const Grid&;
using GridCPtrVec = std::vector<GridCPtr>;
template <typename GridType> GridType& vdbGridCast(Grid& g) {
    return static_cast<GridType&>(g);
}
template <typename GridType> const GridType* vdbGridCast(const Grid* g) {
    return static_cast<const GridType*>(g);
}

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
struct MFloatVector {
    float v[3];
    MFloatVector(float x = 0, float y = 0, float z = 0) : v{x, y, z} {}
    float operator[](int axis) const {
        return v[axis];
    }
};
struct InputValue {
    float number = 0.0f;
    int integer = 0;
    std::string text;
    MFloatVector vector;
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
    int asInt() const {
        check();
        return value.integer;
    }
    MFloatVector asFloatVector() const {
        check();
        return value.vector;
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

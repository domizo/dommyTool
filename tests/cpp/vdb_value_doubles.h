#pragma once

// Explicit boundaries, not an OpenVDB sparse-tree/kernel implementation.
// Production compute and traversal bodies run against small recording trees.
#define MRampAttribute AttributeRampOnly
#include "maya_attribute_doubles.h"
#undef MRampAttribute
#include <array>
#include <type_traits>
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "math/DRemapMath.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>

static int publications, gridCopies, invalidReads;
static bool nullCopy, invalidNativeOutput, wrongCopy;
static int rampMode, rampFailure;
static bool worker;
static int rampSamples;
static long double pruneTolerance;
static std::vector<std::string> filterCalls;

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

namespace tbb {
template <typename T> struct blocked_range {
    T a, b;
    blocked_range(T x, T y) : a(x), b(y) {}
    T begin() const {
        return a;
    }
    T end() const {
        return b;
    }
};
template <typename R, typename Op> void parallel_for(const R& r, const Op& op) {
    nativeCall("parallel_for");
    worker = true;
    try {
        op(r);
    } catch (...) {
        worker = false;
        throw;
    }
    worker = false;
}
} // namespace tbb
namespace boost {
namespace mpl {
template <int I> struct int_ {};
template <typename Chain, typename I> struct at {
    using type = typename Chain::Internal;
};
} // namespace mpl
} // namespace boost
namespace openvdb {
enum GridClass { GRID_UNKNOWN, GRID_LEVEL_SET };
namespace math {
template <typename T> struct Vec3 {
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
template <typename T> bool isNegative(T value) {
    return value < T(0);
}
template <typename T> bool isNegative(const Vec3<T>& value) {
    return value[0] < 0 && value[1] < 0 && value[2] < 0;
}
template <typename T> struct Tolerance {
    static T value() {
        return T(0);
    }
};
template <> struct Tolerance<float> {
    static float value() {
        return 1e-8f;
    }
};
template <> struct Tolerance<double> {
    static double value() {
        return 1e-15;
    }
};
template <typename T> struct Tolerance<Vec3<T>> {
    static Vec3<T> value() {
        return Vec3<T>(Tolerance<T>::value());
    }
};
} // namespace math
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() = default;
    std::string name = "source";
    GridClass kind = GRID_LEVEL_SET;
    template <typename T> bool isType() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    virtual Ptr deepCopyGrid() const {
        nativeCall("grid.copy");
        ++gridCopies;
        return nullCopy ? nullptr : std::make_shared<GridBase>(*this);
    }
    const std::string& getName() const {
        return name;
    }
    void setGridClass(GridClass value) {
        kind = value;
    }
};
template <typename T> struct Node {
    using ValueType = T;
    static const std::size_t SIZE = 4;
    std::array<T, 4> values{{T(0), T(1), T(0), T(1)}};
    std::array<bool, 4> active{{true, true, false, false}};
    Node& buffer() {
        return *this;
    }
    T* data() {
        return values.data();
    }
    struct ValueOnIter {
        Node* n;
        std::size_t i;
        ValueOnIter(Node* node, std::size_t start) : n(node), i(start) {
            skip();
        }
        void skip() {
            while (i < SIZE && !n->active[i])
                ++i;
        }
        explicit operator bool() const {
            return i < SIZE;
        }
        ValueOnIter& operator++() {
            ++i;
            skip();
            return *this;
        }
        T operator*() const {
            return n->values[i];
        }
        void setValueOff() const {
            n->active[i] = false;
        }
    };
    ValueOnIter beginValueOn() {
        return {this, 0};
    }
};
template <typename T> struct InternalNode : Node<T> {};
template <typename T> struct Tree {
    using ValueType = T;
    using LeafNodeType = Node<T>;
    struct RootNodeType {
        struct NodeChainType {
            using Internal = InternalNode<T>;
        };
    };
    struct ValueAllCIter {
        static const int LEAF_DEPTH = 3;
    };
    LeafNodeType leaf;
    InternalNode<T> internal;
    Node<T> tiles;
    T bg = T(0);
    void getNodes(std::vector<LeafNodeType*>& out) {
        out.push_back(&leaf);
    }
    void getNodes(std::vector<InternalNode<T>*>& out) {
        out.push_back(&internal);
    }
    T background() const {
        return bg;
    }
    Tree& root() {
        return *this;
    }
    void setBackground(T value, bool update) {
        assert(!update);
        bg = value;
    }
    struct ValueOnIter : Node<T>::ValueOnIter {
        explicit ValueOnIter(Tree& tree) : Node<T>::ValueOnIter(&tree.tiles, 0) {}
        void setMaxDepth(int depth) {
            assert(depth == 1);
        }
    };
};
template <typename T> struct ValueGrid : GridBase {
    using ValueType = T;
    using TreeType = Tree<T>;
    TreeType data;
    TreeType& tree() {
        return data;
    }
    const TreeType& tree() const {
        return data;
    }
    T background() const {
        return data.bg;
    }
    Ptr deepCopyGrid() const override {
        nativeCall("grid.copy");
        ++gridCopies;
        if (wrongCopy)
            return std::make_shared<GridBase>();
        return nullCopy ? nullptr : std::make_shared<ValueGrid>(*this);
    }
    struct ConstIterator {
        const ValueGrid* grid;
        std::size_t i;
        explicit operator bool() const {
            return i < 12;
        }
        ConstIterator& operator++() {
            ++i;
            return *this;
        }
        T operator*() const {
            return i < 4   ? grid->data.leaf.values[i]
                   : i < 8 ? grid->data.internal.values[i - 4]
                           : grid->data.tiles.values[i - 8];
        }
    };
    ConstIterator cbeginValueAll() const {
        return {this, 0};
    }
    struct ValueAllIter {
        static const int LEAF_DEPTH = 3;
        ValueGrid* grid;
        std::size_t i;
        explicit operator bool() const {
            return i < 12;
        }
        ValueAllIter& operator++() {
            ++i;
            return *this;
        }
        void setMaxDepth(int depth) {
            assert(depth == 2);
            i = 4;
        }
        T& value() const {
            return i < 8 ? grid->data.internal.values[i - 4] : grid->data.tiles.values[i - 8];
        }
        T operator*() const {
            return value();
        }
        void setValue(T v) const {
            value() = v;
        }
    };
    ValueAllIter beginValueAll() {
        return {this, 0};
    }
};
using FloatGrid = ValueGrid<float>;
using DoubleGrid = ValueGrid<double>;
using Int32Grid = ValueGrid<std::int32_t>;
using Int64Grid = ValueGrid<std::int64_t>;
using BoolGrid = ValueGrid<bool>;
using Vec3SGrid = ValueGrid<math::Vec3<float>>;
using Vec3DGrid = ValueGrid<math::Vec3<double>>;
using Vec3IGrid = ValueGrid<math::Vec3<std::int32_t>>;
using Vec3fGrid = Vec3SGrid;
using Vec3dGrid = Vec3DGrid;
namespace tools {
struct PointIndexGrid : GridBase {};
template <typename T> long double toleranceValue(T v) {
    return v;
}
template <typename T> long double toleranceValue(const math::Vec3<T>& v) {
    assert(v[0] == v[1] && v[0] == v[2]);
    return v[0];
}
template <typename T> void nativePrune(Tree<T>& tree, const char* name) {
    nativeCall(name);
    filterCalls.push_back(name);
    if (invalidNativeOutput)
        tree.bg = std::numeric_limits<T>::quiet_NaN();
}
template <typename T> void prune(Tree<T>& tree, T tolerance) {
    pruneTolerance = toleranceValue(tolerance);
    nativePrune(tree, "prune.value");
}
template <typename T> void pruneInactive(Tree<T>& tree) {
    nativePrune(tree, "prune.inactive");
}
template <typename T> void pruneLevelSet(Tree<T>& tree) {
    nativePrune(tree, "prune.levelset");
}
template <typename It, typename Op> void foreach (It it, Op & op, bool threaded) {
    assert(threaded);
    nativeCall("foreach");
    worker = true;
    try {
        for (; it; ++it)
            op(it);
    } catch (...) {
        worker = false;
        throw;
    }
    worker = false;
}
} // namespace tools
namespace points {
struct PointDataGrid : GridBase {};
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
static DOpenVDBData inputData, maskData, stagedData, outputData;
struct InputValue {
    double number = 0;
    float pair[2] = {0, 1};
    int integer = 0;
    std::string text;
    MPxData* payload = nullptr;
    MTypeId type = DOpenVDBData::id;
    bool nonnullObject = false;
};
using float2 = float[2];
struct MDataHandle {
    double asDouble() const {
        check();
        return value.number;
    }
    short asShort() const {
        check();
        return static_cast<short>(value.integer);
    }
    const float2& asFloat2() const {
        check();
        return value.pair;
    }
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
    MPlug(MObject, MObject value) : object(value) {}
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

struct MRampAttribute {
    MRampAttribute(const MPlug&, MStatus* status) {
        *status = operation("ramp.create");
    }
    void getValueAtPosition(float position, float& value, MStatus* status) {
        assert(!worker);
        const int sample = rampSamples++;
        *status = sample == rampFailure ? MS::kFailure : MS::kSuccess;
        value = rampMode == 1   ? .5f
                : rampMode == 2 ? (position <= .5f ? std::numeric_limits<float>::max()
                                                   : -std::numeric_limits<float>::max())
                : rampMode == 3 ? std::numeric_limits<float>::quiet_NaN()
                                : position;
    }
};

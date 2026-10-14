#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>
#include <typeinfo>

// Minimal public grid boundary, not an OpenVDB implementation. A forged type
// label deliberately differs from RTTI so dispatch cannot trust labels alone.
namespace openvdb {
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    int label = 0;
    virtual ~GridBase() = default;
    template<class T> bool isType() const { return label == T::number; }
};
template<int Number> struct TestGrid : GridBase {
    static const int number = Number;
    using Ptr = std::shared_ptr<TestGrid>;
    using ConstPtr = std::shared_ptr<const TestGrid>;
    TestGrid() { label = Number; }
};
using FloatGrid = TestGrid<1>; using DoubleGrid = TestGrid<2>;
using Int32Grid = TestGrid<3>; using Int64Grid = TestGrid<4>;
using BoolGrid = TestGrid<5>; using Vec3SGrid = TestGrid<6>;
using Vec3DGrid = TestGrid<7>; using Vec3IGrid = TestGrid<8>;
namespace tools { using PointIndexGrid = TestGrid<9>; }
namespace points { using PointDataGrid = TestGrid<10>; }
template<class G> typename G::Ptr gridPtrCast(const GridBase::Ptr& value) {
    return std::dynamic_pointer_cast<G>(value);
}
template<class G> typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr& value) {
    return std::dynamic_pointer_cast<const G>(value);
}
}
#include "vdb_dispatch_under_test.h"

struct Probe {
    int calls = 0;
    bool constant = false;
    const openvdb::GridBase* seen = nullptr;
    template<class G> void operator()(G& value) { ++calls; constant = false; seen = &value; }
    template<class G> void operator()(const G& value) { ++calls; constant = true; seen = &value; }
    template<class G> void operator()(G* value) { ++calls; constant = false; seen = value; }
    template<class G> void operator()(const G* value) { ++calls; constant = true; seen = value; }
    template<class G> void operator()(typename G::Ptr value) { ++calls; constant = false; seen = value.get(); }
    template<class G> void operator()(typename G::ConstPtr value) { ++calls; constant = true; seen = value.get(); }
};
template<class View> bool visit(int family, DVdbGridType kind, View&& view, Probe& probe) {
    switch (family) {
    case 0: return processVdbGrid(kind, std::forward<View>(view), probe);
    case 1: return processVdbTopologyGrid(kind, std::forward<View>(view), probe);
    case 2: return processVdbScalarGrid(kind, std::forward<View>(view), probe);
    case 3: return processVdbRealGrid(kind, std::forward<View>(view), probe);
    case 4: return processVdbVectorGrid(kind, std::forward<View>(view), probe);
    case 5: return processVdbRealVectorGrid(kind, std::forward<View>(view), probe);
    default: return processVdbPointGrid(kind, std::forward<View>(view), probe);
    }
}
bool allowed(int family, int kind) {
    switch (family) {
    case 0: return (kind >= 1 && kind <= 4) || (kind >= 6 && kind <= 8);
    case 1: return kind >= 1 && kind <= 8;
    case 2: return kind >= 1 && kind <= 4;
    case 3: return kind == 1 || kind == 2;
    case 4: return kind >= 6 && kind <= 8;
    case 5: return kind == 6 || kind == 7;
    default: return kind == 9 || kind == 10;
    }
}
template<class View> void checkView(View&& view, const openvdb::GridBase* address,
                                   DVdbGridType actual, bool constant) {
    for (int family = 0; family < 7; ++family) for (int kind = 0; kind <= 11; ++kind) {
        Probe probe;
        const bool expected = kind == static_cast<int>(actual) && allowed(family, kind);
        assert(visit(family, static_cast<DVdbGridType>(kind), std::forward<View>(view), probe) == expected);
        assert(probe.calls == (expected ? 1 : 0));
        if (expected) assert(probe.seen == address && probe.constant == constant);
    }
}
template<int N> void checkGrid() {
    const auto typed = std::make_shared<openvdb::TestGrid<N>>();
    openvdb::GridBase::Ptr mutablePtr = typed;
    openvdb::GridBase::ConstPtr constantPtr = typed;
    openvdb::GridBase& mutableRef = *typed;
    const openvdb::GridBase& constantRef = *typed;
    const auto kind = static_cast<DVdbGridType>(N);
    assert(getVdbGridType(constantRef) == kind);
    const char* names[] = {"invalid type","float","double","int32","int64","bool",
                          "Vec3f","Vec3d","Vec3i","PointIndex","PointData"};
    assert(std::strcmp(getVdbGridTypeName(constantRef), names[N]) == 0);
    assert(getVdbGridTupleSize(kind) == (N <= 5 ? 1 : N <= 8 ? 3 : 0));
    assert(vdbGridCast<openvdb::TestGrid<N>>(&mutableRef) == typed.get());
    assert(vdbGridCast<openvdb::TestGrid<N>>(&constantRef) == typed.get());
    assert(&vdbGridCast<openvdb::TestGrid<N>>(mutableRef) == typed.get());
    assert(&vdbGridCast<openvdb::TestGrid<N>>(constantRef) == typed.get());
    assert(vdbGridCast<openvdb::TestGrid<N>>(mutablePtr).get() == typed.get());
    assert(vdbGridCast<openvdb::TestGrid<N>>(constantPtr).get() == typed.get());
    checkView(mutableRef, typed.get(), kind, false);
    checkView(constantRef, typed.get(), kind, true);
    checkView(&mutableRef, typed.get(), kind, false);
    checkView(&constantRef, typed.get(), kind, true);
    checkView(mutablePtr, typed.get(), kind, false);
    checkView(constantPtr, typed.get(), kind, true);
    assert(typed.use_count() == 3);
}
struct RealOnly {
    template<class G> void operator()(const G&) {
        static_assert(std::is_same<G,openvdb::FloatGrid>::value ||
                      std::is_same<G,openvdb::DoubleGrid>::value,
                      "Real-only functor must not instantiate other grid types");
    }
};
int main() {
    checkGrid<1>(); checkGrid<2>(); checkGrid<3>(); checkGrid<4>(); checkGrid<5>();
    checkGrid<6>(); checkGrid<7>(); checkGrid<8>(); checkGrid<9>(); checkGrid<10>();
    openvdb::GridBase forged;
    forged.label = 1;
    assert(getVdbGridType(forged) == DVdbGridType::kInvalid);
    assert(!vdbGridCast<openvdb::FloatGrid>(&forged));
    bool threw = false;
    try { (void)vdbGridCast<openvdb::FloatGrid>(forged); } catch (const std::bad_cast&) { threw = true; }
    assert(threw);
    Probe probe;
    assert(!processVdbRealGrid(DVdbGridType::kFloat, forged, probe) && probe.calls == 0);
    openvdb::GridBase::Ptr empty;
    assert(!vdbGridCast<openvdb::FloatGrid>(empty));
    assert(!processVdbGrid(DVdbGridType::kFloat, empty, probe));
    assert(!processVdbGrid(DVdbGridType::kFloat, static_cast<openvdb::GridBase*>(nullptr), probe));
    assert(getVdbGridTupleSize(static_cast<DVdbGridType>(-1)) == 0);
    assert(getVdbGridTupleSize(static_cast<DVdbGridType>(1000)) == 0);
    const openvdb::DoubleGrid value;
    RealOnly real;
    assert(processVdbRealGrid(DVdbGridType::kDouble, value, real));
}

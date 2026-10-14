#pragma once
#include "vdb_clip_doubles.h"
#include "modules/point/DPoint/math/DVectorMath.h"

// Maya and sparse kernels below are recording boundaries, not native OpenVDB.
// The node's production operators/readers/preflight and stable arithmetic run unchanged.
static int analysisCalls, resampleCalls, maskedCalls, failAnalysisAt = -1;
static bool nullAnalysis, wrongAnalysis, aliasAnalysis, badAnalysis;
static bool lastDensify;
namespace openvdb {
namespace util {
struct NullInterrupter {};
} // namespace util
namespace tools {
struct PointSampler {};
template <typename Sampler, typename G, typename I> void resampleToMatch(const G&, G&, I&) {
    nativeCall("native.resample");
    ++resampleCalls;
}
template <typename G> struct VectorToScalarConverter;
template <typename T> struct VectorToScalarConverter<ScalarGrid<math::Vec3<T>>> {
    using Type = ScalarGrid<T>;
};
template <typename G> struct ScalarToVectorConverter {
    using Type = ScalarGrid<math::Vec3<typename G::ValueType>>;
};
template <typename G, typename Out>
typename Out::Ptr analysisResult(const G& input, typename Out::Ptr result, bool masked,
                                 bool densify) {
    nativeCall("native.analysis");
    if (analysisCalls++ == failAnalysisAt)
        throw std::runtime_error("late analysis failure");
    maskedCalls += masked;
    lastDensify = densify;
    if (nullAnalysis)
        return nullptr;
    if (badAnalysis)
        result->values[0] = InvalidFillValue<typename Out::ValueType>::get();
    result->xform = input.xform;
    return result;
}
template <int Kind, typename G> struct OutputGrid {
    using Type = G;
};
template <typename G> struct OutputGrid<0, G> : ScalarToVectorConverter<G> {};
template <typename G> struct OutputGrid<3, G> : ScalarToVectorConverter<G> {};
template <typename G> struct OutputGrid<4, G> : VectorToScalarConverter<G> {};
template <int Kind, typename G, typename M, typename I> struct AnalysisTool {
    const G& grid;
    bool masked;
    AnalysisTool(const G& g, I*) : grid(g), masked(false) {}
    AnalysisTool(const G& g, const M&, I*) : grid(g), masked(true) {}
    GridBase::Ptr process(bool) {
        using Out = typename OutputGrid<Kind, G>::Type;
        auto result = analysisResult<G, Out>(grid, std::make_shared<Out>(), masked, true);
        if (wrongAnalysis)
            return std::make_shared<GridBase>();
        if (aliasAnalysis)
            return GridBase::Ptr(const_cast<G*>(&grid), [](GridBase*) {});
        return result;
    }
};
template <typename G, typename M, typename I> using Gradient = AnalysisTool<0, G, M, I>;
template <typename G, typename M, typename I> using MeanCurvature = AnalysisTool<1, G, M, I>;
template <typename G, typename M, typename I> using Laplacian = AnalysisTool<2, G, M, I>;
template <typename G, typename M, typename I> using Cpt = AnalysisTool<3, G, M, I>;
template <typename G, typename M, typename I> using Divergence = AnalysisTool<4, G, M, I>;
template <typename G, typename M, typename I> using Curl = AnalysisTool<5, G, M, I>;
namespace gridop {
template <typename G, typename Mask, typename Out, typename Map, typename Op, typename Interrupt>
struct GridOperator {
    const G& grid;
    const Mask* mask;
    const Map& map;
    bool densify;
    GridOperator(const G& g, const Mask* m, const Map& a, Interrupt*, bool d)
        : grid(g), mask(m), map(a), densify(d) {}
    struct Accessor {
        typename G::ValueType value;
        typename G::ValueType getValue(const Coord&) const {
            return value;
        }
    };
    typename Out::Ptr process(bool) {
        auto result = std::make_shared<Out>();
        result->bg = Op::result(map, Accessor{grid.bg}, Coord());
        result->values.clear();
        for (const auto& value : grid.values)
            result->values.push_back(Op::result(map, Accessor{value}, Coord()));
        return analysisResult<G, Out>(grid, result, mask != nullptr, densify);
    }
};
} // namespace gridop
} // namespace tools
} // namespace openvdb

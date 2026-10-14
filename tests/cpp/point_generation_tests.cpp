#define JC_VORONOI_IMPLEMENTATION
#include "common/utility/DRelaxPoint2D.h"
#include "common/utility/DConvexHull2D.h"
#include <ImathVec.h>

#include <cassert>
#include <climits>
#include <cfloat>
#include <map>
#include <iostream>

namespace {

struct Allocations {
    std::map<void*, std::size_t> live;
    std::size_t calls = 0U;
    std::size_t failAt = std::numeric_limits<std::size_t>::max();
    bool throwFailure = false;
    ~Allocations() { assert(live.empty()); }
    static void* allocate(void* context, std::size_t bytes) {
        auto& state = *static_cast<Allocations*>(context);
        if (state.calls++ == state.failAt) {
            if (state.throwFailure) throw std::bad_alloc();
            return nullptr;
        }
        void* pointer = std::malloc(bytes);
        if (!pointer) throw std::bad_alloc();
        std::memset(pointer, 0xa5, bytes);
        state.live[pointer] = bytes;
        return pointer;
    }
    static void release(void* context, void* pointer) {
        auto& state = *static_cast<Allocations*>(context);
        assert(state.live.erase(pointer) == 1U);
        std::free(pointer);
    }
};

std::vector<jcv_point> randomSites(unsigned int count, unsigned int seed) {
    std::mt19937 engine(seed);
    std::uniform_real_distribution<double> random(0.0, 1.0);
    std::vector<jcv_point> points(count);
    for (auto& point : points) point = jcv_point{random(engine), random(engine)};
    return points;
}

void checkDiagram(const jcv_diagram& diagram) {
    assert(diagram.internal);
    assert(reinterpret_cast<std::uintptr_t>(diagram.internal->eventmem) % alignof(void*) == 0U);
    assert(reinterpret_cast<std::uintptr_t>(diagram.internal->sites) % alignof(jcv_site) == 0U);
    std::size_t total = 0U;
    const auto sites = jcv_diagram_get_sites(&diagram);
    for (int i = 0; i < diagram.numsites; ++i) {
        assert(sites[i].edges);
        for (auto edge = sites[i].edges; edge; edge = edge->next) {
            assert(++total < 100000U);
            assert(std::isfinite(edge->angle));
            for (const auto& point : edge->pos) {
                assert(std::isfinite(point.x) && std::isfinite(point.y));
                assert(point.x >= 0 && point.x <= diagram.width);
                assert(point.y >= 0 && point.y <= diagram.height);
            }
        }
    }
}

void testVoronoiMemory() {
    const auto points = randomSites(513U, 7U);
    std::size_t calls = 0U;
    {
        Allocations allocations;
        dommy::geometry::RelaxDiagram diagram;
        jcv_diagram_generate_useralloc(static_cast<int>(points.size()), points.data(), 1.0, 1.0,
            &allocations, Allocations::allocate, Allocations::release, &diagram.value);
        checkDiagram(diagram.value);
        calls = allocations.calls;
        assert(calls > 2U);
    }
    for (bool throws : {false, true}) for (std::size_t failure = 0U; failure < calls; ++failure) {
        Allocations allocations;
        allocations.failAt = failure; allocations.throwFailure = throws;
        dommy::geometry::RelaxDiagram diagram;
        bool failed = false;
        try {
            jcv_diagram_generate_useralloc(static_cast<int>(points.size()), points.data(), 1.0, 1.0,
                &allocations, Allocations::allocate, Allocations::release, &diagram.value);
        } catch (const std::bad_alloc&) { failed = true; }
        assert(failed && !diagram.value.internal && allocations.live.empty());
    }
    Allocations allocations;
    dommy::geometry::RelaxDiagram diagram;
    jcv_diagram_generate_useralloc(3, points.data(), 1.0, 1.0,
        &allocations, Allocations::allocate, Allocations::release, &diagram.value);
    const auto previous = diagram.value.internal;
    const auto live = allocations.live.size();
    allocations.failAt = allocations.calls + 1U;
    bool failed = false;
    try {
        jcv_diagram_generate_useralloc(513, points.data(), 1.0, 1.0,
            &allocations, Allocations::allocate, Allocations::release, &diagram.value);
    } catch (const std::bad_alloc&) { failed = true; }
    assert(failed && diagram.value.internal == previous && allocations.live.size() == live);
    checkDiagram(diagram.value);
    for (double width : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
        failed = false;
        try { jcv_diagram_generate(3, points.data(), width, 1.0, &diagram.value); }
        catch (const std::invalid_argument&) { failed = true; }
        assert(failed && diagram.value.internal == previous);
    }
    jcv_diagram_free(&diagram.value);
    jcv_diagram_free(&diagram.value);
    assert(!diagram.value.internal && !jcv_diagram_get_sites(&diagram.value));

    // Fail release-build invariants explicitly; never read an undefined corner.
    jcv_context_internal internal = {};
    internal.width = internal.height = 1.0;
    jcv_site site = {};
    jcv_graphedge edge = {}, gap = {};
    edge.pos[1] = jcv_point{0.5,0.5};
    failed = false;
    try { jcv_create_corner_edge(&internal, &site, &edge, &gap); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed);
    failed = false;
    try { (void)jcv_halfedge_rightof(nullptr, &site.p); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed);
    failed = false;
    try { jcv_spend_work(&internal); }
    catch (const std::length_error&) { failed = true; }
    assert(failed);
    internal.allocated = 256U * 1024U * 1024U;
    failed = false;
    try { (void)jcv_alloc(&internal, 1U); }
    catch (const std::length_error&) { failed = true; }
    assert(failed);
}

void testVoronoiInputs() {
    for (unsigned int count : {1U,2U,3U,8U,31U,128U}) {
        for (unsigned int seed = 0U; seed < 20U; ++seed) {
            auto points = randomSites(count, seed);
            for (double extent : {0.5,1.5,1.0}) {
                auto scaled = points;
                for (auto& point : scaled) { point.x *= extent; point.y *= extent; }
                dommy::geometry::RelaxDiagram diagram;
                jcv_diagram_generate(static_cast<int>(count), scaled.data(), extent, extent, &diagram.value);
                assert(diagram.value.width == extent);
                checkDiagram(diagram.value);
            }
        }
    }
    for (const auto& points : std::vector<std::vector<jcv_point>>{
            {{0,0},{1,0},{0,1},{1,1}}, {{0.5,0.5},{0.5,0.5},{0.5,0.5}},
            {{0.2,0.2},{0.2,0.2},{0.4,0.4},{0.4,0.4},{0.8,0.8}},
            {{0.1,0.5},{0.3,0.5},{0.8,0.5}}}) {
        dommy::geometry::RelaxDiagram diagram;
        jcv_diagram_generate(static_cast<int>(points.size()), points.data(), 1.0, 1.0, &diagram.value);
        checkDiagram(diagram.value);
    }
}

void testRelaxation() {
    using dommy::geometry::generateRelaxedPoints2D;
    std::vector<std::array<double, 3>> output;
    for (float width : {0.5f,1.5f,1e20f}) {
        for (int seed : {0,1,17,INT_MAX - 49,INT_MAX}) {
            assert(generateRelaxedPoints2D(64U, 3U, seed, 0, width, 0, width * 0.5f, output));
            assert(output.size() == 64U);
            for (const auto& point : output)
                assert(point[0] >= 0 && point[0] <= width && point[1] == 0 &&
                       point[2] >= 0 && point[2] <= width * 0.5f);
        }
    }
    std::vector<std::array<double, 3>> base, scaled;
    assert(generateRelaxedPoints2D(32, 2, 3, 0, 2, 0, 1, base));
    assert(generateRelaxedPoints2D(32, 2, 3, 10, 14, -7, -5, scaled));
    for (std::size_t i = 0; i < base.size(); ++i) {
        assert(std::abs(scaled[i][0] - (10 + 2 * base[i][0])) < 1e-5);
        assert(std::abs(scaled[i][2] - (-7 + 2 * base[i][2])) < 1e-5);
    }
    // Zero-iteration ordinary requests retain the original RNG sequence.
    assert(generateRelaxedPoints2D(10, 0, 7, 0, 1, 0, 1, output));
    std::mt19937 u(7), v(57);
    std::uniform_real_distribution<float> distribution(0.0f, std::nextafter(1.0f, FLT_MAX));
    for (const auto& point : output) assert(point[0] == distribution(u) && point[2] == distribution(v));
    const auto previous = output;
    assert(!generateRelaxedPoints2D(1, 1, -1, 0, 1, 0, 1, output) && output == previous);
    assert(!generateRelaxedPoints2D(1, 1, 0, NAN, 1, 0, 1, output) && output == previous);
    assert(!generateRelaxedPoints2D(1, 1, 0, 0, 0, 0, 1, output) && output == previous);
    assert(!generateRelaxedPoints2D(10000001, 0, 0, 0, 1, 0, 1, output) && output == previous);
    assert(!generateRelaxedPoints2D(1, 100000001, 0, 0, 1, 0, 1, output) && output == previous);
    assert(generateRelaxedPoints2D(0, 1, 0, 0, 1, 0, 1, output) && output.empty());
    const float tiny = std::numeric_limits<float>::denorm_min();
    assert(generateRelaxedPoints2D(1, 1, 0, 0, tiny, 0, tiny, output));
    assert(generateRelaxedPoints2D(1, 1, 0, -FLT_MAX, FLT_MAX, -FLT_MAX, FLT_MAX, output));
}

void testConvexHull() {
    using Point = std::array<float, 3>;
    using Points = std::vector<Point>;
    using dommy::geometry::buildConvexHull2D;
    Points hull;
    std::vector<Imath::V3f> imathHull;
    assert(buildConvexHull2D(std::vector<Imath::V3f>{Imath::V3f(0,0,0),
        Imath::V3f(1e20f,1e20f,0), Imath::V3f(2e20f,2.5e20f,0)}, imathHull));
    assert(imathHull.size() == 3U);
    for (float scale : {1.0f,1e20f,1e-30f}) {
        Points triangle = {{{0,0,0}}, {{scale,scale,0}}, {{2*scale,2.5f*scale,0}}};
        assert(buildConvexHull2D(triangle, hull) && hull.size() == 3U);
        Points line = {{{0,0,0}}, {{scale,scale,0}}, {{2*scale,2*scale,0}}};
        assert(buildConvexHull2D(line, hull) && hull.size() == 2U);
    }
    assert(buildConvexHull2D(Points{{{0,0,3}},{{0,0,7}},{{0,0,9}}}, hull) && hull.size() == 1U);
    assert(hull[0][2] == 3);
    const auto previous = hull;
    assert(!buildConvexHull2D(Points{{{NAN,0,0}}}, hull) && hull == previous);
    assert(!buildConvexHull2D(Points{{{0,0,INFINITY}}}, hull) && hull == previous);
    assert(buildConvexHull2D(Points(), hull) && hull.empty());
    assert(buildConvexHull2D(Points{{{-FLT_MAX,-FLT_MAX,0}},{{FLT_MAX,-FLT_MAX,0}},
                                   {{FLT_MAX,FLT_MAX,0}},{{-FLT_MAX,FLT_MAX,0}},{{0,0,0}}}, hull));
    assert(hull.size() == 4U);
    Points repeated = {{{0,0,0}},{{1,0,0}},{{1,1,0}},{{0,1,0}},{{0.5f,0.5f,0}}};
    std::mt19937 engine(11);
    for (unsigned int i = 0; i < 100U; ++i) {
        std::shuffle(repeated.begin(), repeated.end(), engine);
        assert(buildConvexHull2D(repeated, hull) && hull.size() == 4U);
    }
    int seed = 99;
    assert(dommy::random::offsetNonnegativeSeed(INT_MAX, 50U, seed) && seed == 49);
    assert(dommy::random::offsetNonnegativeSeed(INT_MAX - 50, 50U, seed) && seed == INT_MAX);
    assert(dommy::random::offsetNonnegativeSeed(0, UINT_MAX, seed) && seed == INT_MAX);
    assert(!dommy::random::offsetNonnegativeSeed(-1, 50U, seed) && seed == INT_MAX);
}

} // namespace

void runPointGenerationTests() {
    testVoronoiMemory();
    testVoronoiInputs();
    testRelaxation();
    testConvexHull();
}

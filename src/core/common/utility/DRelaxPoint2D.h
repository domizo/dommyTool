#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"
#include "DPointMeanAccumulator.h"
#include "DRandomValueGeneration.h"
#include "modules/point/DPoint/mesh/voro/jc_voronoi.h"

#include <array>
#include <vector>

namespace dommy {
namespace geometry {

class RelaxDiagram final {
public:
    RelaxDiagram() : value() {}
    ~RelaxDiagram() { jcv_diagram_free(&value); }
    RelaxDiagram(const RelaxDiagram&) = delete;
    RelaxDiagram& operator=(const RelaxDiagram&) = delete;
    jcv_diagram value;
};

// Preserve the historical mean of the site plus its boundary vertices, not an
// area-weighted centroid. All graph reads are bounded before output is changed.
inline bool relaxDiagramPoints(const jcv_diagram& diagram, std::vector<jcv_point>& points,
                                std::size_t& work)
{
    const jcv_site* sites = jcv_diagram_get_sites(&diagram);
    if (diagram.numsites < 0 || static_cast<std::size_t>(diagram.numsites) > points.size() ||
        (diagram.numsites && !sites)) return false;
    for (int i = 0; i < diagram.numsites; ++i) {
        const jcv_site& site = sites[i];
        if (site.index < 0 || static_cast<std::size_t>(site.index) >= points.size()) return false;
        numeric::detail::PointMeanAccumulator x, y;
        if (!x.add(site.p.x) || !y.add(site.p.y)) return false;
        std::uint32_t count = 1U;
        for (const jcv_graphedge* edge = site.edges; edge; edge = edge->next) {
            if (!generation::checkedSum(work, 1U, generation::kMaxWorkItemCount, work) ||
                count >= generation::kMaxGeneratedPointCount ||
                !x.add(edge->pos[0].x) || !y.add(edge->pos[0].y)) return false;
            ++count;
        }
        jcv_point result;
        if (!x.mean(count, result.x) || !y.mean(count, result.y) ||
            result.x < 0 || result.x > diagram.width || result.y < 0 || result.y > diagram.height)
            return false;
        points[static_cast<std::size_t>(site.index)] = result;
    }
    return true;
}

inline bool generateRelaxedPoints2D(std::size_t count, std::size_t iterations, int seed,
    float minX, float maxX, float minY, float maxY, std::vector<std::array<double, 3>>& output)
{
    std::size_t bytes = 0U, work = 0U;
    int secondSeed = 0;
    if (!random::offsetNonnegativeSeed(seed, 50U, secondSeed) ||
        !std::isfinite(minX) || !std::isfinite(maxX) || !std::isfinite(minY) || !std::isfinite(maxY) ||
        minX >= maxX || minY >= maxY || count > generation::kMaxGeneratedPointCount ||
        iterations > generation::kMaxWorkItemCount ||
        !generation::checkedProduct(count, sizeof(jcv_point) + sizeof(std::array<double, 3>),
                                    64U * 1024U * 1024U, bytes) ||
        !generation::checkedProduct(count, std::max<std::size_t>(iterations, 1U),
                                    generation::kMaxWorkItemCount, work)) return false;
    std::vector<std::array<double, 3>> candidate(count);
    // Retain the legacy float RNG sequence wherever its distribution range is
    // representable. Exceptionally wide ranges are interpolated in double.
    for (unsigned int axis = 0U; axis < 2U; ++axis) {
        const float minimum = axis == 0U ? minX : minY;
        const float maximum = axis == 0U ? maxX : maxY;
        const float upper = std::nextafter(maximum, std::numeric_limits<float>::max());
        const bool ordinary = static_cast<double>(upper) - minimum <= std::numeric_limits<float>::max();
        std::mt19937 engine(static_cast<unsigned int>(axis == 0U ? seed : secondSeed));
        std::uniform_real_distribution<float> distribution(ordinary ? minimum : 0.0f,
                                                            ordinary ? upper : 1.0f);
        for (std::size_t i = 0U; i < count; ++i) {
            const double sample = distribution(engine);
            const double value = ordinary ? sample :
                static_cast<double>(minimum) * (1.0 - sample) + static_cast<double>(maximum) * sample;
            float stored = 0.0f;
            if (!numeric::checkedFloatCast(value, stored)) return false;
            candidate[i][axis == 0U ? 0U : 2U] = stored;
            candidate[i][1] = 0.0;
        }
    }
    if (count && iterations) {
        const double width = static_cast<double>(maxX) - minX;
        const double height = static_cast<double>(maxY) - minY;
        const double scale = std::max(width, height);
        const double normalizedWidth = width / scale, normalizedHeight = height / scale;
        std::vector<jcv_point> points(count);
        for (std::size_t i = 0U; i < count; ++i)
            points[i] = jcv_point{(candidate[i][0] - minX) / scale, (candidate[i][2] - minY) / scale};
        work = 0U;
        for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
            RelaxDiagram diagram;
            jcv_diagram_generate(static_cast<int>(count), points.data(),
                                 normalizedWidth, normalizedHeight, &diagram.value);
            if (!relaxDiagramPoints(diagram.value, points, work)) return false;
        }
        for (std::size_t i = 0U; i < count; ++i) {
            float x = 0.0f, y = 0.0f;
            if (!numeric::checkedFloatCast(points[i].x * scale + minX, x) ||
                !numeric::checkedFloatCast(points[i].y * scale + minY, y)) return false;
            candidate[i] = {{x, 0.0, y}};
        }
    }
    output.swap(candidate);
    return true;
}

} // namespace geometry
} // namespace dommy

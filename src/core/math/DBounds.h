#pragma once

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace math {

// Bounds reduction only. Arithmetic vertex means belong to the checked numeric
// helpers; accumulating an unused sum here can overflow for finite coordinates.
class Bounds3d {
  public:
    Bounds3d()
        : m_min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max()},
          m_max{std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest()},
          m_count(0) {}

    bool add(double x, double y, double z) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            return false;
        }

        const double values[3] = {x, y, z};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            if (values[axis] < m_min[axis]) {
                m_min[axis] = values[axis];
            }
            if (values[axis] > m_max[axis]) {
                m_max[axis] = values[axis];
            }
        }
        ++m_count;
        return true;
    }

    bool empty() const {
        return m_count == 0;
    }
    std::size_t count() const {
        return m_count;
    }
    double minimum(std::size_t axis) const {
        return m_min[axis];
    }
    double maximum(std::size_t axis) const {
        return m_max[axis];
    }

  private:
    double m_min[3];
    double m_max[3];
    std::size_t m_count;
};

} // namespace math
} // namespace dommy

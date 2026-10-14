#ifndef DOMMY_D_RANDOM_GENERATOR_H
#define DOMMY_D_RANDOM_GENERATOR_H

#include "../common/utility/DRandomValueGeneration.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace dommy {
namespace math {

class RandomGenerator {
public:
    explicit RandomGenerator(const std::uint32_t seed) : engine_(seed) {}

    double uniform(double minimum, double maximum) {
        const double upper = dommy::random::prepareRealBounds(minimum, maximum, false);
        // Retain distribution calls even for equal bounds: skipping one would
        // change the subsequent random sequence of existing scenes.
        std::uniform_real_distribution<double> distribution(
            minimum, upper);
        const double value = distribution(engine_);
        if (!std::isfinite(value))
            throw std::runtime_error("Random generation produced a non-finite value");
        return value;
    }

    std::size_t index(const std::size_t count) {
        if (count == 0) return 0;
        std::uniform_int_distribution<std::size_t> distribution(
            0, count - 1);
        return distribution(engine_);
    }

private:
    std::mt19937 engine_;
};

} // namespace math
} // namespace dommy

#endif

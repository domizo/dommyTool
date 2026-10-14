#include <ImathVec.h>

#include <cmath>

int main() {
    const Imath::V3f source(4.0f, 5.0f, 6.0f);
    const Imath::V3f offset(1.0f, 2.0f, 3.0f);
    Imath::V3f direction = source - offset;

    if (std::fabs(direction.length() - std::sqrt(27.0f)) > 1.0e-5f) {
        return 1;
    }

    direction.normalize();
    direction = -direction;
    direction *= 2.0f;
    return std::isfinite(direction[0]) &&
                   std::isfinite(direction[1]) &&
                   std::isfinite(direction[2])
               ? 0
               : 1;
}

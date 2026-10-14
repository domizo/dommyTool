#pragma once

#include <ai_ray.h>
#include <cstdint>

namespace dommy {
namespace arnold {

// The Maya attribute is Boolean; Arnold sidedness is a per-ray byte mask.
// Preserve the saved schema while applying double-sidedness to every ray type.
inline std::uint8_t sidednessMask(bool doubleSided) noexcept
{
    return doubleSided ? static_cast<std::uint8_t>(AI_RAY_ALL) : std::uint8_t(0);
}

} // namespace arnold
} // namespace dommy

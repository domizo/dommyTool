#pragma once

#include "DArnoldArrayBudget.h"
#include "DArnoldResources.h"

namespace dommy {
namespace arnold {

// Raw key copies have no source-buffer length argument. Validate both arrays
// before mapping, narrowing the key index, or calling the renderer copy API.
inline bool setCheckedVectorMotionKey(AtArray* destination, AtArray* source,
                                      unsigned int key, unsigned int expectedKeys)
{
    ArrayLayout layout;
    if (!destination || !source || key >= expectedKeys ||
        !checkedArrayLayout(AiArrayGetNumElements(source), expectedKeys, layout) ||
        AiArrayGetType(source) != AI_TYPE_VECTOR ||
        AiArrayGetType(destination) != AI_TYPE_VECTOR ||
        AiArrayGetNumKeys(source) != 1U ||
        AiArrayGetNumKeys(destination) != layout.keyCount ||
        AiArrayGetNumElements(destination) != layout.elementCount) {
        return false;
    }
    ArrayMapGuard mapped(source);
    return mapped && AiArraySetKey(destination, static_cast<std::uint8_t>(key), mapped.data());
}

} // namespace arnold
} // namespace dommy

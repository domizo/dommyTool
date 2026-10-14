#pragma once

#include <maya/MTypeId.h>

#define ID_LEGACY_SURFACE_DUPLICATER 0x00335
#define ID_LEGACY_SPACE_DUPLICATER 0x00336

namespace dommy {
namespace legacy {

const MTypeId kSurfaceDuplicaterTypeId(ID_LEGACY_SURFACE_DUPLICATER);
const MTypeId kSpaceDuplicaterTypeId(ID_LEGACY_SPACE_DUPLICATER);

} // namespace legacy
} // namespace dommy

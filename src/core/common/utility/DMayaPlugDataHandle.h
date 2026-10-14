#pragma once

#include <maya/MDataHandle.h>
#include <maya/MPlug.h>
#include "DPlugHandleOwner.h"

namespace dommy {
namespace maya {
using ScopedPlugDataHandle = PlugHandleOwner<MPlug, MDataHandle>;
} // namespace maya
} // namespace dommy

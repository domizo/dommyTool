#pragma once

#include "common/utility/DLegacyDrawGuard.h"
#include "geometry/DFieldIconGeometry.h"

namespace dommy {
namespace maya {

inline void drawFieldIcon(M3dView& view)
{
    ScopedLegacyGL drawing(view);
    ScopedLegacyGLPrimitive lines(GL_LINES);
    geometry::visitFieldIconLines([](const geometry::FieldIconPoint& first,
                                    const geometry::FieldIconPoint& second) {
        glVertex3d(first[0], first[1], first[2]);
        glVertex3d(second[0], second[1], second[2]);
    });
}

} // namespace maya
} // namespace dommy

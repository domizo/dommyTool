#pragma once

#include "DAttributeArrayConversion.h"
#include "DColorMode.h"
#include "DMayaCheckedArray.h"
#include "DStatusOperation.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MColor.h>
#include <maya/MColorArray.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MVector.h>

#include <limits>

namespace dommy {
namespace color {

// Read physical array order, including sparse logical indices. Callers must
// discard the local palette on failure: a failed Maya copy can modify it.
// No geometry/attribute output may be published until this returns success.
inline MStatus readPalette(MDataBlock& data, const MObject& attribute,
                           MColorArray& palette)
{
    MStatus status;
    MArrayDataHandle elements = data.inputArrayValue(attribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int count = elements.elementCount(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    // Existing palette consumers use signed random indices. Apply the shared
    // staging/copy budget before allocation or reading any palette element.
    if (count == 0U || count > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        !attribute::validArrayConversionBudget(count, sizeof(MColor)))
        return MS::kInvalidParameter;

    return maya::buildCheckedArray(count,
        [&](unsigned int index, MColor& color) -> MStatus {
            const MDataHandle handle = elements.inputValue(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            const MVector value = handle.asFloatVector();
            if (!isFiniteTriple(value[0], value[1], value[2]))
                return MS::kInvalidParameter;
            color = MColor(value[0], value[1], value[2]);
            if (index + 1U < count) DOMMY_RETURN_IF_FAILURE(elements.next());
            return MS::kSuccess;
        }, palette);
}

} // namespace color
} // namespace dommy

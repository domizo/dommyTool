#pragma once

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace maya {

// Publishes a complete array of Maya data objects only after every object has
// been constructed successfully by the caller. This centralizes the status
// checks required by MArrayDataBuilder and prevents a failed element from being
// hidden by a later setAllClean() result.
inline MStatus setObjectArrayOutput(MDataBlock& block,
                                    const MObject& outputAttribute,
                                    const MPlug& requestedPlug,
                                    const std::vector<MObject>& objects,
                                    bool markClean = true)
{
    if (objects.size() >
        static_cast<std::size_t>(
            std::numeric_limits<unsigned int>::max())) {
        return MS::kFailure;
    }

    MStatus status;
    MArrayDataHandle output =
        block.outputArrayValue(outputAttribute, &status);
    if (!status) return status;

    MArrayDataBuilder builder(
        &block, outputAttribute,
        static_cast<unsigned int>(objects.size()), &status);
    if (!status) return status;

    for (std::size_t index = 0U; index < objects.size(); ++index) {
        if (objects[index].isNull()) return MS::kFailure;

        MDataHandle element = builder.addElement(
            static_cast<unsigned int>(index), &status);
        if (!status) return status;

        status = element.set(objects[index]);
        if (!status) return status;
    }

    status = output.set(builder);
    if (!status) return status;
    // Multi-output nodes defer all clean marking until every output is set.
    if (!markClean) return MS::kSuccess;
    status = output.setAllClean();
    if (!status) return status;
    return block.setClean(requestedPlug);
}

} // namespace maya
} // namespace dommy

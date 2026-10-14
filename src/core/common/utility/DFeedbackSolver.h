#pragma once

#include "DMayaPluginDataOutput.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MTypeId.h>

#include <cmath>

namespace dommy {
namespace maya {

// Implements the shared one-frame feedback contract used by the DPoint,
// DCurve, and DMesh solver nodes. Every plug-in data object is prepared before
// either output is touched. If the optional previous-state commit fails, the
// requested output is restored to its prior object when rollback succeeds.
template <typename DataType, typename InputReader, typename Copier>
MStatus computeFeedbackSolver(const MPlug& plug,
                              MDataBlock& block,
                              const MObject& inputAttribute,
                              const MObject& currentTimeAttribute,
                              const MObject& previousAttribute,
                              const MObject& nextAttribute,
                              const MObject& outputAttribute,
                              const MTypeId& dataTypeId,
                              InputReader readInput,
                              Copier copyData)
{
    if (plug != previousAttribute && plug != outputAttribute) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle timeHandle = block.inputValue(currentTimeAttribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    const MTime currentTime = timeHandle.asTime();
    if (!std::isfinite(currentTime.value())) {
        return MS::kInvalidParameter;
    }

    const bool outputRequested = plug == outputAttribute;
    const bool updatePrevious = outputRequested && currentTime.value() > 1.0;
    const MObject& sourceAttribute =
        !outputRequested && currentTime.value() <= 1.0
            ? inputAttribute
            : nextAttribute;
    const DataType* source = readInput(sourceAttribute, block);
    if (!source) {
        return MS::kFailure;
    }

    MObject primaryObject;
    status = createPluginDataObject<DataType>(
        dataTypeId,
        [source, copyData](DataType& destination) {
            return copyData(destination, *source);
        },
        primaryObject);
    if (status != MS::kSuccess) {
        return status;
    }

    MObject previousObject;
    if (updatePrevious) {
        status = createPluginDataObject<DataType>(
            dataTypeId,
            [source, copyData](DataType& destination) {
                return copyData(destination, *source);
            },
            previousObject);
        if (status != MS::kSuccess) {
            return status;
        }
    }

    const MObject& primaryAttribute =
        outputRequested ? outputAttribute : previousAttribute;
    MDataHandle primaryOutput = block.outputValue(primaryAttribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MDataHandle previousOutput;
    if (updatePrevious) {
        previousOutput = block.outputValue(previousAttribute, &status);
        if (status != MS::kSuccess) {
            return status;
        }
    }

    const MObject originalPrimary = primaryOutput.data();
    status = primaryOutput.set(primaryObject);
    if (status != MS::kSuccess) {
        return status;
    }

    if (updatePrevious) {
        status = previousOutput.set(previousObject);
        if (status != MS::kSuccess) {
            const MStatus rollbackStatus = primaryOutput.set(originalPrimary);
            return rollbackStatus == MS::kSuccess ? status : rollbackStatus;
        }
    }

    primaryOutput.setClean();
    if (updatePrevious) {
        previousOutput.setClean();
    }
    return block.setClean(plug);
}

} // namespace maya
} // namespace dommy

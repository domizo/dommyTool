#pragma once

#include "DGenerationLimits.h"
#include "DMayaValueValidation.h"
#include <maya/MItGeometry.h>
#include <maya/MPointArray.h>

namespace dommy {
namespace maya {

// Maya component IDs may be sparse. Never use an ID as an offset into the
// compact iterator array; callbacks receive the actual geometry index.
template <typename Visitor> MStatus visitDeformerPoints(MItGeometry& iterator, Visitor visit) {
    MStatus status = iterator.reset();
    if (!status)
        return status;
    std::size_t count = 0U;
    for (;;) {
        const bool done = iterator.isDone(&status);
        if (!status)
            return status;
        if (done)
            return MS::kSuccess;
        if (++count > generation::kMaxGeneratedPointCount)
            return MS::kInvalidParameter;
        const int index = iterator.index(&status);
        if (!status)
            return status;
        if (index < 0)
            return MS::kInvalidParameter;
        const MPoint point = iterator.position(MSpace::kObject, &status);
        if (!status)
            return status;
        if (!isFinitePoint(point))
            return MS::kInvalidParameter;
        status = visit(static_cast<unsigned int>(index), point);
        if (!status)
            return status;
        status = iterator.next();
        if (!status)
            return status;
    }
}

// No point is written while evaluating. A host commit failure is propagated;
// Maya owns setAllPositions and its internal failure/rollback behavior.
template <typename Evaluator>
MStatus editDeformerPoints(MItGeometry& iterator, Evaluator evaluate) {
    MStatus status;
    const int count = iterator.exactCount(&status);
    if (!status)
        return status;
    if (count < 0 || static_cast<std::size_t>(count) > generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    MPointArray candidate;
    status = candidate.setLength(static_cast<unsigned int>(count));
    if (!status)
        return status;
    unsigned int offset = 0U;
    status =
        visitDeformerPoints(iterator, [&](unsigned int index, const MPoint& source) -> MStatus {
            if (offset >= candidate.length())
                return MS::kInvalidParameter;
            MPoint target(source);
            const MStatus evaluated = evaluate(index, source, target);
            if (!evaluated)
                return evaluated;
            if (!isFinitePoint(target))
                return MS::kInvalidParameter;
            return candidate.set(target, offset++);
        });
    if (!status)
        return status;
    if (offset != candidate.length())
        return MS::kInvalidParameter;
    if (candidate.length() == 0U)
        return MS::kSuccess;
    status = iterator.reset();
    if (!status)
        return status;
    return iterator.setAllPositions(candidate, MSpace::kObject);
}

} // namespace maya
} // namespace dommy

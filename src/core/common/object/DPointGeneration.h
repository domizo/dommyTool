//
//  DPointGeneration.h
//  dommyTool
//
//  Focused declarations for point-generation utilities.
//

#pragma once

#include <cstddef>

#include <maya/MPointArray.h>
#include <maya/MStatus.h>

// Replaces the output only after successful generation and checked Maya-array
// staging. C++ allocation/geometry exceptions are caught by the node boundary.
MStatus DGenerateRelaxPoint2D(
    std::size_t pointCount,
    std::size_t maxIteration,
    int seed,
    float minX,
    float maxX,
    float minY,
    float maxY,
    MPointArray &newPoints);

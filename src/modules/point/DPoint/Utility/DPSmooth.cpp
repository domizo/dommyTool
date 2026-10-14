//
//  DPSmooth.cpp
//  dommyTool
//
//  Created by yuzo on 11/13/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSmooth.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointPayloadBudget.h"

#include "DMayaHeaders.h"
#include "DComputeGuard.h"

#include "DPData.h"
#include "dpoint_ids.h"

#include <vector>
#include <cmath>

#include "DHashGrid.h"

const MTypeId DPSmooth::typeId( ID_DP_SMOOTH );

MObject DPSmooth::inDP;

MObject DPSmooth::aRadius;
MObject DPSmooth::aMaxEffectiveRadius;
MObject DPSmooth::aIteration;
MObject DPSmooth::aExpandBound;

MObject DPSmooth::outDP;

void *DPSmooth::creator() {
    return ::dommy::maya::createObject("DPSmooth", [&]() {
        return new DPSmooth();
    });
}

MStatus DPSmooth::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const MPlug inputPlug(thisMObject(), inDP);
    const bool connected = inputPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kFailure;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;
    const std::size_t ptCount = input->numberOfPoints();
    float radius = 0.0f, maxEffectRadius = 0.0f, expandBound = 0.0f;
    std::size_t iterationCount = 0U, workCount = 0U, scalarCount = 0U;
    if (ptCount != 0U) {
        const MDataHandle radiusHandle = data.inputValue(aRadius, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        radius = radiusHandle.asFloat();
        const MDataHandle maximumHandle = data.inputValue(aMaxEffectiveRadius, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        maxEffectRadius = maximumHandle.asFloat();
        const MDataHandle iterationHandle = data.inputValue(aIteration, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int iteration = iterationHandle.asInt();
        const MDataHandle expansionHandle = data.inputValue(aExpandBound, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        expandBound = expansionHandle.asFloat();
        if (!std::isfinite(radius) || radius <= D_EPS ||
            !std::isfinite(maxEffectRadius) || maxEffectRadius < 0.0f ||
            !std::isfinite(expandBound) || expandBound < 0.0f ||
            ptCount > dommy::generation::kMaxGeneratedPointCount ||
            !dommy::generation::checkedCount(iteration,
                dommy::generation::kMaxWorkItemCount, iterationCount) ||
            !dommy::generation::checkedProduct(ptCount, iterationCount,
                dommy::generation::kMaxWorkItemCount, workCount) ||
            !dommy::generation::checkedProduct(ptCount, 3U,
                dommy::generation::kMaxWorkItemCount, scalarCount)) return MS::kInvalidParameter;
    }
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*input, sizeof(DPoint) + 3U * sizeof(DReal), workspace);
    DPointList newPtArray;
    if (ptCount != 0U) {
        newPtArray = *input->getPositionPtrConst();
        std::vector<DReal> hPos(scalarCount);
        for (std::size_t i = 0U; i < ptCount; ++i)
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                hPos[3U * i + axis] = newPtArray[i][axis];
        if (iterationCount > 0U) {
            DHashGrid nnGrid(3);
            MBoundingBox bbox;
            input->getBBox(bbox);
            const MPoint minimum = bbox.min(), maximum = bbox.max();
            const DTools::Vec3 lower(minimum.x - expandBound, minimum.y - expandBound, minimum.z - expandBound);
            const DTools::Vec3 upper(maximum.x + expandBound, maximum.y + expandBound, maximum.z + expandBound);
            const uint particleCount = static_cast<uint>(ptCount);
            DReal h = maxEffectRadius;
            if (h < D_EPS) h = radius;
            if (!nnGrid.Setup(lower, upper, h, static_cast<int>(particleCount),
                    [&workspace](std::size_t points, std::size_t cells) -> bool {
                        // Grid index arrays, the sort temporary, and one
                        // query's neighbor list coexist with node staging.
                        workspace.add(points, 2U * sizeof(uint) + sizeof(DHashSort) + sizeof(DNeigh));
                        workspace.add(cells, 2U * sizeof(uint));
                        return true;
                    })) return MS::kInvalidParameter;
            for (std::size_t iteration = 0U; iteration < iterationCount; ++iteration) {
                if (!nnGrid.SetObjectToCell(hPos.data(), particleCount)) return MS::kInvalidParameter;
                for (uint i = 0U; i < particleCount; ++i) {
                    const DTools::Vec3 position(hPos[3U * i], hPos[3U * i + 1U], hPos[3U * i + 2U]);
                    std::vector<DNeigh> neighbors;
                    if (!nnGrid.GetNN(position, hPos.data(), particleCount, neighbors, radius))
                        return MS::kInvalidParameter;
                    if (neighbors.size() < 2U) continue;
                    DVec3Data velocity(0.0f, 0.0f, 0.0f);
                    uint count = 0U;
                    // Preserve sequential updates within each iteration and
                    // the previous-iteration positions used by the hash grid.
                    for (const auto& neighbor : neighbors) {
                        const int j = neighbor.Idx;
                        if (j < 0 || j == static_cast<int>(i)) continue;
                        const DReal distance = (newPtArray[i] - newPtArray[j]).length();
                        if (!std::isfinite(distance) || distance <= D_EPS || distance > radius) continue;
                        DVec3Data direction = newPtArray[i] - newPtArray[j];
                        direction.normalize();
                        velocity += direction * (1.0 - distance / radius);
                        ++count;
                    }
                    if (count > 0U) newPtArray[i] += velocity / static_cast<DReal>(count);
                }
                if (iteration + 1U < iterationCount)
                    for (std::size_t i = 0U; i < ptCount; ++i)
                        for (unsigned int axis = 0U; axis < 3U; ++axis)
                            hPos[3U * i + axis] = newPtArray[i][axis];
            }
        }
    }
    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, &newPtArray](DPData& output) -> MStatus {
            return output.copyRepeatedFrom(*input, 1U,
                [&newPtArray](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                    point = newPtArray[index];
                    return MS::kSuccess;
                });
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPSmooth")

MStatus DPSmooth::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 1.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aMaxEffectiveRadius = nAttr.create("MaxEffectiveRadius", "mer", MFnNumericData::kFloat, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxEffectiveRadius));

    aIteration = nAttr.create("Iteration", "itr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIteration));

    aExpandBound = nAttr.create("ExpandBound", "exbnd", MFnNumericData::kFloat, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExpandBound));

    // Output data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxEffectiveRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIteration, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExpandBound, outDP));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPSmooth", "initialize", std::current_exception());
}

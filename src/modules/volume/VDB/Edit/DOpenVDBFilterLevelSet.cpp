// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Filter_Level_Set.cc

//
//  DOpenVDBFilterLevelSet.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/20.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBFilterLevelSet.h"
#include "DOpenVDBFilterPolicy.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "../Common/DVDBComputeGuard.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MGlobal.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include <openvdb/tools/LevelSetFilter.h>

#include <vector>
#include <string>

#include "dvdb_ids.h"

const MTypeId DOpenVDBFilterLevelSet::typeId(ID_VDB_DOPENVDB_FILTER_LEVEL_SET);

MObject DOpenVDBFilterLevelSet::aVdbInput;
MObject DOpenVDBFilterLevelSet::aVdbSelectedGridNames;

MObject DOpenVDBFilterLevelSet::isMask;
MObject DOpenVDBFilterLevelSet::maskName;
MObject DOpenVDBFilterLevelSet::maskVdbInput;

MObject DOpenVDBFilterLevelSet::aVdbOutput;

MObject DOpenVDBFilterLevelSet::filterType;
MObject DOpenVDBFilterLevelSet::iterations;
MObject DOpenVDBFilterLevelSet::accuracyType;
MObject DOpenVDBFilterLevelSet::isInvert;
MObject DOpenVDBFilterLevelSet::minMask;
MObject DOpenVDBFilterLevelSet::maxMask;
MObject DOpenVDBFilterLevelSet::isWorldSpaceUnits;
MObject DOpenVDBFilterLevelSet::stencilWidth;
MObject DOpenVDBFilterLevelSet::stencilWidthWorld;
MObject DOpenVDBFilterLevelSet::halfWidth;
MObject DOpenVDBFilterLevelSet::halfWidthWorld;
MObject DOpenVDBFilterLevelSet::voxelOffset;

void* DOpenVDBFilterLevelSet::creator() {
    return ::dommy::maya::createObject("DOpenVDBFilterLevelSet", [&]() {
        return new DOpenVDBFilterLevelSet();
    });
}

namespace {

// Add new items to the *end* of this list, and update NUM_FILTER_TYPES.
enum {
    FILTER_TYPE_NONE = -1,
    FILTER_TYPE_RENORMALIZE = 0,
    FILTER_TYPE_MEAN_VALUE,
    FILTER_TYPE_MEDIAN_VALUE,
    FILTER_TYPE_MEAN_CURVATURE,
    FILTER_TYPE_LAPLACIAN_FLOW,
    FILTER_TYPE_DILATE,
    FILTER_TYPE_ERODE,
    FILTER_TYPE_OPEN,
    FILTER_TYPE_CLOSE,
    FILTER_TYPE_TRACK,
    FILTER_TYPE_GAUSSIAN,
    FILTER_TYPE_RESIZE
};
typedef int FilterType;

enum { NUM_FILTER_TYPES = FILTER_TYPE_RESIZE + 1 };

std::string filterTypeToMenuName(FilterType filter) {
    std::string ret;
    switch (filter) {
    case FILTER_TYPE_NONE:
        ret = "None";
        break;
    case FILTER_TYPE_RENORMALIZE:
        ret = "Renormalize";
        break;
    case FILTER_TYPE_RESIZE:
        ret = "Resize Narrow Band";
        break;
    case FILTER_TYPE_MEAN_VALUE:
        ret = "Mean Value";
        break;
    case FILTER_TYPE_GAUSSIAN:
        ret = "Gaussian";
        break;
    case FILTER_TYPE_MEDIAN_VALUE:
        ret = "Median Value";
        break;
    case FILTER_TYPE_MEAN_CURVATURE:
        ret = "Mean Curvature Flow";
        break;
    case FILTER_TYPE_LAPLACIAN_FLOW:
        ret = "Laplacian Flow";
        break;
    case FILTER_TYPE_DILATE:
        ret = "Dilate";
        break;
    case FILTER_TYPE_ERODE:
        ret = "Erode";
        break;
    case FILTER_TYPE_OPEN:
        ret = "Open";
        break;
    case FILTER_TYPE_CLOSE:
        ret = "Close";
        break;
    case FILTER_TYPE_TRACK:
        ret = "Track Narrow Band";
        break;
    }
    return ret;
}
// Add new items to the *end* of this list, and update NUM_ACCURACY_TYPES.
enum {
    ACCURACY_UPWIND_FIRST = 0,
    ACCURACY_UPWIND_SECOND,
    ACCURACY_UPWIND_THIRD,
    ACCURACY_WENO,
    ACCURACY_HJ_WENO
};
typedef int Accuracy;

enum { NUM_ACCURACY_TYPES = ACCURACY_HJ_WENO + 1 };

std::string accuracyToMenuName(Accuracy ac) {
    std::string ret;
    switch (ac) {
    case ACCURACY_UPWIND_FIRST:
        ret = "First-order upwinding";
        break;
    case ACCURACY_UPWIND_SECOND:
        ret = "Second-order upwinding";
        break;
    case ACCURACY_UPWIND_THIRD:
        ret = "Third-order upwinding";
        break;
    case ACCURACY_WENO:
        ret = "Fifth-order WENO";
        break;
    case ACCURACY_HJ_WENO:
        ret = "Fifth-order HJ-WENO";
        break;
    }
    return ret;
}

template <typename GridT>
bool preflightLevelSetFilter(const openvdb::GridBase& grid,
                             const dommy::vdb::LevelSetFilterSettings& settings,
                             const openvdb::FloatGrid* mask, dommy::vdb::FilterPlan& plan) {
    if (!grid.isType<GridT>())
        return false;
    const GridT& source = static_cast<const GridT&>(grid);
    return dommy::vdb::validLevelSetScale(source) &&
           dommy::vdb::makeLevelSetFilterPlan(settings, source.voxelSize()[0], plan) &&
           dommy::vdb::validFilterGrid(source, plan) &&
           (!mask || dommy::vdb::validFilterMaskDomain(source, *mask, plan));
}
template <typename GridT>
bool applyLevelSetFilter(openvdb::GridBase& grid,
                         const dommy::vdb::LevelSetFilterSettings& settings,
                         const dommy::vdb::FilterPlan& plan, const openvdb::FloatGrid* mask) {
    if (!grid.isType<GridT>())
        return false;
    GridT& output = static_cast<GridT&>(grid);
    typedef openvdb::tools::LevelSetFilter<GridT, openvdb::FloatGrid> FilterT;
    FilterT filter(output);
    filter.setTemporalScheme(openvdb::math::TVD_RK1);
    switch (settings.accuracy) {
    case ACCURACY_UPWIND_FIRST:
        filter.setSpatialScheme(openvdb::math::FIRST_BIAS);
        break;
    case ACCURACY_UPWIND_SECOND:
        filter.setSpatialScheme(openvdb::math::SECOND_BIAS);
        break;
    case ACCURACY_UPWIND_THIRD:
        filter.setSpatialScheme(openvdb::math::THIRD_BIAS);
        break;
    case ACCURACY_WENO:
        filter.setSpatialScheme(openvdb::math::WENO5_BIAS);
        break;
    case ACCURACY_HJ_WENO:
        filter.setSpatialScheme(openvdb::math::HJWENO5_BIAS);
        break;
    default:
        return false;
    }
    if (mask) {
        filter.setMaskRange(settings.maskMin, settings.maskMax);
        filter.invertMask(settings.invertMask);
    }
    if (output.background() < 3.0L * output.voxelSize()[0])
        MGlobal::displayWarning("The level-set narrow band is less than three voxels wide.");
    // All changes remain private until the complete batch succeeds. No tracker
    // state restoration is required: this tracker is local to this operation.
    switch (settings.operation) {
    case FILTER_TYPE_NONE:
        break;
    case FILTER_TYPE_RENORMALIZE:
        filter.setNormCount(settings.iterations);
        filter.setTemporalScheme(openvdb::math::TVD_RK3);
        filter.normalize();
        filter.prune();
        break;
    case FILTER_TYPE_RESIZE:
        filter.setNormCount(1);
        // resize returns "width changed", not success/failure.
        filter.resize(plan.halfWidth);
        break;
    case FILTER_TYPE_DILATE:
        filter.offset(-plan.distance, mask);
        break;
    case FILTER_TYPE_ERODE:
        filter.offset(plan.distance, mask);
        break;
    case FILTER_TYPE_OPEN:
        filter.offset(plan.distance, mask);
        filter.offset(-plan.distance, mask);
        break;
    case FILTER_TYPE_CLOSE:
        filter.offset(-plan.distance, mask);
        filter.offset(plan.distance, mask);
        break;
    case FILTER_TYPE_MEAN_VALUE:
    case FILTER_TYPE_GAUSSIAN:
    case FILTER_TYPE_MEDIAN_VALUE:
    case FILTER_TYPE_MEAN_CURVATURE:
    case FILTER_TYPE_LAPLACIAN_FLOW:
    case FILTER_TYPE_TRACK:
        for (int i = 0; i < settings.iterations; ++i) {
            switch (settings.operation) {
            case FILTER_TYPE_MEAN_VALUE:
                filter.mean(plan.radius, mask);
                break;
            case FILTER_TYPE_GAUSSIAN:
                filter.gaussian(plan.radius, mask);
                break;
            case FILTER_TYPE_MEDIAN_VALUE:
                filter.median(plan.radius, mask);
                break;
            case FILTER_TYPE_MEAN_CURVATURE:
                filter.meanCurvature(mask);
                break;
            case FILTER_TYPE_LAPLACIAN_FLOW:
                filter.laplacian(mask);
                break;
            case FILTER_TYPE_TRACK:
                filter.track();
                break;
            }
            if (!dommy::vdb::finiteFilterValues(output) ||
                output.activeVoxelCount() > dommy::generation::kMaxWorkItemCount)
                return false;
        }
        break;
    default:
        return false;
    }
    return dommy::vdb::finiteFilterValues(output) &&
           output.activeVoxelCount() <= dommy::generation::kMaxWorkItemCount;
}
} // namespace

MStatus DOpenVDBFilterLevelSet::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;

    MStatus stat;
    const MDataHandle inputHandle = data.inputValue(aVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DOpenVDBData::id, inputVdb));
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;

    dommy::vdb::LevelSetFilterSettings settings;
    const MDataHandle iterationsHandle = data.inputValue(iterations, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.iterations = iterationsHandle.asInt();
    const MDataHandle halfWidthHandle = data.inputValue(halfWidth, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.halfWidth = halfWidthHandle.asInt();
    const MDataHandle halfWidthWorldHandle = data.inputValue(halfWidthWorld, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.worldHalfWidth = halfWidthWorldHandle.asFloat();
    const MDataHandle stencilWidthHandle = data.inputValue(stencilWidth, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.radius = stencilWidthHandle.asInt();
    const MDataHandle stencilWidthWorldHandle = data.inputValue(stencilWidthWorld, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.worldRadius = stencilWidthWorldHandle.asFloat();
    const MDataHandle voxelOffsetHandle = data.inputValue(voxelOffset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.offset = voxelOffsetHandle.asFloat();
    const MDataHandle minMaskHandle = data.inputValue(minMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.maskMin = minMaskHandle.asFloat();
    const MDataHandle maxMaskHandle = data.inputValue(maxMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.maskMax = maxMaskHandle.asFloat();
    const MDataHandle isInvertHandle = data.inputValue(isInvert, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.invertMask = isInvertHandle.asBool();
    const MDataHandle isWorldSpaceUnitsHandle = data.inputValue(isWorldSpaceUnits, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.worldUnits = isWorldSpaceUnitsHandle.asBool();
    const MDataHandle filterTypeHandle = data.inputValue(filterType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.operation = filterTypeHandle.asInt();
    const MDataHandle accuracyTypeHandle = data.inputValue(accuracyType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.accuracy = accuracyTypeHandle.asInt();
    const MDataHandle enabledHandle = data.inputValue(isMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool masked = enabledHandle.asBool();
    if (!dommy::vdb::validLevelSetFilterSettings(settings, masked))
        return MS::kInvalidParameter;
    const openvdb::FloatGrid* mask = nullptr;
    if (masked) {
        const MDataHandle maskHandle = data.inputValue(maskVdbInput, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const DOpenVDBData* maskVdb = nullptr;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(maskHandle, DOpenVDBData::id, maskVdb));
        if (!maskVdb || !maskVdb->hasValidData())
            return MS::kInvalidParameter;
        const MDataHandle nameHandle = data.inputValue(maskName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        GridCPtrVec masks;
        // Preserve exact-name/all selection and the last valid FloatGrid rule.
        getGrids(masks, *maskVdb, nameHandle.asString().asChar());
        for (const auto& candidate : masks) {
            if (candidate->isType<openvdb::FloatGrid>())
                mask = static_cast<const openvdb::FloatGrid*>(candidate.get());
            else
                MGlobal::displayWarning(
                    "The mask grid has to be a FloatGrid; unsupported mask skipped.");
        }
        if (!mask || mask->activeVoxelCount() > dommy::generation::kMaxWorkItemCount ||
            !dommy::vdb::finiteFilterValues(*mask))
            return MS::kInvalidParameter;
    }

    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string selection = selectionHandle.asString().asChar();
    GridCPtrVec selected;
    if (!getSelectedGrids(selected, selection, *inputVdb))
        MGlobal::displayWarning("No grids are selected.");

    std::size_t work = 0;
    std::vector<dommy::vdb::FilterPlan> plans(selected.size());
    for (std::size_t i = 0; i < selected.size(); ++i) {
        const auto& grid = selected[i];
        if (grid->getGridClass() != openvdb::GRID_LEVEL_SET)
            continue;
        const bool isFloat = grid->isType<openvdb::FloatGrid>();
        const bool isDouble = grid->isType<openvdb::DoubleGrid>();
        if (!isFloat && !isDouble)
            continue;
        if (!(isFloat ? preflightLevelSetFilter<openvdb::FloatGrid>(*grid, settings, mask, plans[i])
                      : preflightLevelSetFilter<openvdb::DoubleGrid>(*grid, settings, mask,
                                                                     plans[i])) ||
            !dommy::vdb::addFilterWork(grid->activeVoxelCount(), plans[i], work))
            return MS::kInvalidParameter;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    getSelectedGrids(selected, selection, *inputVdb, *outputVdb);
    for (std::size_t i = 0; i < selected.size(); ++i) {
        const auto& source = selected[i];
        const bool isFloat = source->isType<openvdb::FloatGrid>();
        const bool isDouble = source->isType<openvdb::DoubleGrid>();
        if (source->getGridClass() != openvdb::GRID_LEVEL_SET || (!isFloat && !isDouble)) {
            MGlobal::displayWarning(("Passed through '" + source->getName() +
                                     "', not a supported floating-point level set.")
                                        .c_str());
            outputVdb->insert(*source);
            continue;
        }
        auto copy = source->deepCopyGrid();
        if (!copy ||
            !(isFloat ? applyLevelSetFilter<openvdb::FloatGrid>(*copy, settings, plans[i], mask)
                      : applyLevelSetFilter<openvdb::DoubleGrid>(*copy, settings, plans[i], mask)))
            return MS::kFailure;
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFilterLevelSet")
MStatus DOpenVDBFilterLevelSet::initialize() try {
    MStatus stat;

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;

    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    isMask = nAttr.create("isMask", "ism", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isMask));

    const MObject maskNameDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    maskName = tAttr.create("maskName", "mna", MFnData::kString, maskNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskName));

    MFnEnumAttribute eAttr;

    filterType = eAttr.create("filterType", "ft", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = FILTER_TYPE_NONE; i < NUM_FILTER_TYPES; i++) {
        std::string fName = filterTypeToMenuName(i);
        DOMMY_RETURN_IF_FAILURE(eAttr.addField(fName.c_str(), i));
    }
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(filterType));

    accuracyType = eAttr.create("accuracyType", "act", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = ACCURACY_UPWIND_FIRST; i < NUM_ACCURACY_TYPES; i++) {
        std::string fName = accuracyToMenuName(i);
        DOMMY_RETURN_IF_FAILURE(eAttr.addField(fName.c_str(), i));
    }
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(accuracyType));

    iterations = nAttr.create("iterations", "itr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(iterations));

    isWorldSpaceUnits =
        nAttr.create("isWorldSpaceUnits", "iws", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isWorldSpaceUnits));

    stencilWidth = nAttr.create("stencilWidth", "stw", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(stencilWidth));

    stencilWidthWorld =
        nAttr.create("stencilWidthWorld", "stww", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(stencilWidthWorld));

    halfWidth = nAttr.create("halfWidth", "hw", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(halfWidth));

    halfWidthWorld = nAttr.create("halfWidthWorld", "hww", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(halfWidthWorld));

    voxelOffset = nAttr.create("voxelOffset", "vof", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(voxelOffset));

    isInvert = nAttr.create("isInvert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isInvert));

    minMask = nAttr.create("minMask", "minm", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(minMask));

    maxMask = nAttr.create("maxMask", "maxm", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maxMask));

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    maskVdbInput =
        tAttr.create("maskVdbInput", "mvdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskVdbInput));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(isMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(filterType, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(iterations, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(accuracyType, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isInvert, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(minMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maxMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isWorldSpaceUnits, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(stencilWidth, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(stencilWidthWorld, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(halfWidth, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(halfWidthWorld, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(voxelOffset, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFilterLevelSet", "initialize",
                                                  std::current_exception());
}

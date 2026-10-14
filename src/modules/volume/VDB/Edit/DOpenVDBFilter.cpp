//
//  DOpenVDBFilter.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBFilter.h"
#include "DOpenVDBFilterPolicy.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "../Common/DVDBComputeGuard.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include <openvdb/tools/Filter.h>
#include <string>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnPluginData.h>
#include <maya/MGlobal.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>

#include "dvdb_ids.h"

MObject DOpenVDBFilter::aVdbOutput;
MObject DOpenVDBFilter::aVdbInput;
MObject DOpenVDBFilter::aVdbSelectedGridNames;
MObject DOpenVDBFilter::aFilter;
MObject DOpenVDBFilter::aRadius;
MObject DOpenVDBFilter::aOffset;
MObject DOpenVDBFilter::aIterations;

const MTypeId DOpenVDBFilter::typeId(ID_VDB_DOPENVDB_FILTER);

void* DOpenVDBFilter::creator() {
    return ::dommy::maya::createObject("DOpenVDBFilter", [&]() {
        return new DOpenVDBFilter();
    });
}

namespace {
template <typename GridT>
bool preflightFilter(const openvdb::GridBase& grid, const dommy::vdb::FilterPlan& plan) {
    return grid.isType<GridT>() &&
           dommy::vdb::validFilterGrid(static_cast<const GridT&>(grid), plan);
}
template <typename GridT>
bool applyFilter(openvdb::GridBase& grid, const dommy::vdb::FilterSettings& settings) {
    if (!grid.isType<GridT>())
        return false;
    GridT& output = static_cast<GridT&>(grid);
    openvdb::tools::Filter<GridT> filter(output);
    switch (settings.operation) {
    case 0:
        filter.mean(settings.radius, settings.iterations);
        break;
    case 1:
        filter.gaussian(settings.radius, settings.iterations);
        break;
    case 2:
        filter.median(settings.radius, settings.iterations);
        break;
    case 3:
        filter.offset(typename GridT::ValueType(settings.offset));
        break;
    default:
        return false;
    }
    return dommy::vdb::finiteFilterValues(output);
}
} // namespace

MStatus DOpenVDBFilter::compute(const MPlug& plug, MDataBlock& data) try {
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

    dommy::vdb::FilterSettings settings;
    const MDataHandle aFilterHandle = data.inputValue(aFilter, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.operation = aFilterHandle.asInt();
    const MDataHandle aRadiusHandle = data.inputValue(aRadius, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.radius = aRadiusHandle.asInt();
    const MDataHandle aIterationsHandle = data.inputValue(aIterations, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.iterations = aIterationsHandle.asInt();
    const MDataHandle aOffsetHandle = data.inputValue(aOffset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.offset = aOffsetHandle.asFloat();
    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string selection = selectionHandle.asString().asChar();
    GridCPtrVec selected;
    if (!getSelectedGrids(selected, selection, *inputVdb))
        MGlobal::displayWarning("No grids are selected.");

    dommy::vdb::FilterPlan plan;
    if (!dommy::vdb::makeFilterPlan(settings, plan))
        return MS::kInvalidParameter;
    std::size_t work = 0;
    // Validate the whole selected batch before creating any modifiable grid.
    for (const auto& grid : selected) {
        const bool isFloat = grid->isType<openvdb::FloatGrid>();
        const bool isDouble = grid->isType<openvdb::DoubleGrid>();
        if (!isFloat && !isDouble)
            continue;
        if (!dommy::vdb::addFilterWork(grid->activeVoxelCount(), plan, work) ||
            !(isFloat ? preflightFilter<openvdb::FloatGrid>(*grid, plan)
                      : preflightFilter<openvdb::DoubleGrid>(*grid, plan)))
            return MS::kInvalidParameter;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    getSelectedGrids(selected, selection, *inputVdb, *outputVdb);
    for (const auto& source : selected) {
        const bool isFloat = source->isType<openvdb::FloatGrid>();
        const bool isDouble = source->isType<openvdb::DoubleGrid>();
        if (!isFloat && !isDouble) {
            MGlobal::displayWarning(
                ("Passed through '" + source->getName() + "', unsupported type.").c_str());
            outputVdb->insert(*source);
            continue;
        }
        auto copy = source->deepCopyGrid();
        if (!copy || !(isFloat ? applyFilter<openvdb::FloatGrid>(*copy, settings)
                               : applyFilter<openvdb::DoubleGrid>(*copy, settings)))
            return MS::kFailure;
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFilter")
MStatus DOpenVDBFilter::initialize() try {
    MStatus stat;

    MFnTypedAttribute tAttr;
    MFnStringData strData;

    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aVdbSelectedGridNames);
    DOMMY_RETURN_IF_FAILURE(stat);

    MFnEnumAttribute eAttr;
    aFilter = eAttr.create("Filter", "filter", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Mean", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Gauss", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Median", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Offset", 3));

    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    stat = addAttribute(aFilter);
    DOMMY_RETURN_IF_FAILURE(stat);

    MFnNumericAttribute nAttr;

    aRadius = nAttr.create("FilterVoxelRadius", "r", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));

    stat = addAttribute(aRadius);
    DOMMY_RETURN_IF_FAILURE(stat);

    aIterations = nAttr.create("Iterations", "it", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));

    stat = addAttribute(aIterations);
    DOMMY_RETURN_IF_FAILURE(stat);

    aOffset = nAttr.create("Offset", "o", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));

    stat = addAttribute(aOffset);
    DOMMY_RETURN_IF_FAILURE(stat);

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    stat = addAttribute(aVdbInput);
    DOMMY_RETURN_IF_FAILURE(stat);

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    stat = addAttribute(aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aVdbInput, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aFilter, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aVdbSelectedGridNames, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aRadius, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aOffset, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aIterations, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFilter", "initialize",
                                                  std::current_exception());
}

//
//  DOpenVDBPrune.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/15.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBPrune.h"
#include "DOpenVDBValuePolicy.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "../Common/DVDBComputeGuard.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "DMayaHeaders.h"

#include <openvdb/tools/Prune.h>

#include <string>
#include <vector>

#include "dvdb_ids.h"

namespace {
struct PrunePreflight : dommy::vdb::ValuePreflight {
    short mode;
    PrunePreflight(std::size_t& work, short value) : ValuePreflight(work, 2), mode(value) {}
    template <typename GridT> void operator()(const GridT& grid) {
        ValuePreflight::operator()(grid);
        if (mode == 0 && valid) {
            dommy::vdb::PruneRange<typename GridT::ValueType> range;
            valid = range.add(grid.background());
            for (auto it = grid.cbeginValueAll(); valid && it; ++it)
                valid = range.add(*it);
        }
        // OpenVDB 5.2 negates the background before checking its sign.
        if (mode == 2)
            valid = valid &&
                    dommy::vdb::ValueComponents<typename GridT::ValueType>::negatable(
                        grid.background()) &&
                    !openvdb::math::isNegative(grid.background());
    }
};
struct PruneOp {
    short mode;
    double tolerance;
    PruneOp(short value, double tol) : mode(value), tolerance(tol) {}
    template <typename GridT> void operator()(GridT& grid) const {
        if (!dommy::vdb::validPruneSettings(mode, tolerance))
            throw std::invalid_argument("Invalid VDB prune settings");
        using ValueT = typename GridT::ValueType;
        if (mode == 0) {
            ValueT converted;
            if (!dommy::vdb::ValueComponents<ValueT>::tolerance(tolerance, converted))
                throw std::overflow_error("VDB prune tolerance is out of range");
            openvdb::tools::prune(grid.tree(), converted);
        } else if (mode == 1) {
            openvdb::tools::pruneInactive(grid.tree());
        } else {
            openvdb::tools::pruneLevelSet(grid.tree());
        }
    }
};
} // namespace

const MTypeId DOpenVDBPrune::typeId(ID_VDB_DOPENVDB_PRUNE);

MObject DOpenVDBPrune::aVdbInput;
MObject DOpenVDBPrune::aVdbSelectedGridNames;

MObject DOpenVDBPrune::aMode;
MObject DOpenVDBPrune::aTolerance;

MObject DOpenVDBPrune::aVdbOutput;

void* DOpenVDBPrune::creator() {
    return ::dommy::maya::createObject("DOpenVDBPrune", [&]() {
        return new DOpenVDBPrune();
    });
}

MStatus DOpenVDBPrune::compute(const MPlug& plug, MDataBlock& data) try {
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
    const MDataHandle modeHandle = data.inputValue(aMode, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short mode = modeHandle.asShort();
    const MDataHandle toleranceHandle = data.inputValue(aTolerance, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double tolerance = toleranceHandle.asDouble();

    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string selection = selectionHandle.asString().asChar();

    if (!dommy::vdb::validPruneSettings(mode, tolerance))
        return MS::kInvalidParameter;
    GridCPtrVec selected;
    if (!getSelectedGrids(selected, selection, *inputVdb)) {
        MGlobal::displayWarning("No grids are selected.");
        return MS::kFailure;
    }
    std::size_t work = 0;
    std::vector<bool> supported;
    for (const auto& source : selected) {
        PrunePreflight preflight(work, mode);
        supported.push_back(processVdbTopologyGrid(getVdbGridType(*source), *source, preflight));
        if (!preflight.valid)
            return MS::kInvalidParameter;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    getSelectedGrids(selected, selection, *inputVdb, *outputVdb);
    PruneOp prune(mode, tolerance);
    for (std::size_t index = 0; index < selected.size(); ++index) {
        const auto& source = selected[index];
        if (!supported[index]) {
            MGlobal::displayWarning(
                ("Passed through '" + source->getName() + "', unsupported prune type.").c_str());
            outputVdb->insert(*source);
            continue;
        }
        auto copy = source->deepCopyGrid();
        if (!copy || !processVdbTopologyGrid(getVdbGridType(*source), *copy, prune))
            return MS::kFailure;
        std::size_t outputWork = 0;
        dommy::vdb::ValuePreflight result(outputWork, 1);
        if (!processVdbTopologyGrid(getVdbGridType(*source), *copy, result) || !result.valid)
            return MS::kFailure;
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBPrune")

MStatus DOpenVDBPrune::initialize() try {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;
    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    MFnEnumAttribute minAttr;
    aMode = minAttr.create("Mode", "md", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Value", 0));
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Inactive", 1));
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Level Set", 2));
    DOMMY_RETURN_IF_FAILURE(minAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));

    aTolerance = nAttr.create("Tolerance", "tol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMode, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBPrune", "initialize",
                                                  std::current_exception());
}

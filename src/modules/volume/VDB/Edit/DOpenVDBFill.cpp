//
//  DOpenVDBFill.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBFill.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DOpenVDBTransportPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "../Common/DOpenVDBLimits.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"

#include <cmath>
#include <string>
#include <vector>

#include "DMayaHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include "DNodeOperation.h"
#include "../Common/DVDBComputeGuard.h"

const MTypeId DOpenVDBFill::typeId(ID_VDB_DOPENVDB_FILL);

MObject DOpenVDBFill::aVdbInput;
MObject DOpenVDBFill::aVdbSelectedGridNames;

MObject DOpenVDBFill::aBounds;
MObject DOpenVDBFill::aMinCoord;
MObject DOpenVDBFill::aMaxCoord;
MObject DOpenVDBFill::aMinWorldCoord;
MObject DOpenVDBFill::aMaxWorldCoord;

MObject DOpenVDBFill::aInMesh;

MObject DOpenVDBFill::aValue;
MObject DOpenVDBFill::aActive;
MObject DOpenVDBFill::aSparse;

MObject DOpenVDBFill::aVdbOutput;

void* DOpenVDBFill::creator() {
    return ::dommy::maya::createObject("DOpenVDBFill", [&]() {
        return new DOpenVDBFill();
    });
}

namespace {
struct FillSettings {
    short bounds = 0;
    bool active = true, sparse = true;
    openvdb::Vec3R value, minimum, maximum;
};

bool validFillBounds(const openvdb::Vec3R& minimum, const openvdb::Vec3R& maximum) {
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
            minimum[axis] > maximum[axis])
            return false;
    return true;
}

MStatus readFillSettings(MDataBlock& data, const MObject& node, FillSettings& settings) {
    using Node = DOpenVDBFill;
    MStatus status;
    const MDataHandle mode = data.inputValue(Node::aBounds, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.bounds = mode.asShort();
    if (settings.bounds < 0 || settings.bounds > 2)
        return MS::kInvalidParameter;
    const MDataHandle value = data.inputValue(Node::aValue, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float3& components = value.asFloat3();
    settings.value = openvdb::Vec3R(components[0], components[1], components[2]);
    const MDataHandle active = data.inputValue(Node::aActive, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.active = active.asBool();
    const MDataHandle sparse = data.inputValue(Node::aSparse, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.sparse = sparse.asBool();
    if (settings.bounds == 0) {
        const MDataHandle minimum = data.inputValue(Node::aMinCoord, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int3& low = minimum.asInt3();
        settings.minimum = openvdb::Vec3R(low[0], low[1], low[2]);
        const MDataHandle maximum = data.inputValue(Node::aMaxCoord, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int3& high = maximum.asInt3();
        settings.maximum = openvdb::Vec3R(high[0], high[1], high[2]);
    } else if (settings.bounds == 1) {
        const MDataHandle minimum = data.inputValue(Node::aMinWorldCoord, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float3& low = minimum.asFloat3();
        settings.minimum = openvdb::Vec3R(low[0], low[1], low[2]);
        const MDataHandle maximum = data.inputValue(Node::aMaxWorldCoord, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float3& high = maximum.asFloat3();
        settings.maximum = openvdb::Vec3R(high[0], high[1], high[2]);
    } else {
        const bool connected = MPlug(node, Node::aInMesh).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected)
            return MS::kInvalidParameter;
        MObject mesh;
        MBoundingBox bounds;
        DOMMY_RETURN_IF_FAILURE(getMeshInformation(Node::aInMesh, data, bounds, mesh));
        const MPoint low = bounds.min(), high = bounds.max();
        settings.minimum = openvdb::Vec3R(low.x, low.y, low.z);
        settings.maximum = openvdb::Vec3R(high.x, high.y, high.z);
    }
    return validFillBounds(settings.minimum, settings.maximum) ? MS::kSuccess
                                                               : MS::kInvalidParameter;
}

// Preserve scalar-first-component, integer truncation, and nonzero-to-bool
// semantics while checking every component that the destination actually uses.
template <typename T> struct FillValue {
    static bool convert(const openvdb::Vec3R& value, T& result) {
        return dommy::numeric::checkedNumericCast(value[0], result);
    }
};
template <> struct FillValue<bool> {
    static bool convert(const openvdb::Vec3R& value, bool& result) {
        if (!std::isfinite(value[0]))
            return false;
        result = value[0] != 0;
        return true;
    }
};
template <typename T> struct FillValue<openvdb::math::Vec3<T>> {
    static bool convert(const openvdb::Vec3R& value, openvdb::math::Vec3<T>& result) {
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::numeric::checkedNumericCast(value[axis], result[axis]))
                return false;
        return true;
    }
};

using FillBudget = dommy::vdb::TreeEditBudget;
using dommy::vdb::chargeTreeNode;
using dommy::vdb::treeBlockCount;
template <typename T> using FillNodeBudget = dommy::vdb::TreeNodeBudget<T>;

struct FillPlan {
    openvdb::CoordBBox bounds;
    bool supported = false;
};

struct PlanFill {
    const FillSettings& settings;
    FillBudget& budget;
    FillPlan& plan;
    bool valid = false;
    PlanFill(const FillSettings& s, FillBudget& b, FillPlan& p) : settings(s), budget(b), plan(p) {}
    template <typename GridT> void operator()(const GridT& grid) {
        using ValueT = typename GridT::ValueType;
        using RootChild = typename GridT::TreeType::RootNodeType::ChildNodeType;
        ValueT value;
        if (!FillValue<ValueT>::convert(settings.value, value))
            return;
        auto minimum = settings.minimum, maximum = settings.maximum;
        if (settings.bounds != 0) {
            if (!dommy::vdb::validTransportTransform(grid))
                return;
            openvdb::math::calculateBounds(grid.transform(), settings.minimum, settings.maximum,
                                           minimum, maximum);
        }
        if (!validFillBounds(minimum, maximum))
            return;
        openvdb::Int32 low[3], high[3];
        for (int axis = 0; axis < 3; ++axis) {
            const double first = std::floor(minimum[axis]), last = std::ceil(maximum[axis]);
            // Native node-origin/extent arithmetic needs room around the box.
            if (!dommy::vdb::filterCoordinate(first, RootChild::DIM) ||
                !dommy::vdb::filterCoordinate(last, RootChild::DIM) ||
                !dommy::numeric::checkedNumericCast(first, low[axis]) ||
                !dommy::numeric::checkedNumericCast(last, high[axis]))
                return;
        }
        plan.bounds.reset(openvdb::Coord(low[0], low[1], low[2]),
                          openvdb::Coord(high[0], high[1], high[2]));
        if (!FillNodeBudget<RootChild>::add(plan.bounds, budget))
            return;
        dommy::vdb::ValuePreflight inputValues(budget.work, 2);
        inputValues(grid); // Reserve both the input and retained-value output scan.
        valid = inputValues.valid;
    }
};

struct ApplyFill {
    const FillSettings& settings;
    const FillPlan& plan;
    bool valid = false;
    ApplyFill(const FillSettings& s, const FillPlan& p) : settings(s), plan(p) {}
    template <typename GridT> void operator()(GridT& grid) {
        typename GridT::ValueType value;
        if (!FillValue<typename GridT::ValueType>::convert(settings.value, value))
            return;
        if (settings.sparse)
            grid.sparseFill(plan.bounds, value, settings.active);
        else
            grid.denseFill(plan.bounds, value, settings.active);
        std::size_t work = 0;
        dommy::vdb::ValuePreflight result(work, 1);
        result(grid);
        valid = result.valid;
    }
};
} // namespace

MStatus DOpenVDBFill::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle input = data.inputValue(aVdbInput, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(input, DOpenVDBData::id, inputVdb));
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kFailure;
    const MDataHandle names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = names.asString().asChar();
    GridCPtrVec grids;
    if (!getSelectedGrids(grids, selection, *inputVdb))
        return MS::kInvalidParameter;
    FillSettings settings;
    DOMMY_RETURN_IF_FAILURE(readFillSettings(data, thisMObject(), settings));

    FillBudget budget;
    std::vector<FillPlan> plans(grids.size());
    for (std::size_t i = 0; i < grids.size(); ++i) {
        const auto& grid = grids[i];
        if (!grid || !budget.addBytes(1U, grid->memUsage()))
            return MS::kInvalidParameter;
        PlanFill operation(settings, budget, plans[i]);
        plans[i].supported = processVdbTopologyGrid(getVdbGridType(*grid), *grid, operation);
        if (plans[i].supported && !operation.valid)
            return MS::kInvalidParameter;
    }
    // Preserve unselected-first output order and copied unsupported selections.
    // No output allocation or deep copy occurs until the entire batch is admitted.
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    GridCPtrVec selectedAgain;
    if (!getSelectedGrids(selectedAgain, selection, *inputVdb, *outputVdb) ||
        selectedAgain != grids)
        return MS::kFailure;
    for (std::size_t i = 0; i < grids.size(); ++i) {
        auto copy = grids[i]->deepCopyGrid();
        if (!copy || getVdbGridType(*copy) != getVdbGridType(*grids[i]))
            return MS::kFailure;
        if (plans[i].supported) {
            ApplyFill operation(settings, plans[i]);
            if (!processVdbTopologyGrid(getVdbGridType(*copy), *copy, operation) ||
                !operation.valid)
                return MS::kFailure;
        }
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFill")

MStatus DOpenVDBFill::initialize() try {
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // input vdb
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;
    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames =
        tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    MFnEnumAttribute boundAttr;
    aBounds = boundAttr.create("Bounds", "bnd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(boundAttr.addField("Min and Max in Index Space", 0));
    DOMMY_RETURN_IF_FAILURE(boundAttr.addField("Min and Max in World Space", 1));
    DOMMY_RETURN_IF_FAILURE(boundAttr.addField("Reference Geometry", 2));
    DOMMY_RETURN_IF_FAILURE(boundAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBounds));

    aMinCoord = nAttr.create("MinCoord", "mincrd", MFnNumericData::k3Int, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-1, -1, -1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinCoord));

    aMaxCoord = nAttr.create("MaxCoord", "maxcrd", MFnNumericData::k3Int, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1, 1, 1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxCoord));

    aMinWorldCoord = nAttr.create("MinWorldCoord", "minwldcrd", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-1.0, -1.0, -1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinWorldCoord));

    aMaxWorldCoord = nAttr.create("MaxWorldCoord", "maxwldcrd", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxWorldCoord));

    aValue = nAttr.create("Value", "val", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0, 0, 0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValue));

    aActive = nAttr.create("Active", "act", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aActive));

    aSparse = nAttr.create("Sparse", "sprs", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSparse));

    MFnMeshData meshCreator;
    MObject emptyMesh = meshCreator.create(&stat);
    if (stat != MS::kSuccess)
        return stat;
    aInMesh = tAttr.create("InMesh", "inmsh", MFnData::kMesh, emptyMesh, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInMesh));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBounds, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinCoord, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxCoord, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinWorldCoord, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxWorldCoord, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aValue, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aActive, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSparse, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInMesh, aVdbOutput));

    return MS::kSuccess;
}

catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFill", "initialize",
                                                  std::current_exception());
}

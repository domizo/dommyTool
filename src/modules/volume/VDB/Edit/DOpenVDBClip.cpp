//
//  DOpenVDBClip.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/06.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBClip.h"
#include "../Common/DVDBComputeGuard.h"

#include <openvdb/tools/Clip.h>
#include <openvdb/tools/LevelSetUtil.h>

#include "DMayaHeaders.h"
#include "DNodeOperation.h"
#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "dvdb_ids.h"

#include <cmath>
#include <vector>
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "math/DBounds.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"

const MTypeId DOpenVDBClip::typeId(ID_VDB_DOPENVDB_CLIP);

MObject DOpenVDBClip::aVdbInput;
MObject DOpenVDBClip::aVdbSelectedGridNames;
MObject DOpenVDBClip::aMaskName;
MObject DOpenVDBClip::aMaskVdbInput;
MObject DOpenVDBClip::aClipType;
MObject DOpenVDBClip::aInObject;
MObject DOpenVDBClip::aClipAxis;
MObject DOpenVDBClip::aClipPosition;
MObject DOpenVDBClip::aClipWidth;
MObject DOpenVDBClip::aKeepInside;
MObject DOpenVDBClip::aVdbOutput;

void* DOpenVDBClip::creator() {
    return ::dommy::maya::createObject("DOpenVDBClip", [&]() {
        return new DOpenVDBClip();
    });
}

namespace {
using dommy::vdb::gridBounds;
using dommy::vdb::indexGridBox;
using dommy::vdb::mapGridBox;
using dommy::vdb::validGridBox;
using dommy::vdb::validRelativeGridMap;
struct ClipSettings {
    short mode = 1, axis = 1;
    float position = .5f, width = 1;
    bool inside = true;
};

MStatus readClipSettings(MDataBlock& data, ClipSettings& settings) {
    using Node = DOpenVDBClip;
    MStatus status;
    const auto mode = data.inputValue(Node::aClipType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.mode = mode.asShort();
    if (settings.mode < 0 || settings.mode > 3)
        return MS::kInvalidParameter;
    const auto inside = data.inputValue(Node::aKeepInside, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.inside = inside.asBool();
    if (settings.mode <= 1) {
        const auto axis = data.inputValue(Node::aClipAxis, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.axis = axis.asShort();
        const auto position = data.inputValue(Node::aClipPosition, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.position = position.asFloat();
        if (settings.axis < 0 || settings.axis > 2 || !std::isfinite(settings.position) ||
            settings.position < 0 || settings.position > 1)
            return MS::kInvalidParameter;
        if (settings.mode == 1) {
            const auto width = data.inputValue(Node::aClipWidth, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.width = width.asFloat();
            if (!std::isfinite(settings.width) || settings.width <= 0)
                return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

// Validate and bound the actual stored topology, including inactive leaf
// voxels and non-background tiles. Counting only active voxels misses these.
struct ClipInspect {
    dommy::vdb::TreeEditBudget& budget;
    dommy::math::Bounds3d bounds;
    bool valid = false;
    explicit ClipInspect(dommy::vdb::TreeEditBudget& b) : budget(b) {}
    template <typename GridT> void operator()(const GridT& grid) {
        using ValueT = typename GridT::ValueType;
        using RootChild = typename GridT::TreeType::RootNodeType::ChildNodeType;
        dommy::vdb::ValuePreflight values(budget.work, 6);
        values(grid);
        if (!values.valid || !dommy::vdb::validTransportTransform(grid))
            return;
        for (auto it = grid.cbeginValueAll(); it; ++it) {
            if (it.isTileValue() && !it.isValueOn() &&
                dommy::vdb::ValueComponents<ValueT>::matchesBackground(*it, grid.background(),
                                                                       ValueT(0)))
                continue;
            openvdb::CoordBBox box;
            it.getBoundingBox(box);
            if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
                !bounds.add(box.max()[0], box.max()[1], box.max()[2]))
                return;
        }
        if (!bounds.empty()) {
            openvdb::CoordBBox region;
            if (!indexGridBox(gridBounds(bounds), region) ||
                !dommy::vdb::TreeNodeBudget<RootChild>::add(region, budget))
                return;
        }
        valid = true;
    }
};

struct ClipPlan {
    bool supported = false;
    bool hasBounds = false;
    openvdb::BBoxd stored;
};

struct ClipRegionBudget {
    const openvdb::CoordBBox& region;
    dommy::vdb::TreeEditBudget& budget;
    bool valid = false;
    ClipRegionBudget(const openvdb::CoordBBox& r, dommy::vdb::TreeEditBudget& b)
        : region(r), budget(b) {}
    template <typename GridT> void operator()(const GridT&) {
        using RootChild = typename GridT::TreeType::RootNodeType::ChildNodeType;
        valid = dommy::vdb::TreeNodeBudget<RootChild>::add(region, budget);
    }
};

bool clipMaskPlan(const Grid& source, const Grid& mask, const ClipPlan& maskPlan,
                  dommy::vdb::TreeEditBudget& budget) {
    if (!validRelativeGridMap(mask.transform(), source.transform()) ||
        !validRelativeGridMap(source.transform(), mask.transform()))
        return false;
    if (!maskPlan.hasBounds)
        return true;
    auto input = maskPlan.stored;
    // Native leaf boxes include origin+DIM, then transformBBox adds another
    // endpoint cell; BoolSampler expands each transformed box by radius 2.
    for (int axis = 0; axis < 3; ++axis)
        input.max()[axis] += 2;
    openvdb::BBoxd mapped;
    if (!mapGridBox(input, mask.transform(), source.transform(), mapped))
        return false;
    mapped.expand(3.0); // radius 2 plus inclusive floor/ceil rounding
    openvdb::CoordBBox region;
    if (!indexGridBox(mapped, region, true))
        return false;
    openvdb::BBoxd samples;
    if (!mapGridBox(mapped, source.transform(), mask.transform(), samples) ||
        !indexGridBox(samples, region, true))
        return false;
    // Charge expanded output masks/results in the source tree configuration.
    if (!indexGridBox(mapped, region, true))
        return false;
    ClipRegionBudget operation(region, budget);
    return processVdbTopologyGrid(getVdbGridType(source), source, operation) && operation.valid;
}

struct LevelSetMaskOp {
    GridPtr outputGrid;
    template <typename GridType> void operator()(const GridType& grid) {
        if (!std::isfinite(static_cast<long double>(grid.background())) || grid.background() <= 0)
            return;
        outputGrid = openvdb::tools::sdfInteriorMask(grid);
    }
};
template <typename GridType> struct MaskClipDispatchOp {
    const GridType& source;
    bool inside;
    GridPtr outputGrid;
    MaskClipDispatchOp(const GridType& g, bool i) : source(g), inside(i) {}
    template <typename MaskGridType> void operator()(const MaskGridType& mask) {
        outputGrid = openvdb::tools::clip(source, mask, inside);
    }
};
struct MaskClipOp {
    const GridCPtr& mask;
    bool inside;
    GridPtr outputGrid;
    MaskClipOp(const GridCPtr& m, bool i) : mask(m), inside(i) {}
    template <typename GridType> void operator()(const GridType& grid) {
        MaskClipDispatchOp<GridType> operation(grid, inside);
        if (mask && processVdbTopologyGrid(getVdbGridType(*mask), *mask, operation))
            outputGrid = operation.outputGrid;
    }
};
using dommy::vdb::PositiveMaskBackground;
} // namespace

MStatus DOpenVDBClip::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, inputVdb));
    MStatus status;
    const auto names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = names.asString().asChar();
    GridCPtrVec selected, grids;
    getSelectedGrids(selected, selection, *inputVdb);
    for (const auto& grid : selected)
        if (!grid->empty())
            grids.push_back(grid);

    ClipSettings settings;
    openvdb::BBoxd bounds;
    GridCPtr sourceMask;
    ClipPlan maskPlan;
    dommy::vdb::TreeEditBudget budget;
    std::vector<ClipPlan> plans(grids.size());
    if (!grids.empty()) {
        DOMMY_RETURN_IF_FAILURE(readClipSettings(data, settings));
        for (std::size_t i = 0; i < grids.size(); ++i) {
            if (!budget.addBytes(1, grids[i]->memUsage()))
                return MS::kInvalidParameter;
            ClipInspect inspect(budget);
            plans[i].supported =
                processVdbTopologyGrid(getVdbGridType(*grids[i]), *grids[i], inspect);
            if (plans[i].supported && !inspect.valid)
                return MS::kInvalidParameter;
            plans[i].hasBounds = !inspect.bounds.empty();
            if (plans[i].hasBounds)
                plans[i].stored = gridBounds(inspect.bounds);
        }
        if (settings.mode <= 1) {
            openvdb::BBoxd activeBounds;
            bool found = false;
            for (const auto& grid : grids) {
                openvdb::BBoxd gridBounds;
                if (!getActiveVoxelWorldBounds(*grid, gridBounds) || !validGridBox(gridBounds))
                    return MS::kInvalidParameter;
                if (found)
                    activeBounds.expand(gridBounds);
                else {
                    activeBounds = gridBounds;
                    found = true;
                }
            }
            if (!found || !validGridBox(activeBounds))
                return MS::kInvalidParameter;
            bounds = activeBounds;
            bounds.expand(2.0);
            const int axis = settings.axis;
            const double boundary =
                activeBounds.min()[axis] + activeBounds.extents()[axis] * double(settings.position);
            bounds.min()[axis] = boundary;
            if (settings.mode == 1) {
                bounds.min()[axis] -= double(settings.width) * .5;
                bounds.max()[axis] = boundary + double(settings.width) * .5;
            }
        } else if (settings.mode == 2) {
            MObject mesh;
            MBoundingBox box;
            DOMMY_RETURN_IF_FAILURE(getMeshInformation(aInObject, data, box, mesh));
            const auto low = box.min(), high = box.max();
            bounds = openvdb::BBoxd(openvdb::Vec3d(low.x, low.y, low.z),
                                    openvdb::Vec3d(high.x, high.y, high.z));
        } else {
            const DOpenVDBData* maskVdb = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aMaskVdbInput, data, maskVdb));
            const auto maskNames = data.inputValue(aMaskName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            GridCPtrVec masks;
            if (!getSelectedGrids(masks, maskNames.asString().asChar(), *maskVdb))
                return MS::kInvalidParameter;
            sourceMask = masks.front();
            if (!budget.addBytes(1, sourceMask->memUsage()))
                return MS::kInvalidParameter;
            ClipInspect inspect(budget);
            const bool knownMask =
                processVdbTopologyGrid(getVdbGridType(*sourceMask), *sourceMask, inspect);
            if (!knownMask) {
                if (sourceMask->getGridClass() == openvdb::GRID_LEVEL_SET)
                    return MS::kInvalidParameter;
                // An unsupported ordinary mask historically passes sources through.
                // Do not search for a later supported mask in the selection.
                for (auto& plan : plans)
                    plan.supported = false;
                sourceMask.reset();
            } else {
                if (!inspect.valid)
                    return MS::kInvalidParameter;
                maskPlan.hasBounds = !inspect.bounds.empty();
                if (maskPlan.hasBounds)
                    maskPlan.stored = gridBounds(inspect.bounds);
            }
            if (sourceMask && sourceMask->getGridClass() == openvdb::GRID_LEVEL_SET) {
                PositiveMaskBackground check;
                if (!processVdbScalarGrid(getVdbGridType(*sourceMask), *sourceMask, check) ||
                    !check.valid)
                    return MS::kInvalidParameter;
            }
        }
        if (settings.mode != 3 && !validGridBox(bounds))
            return MS::kInvalidParameter;
        // Complete every plan before allocating a mask or invoking any kernel.
        for (std::size_t i = 0; i < grids.size(); ++i) {
            if (!plans[i].supported)
                continue;
            if (settings.mode == 3) {
                if (!clipMaskPlan(*grids[i], *sourceMask, maskPlan, budget))
                    return MS::kInvalidParameter;
            } else {
                openvdb::Vec3d low, high;
                openvdb::math::calculateBounds(grids[i]->transform(), bounds.min(), bounds.max(),
                                               low, high);
                openvdb::CoordBBox region;
                // Native Clip rounds BOTH endpoints down (unlike Fill).
                if (!indexGridBox(openvdb::BBoxd(low, high), region))
                    return MS::kInvalidParameter;
                ClipRegionBudget op(region, budget);
                if (!processVdbTopologyGrid(getVdbGridType(*grids[i]), *grids[i], op) || !op.valid)
                    return MS::kInvalidParameter;
            }
        }
    }

    GridCPtr mask;
    if (sourceMask) {
        if (sourceMask->getGridClass() == openvdb::GRID_LEVEL_SET) {
            LevelSetMaskOp op;
            if (!processVdbScalarGrid(getVdbGridType(*sourceMask), *sourceMask, op) ||
                !op.outputGrid)
                return MS::kFailure;
            mask = op.outputGrid;
        } else
            mask = sourceMask;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    GridCPtrVec selectedAgain;
    getSelectedGrids(selectedAgain, selection, *inputVdb, *outputVdb);
    if (selectedAgain != selected)
        return MS::kFailure;
    // Historical order: unselected, selected empty, selected nonempty grids.
    for (const auto& grid : selected)
        if (grid->empty())
            outputVdb->insert(*grid);
    bool levelSet = false;
    for (std::size_t i = 0; i < grids.size(); ++i) {
        const auto& grid = grids[i];
        levelSet = levelSet || grid->getGridClass() == openvdb::GRID_LEVEL_SET;
        if (!plans[i].supported) {
            outputVdb->insert(*grid);
            continue;
        }
        GridPtr result;
        if (settings.mode == 3) {
            MaskClipOp op(mask, settings.inside);
            if (!processVdbTopologyGrid(getVdbGridType(*grid), *grid, op))
                return MS::kFailure;
            result = op.outputGrid;
        } else {
            BBoxClipOp op(bounds, settings.inside);
            if (!processVdbTopologyGrid(getVdbGridType(*grid), *grid, op))
                return MS::kFailure;
            result = op.outputGrid;
        }
        if (!result || result.get() == grid.get() ||
            getVdbGridType(*result) != getVdbGridType(*grid))
            return MS::kFailure;
        std::size_t work = 0;
        dommy::vdb::ValuePreflight values(work, 1);
        if (!processVdbTopologyGrid(getVdbGridType(*result), *result, values) || !values.valid)
            return MS::kFailure;
        result->setName(grid->getName() + "_cliped");
        outputVdb->insert(result);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    if (levelSet)
        MGlobal::displayWarning("Clipping can produce a grid that is no longer a valid level set.");
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBClip")

MStatus DOpenVDBClip::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnGenericAttribute genericAttribute;

    aVdbInput =
        typedAttribute.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData stringData;
    const MObject selectionDefault = stringData.create("", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aVdbSelectedGridNames = typedAttribute.create("SelectedGridNames", "grids", MFnData::kString,
                                                  selectionDefault, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    const MObject maskDefault = stringData.create("", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aMaskName = typedAttribute.create("MaskName", "mname", MFnData::kString, maskDefault, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskName));

    aMaskVdbInput = typedAttribute.create("MaskVdbInput", "maskvdb", DOpenVDBData::id,
                                          MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskVdbInput));

    MFnEnumAttribute clipTypeAttribute;
    aClipType = clipTypeAttribute.create("ClipType", "clt", 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(clipTypeAttribute.addField("Axis Clip", 0));
    DOMMY_RETURN_IF_FAILURE(clipTypeAttribute.addField("Axis Slice", 1));
    DOMMY_RETURN_IF_FAILURE(clipTypeAttribute.addField("Object BBox", 2));
    DOMMY_RETURN_IF_FAILURE(clipTypeAttribute.addField("Mask VDB", 3));
    DOMMY_RETURN_IF_FAILURE(clipTypeAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClipType));

    aInObject = genericAttribute.create("InObject", "ino", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    MFnEnumAttribute axisAttribute;
    aClipAxis = axisAttribute.create("ClipAxis", "clax", 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(axisAttribute.addField("X-Axis", 0));
    DOMMY_RETURN_IF_FAILURE(axisAttribute.addField("Y-Axis", 1));
    DOMMY_RETURN_IF_FAILURE(axisAttribute.addField("Z-Axis", 2));
    DOMMY_RETURN_IF_FAILURE(axisAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClipAxis));

    aClipPosition =
        numericAttribute.create("ClipPosition", "clpos", MFnNumericData::kFloat, 0.5, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClipPosition));

    aClipWidth = numericAttribute.create("ClipWidth", "clw", MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClipWidth));

    aKeepInside =
        numericAttribute.create("KeepInside", "kin", MFnNumericData::kBoolean, true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepInside));

    aVdbOutput = typedAttribute.create("VdbOutput", "vdboutput", DOpenVDBData::id,
                                       MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKeepInside, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aClipType, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInObject, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aClipAxis, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aClipPosition, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aClipWidth, aVdbOutput));

    return MS::kSuccess;
}

catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBClip", "initialize",
                                                  std::current_exception());
}

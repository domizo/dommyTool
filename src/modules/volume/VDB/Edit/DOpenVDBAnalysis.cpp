//
//  DOpenVDBAnalysis.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/11.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBAnalysis.h"
#include "../Common/DVDBComputeGuard.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include "common/utility/DStatusOperation.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/util/NullInterrupter.h>

#include "DMayaHeaders.h"

#include "dvdb_ids.h"

const MTypeId DOpenVDBAnalysis::typeId(ID_VDB_DOPENVDB_ANALYSIS);

MObject DOpenVDBAnalysis::aVdbInput;
MObject DOpenVDBAnalysis::aVdbSelectedGridNames;

MObject DOpenVDBAnalysis::aOperator;

MObject DOpenVDBAnalysis::aMaskVdbInput;
MObject DOpenVDBAnalysis::aMaskGridNames;

MObject DOpenVDBAnalysis::aOutputName;
MObject DOpenVDBAnalysis::aCustomName;

MObject DOpenVDBAnalysis::aVdbOutput;

void* DOpenVDBAnalysis::creator() {
    return ::dommy::maya::createObject("DOpenVDBAnalysis", [&]() {
        return new DOpenVDBAnalysis();
    });
}

namespace {
enum OpId {
    kGradient,
    kCurvature,
    kLaplacian,
    kClosestPoint,
    kDivergence,
    kCurl,
    kMagnitude,
    kNormalize
};
struct AnalysisSettings {
    short operation = kGradient, renaming = 0;
    std::string customName;
};
MStatus readAnalysisSettings(MDataBlock& data, AnalysisSettings& settings) {
    MStatus status;
    const auto operation = data.inputValue(DOpenVDBAnalysis::aOperator, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.operation = operation.asShort();
    if (settings.operation < kGradient || settings.operation > kNormalize)
        return MS::kInvalidParameter;
    const auto renaming = data.inputValue(DOpenVDBAnalysis::aOutputName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.renaming = renaming.asShort();
    if (settings.renaming < 0 || settings.renaming > 2)
        return MS::kInvalidParameter;
    if (settings.renaming == 2) {
        const auto name = data.inputValue(DOpenVDBAnalysis::aCustomName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.customName = name.asString().asChar();
    }
    return MS::kSuccess;
}

// Preserve grid precision. Integer arithmetic is not a meaningful/safe
// implementation of these differential and normalization operators.
DVdbGridType analysisOutputType(short operation, DVdbGridType input) {
    using Type = DVdbGridType;
    if (operation <= kClosestPoint) {
        if (input != Type::kFloat && input != Type::kDouble)
            return Type::kInvalid;
        if (operation == kGradient || operation == kClosestPoint)
            return input == Type::kFloat ? Type::kVec3f : Type::kVec3d;
        return input;
    }
    if (input != Type::kVec3f && input != Type::kVec3d)
        return Type::kInvalid;
    if (operation == kDivergence || operation == kMagnitude)
        return input == Type::kVec3f ? Type::kFloat : Type::kDouble;
    return input;
}

template <typename T> T analysisMagnitude(const openvdb::math::Vec3<T>& value) {
    static_assert(std::is_floating_point<T>::value, "Analysis requires floating-point vectors");
    const long double length = std::hypot(
        std::hypot(static_cast<long double>(value[0]), static_cast<long double>(value[1])),
        static_cast<long double>(value[2]));
    T result;
    if (!dommy::numeric::checkedNumericCast(length, result))
        throw std::overflow_error("VDB vector length is not representable in its output type");
    return result;
}
template <typename T>
openvdb::math::Vec3<T> analysisNormalized(const openvdb::math::Vec3<T>& value) {
    static_assert(std::is_floating_point<T>::value, "Analysis requires floating-point vectors");
    T x, y, z;
    // Retain the legacy zero/near-zero convention while avoiding lengthSqr
    // overflow. The shared routine scales before taking the vector length.
    dommy::math::normalized3OrZero(value[0], value[1], value[2], x, y, z, T(1.0e-7));
    return openvdb::math::Vec3<T>(x, y, z);
}
struct MagnitudePreflight {
    template <typename GridT> void operator()(const GridT& grid) {
        (void)analysisMagnitude(grid.background());
        for (auto it = grid.cbeginValueAll(); it; ++it)
            (void)analysisMagnitude(*it);
    }
};
template <typename GridT, bool Normalize> struct AnalysisVectorOperator;
template <typename GridT> struct AnalysisVectorOperator<GridT, false> {
    using Output = typename openvdb::tools::VectorToScalarConverter<GridT>::Type;
    template <typename Map, typename Accessor>
    static typename Output::ValueType result(const Map&, const Accessor& values,
                                             const openvdb::Coord& xyz) {
        return analysisMagnitude(values.getValue(xyz));
    }
};
template <typename GridT> struct AnalysisVectorOperator<GridT, true> {
    using Output = GridT;
    template <typename Map, typename Accessor>
    static typename Output::ValueType result(const Map&, const Accessor& values,
                                             const openvdb::Coord& xyz) {
        return analysisNormalized(values.getValue(xyz));
    }
};
template <bool Normalize> struct StableVectorOp {
    const openvdb::BoolGrid* mask;
    openvdb::util::NullInterrupter& boss;
    GridPtr output;
    StableVectorOp(const openvdb::BoolGrid* m, openvdb::util::NullInterrupter& b)
        : mask(m), boss(b) {}
    template <typename GridT> void operator()(const GridT& grid) {
        using Op = AnalysisVectorOperator<GridT, Normalize>;
        using Output = typename Op::Output;
        // Value-only operators need no map specialization; GridOperator copies
        // the map and retains the native sparse topology/mask behavior.
        openvdb::tools::gridop::GridOperator<GridT, openvdb::BoolGrid, Output,
                                             openvdb::math::MapBase, Op,
                                             openvdb::util::NullInterrupter>
            operation(grid, mask, *grid.transform().baseMap(), &boss, false);
        output = operation.process(true);
        if (Normalize && output) {
            const auto type = grid.getVectorType();
            output->setVectorType(type == openvdb::VEC_COVARIANT ? openvdb::VEC_COVARIANT_NORMALIZE
                                                                 : type);
        }
    }
};

template <typename GridT>
bool analysisRegionBudget(const openvdb::CoordBBox& region, dommy::vdb::TreeEditBudget& budget) {
    using Root = typename GridT::TreeType::RootNodeType::ChildNodeType;
    return dommy::vdb::TreeNodeBudget<Root>::add(region, budget);
}
bool analysisRegionBudget(DVdbGridType type, const openvdb::CoordBBox& region,
                          dommy::vdb::TreeEditBudget& budget) {
    switch (type) {
    case DVdbGridType::kFloat:
        return analysisRegionBudget<openvdb::FloatGrid>(region, budget);
    case DVdbGridType::kDouble:
        return analysisRegionBudget<openvdb::DoubleGrid>(region, budget);
    case DVdbGridType::kVec3f:
        return analysisRegionBudget<openvdb::Vec3SGrid>(region, budget);
    case DVdbGridType::kVec3d:
        return analysisRegionBudget<openvdb::Vec3DGrid>(region, budget);
    case DVdbGridType::kBool:
        return analysisRegionBudget<openvdb::BoolGrid>(region, budget);
    default:
        return false;
    }
}
struct AnalysisInspect {
    dommy::vdb::TreeEditBudget& budget;
    short operation;
    dommy::math::Bounds3d bounds;
    bool valid = false;
    AnalysisInspect(dommy::vdb::TreeEditBudget& b, short op) : budget(b), operation(op) {}
    template <typename GridT> void operator()(const GridT& grid) {
        using Value = typename GridT::ValueType;
        dommy::vdb::ValuePreflight values(budget.work, 8);
        values(grid);
        if (!values.valid || !dommy::vdb::validTransportTransform(grid) ||
            !dommy::vdb::validDerivativeGridMap(grid.transform()))
            return;
        // Native curvature includes cubic products in the grid's own precision.
        // A fourth-root envelope leaves room for stencils and bounded map factors.
        const auto type = getVdbGridType(grid);
        const long double maximum = type == DVdbGridType::kFloat || type == DVdbGridType::kVec3f
                                        ? std::numeric_limits<float>::max()
                                        : std::numeric_limits<double>::max();
        const long double limit = std::sqrt(std::sqrt(maximum)) / 1024;
        if (operation < kMagnitude &&
            dommy::vdb::TransportMagnitude<Value>::get(grid.background()) > limit)
            return;
        for (auto it = grid.cbeginValueAll(); it; ++it) {
            if (operation < kMagnitude && dommy::vdb::TransportMagnitude<Value>::get(*it) > limit)
                return;
            if (it.isTileValue() && !it.isValueOn() &&
                dommy::vdb::ValueComponents<Value>::matchesBackground(*it, grid.background(),
                                                                      Value(0)))
                continue;
            openvdb::CoordBBox box;
            it.getBoundingBox(box);
            if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
                !bounds.add(box.max()[0], box.max()[1], box.max()[2]))
                return;
        }
        if (!bounds.empty()) {
            openvdb::CoordBBox region;
            if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(bounds), region))
                return;
        }
        valid = true;
    }
};
struct AnalysisPlan {
    DVdbGridType outputType = DVdbGridType::kInvalid;
};
bool analysisMaskRegion(const Grid& source, const Grid& mask, const AnalysisInspect& inspect,
                        DVdbGridType outputType, dommy::vdb::TreeEditBudget& budget) {
    using namespace dommy::vdb;
    if (!validRelativeGridMap(mask.transform(), source.transform()) ||
        !validRelativeGridMap(source.transform(), mask.transform()))
        return false;
    if (inspect.bounds.empty())
        return true;
    auto input = gridBounds(inspect.bounds);
    for (int axis = 0; axis < 3; ++axis)
        input.max()[axis] += 2;
    openvdb::BBoxd mapped, samples;
    openvdb::CoordBBox region;
    if (!mapGridBox(input, mask.transform(), source.transform(), mapped))
        return false;
    mapped.expand(3); // conservative endpoint/sampler/floor-ceil headroom
    if (!indexGridBox(mapped, region, true) ||
        !analysisRegionBudget<openvdb::BoolGrid>(region, budget) ||
        !analysisRegionBudget(outputType, region, budget) ||
        !mapGridBox(mapped, source.transform(), mask.transform(), samples) ||
        !indexGridBox(samples, region, true))
        return false;
    return true;
}
struct AnalysisMaskOp {
    openvdb::BoolGrid::Ptr output;
    template <typename GridT> void operator()(const GridT& grid) {
        if (grid.getGridClass() == openvdb::GRID_LEVEL_SET) {
            if (grid.background() <= 0)
                throw std::invalid_argument("Analysis level-set mask needs a positive background");
            output = openvdb::tools::sdfInteriorMask(grid);
        } else {
            output = openvdb::BoolGrid::create(false);
            output->setTransform(grid.transform().copy());
            output->tree().topologyUnion(grid.tree());
        }
    }
};
template <template <typename, typename, typename> class Tool> struct AnalysisToolOp {
    const openvdb::BoolGrid* mask;
    openvdb::util::NullInterrupter& boss;
    GridPtr output;
    AnalysisToolOp(const openvdb::BoolGrid* m, openvdb::util::NullInterrupter& b)
        : mask(m), boss(b) {}
    template <typename GridT> void operator()(const GridT& grid) {
        if (mask) {
            Tool<GridT, openvdb::BoolGrid, openvdb::util::NullInterrupter> op(grid, *mask, &boss);
            output = op.process(true);
        } else {
            Tool<GridT, openvdb::BoolGrid, openvdb::util::NullInterrupter> op(grid, &boss);
            output = op.process(true);
        }
    }
};
GridPtr runAnalysis(const Grid& grid, short operation, const openvdb::BoolGrid* mask,
                    openvdb::util::NullInterrupter& boss) {
    const auto type = getVdbGridType(grid);
    switch (operation) {
    case kGradient: {
        AnalysisToolOp<openvdb::tools::Gradient> op(mask, boss);
        if (processVdbRealGrid(type, grid, op))
            return op.output;
        break;
    }
    case kCurvature: {
        AnalysisToolOp<openvdb::tools::MeanCurvature> op(mask, boss);
        if (processVdbRealGrid(type, grid, op))
            return op.output;
        break;
    }
    case kLaplacian: {
        AnalysisToolOp<openvdb::tools::Laplacian> op(mask, boss);
        if (processVdbRealGrid(type, grid, op))
            return op.output;
        break;
    }
    case kClosestPoint: {
        AnalysisToolOp<openvdb::tools::Cpt> op(mask, boss);
        if (processVdbRealGrid(type, grid, op))
            return op.output;
        break;
    }
    case kDivergence: {
        AnalysisToolOp<openvdb::tools::Divergence> op(mask, boss);
        if (processVdbRealVectorGrid(type, grid, op))
            return op.output;
        break;
    }
    case kCurl: {
        AnalysisToolOp<openvdb::tools::Curl> op(mask, boss);
        if (processVdbRealVectorGrid(type, grid, op))
            return op.output;
        break;
    }
    case kMagnitude: {
        StableVectorOp<false> op(mask, boss);
        if (processVdbRealVectorGrid(type, grid, op))
            return op.output;
        break;
    }
    case kNormalize: {
        StableVectorOp<true> op(mask, boss);
        if (processVdbRealVectorGrid(type, grid, op))
            return op.output;
        break;
    }
    }
    return GridPtr();
}
} // namespace

MStatus DOpenVDBAnalysis::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, inputVdb));
    MStatus status;
    const auto names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = names.asString().asChar();
    GridCPtrVec grids;
    getSelectedGrids(grids, selection, *inputVdb);
    AnalysisSettings settings;
    dommy::vdb::TreeEditBudget budget;
    std::vector<AnalysisPlan> plans(grids.size());
    GridCPtr sourceMask;
    AnalysisInspect maskInspect(budget, kNormalize);
    if (!grids.empty()) {
        DOMMY_RETURN_IF_FAILURE(readAnalysisSettings(data, settings));
        const bool connected = MPlug(thisMObject(), aMaskVdbInput).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (connected) {
            const DOpenVDBData* maskVdb = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aMaskVdbInput, data, maskVdb));
            const auto maskNames = data.inputValue(aMaskGridNames, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            GridCPtrVec masks;
            getSelectedGrids(masks, maskNames.asString().asChar(), *maskVdb);
            // Historical behavior: only the first selected scalar mask is used.
            // A valid but empty/unsupported selection warns and runs unmasked.
            if (!masks.empty()) {
                const auto type = getVdbGridType(*masks.front());
                if (type == DVdbGridType::kFloat || type == DVdbGridType::kDouble ||
                    type == DVdbGridType::kInt32 || type == DVdbGridType::kInt64)
                    sourceMask = masks.front();
            }
            if (!sourceMask)
                MGlobal::displayWarning(
                    "The selected analysis mask is missing or has an unsupported type.");
        }
        if (sourceMask) {
            if (!budget.addBytes(2, sourceMask->memUsage()) ||
                !processVdbScalarGrid(getVdbGridType(*sourceMask), *sourceMask, maskInspect) ||
                !maskInspect.valid)
                return MS::kInvalidParameter;
            if (!maskInspect.bounds.empty()) {
                openvdb::CoordBBox region;
                if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(maskInspect.bounds), region) ||
                    !analysisRegionBudget<openvdb::BoolGrid>(region, budget))
                    return MS::kInvalidParameter;
            }
        }
        // Finish all plans before mask allocation, output allocation or kernels.
        for (std::size_t i = 0; i < grids.size(); ++i) {
            const auto& grid = *grids[i];
            if (!budget.addBytes(1, grid.memUsage()))
                return MS::kInvalidParameter;
            const auto type = getVdbGridType(grid);
            plans[i].outputType = analysisOutputType(settings.operation, type);
            if (plans[i].outputType == DVdbGridType::kInvalid)
                continue;
            AnalysisInspect inspect(budget, settings.operation);
            if (!processVdbTopologyGrid(type, grid, inspect) || !inspect.valid)
                return MS::kInvalidParameter;
            if (settings.operation == kMagnitude) {
                MagnitudePreflight magnitude;
                if (!processVdbRealVectorGrid(type, grid, magnitude))
                    return MS::kInvalidParameter;
            }
            if (settings.operation < kMagnitude && !inspect.bounds.empty()) {
                openvdb::CoordBBox region;
                if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(inspect.bounds), region) ||
                    !analysisRegionBudget(plans[i].outputType, region, budget))
                    return MS::kInvalidParameter;
            } else if (!budget.addBytes(3, grid.memUsage())) {
                // Value-only kernels do not voxelize tiles. Reserve topology
                // copies instead of charging the tile's expanded voxel volume.
                return MS::kInvalidParameter;
            }
            if (sourceMask &&
                !analysisMaskRegion(grid, *sourceMask, maskInspect, plans[i].outputType, budget))
                return MS::kInvalidParameter;
        }
    }
    openvdb::BoolGrid::Ptr mask;
    if (sourceMask) {
        AnalysisMaskOp operation;
        if (!processVdbScalarGrid(getVdbGridType(*sourceMask), *sourceMask, operation) ||
            !operation.output)
            return MS::kFailure;
        mask = operation.output;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    GridCPtrVec selectedAgain;
    getSelectedGrids(selectedAgain, selection, *inputVdb, *outputVdb);
    if (selectedAgain != grids)
        return MS::kFailure;
    if (grids.empty())
        MGlobal::displayWarning(
            "No VDB grids match the analysis selection; input was passed through.");
    const char* suffixes[] = {"_gradient",   "_curvature", "_laplacian", "_cpt",
                              "_divergence", "_curl",      "_magnitude", "_normalize"};
    openvdb::util::NullInterrupter boss;
    for (std::size_t i = 0; i < grids.size(); ++i) {
        const auto& gridRef = *grids[i];
        if (plans[i].outputType == DVdbGridType::kInvalid) {
            MGlobal::displayWarning(
                "Analysis requires a matching floating-point grid; input was passed through.");
            outputVdb->insert(gridRef);
            continue;
        }
        openvdb::BoolGrid regionMask;
        if (mask) {
            regionMask.setTransform(gridRef.transform().copy());
            openvdb::tools::resampleToMatch<openvdb::tools::PointSampler>(*mask, regionMask, boss);
        }
        GridPtr result =
            runAnalysis(gridRef, settings.operation, mask ? &regionMask : nullptr, boss);
        if (!result || result.get() == grids[i].get() ||
            getVdbGridType(*result) != plans[i].outputType)
            return MS::kFailure;
        std::size_t work = 0;
        dommy::vdb::ValuePreflight values(work, 1);
        if (!processVdbTopologyGrid(getVdbGridType(*result), *result, values) || !values.valid)
            return MS::kFailure;
        std::string name = gridRef.getName();
        if (settings.renaming == 1)
            name += suffixes[settings.operation];
        else if (settings.renaming == 2 && !settings.customName.empty())
            name = settings.customName;
        result->setName(name);
        outputVdb->insert(result);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBAnalysis")

MStatus DOpenVDBAnalysis::initialize() try {
    MStatus stat;

    // attributes

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

    MFnEnumAttribute opAttr;
    aOperator = opAttr.create("Operator", "op", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Gradient       (Scalar->Vector)", 0));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Curvature     (Scalar->Scalar)", 1));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Laplacian      (Scalar->Scalar)", 2));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Closest Point (Scalar->Vector)", 3));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Divergence    (Vector->Scalar)", 4));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Curl             (Vector->Vector)", 5));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Length         (Vector->Scalar)", 6));
    DOMMY_RETURN_IF_FAILURE(opAttr.addField("Normalize     (Vector->Vector)", 7));
    DOMMY_RETURN_IF_FAILURE(opAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOperator));

    // input mask vdb
    aMaskVdbInput =
        tAttr.create("MaskVdbInput", "maskvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskVdbInput));

    const MObject maskDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMaskGridNames = tAttr.create("MaskGridNames", "mgrids", MFnData::kString, maskDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskGridNames));

    MFnEnumAttribute outnameAttr;
    aOutputName = outnameAttr.create("OutputName", "onm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outnameAttr.addField("Keep Incoming VDB Names", 0));
    DOMMY_RETURN_IF_FAILURE(outnameAttr.addField("Append Operation Name", 1));
    DOMMY_RETURN_IF_FAILURE(outnameAttr.addField("Custom Name", 2));
    DOMMY_RETURN_IF_FAILURE(outnameAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputName));

    const MObject customDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCustomName = tAttr.create("CustomName", "cnm", MFnData::kString, customDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCustomName));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOperator, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCustomName, aVdbOutput));

    return MS::kSuccess;
}

catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBAnalysis", "initialize",
                                                  std::current_exception());
}

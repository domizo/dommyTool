// DVDBPotentialFlow -- legacy Potential Flow node, Maya schema retained.
#include "common/utility/DMayaEntryPointGuard.h"
#include "DVDBPotentialFlow.h"
#include "DMayaHeaders.h"
#include "DOpenVDBDataHeaders.h"
#include "../Common/DVDBComputeGuard.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "DOpenVDBPotentialFlowCompat.h"
#include "common/utility/DStatusOperation.h"
#include <openvdb/tools/PotentialFlow.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace {
const int DEFAULT_MAX_ITERATIONS = 10000;
const int kMaxPotentialDilation = 256;
constexpr double DEFAULT_MAX_ERROR = 1.0e-20;

struct PotentialSettings {
    int dilation = 1, iterations = DEFAULT_MAX_ITERATIONS;
    double tolerance = static_cast<float>(DEFAULT_MAX_ERROR);
    openvdb::Vec3f background{0};
    bool applyBackground = false, outputPotential = false;
    short maskType = 0;
};

MStatus readPotentialSettings(MDataBlock& data, double voxelWidth, PotentialSettings& settings) {
    using Node = DVDBPotentialFlow;
    MStatus status;
    const auto world = data.inputValue(Node::aUseWorldSpace, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (world.asBool()) {
        const auto distance = data.inputValue(Node::aDilation, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double value = distance.asFloat();
        if (!std::isfinite(value) || value <= 0 || !std::isfinite(voxelWidth) || voxelWidth <= 0 ||
            !dommy::numeric::checkedNumericCast(std::ceil(value / voxelWidth), settings.dilation))
            return MS::kInvalidParameter;
    } else {
        const auto dilation = data.inputValue(Node::aDilationVoxels, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.dilation = dilation.asInt();
    }
    if (settings.dilation < 1 || settings.dilation > kMaxPotentialDilation)
        return MS::kInvalidParameter;
    const auto useIterations = data.inputValue(Node::aUseIterations, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (useIterations.asBool()) {
        const auto iterations = data.inputValue(Node::aIterations, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.iterations = iterations.asInt();
    }
    if (settings.iterations < 1 || settings.iterations > DEFAULT_MAX_ITERATIONS)
        return MS::kInvalidParameter;
    const auto useTolerance = data.inputValue(Node::aUseTolerance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (useTolerance.asBool()) {
        const auto tolerance = data.inputValue(Node::aTolerance, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.tolerance = tolerance.asFloat();
    }
    if (!std::isfinite(settings.tolerance) || settings.tolerance < 0)
        return MS::kInvalidParameter;
    const auto useBackground = data.inputValue(Node::aUseBackgroundVelocity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (useBackground.asBool()) {
        const auto background = data.inputValue(Node::aBackgroundVelocity, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float3& value = background.asFloat3();
        settings.background = openvdb::Vec3f(value[0], value[1], value[2]);
        const long double limit = std::sqrt(std::sqrt(
            static_cast<long double>(std::numeric_limits<float>::max()))) / 1024;
        if (!dommy::vdb::ValueComponents<openvdb::Vec3f>::finite(settings.background) ||
            dommy::vdb::TransportMagnitude<openvdb::Vec3f>::get(settings.background) > limit)
            return MS::kInvalidParameter;
        const auto apply = data.inputValue(Node::aApplyBackgroundVelocity, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.applyBackground = apply.asBool();
    }
    const auto potential = data.inputValue(Node::aOutputPotential, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.outputPotential = potential.asBool();
    return MS::kSuccess;
}

// An unconnected socket or empty selection disables an optional input. A
// connected, enabled socket with invalid data must not silently disappear.
MStatus potentialOptionalInput(const MObject& node, const MObject& socket,
                               const MObject& names, MDataBlock& data,
                               bool vectorOnly, GridCPtr& result) {
    result.reset();
    MStatus status;
    const bool connected = MPlug(node, socket).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kSuccess;
    const auto selection = data.inputValue(names, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string text = selection.asString().asChar();
    if (text.empty()) return MS::kSuccess;
    const DOpenVDBData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(socket, data, input));
    GridCPtrVec grids;
    if (!getSelectedGrids(grids, text, *input)) return MS::kInvalidParameter;
    for (const auto& grid : grids) {
        const auto type = getVdbGridType(*grid);
        if (!vectorOnly || type == DVdbGridType::kVec3f || type == DVdbGridType::kVec3d) {
            result = grid;
            return MS::kSuccess;
        }
    }
    return MS::kInvalidParameter;
}

struct PotentialInspect {
    dommy::vdb::TreeEditBudget& budget;
    dommy::math::Bounds3d bounds;
    bool valid = false;
    explicit PotentialInspect(dommy::vdb::TreeEditBudget& b) : budget(b) {}
    template<typename GridT> void operator()(const GridT& grid) {
        using Value = typename GridT::ValueType;
        dommy::vdb::ValuePreflight check(budget.work, 3);
        check(grid);
        if (!check.valid || !dommy::vdb::validTransportTransform(grid) ||
            !dommy::vdb::validDerivativeGridMap(grid.transform())) return;
        // The collider gradient is FloatGrid based even for double velocities.
        const long double limit = std::sqrt(std::sqrt(
            static_cast<long double>(std::numeric_limits<float>::max()))) / 1024;
        long double magnitude = dommy::vdb::TransportMagnitude<Value>::get(grid.background());
        for (auto it = grid.cbeginValueAll(); it; ++it) {
            magnitude = std::max(magnitude, dommy::vdb::TransportMagnitude<Value>::get(*it));
            if (it.isTileValue() && !it.isValueOn() &&
                dommy::vdb::ValueComponents<Value>::matchesBackground(*it, grid.background(), Value(0)))
                continue;
            openvdb::CoordBBox box;
            it.getBoundingBox(box);
            if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
                !bounds.add(box.max()[0], box.max()[1], box.max()[2])) return;
        }
        if (magnitude > limit) return;
        if (!bounds.empty()) {
            openvdb::CoordBBox checked;
            if (!dommy::vdb::indexGridBox(dommy::vdb::gridBounds(bounds), checked)) return;
        }
        valid = true;
    }
};

template<typename GridT>
bool potentialTreeBudget(const openvdb::CoordBBox& box, dommy::vdb::TreeEditBudget& budget,
                         std::size_t copies) {
    using Root = typename GridT::TreeType::RootNodeType::ChildNodeType;
    for (std::size_t i = 0; i < copies; ++i)
        if (!dommy::vdb::TreeNodeBudget<Root>::add(box, budget)) return false;
    return true;
}

struct PotentialSolveBudget {
    const openvdb::CoordBBox& box;
    dommy::vdb::TreeEditBudget& budget;
    const PotentialSettings& settings;
    bool valid = false;
    PotentialSolveBudget(const openvdb::CoordBBox& b, dommy::vdb::TreeEditBudget& limits,
                         const PotentialSettings& s) : box(b), budget(limits), settings(s) {}
    template<typename GridT> void check() {
        using Scalar = typename GridT::template ValueConverter<typename GridT::ValueType::value_type>::Type;
        std::size_t voxels = 0;
        // Dense envelope, including inactive interior tiles. 1024 bytes/voxel
        // reserves the 7-entry matrix, IC factors, index/PCG buffers. Work is
        // measured in voxel sweeps (one PCG iteration plus precursor passes),
        // not FLOPs; these are admission estimates, not allocator/time caps.
        valid = dommy::vdb::treeBlockCount(box, 1, voxels) &&
            budget.addBytes(voxels, 1024) &&
            budget.addWork(voxels, std::size_t(settings.iterations) + 64U +
                           8U * std::size_t(settings.dilation)) &&
            potentialTreeBudget<GridT>(box, budget, 3) &&
            potentialTreeBudget<Scalar>(box, budget, 2) &&
            potentialTreeBudget<openvdb::Int32Grid>(box, budget, 1) &&
            potentialTreeBudget<openvdb::BoolGrid>(box, budget, 4);
    }
};

bool potentialSampleBox(const openvdb::BBoxd& box, const Grid& from, const Grid& to) {
    openvdb::BBoxd mapped;
    openvdb::CoordBBox checked;
    if (!dommy::vdb::validRelativeGridMap(from.transform(), to.transform()) ||
        !dommy::vdb::mapGridBox(box, from.transform(), to.transform(), mapped)) return false;
    mapped.expand(3);
    return dommy::vdb::indexGridBox(mapped, checked, true);
}

struct PotentialMaskOp {
    openvdb::BoolGrid::Ptr result;
    template<typename GridT> void operator()(const GridT& grid) {
        result = openvdb::tools::interiorMask(grid);
    }
};

template<typename GridT> bool finitePotentialGrid(const typename GridT::ConstPtr& grid) {
    if (!grid) return false;
    std::size_t work = 0;
    dommy::vdb::ValuePreflight check(work, 1);
    check(*grid);
    return check.valid;
}

template<typename VelocityGrid>
bool potentialFlow(const openvdb::FloatGrid& solid, const openvdb::BoolGrid& domain,
                   const GridCPtr& velocity, const PotentialSettings& settings,
                   openvdb::math::pcg::State& state, GridPtr& flow, GridPtr& potential) {
    using Vector = typename VelocityGrid::ValueType;
    using Scalar = typename Vector::value_type;
    using ScalarGrid = typename VelocityGrid::template ValueConverter<Scalar>::Type;
    const Vector background(settings.background[0], settings.background[1], settings.background[2]);
    state = openvdb::math::pcg::terminationDefaults<Scalar>();
    state.iterations = settings.iterations;
    state.absoluteError = settings.tolerance;
    state.relativeError = 0;
    if (domain.empty()) {
        flow = VelocityGrid::create(Vector(0));
        potential = ScalarGrid::create(Scalar(0));
        if (!flow || !potential) return false;
        flow->setTransform(domain.transform().copy());
        potential->setTransform(domain.transform().copy());
        state.success = true;
        state.iterations = 0;
        state.absoluteError = 0;
        return true;
    }
    typename VelocityGrid::ConstPtr typedVelocity;
    if (velocity) typedVelocity = openvdb::gridConstPtrCast<VelocityGrid>(velocity);
    const auto neumann = openvdb::tools::createPotentialFlowNeumannVelocities(
        solid, domain, typedVelocity, background);
    if (!finitePotentialGrid<VelocityGrid>(neumann)) return false;
    const auto scalar = dommy::vdb::safeScalarPotential(domain, *neumann, state);
    if (!finitePotentialGrid<ScalarGrid>(scalar) ||
        !std::isfinite(state.absoluteError) || state.absoluteError < 0 ||
        !std::isfinite(state.relativeError) || state.relativeError < 0 ||
        state.iterations < 0 || state.iterations > settings.iterations) return false;
    // Compatibility: a finite unconverged solution remains usable with a warning.
    const auto vector = openvdb::tools::computePotentialFlow(
        *scalar, *neumann, settings.applyBackground ? background : Vector(0));
    if (!finitePotentialGrid<VelocityGrid>(vector)) return false;
    flow = vector;
    potential = scalar;
    return true;
}
} // namespace

const MTypeId DVDBPotentialFlow::typeId( ID_VDB_POTENCIAL_FLOW );

MObject DVDBPotentialFlow::aVdbInput;
MObject DVDBPotentialFlow::aVdbSelectedGridNames;

MObject DVDBPotentialFlow::aVelocityGridName;
MObject DVDBPotentialFlow::aVelocityVdb;
MObject DVDBPotentialFlow::aMaskGridName;
MObject DVDBPotentialFlow::aMaskVdb;
MObject DVDBPotentialFlow::aMaskType;

MObject DVDBPotentialFlow::aUseIterations;
MObject DVDBPotentialFlow::aIterations;

MObject DVDBPotentialFlow::aUseTolerance;
MObject DVDBPotentialFlow::aTolerance;

MObject DVDBPotentialFlow::aUseWorldSpace;
MObject DVDBPotentialFlow::aDilationVoxels;
MObject DVDBPotentialFlow::aDilation;

MObject DVDBPotentialFlow::aUseBackgroundVelocity;
MObject DVDBPotentialFlow::aBackgroundVelocity;
MObject DVDBPotentialFlow::aApplyBackgroundVelocity;

MObject DVDBPotentialFlow::aOutputPotential;

MObject DVDBPotentialFlow::aVdbOutput;

void* DVDBPotentialFlow::creator()
{
    return ::dommy::maya::createObject("DVDBPotentialFlow", [&]() {
        return new DVDBPotentialFlow();
    });
}

MStatus DVDBPotentialFlow::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) return MS::kUnknownParameter;
    const DOpenVDBData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, input));
    MStatus status;
    const auto names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    GridCPtrVec selected;
    if (!getSelectedGrids(selected, names.asString().asChar(), *input)) return MS::kInvalidParameter;
    openvdb::FloatGrid::ConstPtr solid;
    for (const auto& candidate : selected) {
        if (getVdbGridType(*candidate) == DVdbGridType::kFloat &&
            candidate->getGridClass() == openvdb::GRID_LEVEL_SET) {
            solid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(candidate);
            break;
        }
    }
    if (!solid || !solid->hasUniformVoxels() || !std::isfinite(solid->background()) ||
        solid->background() <= 0) return MS::kInvalidParameter;

    PotentialSettings settings;
    DOMMY_RETURN_IF_FAILURE(readPotentialSettings(data, solid->voxelSize()[0], settings));
    GridCPtr velocity, maskSource;
    DOMMY_RETURN_IF_FAILURE(potentialOptionalInput(thisMObject(), aVelocityVdb,
        aVelocityGridName, data, true, velocity));
    DOMMY_RETURN_IF_FAILURE(potentialOptionalInput(thisMObject(), aMaskVdb,
        aMaskGridName, data, false, maskSource));
    if (maskSource) {
        const auto type = data.inputValue(aMaskType, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.maskType = type.asShort();
        if (settings.maskType < 0 || settings.maskType > 1) return MS::kInvalidParameter;
    }

    dommy::vdb::TreeEditBudget budget;
    if (!budget.addBytes(1, solid->memUsage())) return MS::kInvalidParameter;
    PotentialInspect source(budget);
    source(*solid);
    if (!source.valid) return MS::kInvalidParameter;
    openvdb::BBoxd domainBounds;
    openvdb::CoordBBox domainBox;
    const bool hasDomain = !source.bounds.empty();
    if (hasDomain) {
        domainBounds = dommy::vdb::gridBounds(source.bounds);
        domainBounds.expand(settings.dilation + 3.0);
        if (!dommy::vdb::indexGridBox(domainBounds, domainBox, true)) return MS::kInvalidParameter;
    }
    if (velocity) {
        if (!budget.addBytes(1, velocity->memUsage())) return MS::kInvalidParameter;
        PotentialInspect inspect(budget);
        if (!processVdbVectorGrid(getVdbGridType(*velocity), *velocity, inspect) || !inspect.valid ||
            (hasDomain && !potentialSampleBox(domainBounds, *solid, *velocity)))
            return MS::kInvalidParameter;
    }

    if (maskSource) {
        if (!budget.addBytes(1, maskSource->memUsage()) ||
            !dommy::vdb::validTransportTransform(*maskSource) ||
            !dommy::vdb::validDerivativeGridMap(maskSource->transform()) ||
            !dommy::vdb::validRelativeGridMap(maskSource->transform(), solid->transform()) ||
            !dommy::vdb::validRelativeGridMap(solid->transform(), maskSource->transform()))
            return MS::kInvalidParameter;
        PotentialInspect inspect(budget);
        const bool topology = processVdbTopologyGrid(getVdbGridType(*maskSource), *maskSource, inspect);
        if (topology && !inspect.valid) return MS::kInvalidParameter;
        if (maskSource->getGridClass() == openvdb::GRID_LEVEL_SET) {
            dommy::vdb::PositiveMaskBackground background;
            if (!processVdbScalarGrid(getVdbGridType(*maskSource), *maskSource, background) ||
                !background.valid) return MS::kInvalidParameter;
        }
        openvdb::BBoxd bounds;
        bool hasBounds = !inspect.bounds.empty();
        if (hasBounds) bounds = dommy::vdb::gridBounds(inspect.bounds);
        if (!topology) {
            const auto type = getVdbGridType(*maskSource);
            if (type != DVdbGridType::kPointData && type != DVdbGridType::kPointIndex)
                return MS::kInvalidParameter;
            openvdb::BBoxd world;
            if (!maskSource->empty()) {
                if (!getActiveVoxelWorldBounds(*maskSource, world)) return MS::kInvalidParameter;
                openvdb::Vec3d low, high;
                openvdb::math::calculateBounds(maskSource->transform(), world.min(), world.max(), low, high);
                bounds = openvdb::BBoxd(low, high);
                // Resampling visits whole point-mask leaf boxes, including
                // inactive slots outside the active-point voxel bounds.
                bounds.expand(8);
                hasBounds = true;
            }
        }
        if (hasBounds) {
            bounds.expand(3);
            openvdb::CoordBBox box;
            if (!dommy::vdb::indexGridBox(bounds, box, true) ||
                !potentialTreeBudget<openvdb::BoolGrid>(box, budget, 2)) return MS::kInvalidParameter;
            openvdb::BBoxd mapped;
            if (!potentialSampleBox(bounds, *maskSource, *solid) ||
                !dommy::vdb::mapGridBox(bounds, maskSource->transform(), solid->transform(), mapped))
                return MS::kInvalidParameter;
            mapped.expand(3);
            if (!dommy::vdb::indexGridBox(mapped, box, true) ||
                !potentialTreeBudget<openvdb::BoolGrid>(box, budget, 2) ||
                !potentialSampleBox(mapped, *solid, *maskSource)) return MS::kInvalidParameter;
        }
    }
    if (hasDomain) {
        PotentialSolveBudget estimate(domainBox, budget, settings);
        if (velocity && getVdbGridType(*velocity) == DVdbGridType::kVec3d)
            estimate.check<openvdb::Vec3dGrid>();
        else estimate.check<openvdb::Vec3fGrid>();
        if (!estimate.valid) return MS::kInvalidParameter;
    }

    // Every allocation/kernel below follows the complete admission pass.
    openvdb::BoolGrid::Ptr mask;
    if (maskSource) {
        PotentialMaskOp operation;
        if (!(processVdbTopologyGrid(getVdbGridType(*maskSource), *maskSource, operation) ||
              processVdbPointGrid(getVdbGridType(*maskSource), *maskSource, operation)) ||
            !finitePotentialGrid<openvdb::BoolGrid>(operation.result)) return MS::kFailure;
        mask = operation.result;
        if (mask->transform() != solid->transform()) {
            auto resampled = openvdb::BoolGrid::create(false);
            if (!resampled) return MS::kFailure;
            resampled->setTransform(solid->transform().copy());
            openvdb::tools::resampleToMatch<openvdb::tools::PointSampler>(*mask, *resampled);
            mask = resampled;
        }
    }
    const auto domain = openvdb::tools::createPotentialFlowMask<openvdb::FloatGrid, openvdb::BoolGrid>(
        *solid, settings.dilation);
    if (!domain) return MS::kFailure;
    if (mask) {
        if (settings.maskType == 0) domain->tree().topologyIntersection(mask->tree());
        else domain->tree().topologyDifference(mask->tree());
    }
    openvdb::math::pcg::State state;
    GridPtr flow, potential;
    const bool valid = velocity && getVdbGridType(*velocity) == DVdbGridType::kVec3d
        ? potentialFlow<openvdb::Vec3dGrid>(*solid, *domain, velocity, settings, state, flow, potential)
        : potentialFlow<openvdb::Vec3fGrid>(*solid, *domain, velocity, settings, state, flow, potential);
    if (!valid) return MS::kFailure;
    if (!state.success)
        MGlobal::displayWarning("Potential Flow did not converge; output contains a finite approximate solution.");

    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, output));
    if (!output) return MS::kFailure;
    flow->setName("flowvel");
    output->insert(flow);
    if (settings.outputPotential) {
        potential->setName("potential");
        output->insert(potential);
    }
    if (!output->hasValidData()) return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DVDBPotentialFlow")

MStatus DVDBPotentialFlow::initialize() try
{
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    //input vdb
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;
    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    const MObject velocityDefault = strData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityGridName = tAttr.create("VelocityGridName", "velgrd", MFnData::kString, velocityDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityGridName));

    aVelocityVdb = tAttr.create("VelocityVdb", "velvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityVdb));

    const MObject maskDefault = strData.create("mask", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMaskGridName = tAttr.create("MaskGridName", "mskgrd", MFnData::kString, maskDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskGridName));

    aMaskVdb = tAttr.create("MaskVdb", "mskvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskVdb));

    MFnEnumAttribute coltypeAttr;
    aMaskType = coltypeAttr.create("MaskType", "mskty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.addField("Intersection",0));
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.addField("Difference",1));
    DOMMY_RETURN_IF_FAILURE(coltypeAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskType));

    aUseIterations = nAttr.create( "UseIterations", "useitr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseIterations));

    aIterations = nAttr.create( "Iterations", "itr", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterations));

    aUseTolerance = nAttr.create( "UseTolerance", "usetol", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseTolerance));

    aTolerance = nAttr.create( "Tolerance", "tol", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(openvdb::math::Delta<float>::value()));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    aUseWorldSpace = nAttr.create( "UseWorldSpace", "usews", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseWorldSpace));

    aDilationVoxels = nAttr.create( "DilationVoxels", "dltvxl", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDilationVoxels));

    aDilation = nAttr.create( "Dilation", "dlt", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDilation));

    aUseBackgroundVelocity = nAttr.create( "UseBackgroundVelocity", "usebgvel", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseBackgroundVelocity));

    aBackgroundVelocity = nAttr.create( "BackgroundVelocity", "bgvel", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f,0.0f,0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBackgroundVelocity));

    aApplyBackgroundVelocity = nAttr.create( "ApplyBackgroundVelocity", "aplybgvel", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aApplyBackgroundVelocity));

    aOutputPotential = nAttr.create( "OutputPotential", "outptn", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputPotential));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVelocityGridName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVelocityVdb, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskGridName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskVdb, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskType, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseIterations, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterations, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseTolerance, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseWorldSpace, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDilationVoxels, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDilation, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseBackgroundVelocity, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBackgroundVelocity, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aApplyBackgroundVelocity, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputPotential, aVdbOutput));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DVDBPotentialFlow", "initialize",
                                                 std::current_exception());
}

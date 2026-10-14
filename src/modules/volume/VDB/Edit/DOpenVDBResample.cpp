// DOpenVDBResample -- bounded resampling with the archived Maya schema.
#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBResample.h"
#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "dvdb_ids.h"
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MPlug.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaValueValidation.h"
#include "../Common/DVDBComputeGuard.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/LevelSetRebuild.h>
#include <openvdb/tools/VectorTransformer.h>
#include <openvdb/tools/Prune.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

const MTypeId DOpenVDBResample::typeId( ID_VDB_DOPENVDB_RESAMPLE );

MObject DOpenVDBResample::aVdbSelectedGridNames;
MObject DOpenVDBResample::aVdbInput;

MObject DOpenVDBResample::referenceName;
MObject DOpenVDBResample::referenceVdbInput;
MObject DOpenVDBResample::referenceMatrix;

MObject DOpenVDBResample::aVdbOutput;

MObject DOpenVDBResample::interpolationMode;
MObject DOpenVDBResample::defineMode;
MObject DOpenVDBResample::voxelSize;
MObject DOpenVDBResample::voxelScale;
MObject DOpenVDBResample::transformVec;
MObject DOpenVDBResample::isRebuild;
MObject DOpenVDBResample::isPrune;
MObject DOpenVDBResample::tolerance;

void* DOpenVDBResample::creator()
{
    return ::dommy::maya::createObject("DOpenVDBResample", [&]() {
        return new DOpenVDBResample();
    });
}

namespace {
struct ResampleSettings {
    short mode = DOpenVDBResample::MODE_REF_GRID, order = 1;
    bool rebuild = true, prune = true, vectors = false;
    float voxel = 1, scale = 1, tolerance = 0;
};
MStatus readResampleSettings(MDataBlock& data, ResampleSettings& settings) {
    MStatus status;
    const auto mode = data.inputValue(DOpenVDBResample::defineMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.mode = mode.asShort();
    const auto order = data.inputValue(DOpenVDBResample::interpolationMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.order = order.asShort();
    if (settings.mode < DOpenVDBResample::MODE_PARMS ||
        settings.mode > DOpenVDBResample::MODE_VOXEL_SCALE || settings.order < 0 || settings.order > 2)
        return MS::kInvalidParameter;
    if (settings.mode == DOpenVDBResample::MODE_VOXEL_SIZE) {
        const auto voxel = data.inputValue(DOpenVDBResample::voxelSize, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.voxel = voxel.asFloat();
        if (!std::isfinite(settings.voxel) || settings.voxel <= 0) return MS::kInvalidParameter;
    }
    if (settings.mode == DOpenVDBResample::MODE_VOXEL_SCALE) {
        const auto scale = data.inputValue(DOpenVDBResample::voxelScale, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.scale = scale.asFloat();
        if (!std::isfinite(settings.scale) || settings.scale < 1.0f / 64 || settings.scale > 64)
            return MS::kInvalidParameter;
    }
    const auto rebuild = data.inputValue(DOpenVDBResample::isRebuild, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.rebuild = rebuild.asBool();
    const auto prune = data.inputValue(DOpenVDBResample::isPrune, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.prune = prune.asBool();
    if (settings.prune) {
        const auto tolerance = data.inputValue(DOpenVDBResample::tolerance, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.tolerance = tolerance.asFloat();
        if (!std::isfinite(settings.tolerance) || settings.tolerance < 0) return MS::kInvalidParameter;
    }
    if (settings.mode == DOpenVDBResample::MODE_PARMS) {
        const auto vectors = data.inputValue(DOpenVDBResample::transformVec, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.vectors = vectors.asBool();
    }
    return MS::kSuccess;
}

struct ResampleTransform {
    double translate[3] = {0, 0, 0}, rotate[3] = {0, 0, 0};
    double scale[3] = {1, 1, 1}, pivot[3] = {0, 0, 0};
    std::string rotationOrder = "xyz";
};
MStatus readResampleTransform(const MObject& node, MDataBlock& data, ResampleTransform& result) {
    MStatus status;
    const bool connected = MPlug(node, DOpenVDBResample::referenceMatrix).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kSuccess;
    // Read in the datablock's current DG context, without re-entering the plug
    // through a separately constructed MDGContext/MFnDependencyNode.
    const auto handle = data.inputValue(DOpenVDBResample::referenceMatrix, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MMatrix matrix = handle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
    const MTransformationMatrix transform(matrix);
    const MVector translation = transform.getTranslation(MSpace::kWorld, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MTransformationMatrix::RotationOrder order;
    DOMMY_RETURN_IF_FAILURE(transform.getRotation(result.rotate, order));
    DOMMY_RETURN_IF_FAILURE(transform.getScale(result.scale, MSpace::kWorld));
    const MPoint pivot = transform.rotatePivot(MSpace::kWorld, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    result.translate[0] = translation.x; result.translate[1] = translation.y; result.translate[2] = translation.z;
    result.pivot[0] = pivot.x; result.pivot[1] = pivot.y; result.pivot[2] = pivot.z;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(result.translate[axis]) || std::fabs(result.translate[axis]) > 1.0e6 ||
            !std::isfinite(result.pivot[axis]) || std::fabs(result.pivot[axis]) > 1.0e6 ||
            !std::isfinite(result.rotate[axis]) ||
            !std::isfinite(result.scale[axis]) || std::fabs(result.scale[axis]) < 1.0 / 64 ||
            std::fabs(result.scale[axis]) > 64) return MS::kInvalidParameter;
    }
    switch (order) {
        case MTransformationMatrix::kXYZ: result.rotationOrder = "xyz"; break;
        case MTransformationMatrix::kYZX: result.rotationOrder = "yzx"; break;
        case MTransformationMatrix::kZXY: result.rotationOrder = "zxy"; break;
        case MTransformationMatrix::kXZY: result.rotationOrder = "xzy"; break;
        case MTransformationMatrix::kYXZ: result.rotationOrder = "yxz"; break;
        case MTransformationMatrix::kZYX: result.rotationOrder = "zyx"; break;
        default: return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

// Besides the shared Int32 tree padding, leave room for legacy intermediate
// pre/post-scale and mip passes, whose coordinates can exceed final bounds.
bool resampleBox(const openvdb::BBoxd& bounds, openvdb::CoordBBox& checked) {
    if (!dommy::vdb::indexGridBox(bounds, checked, true)) return false;
    for (int axis = 0; axis < 3; ++axis)
        if (std::fabs(bounds.min()[axis]) > 1.0e6 || std::fabs(bounds.max()[axis]) > 1.0e6)
            return false;
    return true;
}
bool resampleMap(const openvdb::math::Transform& from, const openvdb::math::Transform& to) {
    if (!from.isLinear() || !to.isLinear() ||
        !dommy::vdb::validDerivativeGridMap(from) || !dommy::vdb::validDerivativeGridMap(to) ||
        !dommy::vdb::validRelativeGridMap(from, to) || !dommy::vdb::validRelativeGridMap(to, from))
        return false;
    const auto origin = to.worldToIndex(from.indexToWorld(openvdb::Vec3d(0)));
    const auto inverse = from.worldToIndex(to.indexToWorld(openvdb::Vec3d(0)));
    for (int axis = 0; axis < 3; ++axis)
        if (std::fabs(origin[axis]) > 1.0e6 || std::fabs(inverse[axis]) > 1.0e6) return false;
    return true;
}
struct ResampleInspect {
    dommy::vdb::TreeEditBudget& budget;
    const ResampleSettings& settings;
    bool valid = false;
    dommy::math::Bounds3d bounds;
    ResampleInspect(dommy::vdb::TreeEditBudget& b, const ResampleSettings& s) : budget(b), settings(s) {}
    template<typename GridT> void operator()(const GridT& grid) {
        using Value = typename GridT::ValueType;
        dommy::vdb::ValuePreflight values(budget.work, 2);
        values(grid);
        if (!values.valid || !dommy::vdb::validTransportTransform(grid)) return;
        const long double limit = std::is_same<Value, bool>::value ? 1.0L :
            dommy::vdb::TransportMagnitude<Value>::limit() / (settings.vectors ? 64 : 1);
        if (dommy::vdb::TransportMagnitude<Value>::get(grid.background()) > limit) return;
        Value tolerance;
        if (settings.prune && !dommy::vdb::ValueComponents<Value>::tolerance(settings.tolerance, tolerance))
            return;
        for (auto it = grid.cbeginValueAll(); it; ++it) {
            if (dommy::vdb::TransportMagnitude<Value>::get(*it) > limit) return;
            if (it.isTileValue() && !it.isValueOn() &&
                dommy::vdb::ValueComponents<Value>::matchesBackground(*it, grid.background(), Value(0)))
                continue;
            openvdb::CoordBBox box;
            it.getBoundingBox(box);
            if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
                !bounds.add(box.max()[0], box.max()[1], box.max()[2])) return;
        }
        valid = true;
    }
};
struct ResampleBand {
    bool valid = false;
    float halfWidth = 0;
    template<typename GridT> void operator()(const GridT& grid) {
        valid = dommy::vdb::validLevelSetScale(grid) &&
            dommy::numeric::checkedNumericCast(
                static_cast<long double>(grid.background()) / grid.voxelSize()[0], halfWidth) &&
            halfWidth > 0;
    }
};
struct ResampleRegionBudget {
    const openvdb::CoordBBox& bounds;
    dommy::vdb::TreeEditBudget& budget;
    bool rebuild, valid = false;
    ResampleRegionBudget(const openvdb::CoordBBox& b, dommy::vdb::TreeEditBudget& total, bool r)
        : bounds(b), budget(total), rebuild(r) {}
    template<typename GridT> void operator()(const GridT&) {
        using Root = typename GridT::TreeType::RootNodeType::ChildNodeType;
        std::size_t voxels = 0;
        valid = dommy::vdb::treeBlockCount(bounds, 1, voxels) &&
            budget.addBytes(voxels, rebuild ? 1024 : 64) &&
            budget.addWork(voxels, rebuild ? 1024 : 128) &&
            dommy::vdb::TreeNodeBudget<Root>::add(bounds, budget) &&
            dommy::vdb::TreeNodeBudget<Root>::add(bounds, budget);
    }
};
struct ResamplePlan {
    bool supported = false, rebuild = false;
    int order = 1;
    float halfWidth = 0;
    openvdb::math::Transform::Ptr target;
};
bool planResample(const Grid& source, ResamplePlan& plan, const ResampleSettings& settings,
                  dommy::vdb::TreeEditBudget& budget) {
    ResampleInspect inspect(budget, settings);
    const auto type = getVdbGridType(source);
    plan.supported = processVdbTopologyGrid(type, source, inspect);
    if (!plan.supported) return true;
    if (!inspect.valid || !budget.addBytes(2, source.memUsage()) || !plan.target ||
        !resampleMap(source.transform(), *plan.target)) return false;
    plan.order = type == DVdbGridType::kBool ? 0 : settings.order;
    plan.rebuild = settings.rebuild && source.getGridClass() == openvdb::GRID_LEVEL_SET &&
        (type == DVdbGridType::kFloat || type == DVdbGridType::kDouble);
    if (plan.rebuild) {
        ResampleBand band;
        if (!processVdbRealGrid(type, source, band) || !band.valid ||
            !plan.target->hasUniformScale()) return false;
        plan.halfWidth = band.halfWidth;
    }
    if (inspect.bounds.empty()) return true;
    auto input = dommy::vdb::gridBounds(inspect.bounds);
    input.expand(8); // Whole leaf endpoints and source sampler neighborhoods.
    openvdb::BBoxd output, samples;
    openvdb::CoordBBox region;
    if (!resampleBox(input, region) ||
        !dommy::vdb::mapGridBox(input, source.transform(), *plan.target, output)) return false;
    output.expand(plan.rebuild ? plan.halfWidth + 8.0 : 3.0);
    if (!resampleBox(output, region) ||
        !dommy::vdb::mapGridBox(output, *plan.target, source.transform(), samples)) return false;
    samples.expand(3);
    if (!resampleBox(samples, region)) return false;
    // Charge source/output and a rotation/mip working envelope. Native peaks
    // are estimates, not hard allocator/time limits. Keep the whole batch's
    // budget before any tree copy or kernel.
    const auto a = input.extents(), b = output.extents();
    const double radius = 0.5 * std::max(std::hypot(std::hypot(a[0], a[1]), a[2]),
                                         std::hypot(std::hypot(b[0], b[1]), b[2])) + 3;
    const openvdb::BBoxd intermediate(openvdb::Vec3d(-radius), openvdb::Vec3d(radius));
    const openvdb::BBoxd boxes[] = {input, output, intermediate};
    // Rebuilding uses mesh/level-set work, not the mipmapped sampler envelope.
    const int count = plan.rebuild ? 2 : 3;
    for (int i = 0; i < count; ++i) {
        if (!resampleBox(boxes[i], region)) return false;
        ResampleRegionBudget cost(region, budget, plan.rebuild);
        if (!processVdbTopologyGrid(type, source, cost) || !cost.valid) return false;
    }
    return true;
}
struct ResampleRebuild {
    const openvdb::math::Transform& transform;
    float halfWidth;
    GridPtr result;
    ResampleRebuild(const openvdb::math::Transform& t, float width) : transform(t), halfWidth(width) {}
    template<typename GridT> void operator()(const GridT& grid) {
        openvdb::util::NullInterrupter interrupter;
        result = openvdb::tools::doLevelSetRebuild(grid, typename GridT::ValueType(0),
                                                  halfWidth, halfWidth, &transform, &interrupter);
    }
};
struct ResamplePrune {
    double tolerance;
    bool valid = false;
    explicit ResamplePrune(double value) : tolerance(value) {}
    template<typename GridT> void operator()(GridT& grid) {
        using Value = typename GridT::ValueType;
        Value converted;
        if (!dommy::vdb::ValueComponents<Value>::tolerance(tolerance, converted)) return;
        dommy::vdb::PruneRange<Value> range;
        if (!range.add(grid.background())) return;
        for (auto it = grid.cbeginValueAll(); it; ++it) if (!range.add(*it)) return;
        openvdb::tools::prune(grid.tree(), converted);
        valid = true;
    }
};
struct ResampleVectors {
    const openvdb::Mat4d& matrix;
    explicit ResampleVectors(const openvdb::Mat4d& m) : matrix(m) {}
    template<typename GridT> void operator()(GridT& grid) const {
        openvdb::tools::transformVectors(grid, matrix);
    }
};
bool finiteResampleResult(const GridCPtr& result, DVdbGridType expected, std::size_t& work) {
    if (!result || getVdbGridType(*result) != expected) return false;
    dommy::vdb::ValuePreflight values(work, 1);
    return processVdbTopologyGrid(expected, *result, values) && values.valid;
}
} // namespace

MStatus DOpenVDBResample::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) return MS::kUnknownParameter;
    const DOpenVDBData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, input));
    MStatus status;
    const auto names = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = names.asString().asChar();
    GridCPtrVec grids;
    getSelectedGrids(grids, selection, *input);
    ResampleSettings settings;
    std::unique_ptr<openvdb::tools::GridTransformer> transform;
    openvdb::math::Transform::Ptr reference;
    if (!grids.empty()) {
        DOMMY_RETURN_IF_FAILURE(readResampleSettings(data, settings));
        if (settings.mode == MODE_REF_GRID) {
            const DOpenVDBData* references = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputVDB(referenceVdbInput, data, references));
            const auto refNames = data.inputValue(referenceName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            GridCPtrVec selected;
            if (!getSelectedGrids(selected, refNames.asString().asChar(), *references))
                return MS::kInvalidParameter;
            reference = selected.front()->transform().copy();
            if (!reference) return MS::kFailure;
        } else if (settings.mode == MODE_VOXEL_SIZE) {
            reference = openvdb::math::Transform::createLinearTransform(settings.voxel);
        } else if (settings.mode == MODE_PARMS) {
            ResampleTransform values;
            DOMMY_RETURN_IF_FAILURE(readResampleTransform(thisMObject(), data, values));
            const openvdb::Vec3d pivot(values.pivot[0], values.pivot[1], values.pivot[2]);
            const openvdb::Vec3d scale(values.scale[0], values.scale[1], values.scale[2]);
            const openvdb::Vec3d rotate(values.rotate[0], values.rotate[1], values.rotate[2]);
            const openvdb::Vec3d translate(values.translate[0], values.translate[1], values.translate[2]);
            // Scale is bounded before the legacy constructor's mip-level shifts.
            transform.reset(new openvdb::tools::GridTransformer(
                pivot, scale, rotate, translate, "tsr", values.rotationOrder));
        }
    }

    dommy::vdb::TreeEditBudget budget;
    std::vector<ResamplePlan> plans(grids.size());
    for (std::size_t i = 0; i < grids.size(); ++i) {
        auto& plan = plans[i];
        const auto& source = *grids[i];
        if (getVdbGridTupleSize(getVdbGridType(source)) == 0) continue;
        if (settings.mode == MODE_PARMS) {
            plan.target = source.transform().copy();
            if (!plan.target) return MS::kFailure;
            plan.target->preMult(transform->getTransform().inverse());
        } else if (settings.mode == MODE_VOXEL_SCALE) {
            plan.target = openvdb::math::Transform::createLinearTransform();
            if (!plan.target) return MS::kFailure;
            const auto size = source.voxelSize();
            openvdb::Vec3d scaled;
            for (int axis = 0; axis < 3; ++axis)
                if (!dommy::numeric::checkedNumericCast(
                    static_cast<long double>(size[axis]) * settings.scale, scaled[axis]) ||
                    scaled[axis] <= 0) return MS::kInvalidParameter;
            plan.target->preScale(scaled);
        } else plan.target = reference;
        if (!planResample(source, plan, settings, budget)) return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, output));
    if (!output) return MS::kFailure;
    getSelectedGrids(grids, selection, *input, *output); // Preserve unselected-first ordering.
    std::size_t outputWork = 0;
    for (std::size_t i = 0; i < grids.size(); ++i) {
        const auto& source = grids[i];
        const auto& plan = plans[i];
        if (!plan.supported) { output->insert(*source); continue; }
        const auto type = getVdbGridType(*source);
        GridPtr outGrid;
        bool processed = false;
        if (plan.rebuild) {
            ResampleRebuild operation(*plan.target, plan.halfWidth);
            processed = processVdbRealGrid(type, *source, operation);
            outGrid = operation.result;
            if (!outGrid || outGrid.get() == source.get() || getVdbGridType(*outGrid) != type)
                return MS::kFailure;
            if (settings.mode == MODE_PARMS) {
                const auto outputMap = source->transform().copy();
                if (!outputMap) return MS::kFailure;
                outGrid->setTransform(outputMap);
            }
        } else {
            auto copy = source->deepCopyGrid();
            if (!copy || copy.get() == source.get() || getVdbGridType(*copy) != type) return MS::kFailure;
            if (source->getGridClass() == openvdb::GRID_LEVEL_SET &&
                (type == DVdbGridType::kFloat || type == DVdbGridType::kDouble))
                copy->setGridClass(openvdb::GRID_UNKNOWN);
            outGrid = copy->copyGridWithNewTree();
            if (!outGrid || outGrid.get() == source.get() || outGrid.get() == copy.get() ||
                getVdbGridType(*outGrid) != type) return MS::kFailure;
            if (settings.mode != MODE_PARMS) {
                const auto outputMap = plan.target->copy();
                if (!outputMap) return MS::kFailure;
                outGrid->setTransform(outputMap);
            }
            // Dispatch the metadata-adjusted COPY, never the original level set.
            if (settings.mode == MODE_PARMS) {
                if (plan.order == 0) {
                    GridTransformOp<openvdb::tools::PointSampler> operation(outGrid, *transform);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                } else if (plan.order == 1) {
                    GridTransformOp<openvdb::tools::BoxSampler> operation(outGrid, *transform);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                } else {
                    GridTransformOp<openvdb::tools::QuadraticSampler> operation(outGrid, *transform);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                }
            } else {
                if (plan.order == 0) {
                    GridResampleToMatchOp<openvdb::tools::PointSampler> operation(outGrid);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                } else if (plan.order == 1) {
                    GridResampleToMatchOp<openvdb::tools::BoxSampler> operation(outGrid);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                } else {
                    GridResampleToMatchOp<openvdb::tools::QuadraticSampler> operation(outGrid);
                    processed = processVdbTopologyGrid(type, *copy, operation);
                }
            }
        }
        if (!processed || !outGrid || outGrid.get() == source.get() ||
            !finiteResampleResult(outGrid, type, outputWork)) return MS::kFailure;
        if (settings.mode == MODE_PARMS && settings.vectors && outGrid->isInWorldSpace() &&
            outGrid->getVectorType() != openvdb::VEC_INVARIANT) {
            ResampleVectors operation(transform->getTransform());
            processVdbVectorGrid(type, *outGrid, operation);
            if (!finiteResampleResult(outGrid, type, outputWork)) return MS::kFailure;
        }
        if (settings.prune) {
            ResamplePrune operation(settings.tolerance);
            if (!processVdbTopologyGrid(type, *outGrid, operation) || !operation.valid ||
                !finiteResampleResult(outGrid, type, outputWork)) return MS::kFailure;
        }
        output->insert(outGrid);
    }
    if (!output->hasValidData()) return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBResample")

MStatus DOpenVDBResample::initialize() try
{
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;

    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    const MObject referenceDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    referenceName = tAttr.create("referenceName", "refn", MFnData::kString, referenceDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(referenceName));

    MFnMatrixAttribute matAttr;

    referenceMatrix = matAttr.create( "referenceMatrix", "refmat",MFnMatrixAttribute::kDouble, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(referenceMatrix));

    MFnEnumAttribute eAttr;

    interpolationMode = eAttr.create("interpolationMode", "inpm", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Nearest", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Linear", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Quadratic", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(interpolationMode));

    MFnEnumAttribute defAttr;
    defineMode = defAttr.create("defineMode", "dfm", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(defAttr.addField("Explicitly", MODE_PARMS));
    DOMMY_RETURN_IF_FAILURE(defAttr.addField("To Match Reference VDB", MODE_REF_GRID));
    DOMMY_RETURN_IF_FAILURE(defAttr.addField("Using Voxel Size Only", MODE_VOXEL_SIZE));
    DOMMY_RETURN_IF_FAILURE(defAttr.addField("Using Voxel Scale Only", MODE_VOXEL_SCALE));
    DOMMY_RETURN_IF_FAILURE(defAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(defineMode));

    voxelSize = nAttr.create("voxelSize", "vsi", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.000001f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(voxelSize));

    voxelScale = nAttr.create("voxelScale", "vsc", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.000001f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(voxelScale));

    transformVec = nAttr.create("transformVec", "tvec", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(transformVec));

    isRebuild = nAttr.create("isRebuild", "isr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isRebuild));

    isPrune = nAttr.create("isPrune", "ipr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isPrune));

    tolerance = nAttr.create("tolerance", "tor", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(tolerance));

    // input / output

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    referenceVdbInput = tAttr.create("referenceVdbInput", "refvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(referenceVdbInput));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(referenceVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(referenceName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(referenceMatrix, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(interpolationMode, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(defineMode, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(voxelSize, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(voxelScale, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(transformVec, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isRebuild, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isPrune, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(tolerance, aVdbOutput));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBResample", "initialize", std::current_exception());
}

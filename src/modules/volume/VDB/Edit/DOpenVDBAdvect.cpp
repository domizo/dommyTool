//
//  DOpenVDBAdvect.cpp
//  dommyTool
//
//  Created by yuzo on 1/10/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBAdvect.h"
#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBTransportPolicy.h"
#include "DMayaHeaders.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "dvdb_ids.h"
#include <openvdb/math/FiniteDifference.h>
#include <openvdb/util/NullInterrupter.h>
#include <openvdb/tools/VolumeAdvect.h>
#include <openvdb/tools/LevelSetAdvect.h>
#include <openvdb/tools/Interpolation.h>
#include <memory>

const MTypeId DOpenVDBAdvect::typeId(ID_VDB_DOPENVDB_VEC_ADVECT);
MObject DOpenVDBAdvect::aVdbInput;
MObject DOpenVDBAdvect::aVdbSelectedGridNames;
MObject DOpenVDBAdvect::aVdbVelocityInput;
MObject DOpenVDBAdvect::aVdbVelocityGridNames;
MObject DOpenVDBAdvect::aRespectGridClass;
MObject DOpenVDBAdvect::aTimestep;
MObject DOpenVDBAdvect::aSubsteps;
MObject DOpenVDBAdvect::aAdvectionScheme;
MObject DOpenVDBAdvect::aLimiterScheme;
MObject DOpenVDBAdvect::aSpatialScheme;
MObject DOpenVDBAdvect::aTemporalScheme;
MObject DOpenVDBAdvect::aRenormalizationSteps;
MObject DOpenVDBAdvect::aSpatialRenormalization;
MObject DOpenVDBAdvect::aTemporalRenormalization;
MObject DOpenVDBAdvect::aVdbOutput;

void* DOpenVDBAdvect::creator() {
    return ::dommy::maya::createObject("DOpenVDBAdvect", [&]() {
        return new DOpenVDBAdvect();
    });
}

namespace {
using dommy::vdb::TransportPlan;
using dommy::vdb::TransportSettings;

MStatus readAdvectionSettings(MDataBlock& data, TransportSettings& settings) {
    using Node = DOpenVDBAdvect;
    MStatus stat;
    const MDataHandle time = data.inputValue(Node::aTimestep, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!dommy::numeric::checkedNumericCast(time.asDouble(), settings.time))
        return MS::kInvalidParameter;
    const MDataHandle spatial = data.inputValue(Node::aSpatialScheme, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.spatial = spatial.asShort();
    const MDataHandle temporal = data.inputValue(Node::aTemporalScheme, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.temporal = temporal.asShort();
    const MDataHandle renormSpatial = data.inputValue(Node::aSpatialRenormalization, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.renormSpatial = renormSpatial.asShort();
    const MDataHandle renormTemporal = data.inputValue(Node::aTemporalRenormalization, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.renormTemporal = renormTemporal.asShort();
    const MDataHandle normCount = data.inputValue(Node::aRenormalizationSteps, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.normCount = normCount.asInt();
    const MDataHandle substeps = data.inputValue(Node::aSubsteps, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.substeps = substeps.asInt();
    const MDataHandle integrator = data.inputValue(Node::aAdvectionScheme, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.integrator = integrator.asShort();
    const MDataHandle limiter = data.inputValue(Node::aLimiterScheme, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.limiter = limiter.asShort();
    const MDataHandle respect = data.inputValue(Node::aRespectGridClass, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.respectClass = respect.asBool();
    if (!dommy::vdb::validTransportSettings(settings))
        return MS::kInvalidParameter;
    if (settings.integrator == 4 && settings.limiter == 0)
        MGlobal::displayWarning("MacCormack is unstable without a limiter.");
    return MS::kSuccess;
}

bool supportedAdvectionGrid(const Grid& grid, const TransportSettings& settings) {
    const bool real = grid.isType<openvdb::FloatGrid>() || grid.isType<openvdb::DoubleGrid>();
    return real || (!(settings.respectClass && grid.getGridClass() == openvdb::GRID_LEVEL_SET) &&
                    (grid.isType<openvdb::Vec3SGrid>() || grid.isType<openvdb::Vec3DGrid>()));
}

template <typename GridT, typename VelocityT>
bool preflightAdvection(const Grid& base, const VelocityT& velocity, const TransportSettings& s,
                        long double speed, std::size_t& work, TransportPlan& plan) {
    if (!base.isType<GridT>())
        return false;
    const auto& grid = vdbGridCast<GridT>(base);
    long double magnitude = 0;
    const bool levelSet = s.respectClass && grid.getGridClass() == openvdb::GRID_LEVEL_SET;
    if (!grid.hasUniformVoxels() || !dommy::vdb::validTransportTransform(grid) ||
        !dommy::vdb::transportValues(grid, work, magnitude) ||
        !dommy::vdb::makeTransportPlan(s, speed, grid.voxelSize()[0], levelSet, plan) ||
        !dommy::vdb::transportWork(grid, plan, work) ||
        !dommy::vdb::transportDomain(grid, velocity, plan) ||
        !dommy::vdb::transportDomain(grid, grid, plan))
        return false;
    return true;
}

template <typename GridT>
bool preflightAdvectedLevelSet(const Grid& base, long double speed, std::size_t& work) {
    const auto& grid = vdbGridCast<GridT>(base);
    typedef typename GridT::ValueType ValueT;
    long double magnitude = 0;
    return dommy::vdb::validLevelSetScale(grid) &&
           dommy::vdb::transportValues(grid, work, magnitude) &&
           magnitude / grid.voxelSize()[0] <= dommy::vdb::kMaxFilterWidth &&
           speed <= std::sqrt(static_cast<long double>(std::numeric_limits<ValueT>::max())) / 64;
}

template <typename GridT, typename VelocityT, typename SamplerT>
bool advectLevelSet(Grid& base, const VelocityT& velocity, const TransportSettings& settings,
                    openvdb::util::NullInterrupter& boss) {
    if (!base.isType<GridT>())
        return false;
    auto& grid = vdbGridCast<GridT>(base);
    typedef openvdb::tools::DiscreteField<VelocityT, SamplerT> FieldT;
    const FieldT field(velocity);
    openvdb::tools::LevelSetAdvection<GridT, FieldT, openvdb::util::NullInterrupter> operation(
        grid, field, &boss);
    dommy::vdb::configureTransport(operation, settings);
    operation.advect(0, settings.time);
    std::size_t work = 0;
    long double magnitude = 0;
    return !boss.wasInterrupted() && dommy::vdb::transportValues(grid, work, magnitude) &&
           grid.activeVoxelCount() <= dommy::generation::kMaxWorkItemCount;
}

template <typename GridT, typename VolumeAdvectionT>
bool applyAdvection(const Grid& source, VolumeAdvectionT& operation,
                    const TransportSettings& settings, openvdb::util::NullInterrupter& boss,
                    DOpenVDBData& output) {
    if (!source.isType<GridT>())
        return false;
    const auto result = operation.template advect<GridT, openvdb::tools::Sampler<1, false>>(
        vdbGridCast<GridT>(source), settings.time);
    std::size_t work = 0;
    long double magnitude = 0;
    if (!result || boss.wasInterrupted() ||
        !dommy::vdb::transportValues(*result, work, magnitude) ||
        result->activeVoxelCount() > dommy::generation::kMaxWorkItemCount)
        return false;
    output.insert(result);
    return true;
}

template <typename VelocityGridT, bool StaggeredVelocity>
bool processGrids(DOpenVDBData& output, const GridCPtrVec& grids, const Grid& velocityBase,
                  const TransportSettings& settings, openvdb::util::NullInterrupter& boss) {
    if (!velocityBase.isType<VelocityGridT>())
        return false;
    const auto& velocity = vdbGridCast<VelocityGridT>(velocityBase);
    std::size_t work = 0;
    long double speed = 0;
    if (!dommy::vdb::validTransportTransform(velocity) ||
        !dommy::vdb::transportValues(velocity, work, speed) ||
        velocity.activeVoxelCount() > dommy::generation::kMaxWorkItemCount)
        return false;
    // Validate the entire batch before the first grid copy/native constructor.
    for (const auto& source : grids) {
        if (!source)
            return false;
        if (!supportedAdvectionGrid(*source, settings))
            continue;
        TransportPlan plan;
        bool valid = false;
        if (source->isType<openvdb::FloatGrid>())
            valid = preflightAdvection<openvdb::FloatGrid>(*source, velocity, settings, speed, work,
                                                           plan);
        else if (source->isType<openvdb::DoubleGrid>())
            valid = preflightAdvection<openvdb::DoubleGrid>(*source, velocity, settings, speed,
                                                            work, plan);
        else if (source->isType<openvdb::Vec3SGrid>())
            valid = preflightAdvection<openvdb::Vec3SGrid>(*source, velocity, settings, speed, work,
                                                           plan);
        else if (source->isType<openvdb::Vec3DGrid>())
            valid = preflightAdvection<openvdb::Vec3DGrid>(*source, velocity, settings, speed, work,
                                                           plan);
        if (valid && settings.respectClass && source->getGridClass() == openvdb::GRID_LEVEL_SET)
            valid = source->isType<openvdb::FloatGrid>()
                        ? preflightAdvectedLevelSet<openvdb::FloatGrid>(*source, speed, work)
                        : preflightAdvectedLevelSet<openvdb::DoubleGrid>(*source, speed, work);
        if (!valid)
            return false;
    }
    using VolumeAdvection = openvdb::tools::VolumeAdvection<VelocityGridT, StaggeredVelocity,
                                                            openvdb::util::NullInterrupter>;
    std::unique_ptr<VolumeAdvection> volume;
    for (const auto& source : grids) {
        if (boss.wasInterrupted())
            return false;
        if (!supportedAdvectionGrid(*source, settings)) {
            MGlobal::displayWarning(
                ("Skipped unsupported advection grid: " + source->getName()).c_str());
            continue;
        }
        if (settings.respectClass && source->getGridClass() == openvdb::GRID_LEVEL_SET) {
            auto copy = source->deepCopyGrid();
            if (!copy)
                return false;
            using SamplerT =
                typename std::conditional<StaggeredVelocity, openvdb::tools::StaggeredBoxSampler,
                                          openvdb::tools::BoxSampler>::type;
            const bool success = source->isType<openvdb::FloatGrid>()
                                     ? advectLevelSet<openvdb::FloatGrid, VelocityGridT, SamplerT>(
                                           *copy, velocity, settings, boss)
                                     : advectLevelSet<openvdb::DoubleGrid, VelocityGridT, SamplerT>(
                                           *copy, velocity, settings, boss);
            if (!success)
                return false;
            // Both FloatGrid and DoubleGrid take the same insertion path.
            output.insert(copy);
        } else {
            // The constructor scans velocity values. Reuse it for the whole
            // batch instead of rescanning the same field for every source.
            if (!volume) {
                volume.reset(new VolumeAdvection(velocity, &boss));
                volume->setIntegrator(
                    static_cast<openvdb::tools::Scheme::SemiLagrangian>(settings.integrator));
                volume->setLimiter(static_cast<openvdb::tools::Scheme::Limiter>(settings.limiter));
                volume->setSubSteps(settings.substeps);
            }
            bool success = false;
            if (source->isType<openvdb::FloatGrid>())
                success =
                    applyAdvection<openvdb::FloatGrid>(*source, *volume, settings, boss, output);
            else if (source->isType<openvdb::DoubleGrid>())
                success =
                    applyAdvection<openvdb::DoubleGrid>(*source, *volume, settings, boss, output);
            else if (source->isType<openvdb::Vec3SGrid>())
                success =
                    applyAdvection<openvdb::Vec3SGrid>(*source, *volume, settings, boss, output);
            else if (source->isType<openvdb::Vec3DGrid>())
                success =
                    applyAdvection<openvdb::Vec3DGrid>(*source, *volume, settings, boss, output);
            if (!success)
                return false;
        }
    }
    return output.hasValidData();
}
} // namespace

MStatus DOpenVDBAdvect::compute(const MPlug& plug, MDataBlock& data) try {
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
    TransportSettings settings;
    DOMMY_RETURN_IF_FAILURE(readAdvectionSettings(data, settings));
    const MDataHandle velocityHandle = data.inputValue(aVdbVelocityInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DOpenVDBData* velocityVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(velocityHandle, DOpenVDBData::id, velocityVdb));
    if (!velocityVdb || !velocityVdb->hasValidData())
        return MS::kInvalidParameter;
    const MDataHandle velocityName = data.inputValue(aVdbVelocityGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    GridCPtrVec velocities;
    getSelectedGrids(velocities, velocityName.asString().asChar(), *velocityVdb);
    if (velocities.empty())
        return MS::kInvalidParameter;
    const auto& velocity = *velocities.front(); // Preserve first-selected-grid semantics.
    const DVdbGridType velocityType = getVdbGridType(velocity);
    if (velocityType != DVdbGridType::kVec3f && velocityType != DVdbGridType::kVec3d)
        return MS::kInvalidParameter;
    const MDataHandle selection = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    GridCPtrVec grids;
    getSelectedGrids(grids, selection.asString().asChar(), *inputVdb);
    if (grids.empty())
        return MS::kInvalidParameter;
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    openvdb::util::NullInterrupter boss;
    const bool staggered = velocity.getGridClass() == openvdb::GRID_STAGGERED;
    bool success = false;
    if (velocityType == DVdbGridType::kVec3f)
        success = staggered ? processGrids<openvdb::Vec3SGrid, true>(*outputVdb, grids, velocity,
                                                                     settings, boss)
                            : processGrids<openvdb::Vec3SGrid, false>(*outputVdb, grids, velocity,
                                                                      settings, boss);
    else
        success = staggered ? processGrids<openvdb::Vec3DGrid, true>(*outputVdb, grids, velocity,
                                                                     settings, boss)
                            : processGrids<openvdb::Vec3DGrid, false>(*outputVdb, grids, velocity,
                                                                      settings, boss);
    if (!success) {
        MGlobal::displayError("Advection failed validation or exceeded the runtime safety budget.");
        return MS::kFailure;
    }
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBAdvect")

MStatus DOpenVDBAdvect::initialize() try {
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

    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    // input velocity vdb
    aVdbVelocityInput =
        tAttr.create("VdbVelocityInput", "vdbvelinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbVelocityInput));

    const MObject aVdbVelocityGridNamesDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbVelocityGridNames = tAttr.create("VelocityGridNames", "vgrids", MFnData::kString,
                                         aVdbVelocityGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbVelocityGridNames));

    aRespectGridClass =
        nAttr.create("RespectGridClass", "regrcl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRespectGridClass));

    aTimestep = nAttr.create("Timestep", "ts", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimestep));

    aSubsteps = nAttr.create("Substeps", "sst", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));

    MFnEnumAttribute AdScAttr;
    aAdvectionScheme = AdScAttr.create("AdvectionScheme", "adsc", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("Semi-Lagrangian", 0));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("Mid-Point", 1));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("3rd order Runge-Kutta", 2));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("4th order Runge-Kutta", 3));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("MacCormack", 4));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.addField("BFECC", 5));
    DOMMY_RETURN_IF_FAILURE(AdScAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAdvectionScheme));

    MFnEnumAttribute LiScAttr;
    aLimiterScheme = LiScAttr.create("LimiterScheme", "lisc", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(LiScAttr.addField("No limiter", 0));
    DOMMY_RETURN_IF_FAILURE(LiScAttr.addField("Clamp to extrema", 1));
    DOMMY_RETURN_IF_FAILURE(LiScAttr.addField("Revert to 1st order", 2));
    DOMMY_RETURN_IF_FAILURE(LiScAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLimiterScheme));

    MFnEnumAttribute eAttr;
    aSpatialScheme = eAttr.create("SpatialScheme", "spsc", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        eAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::FIRST_BIAS).c_str(), 0));
    DOMMY_RETURN_IF_FAILURE(
        eAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::HJWENO5_BIAS).c_str(), 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpatialScheme));

    MFnEnumAttribute advtmpAttr;
    aTemporalScheme = advtmpAttr.create("TemporalScheme", "tmpsc", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::math::NUM_TEMPORAL_SCHEMES; ++i) {
        const openvdb::math::TemporalIntegrationScheme it =
            static_cast<openvdb::math::TemporalIntegrationScheme>(i);
        DOMMY_RETURN_IF_FAILURE(
            advtmpAttr.addField(temporalIntegrationSchemeToMenuName(it).c_str(), it));
    }
    DOMMY_RETURN_IF_FAILURE(advtmpAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTemporalScheme));

    aRenormalizationSteps =
        nAttr.create("RenormalizationSteps", "renost", MFnNumericData::kInt, 3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRenormalizationSteps));

    MFnEnumAttribute renosptAttr;
    aSpatialRenormalization = renosptAttr.create("SpatialRenormalization", "sprenorm", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        renosptAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::FIRST_BIAS).c_str(), 0));
    DOMMY_RETURN_IF_FAILURE(renosptAttr.addField(
        biasedGradientSchemeToMenuName(openvdb::math::HJWENO5_BIAS).c_str(), 1));
    DOMMY_RETURN_IF_FAILURE(renosptAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpatialRenormalization));

    MFnEnumAttribute renotmpAttr;
    aTemporalRenormalization = renotmpAttr.create("TemporalRenormalization", "rmprenorm", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::math::NUM_TEMPORAL_SCHEMES; ++i) {
        const openvdb::math::TemporalIntegrationScheme it =
            static_cast<openvdb::math::TemporalIntegrationScheme>(i);
        DOMMY_RETURN_IF_FAILURE(
            renotmpAttr.addField(temporalIntegrationSchemeToMenuName(it).c_str(), it));
    }
    DOMMY_RETURN_IF_FAILURE(renotmpAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTemporalRenormalization));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbVelocityGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRespectGridClass, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimestep, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSubsteps, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdvectionScheme, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLimiterScheme, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpatialScheme, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTemporalScheme, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRenormalizationSteps, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpatialRenormalization, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTemporalRenormalization, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBAdvect", "initialize",
                                                  std::current_exception());
}

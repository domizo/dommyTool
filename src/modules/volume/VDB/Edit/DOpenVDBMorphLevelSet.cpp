//
//  DOpenVDBMorphLevelSet.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/06.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBMorphLevelSet.h"
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
#include <openvdb/tools/LevelSetMorph.h>

const MTypeId DOpenVDBMorphLevelSet::typeId(ID_VDB_DOPENVDB_MORPH);
MObject DOpenVDBMorphLevelSet::aVdbInput;
MObject DOpenVDBMorphLevelSet::aVdbSelectedGridNames;
MObject DOpenVDBMorphLevelSet::aTargetVdbInput;
MObject DOpenVDBMorphLevelSet::aTargetVdbSelectedGridNames;
MObject DOpenVDBMorphLevelSet::aMaskName;
MObject DOpenVDBMorphLevelSet::aInvertMask;
MObject DOpenVDBMorphLevelSet::aMinMask;
MObject DOpenVDBMorphLevelSet::aMaxMask;
MObject DOpenVDBMorphLevelSet::aMaskVdbInput;
MObject DOpenVDBMorphLevelSet::aTimestep;
MObject DOpenVDBMorphLevelSet::aAdvectSpatial;
MObject DOpenVDBMorphLevelSet::aAdvectTemporal;
MObject DOpenVDBMorphLevelSet::aSteps;
MObject DOpenVDBMorphLevelSet::aRenormSpatial;
MObject DOpenVDBMorphLevelSet::aRenormTemporal;
MObject DOpenVDBMorphLevelSet::aVdbOutput;

void* DOpenVDBMorphLevelSet::creator() {
    return ::dommy::maya::createObject("DOpenVDBMorphLevelSet", [&]() {
        return new DOpenVDBMorphLevelSet();
    });
}

namespace {
MStatus readMorphSettings(MDataBlock& data, dommy::vdb::TransportSettings& settings) {
    using Node = DOpenVDBMorphLevelSet;
    MStatus stat;
    const MDataHandle time = data.inputValue(Node::aTimestep, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!dommy::numeric::checkedNumericCast(time.asDouble(), settings.time))
        return MS::kInvalidParameter;
    const MDataHandle spatial = data.inputValue(Node::aAdvectSpatial, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.spatial = spatial.asShort();
    const MDataHandle temporal = data.inputValue(Node::aAdvectTemporal, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.temporal = temporal.asShort();
    const MDataHandle renormSpatial = data.inputValue(Node::aRenormSpatial, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.renormSpatial = renormSpatial.asShort();
    const MDataHandle renormTemporal = data.inputValue(Node::aRenormTemporal, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.renormTemporal = renormTemporal.asShort();
    const MDataHandle steps = data.inputValue(Node::aSteps, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.normCount = steps.asInt();
    const MDataHandle maskMin = data.inputValue(Node::aMinMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!dommy::numeric::checkedNumericCast(maskMin.asDouble(), settings.maskMin))
        return MS::kInvalidParameter;
    const MDataHandle maskMax = data.inputValue(Node::aMaxMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!dommy::numeric::checkedNumericCast(maskMax.asDouble(), settings.maskMax))
        return MS::kInvalidParameter;
    const MDataHandle invert = data.inputValue(Node::aInvertMask, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.invertMask = invert.asBool();
    return dommy::vdb::validTransportSettings(settings) ? MS::kSuccess : MS::kInvalidParameter;
}

bool applyMorph(openvdb::FloatGrid& grid, const openvdb::FloatGrid& target,
                const openvdb::FloatGrid* mask, const dommy::vdb::TransportSettings& settings,
                openvdb::util::NullInterrupter& boss) {
    // OpenVDB 5.2's morph loop only advances when time1 > time0. Keep the
    // historical finite zero/negative-time no-op instead of inventing reversal.
    if (settings.time > 0) {
        openvdb::tools::LevelSetMorphing<openvdb::FloatGrid, openvdb::util::NullInterrupter>
            operation(grid, target, &boss);
        if (mask) {
            operation.setAlphaMask(*mask);
            operation.setMaskRange(settings.maskMin, settings.maskMax);
            operation.invertMask(settings.invertMask);
        }
        dommy::vdb::configureTransport(operation, settings);
        operation.advect(0, settings.time);
    }
    std::size_t work = 0;
    long double magnitude = 0;
    return !boss.wasInterrupted() && dommy::vdb::transportValues(grid, work, magnitude) &&
           grid.activeVoxelCount() <= dommy::generation::kMaxWorkItemCount;
}
} // namespace

MStatus DOpenVDBMorphLevelSet::compute(const MPlug& plug, MDataBlock& data) try {
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
    dommy::vdb::TransportSettings settings;
    DOMMY_RETURN_IF_FAILURE(readMorphSettings(data, settings));
    const MDataHandle targetHandle = data.inputValue(aTargetVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DOpenVDBData* targetVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(targetHandle, DOpenVDBData::id, targetVdb));
    if (!targetVdb || !targetVdb->hasValidData())
        return MS::kInvalidParameter;
    const MDataHandle targetName = data.inputValue(aTargetVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    GridCPtrVec targets;
    getSelectedGrids(targets, targetName.asString().asChar(), *targetVdb);
    if (targets.empty() || !targets.front()->isType<openvdb::FloatGrid>())
        return MS::kInvalidParameter;
    const auto& target = vdbGridCast<openvdb::FloatGrid>(*targets.front());
    std::size_t work = 0;
    long double targetMagnitude = 0;
    if (!dommy::vdb::validLevelSetScale(target) ||
        !dommy::vdb::transportValues(target, work, targetMagnitude) ||
        targetMagnitude / target.voxelSize()[0] > dommy::vdb::kMaxFilterWidth)
        return MS::kInvalidParameter;

    MDataHandle maskHandle = data.inputValue(aMaskVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const openvdb::FloatGrid* mask = nullptr;
    // An unconnected null object is optional; a present but malformed payload
    // or a failed Maya read is not silently interpreted as an absent mask.
    if (!maskHandle.data().isNull()) {
        const DOpenVDBData* maskVdb = nullptr;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(maskHandle, DOpenVDBData::id, maskVdb));
        if (!maskVdb || !maskVdb->hasValidData())
            return MS::kInvalidParameter;
        const MDataHandle maskName = data.inputValue(aMaskName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        GridCPtrVec masks;
        getSelectedGrids(masks, maskName.asString().asChar(), *maskVdb);
        if (masks.empty() || !masks.front()->isType<openvdb::FloatGrid>())
            return MS::kInvalidParameter;
        mask = &vdbGridCast<openvdb::FloatGrid>(*masks.front());
        long double magnitude = 0;
        if (!dommy::vdb::validTransportTransform(*mask) ||
            !dommy::vdb::transportValues(*mask, work, magnitude) ||
            !dommy::vdb::validTransportMask(settings, magnitude))
            return MS::kInvalidParameter;
    }
    const MDataHandle selection = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    GridCPtrVec grids;
    getSelectedGrids(grids, selection.asString().asChar(), *inputVdb);
    for (const auto& source : grids) {
        if (source->getGridClass() != openvdb::GRID_LEVEL_SET ||
            !source->isType<openvdb::FloatGrid>())
            continue;
        const auto& grid = vdbGridCast<openvdb::FloatGrid>(*source);
        long double magnitude = 0;
        dommy::vdb::TransportPlan plan;
        auto planning = settings;
        planning.time = std::max(0.0f, settings.time);
        if (!dommy::vdb::validLevelSetScale(grid) ||
            !dommy::vdb::transportValues(grid, work, magnitude) ||
            magnitude / grid.voxelSize()[0] > dommy::vdb::kMaxFilterWidth ||
            !dommy::vdb::makeTransportPlan(planning, magnitude + targetMagnitude,
                                           grid.voxelSize()[0], true, plan) ||
            !dommy::vdb::transportWork(grid, plan, work) ||
            !dommy::vdb::transportDomain(grid, target, plan) ||
            (mask && !dommy::vdb::transportDomain(grid, *mask, plan)))
            return MS::kInvalidParameter;
    }
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    openvdb::util::NullInterrupter boss;
    for (const auto& source : grids) {
        if (source->getGridClass() != openvdb::GRID_LEVEL_SET ||
            !source->isType<openvdb::FloatGrid>()) {
            MGlobal::displayWarning(
                ("Skipped unsupported morph grid: " + source->getName()).c_str());
            continue;
        }
        auto copy = source->deepCopyGrid();
        if (!copy || !copy->isType<openvdb::FloatGrid>())
            return MS::kFailure;
        auto& grid = vdbGridCast<openvdb::FloatGrid>(*copy);
        if (grid.background() / grid.voxelSize()[0] < openvdb::LEVEL_SET_HALF_WIDTH)
            MGlobal::displayWarning(
                ("Level set has a narrow band less than 3 voxels: " + source->getName()).c_str());
        if (!applyMorph(grid, target, mask, settings, boss))
            return MS::kFailure;
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBMorphLevelSet")

MStatus DOpenVDBMorphLevelSet::initialize() try {
    MStatus stat;

    // attributes
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

    aTargetVdbInput =
        tAttr.create("TargetVdbInput", "tvdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetVdbInput));

    const MObject aTargetVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTargetVdbSelectedGridNames =
        tAttr.create("TargetVdbSelectedGridNames", "tgrids", MFnData::kString,
                     aTargetVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetVdbSelectedGridNames));

    const MObject aMaskNameDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMaskName = tAttr.create("MaskName", "mname", MFnData::kString, aMaskNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskName));

    aInvertMask = nAttr.create("InvertMask", "im", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertMask));

    aMinMask = nAttr.create("MinMask", "minmsk", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinMask));

    aMaxMask = nAttr.create("MaxMask", "maxmsk", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxMask));

    aMaskVdbInput =
        tAttr.create("MaskVdbInput", "maskvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskVdbInput));

    aTimestep = nAttr.create("Timestep", "ts", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimestep));

    MFnEnumAttribute eAttr;
    aAdvectSpatial = eAttr.create("AdvectSpatial", "advspt", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        eAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::FIRST_BIAS).c_str(), 0));
    DOMMY_RETURN_IF_FAILURE(
        eAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::HJWENO5_BIAS).c_str(), 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAdvectSpatial));

    MFnEnumAttribute advtmpAttr;
    aAdvectTemporal = advtmpAttr.create("AdvectTemporal", "advtmp", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::math::NUM_TEMPORAL_SCHEMES; ++i) {
        const openvdb::math::TemporalIntegrationScheme scheme =
            static_cast<openvdb::math::TemporalIntegrationScheme>(i);
        DOMMY_RETURN_IF_FAILURE(
            advtmpAttr.addField(temporalIntegrationSchemeToMenuName(scheme).c_str(), scheme));
    }
    DOMMY_RETURN_IF_FAILURE(advtmpAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAdvectTemporal));

    aSteps = nAttr.create("Steps", "stp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSteps));

    MFnEnumAttribute renosptAttr;
    aRenormSpatial = renosptAttr.create("RenormSpatial", "renospt", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        renosptAttr.addField(biasedGradientSchemeToMenuName(openvdb::math::FIRST_BIAS).c_str(), 0));
    DOMMY_RETURN_IF_FAILURE(renosptAttr.addField(
        biasedGradientSchemeToMenuName(openvdb::math::HJWENO5_BIAS).c_str(), 1));
    DOMMY_RETURN_IF_FAILURE(renosptAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRenormSpatial));

    MFnEnumAttribute renotmpAttr;
    aRenormTemporal = renotmpAttr.create("RenormTemporal", "renotmp", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::math::NUM_TEMPORAL_SCHEMES; ++i) {
        const openvdb::math::TemporalIntegrationScheme scheme =
            static_cast<openvdb::math::TemporalIntegrationScheme>(i);
        DOMMY_RETURN_IF_FAILURE(
            renotmpAttr.addField(temporalIntegrationSchemeToMenuName(scheme).c_str(), scheme));
    }
    DOMMY_RETURN_IF_FAILURE(renotmpAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRenormTemporal));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInvertMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxMask, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimestep, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdvectSpatial, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdvectTemporal, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSteps, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRenormSpatial, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRenormTemporal, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBMorphLevelSet", "initialize",
                                                  std::current_exception());
}

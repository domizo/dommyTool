//
//  DOpenVDBScatter.cpp
//  dommyTool
//
//  Created by yuzo on 2016/12/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBScatter.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "../Edit/DOpenVDBScatterPolicy.h"
#include <random>




#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/util/NullInterrupter.h>

#include <cmath>
#include <cstdint>
#include <string>



#include "DOpenVDBData.h"
#include "DPData.h"
#include "DType.h"
#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"

#include "DMayaHeaders.h"

#include "VDBPointUtil.h"

#include "dvdb_ids.h"


const MTypeId DOpenVDBScatter::typeId( ID_VDB_DOPENVDB_SCATTER );

MObject DOpenVDBScatter::aVdbInput;
MObject DOpenVDBScatter::aVdbSelectedGridNames;

MObject DOpenVDBScatter::aSeed;
MObject DOpenVDBScatter::aSpread;
MObject DOpenVDBScatter::aMode;
MObject DOpenVDBScatter::aCount;
MObject DOpenVDBScatter::aDensity;
MObject DOpenVDBScatter::aIsLocal;
MObject DOpenVDBScatter::aPerVoxel;
MObject DOpenVDBScatter::aInterior;

MObject DOpenVDBScatter::outDP;

namespace {
using namespace dommy::vdb;
MStatus readScatterSettings(MDataBlock& data, ScatterSettings& settings) {
    MStatus status;
    const auto mode = data.inputValue(DOpenVDBScatter::aMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.mode = mode.asInt();
    if (settings.mode < 0 || settings.mode > 2) return MS::kInvalidParameter;
    const auto seed = data.inputValue(DOpenVDBScatter::aSeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.seed = seed.asInt();
    const auto spread = data.inputValue(DOpenVDBScatter::aSpread, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.spread = spread.asDouble();
    const auto interior = data.inputValue(DOpenVDBScatter::aInterior, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.interior = interior.asBool();
    if (settings.seed < 0 || !std::isfinite(settings.spread) ||
        settings.spread < 0 || settings.spread > 1) return MS::kInvalidParameter;
    if (settings.mode == 0) {
        const auto count = data.inputValue(DOpenVDBScatter::aCount, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const auto pointCount = count.asInt64();
        if (pointCount < 0 || pointCount > static_cast<decltype(pointCount)>(
                dommy::generation::kMaxGeneratedPointCount)) return MS::kInvalidParameter;
        settings.count = static_cast<openvdb::Index64>(pointCount);
    } else if (settings.mode == 1) {
        const auto density = data.inputValue(DOpenVDBScatter::aDensity, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.density = density.asFloat();
        const auto local = data.inputValue(DOpenVDBScatter::aIsLocal, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.local = local.asBool();
        if (!std::isfinite(settings.density) || settings.density < 0) return MS::kInvalidParameter;
    } else {
        const auto perVoxel = data.inputValue(DOpenVDBScatter::aPerVoxel, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.perVoxel = perVoxel.asFloat();
        if (!std::isfinite(settings.perVoxel) || settings.perVoxel < 0) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}
} // namespace

void* DOpenVDBScatter::creator() {
    return ::dommy::maya::createObject("DOpenVDBScatter", [&]() {
        return new DOpenVDBScatter();
    });
}

MStatus DOpenVDBScatter::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;
    const DOpenVDBData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, input));
    MStatus status;
    const auto selection = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    GridCPtrVec grids;
    getSelectedGrids(grids, selection.asString().asChar(), *input);
    if (grids.empty()) MGlobal::displayWarning("No grids are selected.");
    ScatterSettings settings;
    if (!grids.empty()) DOMMY_RETURN_IF_FAILURE(readScatterSettings(data, settings));
    dommy::vdb::TreeEditBudget budget;
    std::vector<ScatterPlan> plans;
    std::string emptyGrids;
    std::size_t total = 0;
    for (const auto& grid : grids) {
        if (grid->empty()) {
            if (!emptyGrids.empty()) emptyGrids += ", ";
            emptyGrids += grid->getName();
            continue;
        }
        ScatterPlan plan;
        DOMMY_RETURN_IF_FAILURE(planScatter(grid, settings, budget, plan));
        if (!dommy::generation::checkedSum(total, plan.count,
                dommy::generation::kMaxGeneratedPointCount, total)) return MS::kInvalidParameter;
        plans.push_back(plan);
    }
    DPointList positions;
    DOMMY_RETURN_IF_FAILURE(scatterPoints(plans, settings, positions));
    if (!emptyGrids.empty())
        MGlobal::displayWarning(("The following grids were empty: " + emptyGrids).c_str());
    MObject object;
    DPData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DPData>(DPData::id, object, output));
    output->setPosition(positions);
    output->updateBBox();
    auto handle = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DPData>(handle, object, DPData::id));
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBScatter")

MStatus DOpenVDBScatter::initialize() try
{
    MStatus stat;
    
    // attributes
    
    MFnTypedAttribute tAttr;
    MFnStringData strData;
    MFnNumericAttribute nAttr;
    
    
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    stat = addAttribute(aVdbInput);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    
    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));
    
    aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    
    aSpread = nAttr.create( "Spread", "spr", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpread));
    
    
    MFnEnumAttribute eAttr;
    aMode = eAttr.create("Mode", "mo", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("count", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("density", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("points per voxel", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));
    
    
    
    aCount = nAttr.create( "Count", "cnt", MFnNumericData::kInt64 , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));
    
    aDensity = nAttr.create( "Density", "dens", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDensity));
    
    aIsLocal = nAttr.create( "IsLocal", "isl", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsLocal));
    
    aPerVoxel = nAttr.create( "PerVoxel", "pvx", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(8));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPerVoxel));
    
    aInterior = nAttr.create( "Interior", "intr", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInterior));
    
    
    
    
    
    
    
    
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    // attribute dependencies
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, outDP));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpread, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMode, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDensity, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsLocal, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPerVoxel, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInterior, outDP));
    
    
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBScatter", "initialize", std::current_exception());
}

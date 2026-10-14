//
//  DOpenVDBPlatonic.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/22.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBPlatonic.h"
#include "common/utility/DStatusOperation.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include "../Common/DVDBComputeGuard.h"


#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MGlobal.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnUnitAttribute.h>

#include <cmath>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/util/NullInterrupter.h>

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "dvdb_ids.h"


const MTypeId DOpenVDBPlatonic::typeId( ID_VDB_DOPENVDB_PLATONIC );


MObject DOpenVDBPlatonic::aVdbSelectedGridNames;
MObject DOpenVDBPlatonic::aVdbInput;

MObject DOpenVDBPlatonic::aVdbOutput;


MObject DOpenVDBPlatonic::solidType;
MObject DOpenVDBPlatonic::gridclass;
MObject DOpenVDBPlatonic::scalarRadius;
MObject DOpenVDBPlatonic::center;
MObject DOpenVDBPlatonic::voxelSize;
MObject DOpenVDBPlatonic::halfWidth;
MObject DOpenVDBPlatonic::fogVolume;


void* DOpenVDBPlatonic::creator()
{
    return ::dommy::maya::createObject("DOpenVDBPlatonic", [&]() {
        return new DOpenVDBPlatonic();
    });
}


namespace {
struct SphereSettings {
    float radius = 1, voxel = .1f, width = 3;
    openvdb::Vec3f center{0};
    bool fog = false;
};
MStatus readSphereSettings(MDataBlock& data, SphereSettings& settings) {
    MStatus status;
    const auto radius = data.inputValue(DOpenVDBPlatonic::scalarRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.radius = radius.asFloat();
    const auto voxel = data.inputValue(DOpenVDBPlatonic::voxelSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.voxel = voxel.asFloat();
    const auto width = data.inputValue(DOpenVDBPlatonic::halfWidth, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.width = width.asFloat();
    const auto center = data.inputValue(DOpenVDBPlatonic::center, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const auto& c = center.asFloat3();
    settings.center = openvdb::Vec3f(c[0], c[1], c[2]);
    const auto solid = data.inputValue(DOpenVDBPlatonic::solidType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short solidValue = solid.asShort();
    const auto classification = data.inputValue(DOpenVDBPlatonic::gridclass, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short classValue = classification.asShort();
    const auto fog = data.inputValue(DOpenVDBPlatonic::fogVolume, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.fog = fog.asBool();
    if (solidValue != 0 || classValue < 0 || classValue > 3 ||
        !std::isfinite(settings.radius) || settings.radius <= 0 ||
        !std::isfinite(settings.voxel) || settings.voxel < 1e-6f || settings.voxel > 1e6f ||
        !std::isfinite(settings.width) || settings.width <= 1 || settings.width > 64)
        return MS::kInvalidParameter;
    return MS::kSuccess;
}
bool planSphere(const SphereSettings& settings, dommy::vdb::TreeEditBudget& budget) {
    // Use FloatGrid's float-rounded expressions, as in native 5.2 rasterSphere.
    const float radius = settings.radius / settings.voxel;
    const float extent = radius + settings.width;
    const float background = settings.width * settings.voxel;
    if (!std::isfinite(radius) || !std::isfinite(extent) ||
        !std::isfinite(background) || background <= 0) return false;
    openvdb::BBoxd bounds;
    for (int axis = 0; axis < 3; ++axis) {
        const float center = settings.center[axis] / settings.voxel;
        const float low = center - extent, high = center + extent;
        if (!std::isfinite(low) || !std::isfinite(high) ||
            std::fabs(low) > 1e6f || std::fabs(high) > 1e6f) return false;
        bounds.min()[axis] = low;
        bounds.max()[axis] = high;
    }
    bounds.expand(8); // Whole boundary leaves and flood-fill workspace.
    openvdb::CoordBBox checked;
    return dommy::vdb::indexGridBox(bounds, checked, true) &&
        dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(checked, budget, 2, 32, 32);
}
bool finiteSphere(const openvdb::FloatGrid& grid, std::size_t& work) {
    dommy::vdb::ValuePreflight values(work, 1);
    values(grid);
    return values.valid;
}
} // namespace

MStatus DOpenVDBPlatonic::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) return MS::kUnknownParameter;
    SphereSettings settings;
    DOMMY_RETURN_IF_FAILURE(readSphereSettings(data, settings));
    dommy::vdb::TreeEditBudget budget;
    if (!planSphere(settings, budget)) return MS::kInvalidParameter;
    openvdb::util::NullInterrupter interrupter;
    auto grid = openvdb::tools::createLevelSetSphere<
        openvdb::FloatGrid, openvdb::util::NullInterrupter>(
            settings.radius, settings.center, settings.voxel, settings.width, &interrupter);
    std::size_t work = 0;
    if (!grid || !finiteSphere(*grid, work)) return MS::kFailure;
    if (settings.fog) {
        if (grid->empty()) {
            // Native 5.2 fog conversion indexes empty node vectors. A sphere
            // below the rasterizer's Nyquist limit is a valid empty result.
            auto transform = grid->transform().copy();
            if (!transform) return MS::kFailure;
            grid = openvdb::FloatGrid::create(0.0f);
            if (!grid) return MS::kFailure;
            grid->setTransform(transform);
            grid->setGridClass(openvdb::GRID_FOG_VOLUME);
        } else {
            openvdb::tools::sdfToFogVolume(*grid);
        }
        if (!finiteSphere(*grid, work)) return MS::kFailure;
    }
    grid->insertMeta("radius", openvdb::FloatMetadata(settings.radius));
    grid->setName("LevelSetSphere");
    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, output));
    if (!output) return MS::kFailure;
    output->insert(grid);
    if (!output->hasValidData()) return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBPlatonic")

MStatus DOpenVDBPlatonic::initialize() try
{
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;

    const MObject stringDefault1 = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, stringDefault1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    MFnEnumAttribute eAttr;
    solidType = eAttr.create("solidType", "slt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Sphere", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(solidType));

    MFnEnumAttribute gridAttr;
    gridclass = gridAttr.create("gridclass", "gcls", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gridAttr.addField(openvdb::GridBase::gridClassToString(openvdb::GRID_LEVEL_SET).c_str(), 0));
    DOMMY_RETURN_IF_FAILURE(gridAttr.addField(openvdb::GridBase::gridClassToMenuName(openvdb::GRID_LEVEL_SET).c_str(), 1));
    DOMMY_RETURN_IF_FAILURE(gridAttr.addField("sdf", 2));
    DOMMY_RETURN_IF_FAILURE(gridAttr.addField("Signed Distance Field", 3));
    DOMMY_RETURN_IF_FAILURE(gridAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(gridclass));

    scalarRadius = nAttr.create("scalarRadius", "srad", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scalarRadius));

    center = nAttr.create("center", "cen", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(center));

    voxelSize = nAttr.create("voxelSize", "vs", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0001f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(voxelSize));

    halfWidth = nAttr.create("halfWidth", "hw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(halfWidth));

    fogVolume = nAttr.create("fogVolume", "fv", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(fogVolume));

    // input / output

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(solidType, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(gridclass, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(scalarRadius, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(center, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(voxelSize, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(halfWidth, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(fogVolume, aVdbOutput));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBPlatonic", "initialize", std::current_exception());
}

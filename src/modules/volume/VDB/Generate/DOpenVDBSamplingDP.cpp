//
// DOpenVDBSamplingDP.cpp
// Occupied-voxel sampling with the historical float/truncation contract.
//
#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBSamplingDP.h"
#include "DMayaHeaders.h"
#include "DPointCommon.h"
#include "DOpenVDBData.h"
#include "DOpenVDBCommon.h"
#include "DNodeOperation.h"
#include "dvdb_ids.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "../Common/DVDBComputeGuard.h"
#include "../Common/DVdbParticleRasterPlan.h"

const MTypeId DOpenVDBSamplingDP::typeId(ID_VDB_DOPENVDB_SAMPLING_DP);
MObject DOpenVDBSamplingDP::inDP;
MObject DOpenVDBSamplingDP::aVoxelSize;
MObject DOpenVDBSamplingDP::aVdbOutput;
void* DOpenVDBSamplingDP::creator() {
    return ::dommy::maya::createObject("DOpenVDBSamplingDP", [&]() {
        return new DOpenVDBSamplingDP();
    });
}

MStatus DOpenVDBSamplingDP::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    MStatus status;
    const MDataHandle voxelHandle = data.inputValue(aVoxelSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double voxelSize = voxelHandle.asDouble();
    const std::size_t count = input->numberOfPoints();
    if (!dommy::vdb::hasValidPointSamplingInput(count, voxelSize))
        return MS::kInvalidParameter;
    const DPointList* points = input->getPositionPtrConst();
    if (!points || points->size() != count)
        return MS::kInvalidParameter;
    dommy::point::PointPayloadBudget budget;
    budget.add(count, sizeof(openvdb::Coord));
    // Only positions are consumed; source metadata is not copied to the VDB.
    std::vector<openvdb::Coord> coordinates(count);
    const auto transform = openvdb::math::Transform::createLinearTransform(voxelSize);
    if (!transform)
        return MS::kFailure;
    for (std::size_t i = 0U; i < count; ++i) {
        if (!dommy::vdb::isFiniteVector3((*points)[i]))
            return MS::kInvalidParameter;
        const auto index = transform->worldToIndex(
            openvdb::Vec3d((*points)[i][0], (*points)[i][1], (*points)[i][2]));
        int components[3];
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!dommy::vdb::checkedSamplingCoordinate(index[axis], components[axis]))
                return MS::kInvalidParameter;
        coordinates[i] = openvdb::Coord(components[0], components[1], components[2]);
    }
    auto grid = openvdb::FloatGrid::create(0.0f);
    if (!grid)
        return MS::kFailure;
    grid->setTransform(transform);
    grid->setName("density");
    grid->setGridClass(openvdb::GRID_FOG_VOLUME);
    auto accessor = grid->getAccessor();
    for (const auto& coordinate : coordinates)
        accessor.setValue(coordinate, 1.0f);
    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DOpenVDBData::id, output));
    output->insert(grid);
    MDataHandle handle = data.outputValue(aVdbOutput, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
        handle, pluginData.object(), DOpenVDBData::id));
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBSamplingDP")

MStatus DOpenVDBSamplingDP::initialize() try {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;
    MFnGenericAttribute gAttr;

#pragma mark input attribute

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aVoxelSize = nAttr.create("VoxelSize", "vs", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVoxelSize));

    aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(message, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBSamplingDP", "initialize",
                                                  std::current_exception());
}

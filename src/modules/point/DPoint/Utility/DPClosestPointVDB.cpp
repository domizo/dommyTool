//
//  DPClosestPointVDB.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/03.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPClosestPointVDB.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include "DNodeOperation.h"
#include "DDataOp.h"
#include "DPointOp.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DAttributePublication.h"
#include <memory>
#include <stdexcept>

#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToSpheres.h>
#include <openvdb/tools/ParticleAtlas.h>
#include <openvdb/tools/PointIndexGrid.h>
#include "../../../volume/VDB/Common/DVDBComputeGuard.h"

const MTypeId DPClosestPointVDB::typeId(ID_DP_CLOSEST_POINT_VDB);

MObject DPClosestPointVDB::inDP;
MObject DPClosestPointVDB::inVdb;

MObject DPClosestPointVDB::aIsoOffset;

MObject DPClosestPointVDB::aIsDeform;
MObject DPClosestPointVDB::aAddAttribute;
MObject DPClosestPointVDB::aAttributeName;

MObject DPClosestPointVDB::outDP;

void* DPClosestPointVDB::creator() {
    return ::dommy::maya::createObject("DPClosestPointVDB", [&]() {
        return new DPClosestPointVDB();
    });
}

MStatus DPClosestPointVDB::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));

    const std::size_t count = input->numberOfPoints();
    dommy::point::PointPayloadBudget workspace;
    // Include the pass-through copy, query vectors/distances and generated data.
    dommy::point::preflightPointPayload(
        *input, sizeof(DPoint) + sizeof(openvdb::Vec3R) + sizeof(float) + sizeof(DVec3Data),
        workspace);

    MStatus status;
    MFnPluginData pluginData;
    DPData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DPData::id, output));
    output->setDPData(*input);

    if (count != 0U) {
        const MDataHandle vdbHandle = data.inputValue(inVdb, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const DOpenVDBData* vdb = getInputVDBfromHandle(vdbHandle);
        if (vdb) {
            GridCPtrVec grids;
            getGrids(grids, *vdb, "*");
            if (!grids.empty()) {
                const MDataHandle isoHandle = data.inputValue(aIsoOffset, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                const float iso = isoHandle.asFloat();
                if (!std::isfinite(iso))
                    return MS::kInvalidParameter;
                const MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                const std::string attributeName = nameHandle.asString().asChar();
                workspace.add(attributeName.size(), sizeof(char));

                const openvdb::GridBase& grid = *grids.front();
                if (grid.getGridClass() != openvdb::GRID_LEVEL_SET) {
                    EchoWarning(name().asChar(), "Select grid is not level set grid.");
                } else if (getVdbGridType(grid) == DVdbGridType::kFloat) {
                    const MDataHandle deformHandle = data.inputValue(aIsDeform, &status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    const bool deform = deformHandle.asBool();
                    const MDataHandle addHandle = data.inputValue(aAddAttribute, &status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    const bool addAttribute = addHandle.asBool();
                    if (addAttribute && (!dommy::attribute::validAttributeName(attributeName) ||
                        attributeName.size() > dommy::generation::kMaxGeneratedStringByteCount))
                        return MS::kInvalidParameter;

                    // create() reads the grid; no deep copy is needed.
                    const auto& floatGrid = static_cast<const openvdb::FloatGrid&>(grid);
                    auto query = openvdb::tools::ClosestSurfacePoint<openvdb::FloatGrid>::create(
                        floatGrid, iso);
                    // OpenVDB 5.2 can initialize with zero surface points. Its
                    // replacement search would then index an empty array.
                    if (!query || query->indexTree().activeVoxelCount() == 0U)
                        return MS::kFailure;

                    std::vector<openvdb::Vec3R> points;
                    points.reserve(count);
                    for (const auto& point : *input->getPositionPtrConst())
                        points.push_back(openvdb::Vec3R(point[0], point[1], point[2]));
                    std::vector<float> distances;
                    if (!query->searchAndReplace(points, distances) || points.size() != count ||
                        distances.size() != count)
                        return MS::kFailure;
                    for (std::size_t index = 0U; index < count; ++index) {
                        if (!std::isfinite(distances[index]) || distances[index] < 0.0f)
                            return MS::kFailure;
                        for (unsigned int axis = 0U; axis < 3U; ++axis)
                            if (!dommy::numeric::canRepresentAsFloat(points[index][axis]))
                                return MS::kFailure;
                    }

                    if (addAttribute) {
                        DVec3DataList values;
                        values.reserve(count);
                        for (const auto& point : points)
                            values.push_back(DVec3Data(point[0], point[1], point[2]));
                        const DPAttributeData::Ptr candidate = std::make_shared<DPVec3fAttr>(values);
                        if (!dommy::attribute::commitStagedAttribute(
                                *output, attributeName, candidate, count)) return MS::kFailure;
                    }
                    if (deform) {
                        DPointList positions;
                        positions.reserve(count);
                        for (const auto& point : points)
                            positions.push_back(DPoint(point[0], point[1], point[2]));
                        if (!output->replacePositions(positions))
                            return MS::kFailure;
                    }
                }
            }
        }
    }
    MDataHandle handle = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DPData>(handle, pluginData.object(), DPData::id));
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DPClosestPointVDB")

MStatus DPClosestPointVDB::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    // input vdb
    inVdb = tAttr.create("inVdb", "ivdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inVdb));

    aIsoOffset = nAttr.create("IsoOffset", "isooff", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsoOffset));

    aIsDeform = nAttr.create("IsDeform", "isdef", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeform));

    aAddAttribute = nAttr.create("AddAttribute", "addattr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddAttribute));

    const MObject defaultName = fnStringData.create("clp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("AttributeName", "attrn", MFnData::kString, defaultName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inVdb, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsoOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsDeform, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAttributeName, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPClosestPointVDB", "initialize",
                                                  std::current_exception());
}

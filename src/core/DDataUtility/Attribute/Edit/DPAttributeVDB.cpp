//
//  DPAttributeVDB.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/03.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAttributeVDB.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include "DPointOp.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeValidation.h"
#include <memory>
#include <stdexcept>
#include "../../../../modules/volume/VDB/Common/DVDBComputeGuard.h"

const MTypeId DPAttributeVDB::typeId(ID_DP_ATTR_VDB);

MObject DPAttributeVDB::inDP;
MObject DPAttributeVDB::inVdb;

MObject DPAttributeVDB::aSourceGridName;

MObject DPAttributeVDB::outDP;

void* DPAttributeVDB::creator() {
    return ::dommy::maya::createObject("DPAttributeVDB", [&]() {
        return new DPAttributeVDB();
    });
}

namespace {
// Keep legacy truncation toward zero, including for negative coordinates.
template <typename Vector> openvdb::Coord checkedSampleCoord(const Vector& value) {
    int components[3];
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!dommy::numeric::checkedNumericCast(value[axis], components[axis]))
            throw std::invalid_argument("DPAttributeVDB index coordinate is out of range");
    return openvdb::Coord(components[0], components[1], components[2]);
}

// IndexVector deliberately preserves the old per-type float/double precision.
template <typename Attribute, typename IndexVector, typename Grid, typename Convert>
void sampleVdbAttribute(DPData& output, const Grid& grid, const DPointList& points,
                        Convert convert) {
    const std::string& attributeName = grid.getName();
    if (!dommy::attribute::validAttributeName(attributeName) ||
        attributeName.size() > dommy::generation::kMaxGeneratedStringByteCount)
        throw std::invalid_argument("DPAttributeVDB grid name is not a valid output attribute");
    const auto accessor = grid.getConstAccessor();
    const auto& transform = grid.transform();
    std::vector<Attribute> values;
    values.reserve(points.size());
    for (const auto& point : points) {
        const auto transformed = transform.worldToIndex(IndexVector(point[0], point[1], point[2]));
        IndexVector index;
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!dommy::numeric::checkedNumericCast(transformed[axis], index[axis]))
                throw std::invalid_argument(
                    "DPAttributeVDB transformed coordinate is out of range");
        const openvdb::Coord coordinate = checkedSampleCoord(index);
        const Attribute value = convert(accessor.getValue(coordinate));
        if (!dommy::attribute::isValidValue(value))
            throw std::invalid_argument("DPAttributeVDB sampled value is not finite");
        values.push_back(value);
    }
    const DPAttributeData::Ptr candidate =
        std::make_shared<TypedDPAttributeData<Attribute>>(values);
    if (!dommy::attribute::commitStagedAttribute(output, attributeName, candidate, points.size()))
        throw std::runtime_error("DPAttributeVDB attribute publication failed");
}
} // namespace

MStatus DPAttributeVDB::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));

    dommy::point::PointPayloadBudget workspace;
    // The largest supported sample is a double vector. Include all source
    // fields/groups before making the transactional output copy.
    dommy::point::preflightPointPayload(*input, sizeof(DPoint) + sizeof(DA_VEC3D), workspace);
    MStatus status;
    MFnPluginData pluginData;
    DPData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DPData::id, output));
    output->setDPData(*input);

    if (input->numberOfPoints() != 0U) {
        const MDataHandle vdbHandle = data.inputValue(inVdb, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const DOpenVDBData* vdb = getInputVDBfromHandle(vdbHandle);
        if (vdb) {
            const MDataHandle nameHandle = data.inputValue(aSourceGridName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const std::string selection = nameHandle.asString().asChar();
            workspace.add(selection.size(), sizeof(char));
            GridCPtrVec grids;
            getGrids(grids, *vdb, selection);
            if (!grids.empty()) {
                const openvdb::GridBase& grid = *grids.front();
                workspace.add(grid.getName().size(), sizeof(char));
                const DPointList& points = *input->getPositionPtrConst();
                switch (getVdbGridType(grid)) {
                case DVdbGridType::kFloat:
                    sampleVdbAttribute<DA_FLOAT, openvdb::Vec3s>(
                        *output, static_cast<const openvdb::FloatGrid&>(grid), points,
                        [](float value) { return value; });
                    break;
                case DVdbGridType::kDouble:
                    sampleVdbAttribute<DA_DOUBLE, openvdb::Vec3s>(
                        *output, static_cast<const openvdb::DoubleGrid&>(grid), points,
                        [](double value) { return value; });
                    break;
                case DVdbGridType::kInt32:
                    sampleVdbAttribute<DA_INT, openvdb::Vec3s>(
                        *output, static_cast<const openvdb::Int32Grid&>(grid), points,
                        [](int32_t value) { return value; });
                    break;
                case DVdbGridType::kInt64:
                    sampleVdbAttribute<DA_INT64, openvdb::Vec3s>(
                        *output, static_cast<const openvdb::Int64Grid&>(grid), points,
                        [](int64_t value) { return value; });
                    break;
                case DVdbGridType::kVec3f:
                    sampleVdbAttribute<DA_VEC3F, openvdb::Vec3s>(
                        *output, static_cast<const openvdb::Vec3SGrid&>(grid), points,
                        [](const openvdb::Vec3s& value) {
                            return DA_VEC3F(value[0], value[1], value[2]);
                        });
                    break;
                case DVdbGridType::kVec3d:
                    sampleVdbAttribute<DA_VEC3D, openvdb::Vec3d>(
                        *output, static_cast<const openvdb::Vec3DGrid&>(grid), points,
                        [](const openvdb::Vec3d& value) {
                            return DA_VEC3D(value[0], value[1], value[2]);
                        });
                    break;
                default:
                    break; // Keep unsupported grids as pass-through.
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
DOMMY_VDB_COMPUTE_CATCH("DPAttributeVDB")

MStatus DPAttributeVDB::initialize() try {
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

    MFnStringData strData;
    const MObject defaultName = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSourceGridName =
        tAttr.create("SourceGridName", "srcgrd", MFnData::kString, defaultName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceGridName));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inVdb, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSourceGridName, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPAttributeVDB", "initialize",
                                                  std::current_exception());
}

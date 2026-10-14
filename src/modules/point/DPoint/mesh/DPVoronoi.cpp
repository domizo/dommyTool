//
//  DPVoronoi.cpp
//  dommyTool
//
//  Voro++ based Voronoi generation. The former 3D Delaunay mode is retained
//  only as a scene-compatible enum value and reports a clear error.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPVoronoi.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DPointDataAccess.h"
#include "DPData.h"
#include "dpoint_ids.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "DNodeOperation.h"
#include "DPolygonUtil.h"
#include "DVoroUtil.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
#include <vector>

namespace {

const std::size_t kMaxVoronoiSeedCount = 1000000U;

struct DPointLexicographicLess {
    bool operator()(const DPoint& lhs, const DPoint& rhs) const
    {
        if (lhs[0] != rhs[0]) return lhs[0] < rhs[0];
        if (lhs[1] != rhs[1]) return lhs[1] < rhs[1];
        return lhs[2] < rhs[2];
    }
};

bool isFinitePoint(const DPoint& point)
{
    return std::isfinite(point[0]) &&
           std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}

bool pointBounds(const DPointList& points, MBoundingBox& bounds)
{
    if (points.empty()) return false;
    MBoundingBox candidate;
    for (DPointList::const_iterator point = points.begin();
         point != points.end(); ++point) {
        if (!isFinitePoint(*point)) return false;
        candidate.expand(MPoint(
            (*point)[0], (*point)[1], (*point)[2]));
    }
    bounds = candidate;
    return true;
}

MStatus buildCropMesh(const MObject& inputMesh,
                     MObject& cropMesh, MBoundingBox& bounds)
{
    MStatus status;
    MFnMesh inputFunction(inputMesh, &status);
    if (!status) return status;

    dommy::mesh::MeshMergePlan plan;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(inputFunction, plan));

    // Mesh data has no DAG transform. Incoming connections determine whether
    // positions are already transformed; preserve those coordinates verbatim.
    MPointArray points;
    status = inputFunction.getPoints(points, MSpace::kObject);
    if (!status) return status;
    MIntArray faceCounts;
    MIntArray connections;
    status = inputFunction.getVertices(faceCounts, connections);
    if (!status) return status;

    if (points.length() != plan.points || faceCounts.length() != plan.faces ||
        connections.length() != plan.connections) {
        return MS::kFailure;
    }
    MBoundingBox candidateBounds;
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!dommy::maya::isFloatRepresentablePoint(points[index]))
            return MS::kInvalidParameter;
        candidateBounds.expand(points[index]);
    }
    std::size_t totalConnections = 0U;
    for (unsigned int face = 0U; face < faceCounts.length(); ++face) {
        if (faceCounts[face] < 3 || !dommy::generation::checkedSum(
                totalConnections, static_cast<std::size_t>(faceCounts[face]),
                plan.connections, totalConnections)) return MS::kInvalidParameter;
    }
    if (totalConnections != plan.connections) return MS::kInvalidParameter;
    for (unsigned int index = 0U;
         index < connections.length(); ++index) {
        if (connections[index] < 0 ||
            static_cast<unsigned int>(connections[index]) >=
                points.length()) {
            return MS::kInvalidParameter;
        }
    }

    MFnMeshData meshData;
    MObject candidateMesh = meshData.create(&status);
    if (!status) return status;
    if (candidateMesh.isNull()) return MS::kFailure;
    MFnMesh outputFunction;
    const MObject created = outputFunction.create(
        static_cast<int>(points.length()),
        static_cast<int>(faceCounts.length()),
        points, faceCounts, connections, candidateMesh, &status);
    if (!status) return status;
    if (created.isNull()) return MS::kFailure;
    cropMesh = candidateMesh;
    bounds = candidateBounds;
    return MS::kSuccess;
}

MStatus prepareBoundsAndCrop(
    DPVoronoi* node,
    MDataBlock& data,
    const DPointList& points,
    short containerType,
    bool& cropConnected,
    MBoundingBox& bounds,
    MObject& cropMesh)
{
    MBoundingBox seedBounds;
    if (!pointBounds(points, seedBounds)) {
        return MS::kInvalidParameter;
    }

    MPlug cropPlug(node->thisMObject(), DPVoronoi::inObject);
    MStatus status;
    cropConnected = cropPlug.isConnected(&status);
    if (!status) return status;
    MBoundingBox cropBounds;
    MObject inputMesh;
    if (cropConnected) {
        MDataHandle inputHandle = data.inputValue(DPVoronoi::inObject, &status);
        if (!status) return status;
        if (inputHandle.type() != MFnData::kMesh) return MS::kInvalidParameter;
        inputMesh = inputHandle.data();
        if (inputMesh.isNull()) return MS::kInvalidParameter;
        status = buildCropMesh(inputMesh, cropMesh, cropBounds);
        if (!status) return status;
    }

    switch (containerType) {
        case 0:
            bounds = seedBounds;
            if (cropConnected) {
                bounds.expand(cropBounds.min());
                bounds.expand(cropBounds.max());
            }
            break;
        case 1:
            bounds = seedBounds;
            break;
        case 2:
            bounds = cropConnected ? cropBounds : seedBounds;
            break;
        default:
            return MS::kInvalidParameter;
    }

    const MPoint minimum = bounds.min();
    const MPoint maximum = bounds.max();
    bounds.expand(MPoint(
        minimum.x - 2.0, minimum.y - 2.0, minimum.z - 2.0));
    bounds.expand(MPoint(
        maximum.x + 2.0, maximum.y + 2.0, maximum.z + 2.0));
    if (!dommy::maya::isFinitePoint(bounds.min()) ||
        !dommy::maya::isFinitePoint(bounds.max()) ||
        bounds.width() <= 0.0 ||
        bounds.height() <= 0.0 ||
        bounds.depth() <= 0.0) {
        return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

bool addSeeds(voro::container& container,
              const DPointList& points,
              bool removeDuplicates)
{
    DPointList seeds;
    if (removeDuplicates) {
        std::set<DPoint, DPointLexicographicLess> unique;
        seeds.reserve(points.size());
        for (DPointList::const_iterator point = points.begin();
             point != points.end(); ++point) {
            if (unique.insert(*point).second) {
                seeds.push_back(*point);
            }
        }
    }
    const DPointList& selected = removeDuplicates ? seeds : points;
    if (selected.empty() ||
        selected.size() >
            static_cast<std::size_t>(
                std::numeric_limits<int>::max())) {
        return false;
    }

    for (std::size_t index = 0U;
         index < selected.size(); ++index) {
        MPoint seed(
            selected[index][0],
            selected[index][1],
            selected[index][2]);
        if (!dommy::voronoi::addSeed(
                container, seed, static_cast<int>(index), true)) {
            return false;
        }
    }
    return true;
}

MStatus validateLineTopology(
    const std::vector<MPoint>& points,
    const std::vector<int>& faceCounts,
    const std::vector<int>& connections)
{
    if (points.size() >
            dommy::generation::kMaxGeneratedPointCount ||
        faceCounts.size() >
            dommy::generation::kMaxGeneratedFaceCount ||
        connections.size() >
            dommy::generation::kMaxWorkItemCount) {
        return MS::kFailure;
    }
    for (std::vector<MPoint>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        if (!dommy::maya::isFinitePoint(*point)) {
            return MS::kInvalidParameter;
        }
    }

    std::size_t expectedConnectionCount = 0U;
    for (std::vector<int>::const_iterator count = faceCounts.begin();
         count != faceCounts.end(); ++count) {
        if (*count < 0 ||
            !dommy::generation::checkedSum(
                expectedConnectionCount,
                static_cast<std::size_t>(*count),
                dommy::generation::kMaxWorkItemCount,
                expectedConnectionCount)) {
            return MS::kFailure;
        }
    }
    if (expectedConnectionCount != connections.size()) {
        return MS::kFailure;
    }
    for (std::vector<int>::const_iterator index =
             connections.begin();
         index != connections.end(); ++index) {
        if (*index < 0 ||
            static_cast<std::size_t>(*index) >= points.size()) {
            return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

MStatus publishCurveOutput(
    MDataBlock& data,
    const MPlug& plug,
    const std::vector<MPoint>& points,
    const std::vector<int>& faceCounts,
    const std::vector<int>& connections)
{
    MObject outputObject;
    DCurveData* output = nullptr;
    MStatus status =
        dommy::maya::createPluginDataObject<DCurveData>(
            DCurveData::id, outputObject, output);
    if (!status) return status;
    if (!output) return MS::kFailure;

    std::size_t connectionOffset = 0U;
    for (std::size_t face = 0U;
         face < faceCounts.size(); ++face) {
        const std::size_t count =
            static_cast<std::size_t>(faceCounts[face]);
        if (count >= 2U) {
            MPointArray curvePoints;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                count, [&](unsigned int vertex, MPoint& point) -> MStatus {
                    point = points[static_cast<std::size_t>(
                        connections[connectionOffset + vertex])];
                    return MS::kSuccess;
                }, curvePoints));
            DCItem curve(new DCurveInternalData(curvePoints, 1));
            if (!curve || !curve->hasValidCurveTopology() ||
                !output->insert(curve)) {
                return MS::kFailure;
            }
        }
        connectionOffset += count;
    }
    if (!output->hasValidCurveTopology()) return MS::kFailure;
    output->updateBBox();

    MDataHandle handle =
        data.outputValue(DPVoronoi::outDC, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        handle, outputObject, *output);
    if (!status) return status;
    handle.setClean();
    return data.setClean(plug);
}

MStatus clipCellsToMesh(
    const MObject& cropMesh,
    std::vector<MObject>& cells)
{
    for (std::vector<MObject>::iterator cell = cells.begin();
         cell != cells.end(); ++cell) {
        if (cell->isNull()) return MS::kFailure;
        MStatus status;
        MFnMesh cellFunction(*cell, &status);
        if (!status) return status;
        MObjectArray operands;
        status = operands.append(*cell);
        if (!status) return status;
        status = operands.append(cropMesh);
        if (!status) return status;
        status = cellFunction.booleanOps(
            MFnMesh::kIntersection, operands, true);
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus publishMeshOutput(
    MDataBlock& data,
    const MPlug& plug,
    voro::container& container,
    double cellSpace,
    bool cropConnected,
    const MObject& cropMesh)
{
    std::vector<MObject> cells;
    if (!dommy::voronoi::extractCellMeshes(
            container, cellSpace, cells,
            dommy::generation::kMaxConnectedGeometryCount,
            dommy::generation::kMaxGeneratedPointCount,
            dommy::generation::kMaxGeneratedFaceCount,
            dommy::generation::kMaxWorkItemCount)) {
        return MS::kFailure;
    }

    MStatus status;
    MDataHandle useCropHandle =
        data.inputValue(DPVoronoi::aUseCropObject, &status);
    if (!status) return status;
    const bool useCrop =
        useCropHandle.asBool() && cropConnected;

    std::vector<bool> keep(cells.size(), true);
    if (useCrop) {
        MDataHandle checkHandle =
            data.inputValue(DPVoronoi::aIsCheckCell, &status);
        if (!status) return status;
        const bool checkWholeCell = checkHandle.asBool();

        MDataHandle toleranceHandle =
            data.inputValue(DPVoronoi::aTolerance, &status);
        if (!status) return status;
        const double tolerance = toleranceHandle.asDouble();
        if (!std::isfinite(tolerance) || tolerance < 0.0) {
            return MS::kInvalidParameter;
        }

        if (checkWholeCell) {
            DOMMY_RETURN_IF_FAILURE(DPolyIsIntersectMeshCheck(
                cropMesh, cells, keep, true, tolerance));
            if (keep.size() != cells.size()) return MS::kFailure;
        } else {
            status = clipCellsToMesh(cropMesh, cells);
            if (!status) return status;
        }
    }

    MObject outputObject;
    DMeshData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, output);
    if (!status) return status;
    if (!output) return MS::kFailure;

    DMeshData::DMeshList stagedMeshes;
    stagedMeshes.reserve(cells.size());
    dommy::mesh::MeshMergePlan outputPlan;
    for (std::size_t index = 0U;
         index < cells.size(); ++index) {
        if (!keep[index]) continue;
        if (cells[index].isNull()) return MS::kFailure;

        DMItem mesh;
        status = DMeshInternalData::createFromMayaObject(
            cells[index], mesh);
        if (!status || !mesh || !mesh->hasValidMeshData()) {
            return status ? MStatus(MS::kFailure) : status;
        }
        if (!dommy::mesh::appendMeshMergePlan(mesh->numberOfPoints(), mesh->numFace(),
                mesh->numberOfConnections(), mesh->numberOfUVs(), outputPlan) ||
            !dommy::mesh::validMeshOperationStorage(outputPlan)) {
            return MS::kFailure;
        }
        stagedMeshes.push_back(mesh);
    }
    if (!output->insert(stagedMeshes)) return MS::kFailure;
    if (!output->hasValidMeshTopology()) return MS::kFailure;
    output->updateBBox();

    MDataHandle handle =
        data.outputValue(DPVoronoi::outDM, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        handle, outputObject, *output);
    if (!status) return status;
    handle.setClean();
    return data.setClean(plug);
}

} // namespace

const MTypeId DPVoronoi::typeId(ID_DP_VORONOI);

MObject DPVoronoi::inDP;
MObject DPVoronoi::inObject;
MObject DPVoronoi::aGenerateType;
MObject DPVoronoi::aCellSpace;
MObject DPVoronoi::aCellPeriodic;
MObject DPVoronoi::aUseCropObject;
MObject DPVoronoi::aIsCheckCell;
MObject DPVoronoi::aTolerance;
MObject DPVoronoi::aCellContainerType;
MObject DPVoronoi::aCheckSamePoint;
MObject DPVoronoi::outDC;
MObject DPVoronoi::outDM;

void* DPVoronoi::creator()
{
    return ::dommy::maya::createObject("DPVoronoi", [&]() {
        return new DPVoronoi();
    });
}

MStatus DPVoronoi::compute(const MPlug& plug,
                           MDataBlock& data) try
{
    if (plug != outDC && plug != outDM) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    MDataHandle generationHandle =
        data.inputValue(aGenerateType, &status);
    if (!status) return status;
    const short generationType = generationHandle.asShort();
    if (generationType == 0) {
        EchoWarning(
            name().asChar(),
            "3D Delaunay generation was retired. Select Voronoi instead.");
        return MS::kNotImplemented;
    }
    if (generationType != 1) return MS::kInvalidParameter;

    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input || !input->hasValidPointTopology()) {
        EchoWarning(name().asChar(), "Input point data is invalid.");
        return MS::kInvalidParameter;
    }
    const DPointList* points = input->getPositionPtrConst();
    if (!points || points->size() < 2U ||
        points->size() > kMaxVoronoiSeedCount) {
        EchoWarning(
            name().asChar(),
            "Voronoi requires between 2 and 1,000,000 seed points.");
        return MS::kInvalidParameter;
    }
    for (DPointList::const_iterator point = points->begin();
         point != points->end(); ++point) {
        if (!isFinitePoint(*point)) return MS::kInvalidParameter;
    }

    MDataHandle containerHandle =
        data.inputValue(aCellContainerType, &status);
    if (!status) return status;
    const short containerType = containerHandle.asShort();

    bool cropConnected = false;
    MBoundingBox bounds;
    MObject cropMesh;
    status = prepareBoundsAndCrop(
        this, data, *points, containerType,
        cropConnected, bounds, cropMesh);
    if (!status) return status;

    MDataHandle spaceHandle =
        data.inputValue(aCellSpace, &status);
    if (!status) return status;
    const double cellSpace = spaceHandle.asDouble();
    if (!std::isfinite(cellSpace) || cellSpace <= -1.0 ||
        !dommy::numeric::canRepresentAsFloat(1.0 + cellSpace)) {
        return MS::kInvalidParameter;
    }

    MDataHandle periodicHandle =
        data.inputValue(aCellPeriodic, &status);
    if (!status) return status;
    const bool periodic = periodicHandle.asBool();

    voro::container container(
        bounds.min().x, bounds.max().x,
        bounds.min().y, bounds.max().y,
        bounds.min().z, bounds.max().z,
        1, 1, 1,
        periodic, periodic, periodic, 8);

    MDataHandle duplicateHandle =
        data.inputValue(aCheckSamePoint, &status);
    if (!status) return status;
    if (!addSeeds(
            container, *points, duplicateHandle.asBool())) {
        return MS::kFailure;
    }

    if (plug == outDC) {
        std::vector<MPoint> linePoints;
        std::vector<int> faceCounts;
        std::vector<int> connections;
        if (!dommy::voronoi::extractCellLines(
                container, cellSpace,
                linePoints, faceCounts, connections,
                dommy::generation::kMaxGeneratedPointCount,
                dommy::generation::kMaxGeneratedFaceCount,
                dommy::generation::kMaxWorkItemCount)) {
            return MS::kFailure;
        }
        status = validateLineTopology(
            linePoints, faceCounts, connections);
        if (!status) return status;
        return publishCurveOutput(
            data, plug, linePoints, faceCounts, connections);
    }

    return publishMeshOutput(
        data, plug, container, cellSpace,
        cropConnected, cropMesh);
}
DOMMY_COMPUTE_CATCH("DPVoronoi")

MStatus DPVoronoi::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;

    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id,
        MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    inObject = genericAttribute.create(
        "inObject", "ino", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    MFnEnumAttribute generationAttribute;
    aGenerateType = generationAttribute.create(
        "GenerateType", "gty", 1, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.addField("Retired Delaunay", 0));
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.addField("Voronoi", 1));
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGenerateType));

    aCellSpace = numericAttribute.create(
        "CellSpace", "csp", MFnNumericData::kDouble,
        0.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        numericAttribute.setMin(-0.999999));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCellSpace));

    aCellPeriodic = numericAttribute.create(
        "CellPeriodic", "cpe", MFnNumericData::kBoolean,
        false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCellPeriodic));

    aUseCropObject = numericAttribute.create(
        "UseCropObject", "ucob", MFnNumericData::kBoolean,
        false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseCropObject));

    aIsCheckCell = numericAttribute.create(
        "IsCheckCell", "icch", MFnNumericData::kBoolean,
        false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCheckCell));

    aTolerance = numericAttribute.create(
        "Tolerance", "tol", MFnNumericData::kDouble,
        0.001, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    MFnEnumAttribute containerAttribute;
    aCellContainerType = containerAttribute.create(
        "CellContainerType", "cconty", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        containerAttribute.addField("All", 0));
    DOMMY_RETURN_IF_FAILURE(
        containerAttribute.addField("Point Base", 1));
    DOMMY_RETURN_IF_FAILURE(
        containerAttribute.addField("Crop Object BBox", 2));
    DOMMY_RETURN_IF_FAILURE(
        containerAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCellContainerType));

    aCheckSamePoint = numericAttribute.create(
        "CheckSamePoint", "chsmpt",
        MFnNumericData::kBoolean, false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCheckSamePoint));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id,
        MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id,
        MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    const MObject inputs[] = {
        inDP, inObject, aGenerateType, aCellSpace,
        aCellPeriodic, aUseCropObject, aIsCheckCell,
        aTolerance, aCellContainerType, aCheckSamePoint
    };
    const std::size_t inputCount =
        sizeof(inputs) / sizeof(inputs[0]);
    for (std::size_t index = 0U; index < inputCount; ++index) {
        DOMMY_RETURN_IF_FAILURE(
            attributeAffects(inputs[index], outDC));
        DOMMY_RETURN_IF_FAILURE(
            attributeAffects(inputs[index], outDM));
    }

    return MS::kSuccess;
}

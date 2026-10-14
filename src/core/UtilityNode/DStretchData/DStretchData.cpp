//
//  DStretchData.cpp
//  dommyTool
//
//  Created by yuzo on 6/5/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaMeshColorOutput.h"
#include "DStretchData.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"

#include "DHashGrid.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace {

bool hasMatchingEdgeTopology(MObject& sourceMesh,
                             MObject& relaxedMesh,
                             std::size_t expectedCount,
                             MStatus& status)
{
    MItMeshEdge sourceEdge(sourceMesh, &status);
    if (!status) return false;
    MItMeshEdge relaxedEdge(relaxedMesh, &status);
    if (!status) return false;

    std::size_t visited = 0U;
    while (true) {
        const bool sourceDone = sourceEdge.isDone(&status);
        if (!status) return false;
        const bool relaxedDone = relaxedEdge.isDone(&status);
        if (!status) return false;
        if (sourceDone || relaxedDone) return sourceDone && relaxedDone && visited == expectedCount;
        if (visited >= expectedCount) return false;
        ++visited;

        const int sourceA = sourceEdge.index(0, &status);
        if (!status) return false;
        const int sourceB = sourceEdge.index(1, &status);
        if (!status) return false;
        const int relaxedA = relaxedEdge.index(0, &status);
        if (!status) return false;
        const int relaxedB = relaxedEdge.index(1, &status);
        if (!status) return false;

        const bool sameEdge =
            (sourceA == relaxedA && sourceB == relaxedB) ||
            (sourceA == relaxedB && sourceB == relaxedA);
        if (!sameEdge) return false;

        status = sourceEdge.next();
        if (!status) return false;
        status = relaxedEdge.next();
        if (!status) return false;
    }
}

} // namespace

const MTypeId DStretchData::typeId( 0x4005A );

MObject DStretchData::inMesh;
MObject DStretchData::relaxMesh;

MObject DStretchData::aIsStretch;
MObject DStretchData::aCalcType;
MObject DStretchData::aStretchMap;
MObject DStretchData::aCompressMap;

MObject DStretchData::aSampleRadius;

MObject DStretchData::aIsVector;

MObject DStretchData::smIterate;

MObject DStretchData::outMesh;


void *DStretchData::creator() {
    return ::dommy::maya::createObject("DStretchData", [&]() {
        return new DStretchData();
    });
}

void DStretchData::postConstructor() {
    dommy::maya::initializeNodeRamps("DStretchData", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultColorRamp(
            thisMObject(), aStretchMap,
            {MColor(1.0, 0.0, 0.0), MColor(0.0, 1.0, 0.0), MColor(0.0, 0.0, 1.0)},
            {1.0f, 0.5f, 0.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultColorRamp(
            thisMObject(), aCompressMap,
            {MColor(1.0, 0.0, 0.0), MColor(0.0, 1.0, 0.0), MColor(0.0, 0.0, 1.0)},
            {1.0f, 0.5f, 0.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        return MS::kSuccess;
    });
}


MStatus DStretchData::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;
    MStatus status;
    MObject sourceObject, relaxedObject;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMeshObject(inMesh, data, sourceObject));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMeshObject(relaxMesh, data, relaxedObject));
    MFnMesh sourceMesh(sourceObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MFnMesh relaxedMesh(relaxedObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int sourceCount = sourceMesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int relaxedCount = relaxedMesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t vertexCount = 0U;
    if (!dommy::generation::checkedCount(sourceCount,
            dommy::generation::kMaxGeneratedPointCount, vertexCount) ||
        sourceCount != relaxedCount || !dommy::mesh::validMeshPairStorage(vertexCount))
        return MS::kInvalidParameter;

    const MDataHandle radiusHandle = data.inputValue(aSampleRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double radius = radiusHandle.asDouble();
    const MDataHandle modeHandle = data.inputValue(aCalcType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int mode = modeHandle.asInt();
    float searchRadius = 0.0f;
    if (!std::isfinite(radius) || (mode != 0 && mode != 1) ||
        (mode == 1 && (!dommy::numeric::checkedFloatCast(radius, searchRadius) ||
                       searchRadius <= 0.0f))) return MS::kInvalidParameter;

    MColorArray colors;
    DOMMY_RETURN_IF_FAILURE(colors.setLength(static_cast<unsigned int>(vertexCount)));
    if (colors.length() != vertexCount) return MS::kFailure;
    for (unsigned int i = 0U; i < colors.length(); ++i) colors[i] = MColor(0, 0, 0, 1);

    // Historical controls/ramp attributes remain serialized, but are not
    // activated here: the archived implementation did not use them either.
    if (mode == 0) {
        const int sourceEdges = sourceMesh.numEdges(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int relaxedEdges = relaxedMesh.numEdges(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t edgeCount = 0U;
        dommy::mesh::MeshMergePlan plan;
        if (!dommy::generation::checkedCount(sourceEdges,
                dommy::generation::kMaxWorkItemCount, edgeCount) ||
            sourceEdges != relaxedEdges ||
            !dommy::mesh::appendMeshMergePlan(vertexCount, 0U, edgeCount, 0U, plan) ||
            !dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;
        const bool matches = hasMatchingEdgeTopology(sourceObject, relaxedObject, edgeCount, status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!matches) return MS::kInvalidParameter;

        std::vector<double> relaxedLengths(edgeCount, 0.0);
        MItMeshEdge relaxedEdge(relaxedObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        status = dommy::maya::visitMeshElements(relaxedEdge, edgeCount, [&](MItMeshEdge& edge) -> MStatus {
            const int index = edge.index(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (!dommy::memory::indexInRange(index, edgeCount)) return MS::kInvalidParameter;
            double length = 0.0;
            DOMMY_RETURN_IF_FAILURE(edge.getLength(length));
            if (!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
            relaxedLengths[static_cast<std::size_t>(index)] = length;
            return MS::kSuccess;
        });
        DOMMY_RETURN_IF_FAILURE(status);

        MItMeshEdge sourceEdge(sourceObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        status = dommy::maya::visitMeshElements(sourceEdge, edgeCount, [&](MItMeshEdge& edge) -> MStatus {
            const int index = edge.index(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (!dommy::memory::indexInRange(index, edgeCount)) return MS::kInvalidParameter;
            double length = 0.0;
            DOMMY_RETURN_IF_FAILURE(edge.getLength(length));
            if (!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
            const double relaxed = relaxedLengths[static_cast<std::size_t>(index)];
            if (length == relaxed) return MS::kSuccess;
            float change = 0.0f;
            if (!dommy::numeric::checkedFloatCast(std::fabs(length - relaxed), change))
                return MS::kInvalidParameter;
            for (int endpoint = 0; endpoint < 2; ++endpoint) {
                const int vertex = edge.index(endpoint, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (!dommy::memory::indexInRange(vertex, vertexCount)) return MS::kInvalidParameter;
                const MColor previous = colors[static_cast<unsigned int>(vertex)];
                MColor updated;
                // Preserve the edge-mode channel convention and legacy alpha
                // accumulation, while checking both narrowing and addition.
                if (!dommy::numeric::checkedFloatCast(
                        static_cast<double>(previous.r) + (relaxed > length ? change : 0.0f), updated.r) ||
                    !dommy::numeric::checkedFloatCast(
                        static_cast<double>(previous.g) + (length > relaxed ? change : 0.0f), updated.g) ||
                    !dommy::numeric::checkedFloatCast(static_cast<double>(previous.a) + 1.0, updated.a))
                    return MS::kInvalidParameter;
                updated.b = previous.b;
                colors[static_cast<unsigned int>(vertex)] = updated;
            }
            return MS::kSuccess;
        });
        DOMMY_RETURN_IF_FAILURE(status);
    } else if (vertexCount != 0U) {
        MPointArray relaxedPoints, currentPoints;
        DOMMY_RETURN_IF_FAILURE(relaxedMesh.getPoints(relaxedPoints, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(sourceMesh.getPoints(currentPoints, MSpace::kObject));
        if (relaxedPoints.length() != vertexCount || currentPoints.length() != vertexCount)
            return MS::kFailure;
        std::size_t coordinateCount = 0U;
        if (!dommy::generation::checkedProduct(vertexCount, 3U,
                dommy::generation::kMaxWorkItemCount, coordinateCount)) return MS::kInvalidParameter;
        std::vector<DReal> positions(coordinateCount);
        MPoint minimum = relaxedPoints[0U], maximum = relaxedPoints[0U];
        for (unsigned int i = 0U; i < relaxedPoints.length(); ++i) {
            const MPoint& point = relaxedPoints[i];
            if (!dommy::maya::isFinitePoint(point) || !dommy::maya::isFinitePoint(currentPoints[i]))
                return MS::kInvalidParameter;
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!dommy::numeric::checkedFloatCast(point[axis], positions[3U*i + axis]))
                    return MS::kInvalidParameter;
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
        // Grid bounds must share the object-space frame of its stored points.
        // A parent transform's bounding box can include unrelated children and
        // cannot represent an upstream procedural mesh-data node.
        DTools::Vec3 lower, upper;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::checkedFloatCast(minimum[axis] - 0.5, lower[axis]) ||
                !dommy::numeric::checkedFloatCast(maximum[axis] + 0.5, upper[axis]))
                return MS::kInvalidParameter;
        }
        DHashGrid grid(3);
        if (!grid.Setup(lower, upper, 1.0f, static_cast<int>(vertexCount)) ||
            !grid.SetObjectToCell(positions.data(), static_cast<unsigned int>(vertexCount)))
            return MS::kInvalidParameter;
        for (unsigned int i = 0U; i < relaxedPoints.length(); ++i) {
            DTools::Vec3 position;
            for (unsigned int axis = 0U; axis < 3U; ++axis) position[axis] = positions[3U*i + axis];
            std::vector<DNeigh> neighbours;
            if (!grid.GetNN(position, positions.data(), static_cast<unsigned int>(vertexCount),
                            neighbours, searchRadius)) return MS::kInvalidParameter;
            double stretchSum = 0.0, compressionSum = 0.0;
            std::size_t stretchCount = 0U, compressionCount = 0U;
            for (const DNeigh& neighbour : neighbours) {
                if (!dommy::memory::indexInRange(neighbour.Idx, vertexCount)) return MS::kInvalidParameter;
                const unsigned int j = static_cast<unsigned int>(neighbour.Idx);
                if (j == i) continue;
                const double change = currentPoints[i].distanceTo(currentPoints[j]) -
                                      relaxedPoints[i].distanceTo(relaxedPoints[j]);
                if (!std::isfinite(change)) return MS::kInvalidParameter;
                if (change > 0.0) { stretchSum += change; ++stretchCount; }
                else if (change < 0.0) { compressionSum -= change; ++compressionCount; }
                if (!std::isfinite(stretchSum) || !std::isfinite(compressionSum))
                    return MS::kInvalidParameter;
            }
            float red = 0.0f, green = 0.0f;
            if (!dommy::numeric::checkedFloatCast(stretchCount ? stretchSum / stretchCount : 0.0, red) ||
                !dommy::numeric::checkedFloatCast(compressionCount ? compressionSum / compressionCount : 0.0, green))
                return MS::kInvalidParameter;
            colors[i] = MColor(red, green, 0.0f, 1.0f);
        }
    }

    MFnMeshData meshData;
    const MObject outputObject = meshData.create(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (outputObject.isNull()) return MS::kFailure;
    MFnMesh outputMesh;
    const MObject copiedMesh = outputMesh.copy(sourceObject, outputObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (copiedMesh.isNull()) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::setMeshDataVertexColors(
        outputMesh, colors, MString("stretch")));
    MDataHandle output = data.outputValue(outMesh, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(output.set(outputObject));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DStretchData")

MStatus DStretchData::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnEnumAttribute eAttr;
	
	//	MFnGenericAttribute outAttr;
	//	MFnUnitAttribute unitAttr;
	
    inMesh= tAttr.create( "inMesh", "im",MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));
	
	relaxMesh= tAttr.create( "relaxMesh", "rem",MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(relaxMesh));
	
	aIsStretch = nAttr.create("IsStretch", "iss", MFnNumericData::kBoolean, true, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStretch));
	
	aCalcType = eAttr.create("CalcType", "caty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Edge Length", 0));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Grid Sampling", 1));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCalcType));

	
	aStretchMap = MRampAttribute::createColorRamp("StretchMap", "stm", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStretchMap));
	
	aCompressMap = MRampAttribute::createColorRamp("CompressMap", "cmm", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCompressMap));
	
	aSampleRadius = nAttr.create("SampleRadius", "srad", MFnNumericData::kDouble, 1.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSampleRadius));
	
	aIsVector = nAttr.create("IsVector", "isv", MFnNumericData::kBoolean, false, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVector));
	
	smIterate = nAttr.create("smIterate", "sit", MFnNumericData::kInt, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
	DOMMY_RETURN_IF_FAILURE(addAttribute(smIterate));
	
	
	
	
	outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inMesh, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( relaxMesh, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsStretch, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aCalcType, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStretchMap, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aCompressMap, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSampleRadius, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsVector, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( smIterate, outMesh ));
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DStretchData", "initialize", std::current_exception());
}

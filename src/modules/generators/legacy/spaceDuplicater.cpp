

#include "common/utility/DMayaEntryPointGuard.h"
#include "spaceDuplicater.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "DLegacyDuplicaterTypeIds.h"
#include "dupMesh.h"
#include "DRandomGenerator.h"

#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MObjectArray.h>

#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MGlobal.h>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>



const MTypeId spaceDuplicater::typeId(ID_LEGACY_SPACE_DUPLICATER);

void *spaceDuplicater::creator()
{
    return ::dommy::maya::createObject("spaceDuplicater", [&]() {
        return new spaceDuplicater();
    });
}

MObject spaceDuplicater::duplicateObjects;
MObject spaceDuplicater::typeSeed;

MObject spaceDuplicater::baldnessMesh;

MObject spaceDuplicater::numX;
MObject spaceDuplicater::numY;
MObject spaceDuplicater::numZ;

MObject spaceDuplicater::offsetX;
MObject spaceDuplicater::offsetY;
MObject spaceDuplicater::offsetZ;

MObject spaceDuplicater::thickness;

MObject spaceDuplicater::outPutMesh;


MStatus spaceDuplicater::compute( const MPlug& plug, MDataBlock& block ) try
{
	if (plug != outPutMesh) return MS::kUnknownParameter;

	MStatus status;
	MDataHandle input = block.inputValue(typeSeed, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int typeSeedValue = input.asInt();

	input = block.inputValue(numX, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int rawNumX = input.asInt();
	input = block.inputValue(numY, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int rawNumY = input.asInt();
	input = block.inputValue(numZ, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int rawNumZ = input.asInt();

	input = block.inputValue(offsetX, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double offsetXValue = input.asDouble();
	input = block.inputValue(offsetY, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double offsetYValue = input.asDouble();
	input = block.inputValue(offsetZ, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double offsetZValue = input.asDouble();
	input = block.inputValue(thickness, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double thicknessValue = input.asDouble();

	std::size_t countX = 0U;
	std::size_t countY = 0U;
	std::size_t countZ = 0U;
	std::size_t countXY = 0U;
	std::size_t placementBudget = 0U;
	if (!std::isfinite(offsetXValue) || !std::isfinite(offsetYValue) ||
		!std::isfinite(offsetZValue) || !std::isfinite(thicknessValue) ||
		thicknessValue < 0.0 ||
		!dommy::generation::checkedCount(
			rawNumX, dommy::generation::kMaxGeneratedTransformCount, countX) ||
		!dommy::generation::checkedCount(
			rawNumY, dommy::generation::kMaxGeneratedTransformCount, countY) ||
		!dommy::generation::checkedCount(
			rawNumZ, dommy::generation::kMaxGeneratedTransformCount, countZ) ||
		countX == 0U || countY == 0U || countZ == 0U ||
		!dommy::generation::checkedProduct(
			countX, countY, dommy::generation::kMaxGeneratedTransformCount,
			countXY) ||
		!dommy::generation::checkedProduct(
			countXY, countZ, dommy::generation::kMaxGeneratedTransformCount,
			placementBudget)) {
		MGlobal::displayError(
			"spaceDuplicater: grid inputs exceed the safe generation limits.");
		return MS::kInvalidParameter;
	}

	MFnMesh baldnessMeshFunction;
	bool useBaldnessMesh = false;
	MDataHandle baldnessHandle = block.inputValue(baldnessMesh, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (baldnessHandle.type() == MFnData::kMesh &&
		!baldnessHandle.data().isNull()) {
		status = baldnessMeshFunction.setObject(baldnessHandle.data());
		CHECK_MSTATUS_AND_RETURN_IT(status);
		useBaldnessMesh = true;
	}

	MArrayDataHandle duplicateHandle =
		block.inputArrayValue(duplicateObjects, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const unsigned int duplicateCount = duplicateHandle.elementCount(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (duplicateCount == 0U) {
		MGlobal::displayError(
			"spaceDuplicater: at least one duplicate mesh is required.");
		return MS::kInvalidParameter;
	}

	std::vector<dupMesh> duplicateShapes;
	duplicateShapes.reserve(duplicateCount);
	std::size_t maximumVertexCount = 0U;
	std::size_t maximumPolygonCount = 0U;
	std::size_t maximumConnectionCount = 0U;
	std::size_t maximumUvCount = 0U;
	for (unsigned int duplicateIndex = 0U;
		 duplicateIndex < duplicateCount; ++duplicateIndex) {
		status = duplicateHandle.jumpToArrayElement(duplicateIndex);
		CHECK_MSTATUS_AND_RETURN_IT(status);
		MDataHandle duplicateData = duplicateHandle.inputValue(&status);
		CHECK_MSTATUS_AND_RETURN_IT(status);
		duplicateShapes.emplace_back(duplicateData.data());
		const dupMesh& shape = duplicateShapes.back();
		if (!shape.isValid()) {
			MGlobal::displayError(
				"spaceDuplicater: duplicate mesh data is invalid or empty.");
			return MS::kInvalidParameter;
		}
		maximumVertexCount =
			std::max(maximumVertexCount,
				static_cast<std::size_t>(shape.getTotalVertexNum()));
		maximumPolygonCount =
			std::max(maximumPolygonCount,
				static_cast<std::size_t>(shape.getTotalPolygonNum()));
		maximumConnectionCount =
			std::max(maximumConnectionCount,
				static_cast<std::size_t>(shape.getTotalConnectionNum()));
		maximumUvCount =
			std::max(maximumUvCount,
				static_cast<std::size_t>(shape.getTotalUVNum()));
	}

	std::size_t outputBudget = 0U;
	if (!dommy::generation::checkedProduct(
			placementBudget, maximumVertexCount,
			dommy::generation::kMaxGeneratedPointCount, outputBudget) ||
		!dommy::generation::checkedProduct(
			placementBudget, maximumPolygonCount,
			dommy::generation::kMaxGeneratedFaceCount, outputBudget) ||
		!dommy::generation::checkedProduct(
			placementBudget, maximumConnectionCount,
			dommy::generation::kMaxWorkItemCount, outputBudget) ||
		!dommy::generation::checkedProduct(
			placementBudget, maximumUvCount,
			dommy::generation::kMaxGeneratedPointCount, outputBudget)) {
		MGlobal::displayError(
			"spaceDuplicater: output mesh exceeds the generation budget.");
		return MS::kInvalidParameter;
	}

	MFloatPointArray placementPoints;
	for (std::size_t xIndex = 0U; xIndex < countX; ++xIndex) {
		for (std::size_t yIndex = 0U; yIndex < countY; ++yIndex) {
			for (std::size_t zIndex = 0U; zIndex < countZ; ++zIndex) {
				const MFloatPoint worldPosition(
					static_cast<float>(offsetXValue * static_cast<double>(xIndex)),
					static_cast<float>(offsetYValue * static_cast<double>(yIndex)),
					static_cast<float>(offsetZValue * static_cast<double>(zIndex)));
				if (!std::isfinite(worldPosition.x) ||
					!std::isfinite(worldPosition.y) ||
					!std::isfinite(worldPosition.z)) {
					return MS::kInvalidParameter;
				}

				bool includePoint = !useBaldnessMesh;
				if (useBaldnessMesh) {
					const MPoint centerPosition(worldPosition);
					MPoint closestPoint;
					MVector normal;
					status = baldnessMeshFunction.getClosestPointAndNormal(
						centerPosition, closestPoint, normal, MSpace::kWorld);
					CHECK_MSTATUS_AND_RETURN_IT(status);
					const double distance = closestPoint.distanceTo(centerPosition);
					const double dot = normal * (closestPoint - centerPosition);
					if (!std::isfinite(distance) || !std::isfinite(dot)) {
						return MS::kFailure;
					}
					includePoint =
						(thicknessValue == 0.0 || distance < thicknessValue) &&
						(distance < 0.0001 || dot > 0.0);
				}
				if (includePoint) {
					status = placementPoints.append(worldPosition);
					CHECK_MSTATUS_AND_RETURN_IT(status);
				}
			}
		}
	}

	MFloatPointArray finalVertices;
	MFloatArray finalUCoordinates;
	MFloatArray finalVCoordinates;
	MIntArray finalUvIds;
	MIntArray finalUvCounts;
	MIntArray finalPolygonCounts;
	MIntArray finalPolygonConnections;
	int vertexOffset = 0;
	int uvOffset = 0;

	dommy::math::RandomGenerator randomGenerator(
		static_cast<unsigned int>(typeSeedValue));
	for (unsigned int placementIndex = 0U;
		 placementIndex < placementPoints.length(); ++placementIndex) {
		const std::size_t typeIndex =
			randomGenerator.index(duplicateShapes.size());
		const dupMesh& shape = duplicateShapes[typeIndex];

		MFloatPointArray vertices;
		DOMMY_RETURN_IF_FAILURE(vertices.copy(shape.getVertexArray()));
		for (unsigned int vertex = 0U; vertex < vertices.length(); ++vertex) {
			vertices[vertex].x += placementPoints[placementIndex].x;
			vertices[vertex].y += placementPoints[placementIndex].y;
			vertices[vertex].z += placementPoints[placementIndex].z;
			if (!std::isfinite(vertices[vertex].x) ||
				!std::isfinite(vertices[vertex].y) ||
				!std::isfinite(vertices[vertex].z)) {
				return MS::kFailure;
			}
			status = finalVertices.append(vertices[vertex]);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}

		const MIntArray& polygonCounts = shape.getPolyCountsList();
		for (unsigned int polygon = 0U; polygon < polygonCounts.length(); ++polygon) {
			status = finalPolygonCounts.append(polygonCounts[polygon]);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}

		const MIntArray& polygonConnections = shape.getPolyConnectList();
		for (unsigned int connection = 0U;
		 connection < polygonConnections.length(); ++connection) {
			status = finalPolygonConnections.append(
				polygonConnections[connection] + vertexOffset);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}

		const MFloatArray& uCoordinates = shape.getUCoords();
		const MFloatArray& vCoordinates = shape.getVCoords();
		if (uCoordinates.length() != vCoordinates.length()) return MS::kFailure;
		for (unsigned int uv = 0U; uv < uCoordinates.length(); ++uv) {
			status = finalUCoordinates.append(uCoordinates[uv]);
			CHECK_MSTATUS_AND_RETURN_IT(status);
			status = finalVCoordinates.append(vCoordinates[uv]);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}

		const MIntArray& uvIds = shape.getUVIdsArray();
		const MIntArray& uvCounts = shape.getUVCountsArray();
		for (unsigned int uvId = 0U; uvId < uvIds.length(); ++uvId) {
			status = finalUvIds.append(uvIds[uvId] + uvOffset);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}
		for (unsigned int polygon = 0U; polygon < uvCounts.length(); ++polygon) {
			status = finalUvCounts.append(uvCounts[polygon]);
			CHECK_MSTATUS_AND_RETURN_IT(status);
		}

		vertexOffset += static_cast<int>(vertices.length());
		uvOffset += static_cast<int>(uCoordinates.length());
	}

	MFnMeshData meshDataFunction;
	MObject outputData = meshDataFunction.create(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (finalVertices.length() != 0U) {
		MFnMesh meshFunction;
		meshFunction.create(
			static_cast<int>(finalVertices.length()),
			static_cast<int>(finalPolygonCounts.length()),
			finalVertices, finalPolygonCounts, finalPolygonConnections,
			finalUCoordinates, finalVCoordinates, outputData, &status);
		CHECK_MSTATUS_AND_RETURN_IT(status);
		status = meshFunction.assignUVs(finalUvCounts, finalUvIds);
		CHECK_MSTATUS_AND_RETURN_IT(status);
	}

	MDataHandle outputHandle = block.outputValue(outPutMesh, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = outputHandle.set(outputData);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	outputHandle.setClean();
	return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("spaceDuplicater")

MStatus spaceDuplicater::initialize()
{
	MStatus status;
	MFnTypedAttribute typedAttr;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	
	duplicateObjects = typedAttr.create(
		"duplicateObjects", "dpo", MFnMeshData::kMesh, MObject::kNullObj,
		&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(typedAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(typedAttr.setArray(true));
	DOMMY_RETURN_IF_FAILURE(typedAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(typedAttr.setWritable(true));
	
	typeSeed = nAttr.create("typeSeed", "ts", MFnNumericData::kInt, 1, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	baldnessMesh = gAttr.create("baldnessMesh", "bm", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden(true));
	
	numX = nAttr.create("numX", "nx", MFnNumericData::kInt, 3, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	numY = nAttr.create("numY", "ny", MFnNumericData::kInt, 3, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	numZ = nAttr.create("numZ", "nz", MFnNumericData::kInt, 3, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	offsetX = nAttr.create(
		"offsetX", "ox", MFnNumericData::kDouble, 1.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	offsetY = nAttr.create(
		"offsetY", "oy", MFnNumericData::kDouble, 1.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	offsetZ = nAttr.create(
		"offsetZ", "oz", MFnNumericData::kDouble, 1.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	thickness = nAttr.create(
		"thickness", "t", MFnNumericData::kDouble, 0.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	outPutMesh = gAttr.create("outPutMesh", "opm", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(duplicateObjects));
	DOMMY_RETURN_IF_FAILURE(addAttribute(typeSeed));
	DOMMY_RETURN_IF_FAILURE(addAttribute(baldnessMesh));
	DOMMY_RETURN_IF_FAILURE(addAttribute(numX));
	DOMMY_RETURN_IF_FAILURE(addAttribute(numY));
	DOMMY_RETURN_IF_FAILURE(addAttribute(numZ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offsetX));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offsetY));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offsetZ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(thickness));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outPutMesh));

	DOMMY_RETURN_IF_FAILURE(attributeAffects(duplicateObjects, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(typeSeed, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(baldnessMesh, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(numX, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(numY, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(numZ, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetX, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetY, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetZ, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(thickness, outPutMesh));
	
	return MS::kSuccess;
}
	

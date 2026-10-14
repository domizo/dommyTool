//
//  foamGenerateNode.cpp
//  dommyTool
//
//  Created by yuzo on 4/5/13.
//
//

#include "foamGenerateNode.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"

#include <maya/MFloatArray.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <limits>



//attribute
MObject foamGenerateNode::inPosition;
MObject foamGenerateNode::radiusArray;

MObject foamGenerateNode::frequency;
MObject foamGenerateNode::radiusOffset;
MObject foamGenerateNode::radiusResultOffset;

//for deform
MObject foamGenerateNode::selfDeform;

MObject foamGenerateNode::outPutMesh;


const MTypeId foamGenerateNode::typeId( 0x00331 );

void *foamGenerateNode::creator() {
    return ::dommy::maya::createObject("foamGenerateNode", [&]() {
        return new foamGenerateNode();
    });
}

MStatus foamGenerateNode::compute( const MPlug& plug, MDataBlock& block ) try {
	if (plug != outPutMesh) return MS::kUnknownParameter;

	MStatus status;
	const MDataHandle frequencyHandle = block.inputValue(frequency, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int frequencyValue = frequencyHandle.asInt();
	const MDataHandle radiusOffsetHandle = block.inputValue(radiusOffset, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double radiusOffsetValue = radiusOffsetHandle.asDouble();
	const MDataHandle radiusResultOffsetHandle = block.inputValue(radiusResultOffset, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double radiusResultOffsetValue = radiusResultOffsetHandle.asDouble();
	const MDataHandle selfDeformHandle = block.inputValue(selfDeform, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const bool selfDeformValue = selfDeformHandle.asBool();
	if (!std::isfinite(radiusOffsetValue) ||
		!std::isfinite(radiusResultOffsetValue)) {
		return MS::kInvalidParameter;
	}

	MDataHandle positionHandle = block.inputValue(inPosition, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (positionHandle.type() != MFnData::kVectorArray) {
		return MS::kInvalidParameter;
	}
	MFnVectorArrayData positionData(positionHandle.data(), &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const MVectorArray positions = positionData.array(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MDataHandle radiusHandle = block.inputValue(radiusArray, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (radiusHandle.type() != MFnData::kFloatArray) {
		return MS::kInvalidParameter;
	}
	MFnFloatArrayData radiusData(radiusHandle.data(), &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const MFloatArray radii = radiusData.array(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (positions.length() != radii.length()) {
		MGlobal::displayError(
			"foamGenerateNode: position and radius counts must match.");
		return MS::kInvalidParameter;
	}

	const std::size_t objectCount =
		static_cast<std::size_t>(positions.length());
	if (objectCount > dommy::generation::kMaxGeneratedPointCount) {
		return MS::kFailure;
	}

	dommy::geometry::IcosahedronTopology topology;
	if (!dommy::geometry::buildHistoricalIcosahedron(
			frequencyValue, topology)) {
		MGlobal::displayError(
			"foamGenerateNode: frequency exceeds the topology budget.");
		return MS::kInvalidParameter;
	}
	const std::size_t baseVertexCount = topology.points.size();
	const std::size_t baseFaceCount = topology.triangles.size();

	std::size_t totalVertexCount = 0U;
	std::size_t totalFaceCount = 0U;
	std::size_t totalConnectionCount = 0U;
	if (!dommy::generation::checkedProduct(
			baseVertexCount, objectCount,
			dommy::generation::kMaxGeneratedPointCount,
			totalVertexCount) ||
		!dommy::generation::checkedProduct(
			baseFaceCount, objectCount,
			dommy::generation::kMaxGeneratedFaceCount,
			totalFaceCount) ||
		!dommy::generation::checkedProduct(
			totalFaceCount, 3U,
			dommy::generation::kMaxWorkItemCount,
			totalConnectionCount) ||
		totalVertexCount >
			static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
		totalFaceCount >
			static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		MGlobal::displayError(
			"foamGenerateNode: output mesh exceeds the generation budget.");
		return MS::kFailure;
	}
	if (selfDeformValue) {
		std::size_t pairWork = 0U;
		std::size_t deformationWork = 0U;
		if (!dommy::generation::checkedProduct(
				objectCount, objectCount,
				dommy::generation::kMaxWorkItemCount, pairWork) ||
			!dommy::generation::checkedProduct(
				pairWork, baseVertexCount,
				dommy::generation::kMaxWorkItemCount,
				deformationWork)) {
			MGlobal::displayError(
				"foamGenerateNode: self-deformation exceeds the work budget.");
			return MS::kFailure;
		}
	}

	MFloatPointArray outputVertices;
	MIntArray outputFaceCounts;
	MIntArray outputConnections;
	for (std::size_t objectIndex = 0U;
		 objectIndex < objectCount; ++objectIndex) {
		const MVector center = positions[static_cast<unsigned int>(objectIndex)];
		const double effectiveRadius =
			static_cast<double>(radii[static_cast<unsigned int>(objectIndex)]) +
			radiusOffsetValue;
		const double outputRadius =
			effectiveRadius + radiusResultOffsetValue;
		if (!dommy::maya::isFiniteVector(center) ||
			!std::isfinite(effectiveRadius) || effectiveRadius < 0.0 ||
			!std::isfinite(outputRadius) || outputRadius < 0.0 ||
			(selfDeformValue &&
			 effectiveRadius <= std::numeric_limits<double>::epsilon())) {
			MGlobal::displayError(
				"foamGenerateNode: position or effective radius is invalid.");
			return MS::kInvalidParameter;
		}

		MVectorArray deformation;
		if (selfDeformValue) {
			DOMMY_RETURN_IF_FAILURE(deformation.setLength(static_cast<unsigned int>(baseVertexCount)));
			if (deformation.length() != baseVertexCount) return MS::kFailure;
			for (unsigned int vertex = 0U; vertex < deformation.length(); ++vertex) {
				DOMMY_RETURN_IF_FAILURE(deformation.set(MVector(0.0, 0.0, 0.0), vertex));
			}
			for (std::size_t targetIndex = 0U;
				 targetIndex < objectCount; ++targetIndex) {
				if (targetIndex == objectIndex) continue;
				const MVector target =
					positions[static_cast<unsigned int>(targetIndex)];
				const double targetRadius =
					static_cast<double>(
						radii[static_cast<unsigned int>(targetIndex)]) +
					radiusOffsetValue;
				if (!dommy::maya::isFiniteVector(target) ||
					!std::isfinite(targetRadius) || targetRadius < 0.0) {
					return MS::kInvalidParameter;
				}

				const MVector delta = target - center;
				const double distance = delta.length();
				const double radiusSum = targetRadius + effectiveRadius;
				if (!std::isfinite(distance) || !std::isfinite(radiusSum)) {
					return MS::kFailure;
				}
				if (distance <= std::numeric_limits<double>::epsilon() ||
					distance >= radiusSum) {
					continue;
				}

				const double intersection =
					(effectiveRadius * effectiveRadius -
					 targetRadius * targetRadius +
					 distance * distance) /
					(2.0 * distance);
				const MVector planeNormal = delta / distance;
				const MVector planePosition = intersection * planeNormal;
				const double normalLengthSquared =
					planeNormal * planeNormal;
				if (!std::isfinite(intersection) ||
					!dommy::maya::isFiniteVector(planeNormal) ||
					!dommy::maya::isFiniteVector(planePosition) ||
					normalLengthSquared <=
						std::numeric_limits<double>::epsilon()) {
					return MS::kFailure;
				}

				for (std::size_t vertexIndex = 0U;
					 vertexIndex < baseVertexCount; ++vertexIndex) {
					const dommy::geometry::IcosahedronPoint& unitPoint =
						topology.points[vertexIndex];
					const MVector spherePoint(
						unitPoint.x * effectiveRadius,
						unitPoint.y * effectiveRadius,
						unitPoint.z * effectiveRadius);
					const double projection =
						-(planeNormal * (spherePoint - planePosition)) /
						normalLengthSquared;
					const MVector projectedPoint =
						spherePoint + projection * planeNormal;
					const MVector difference =
						spherePoint - projectedPoint;
					const double dot = planePosition * difference;
					const double distanceFromPlane = difference.length();
					if (!std::isfinite(projection) || !std::isfinite(dot) ||
						!std::isfinite(distanceFromPlane)) {
						return MS::kFailure;
					}

					MVector adjustment(0.0, 0.0, 0.0);
					if (intersection < 0.0 && dot < 0.0) {
						double bias =
							1.0 - (-intersection / effectiveRadius);
						if (bias < 0.0) bias = 0.0;
						adjustment =
							-(distanceFromPlane * bias) * planeNormal;
					} else if (intersection >= 0.0 && dot > 0.0) {
						adjustment = -distanceFromPlane * planeNormal;
					}
					const MVector updated =
						deformation[static_cast<unsigned int>(vertexIndex)] +
						adjustment;
					if (!dommy::maya::isFiniteVector(updated)) {
						return MS::kFailure;
					}
					DOMMY_RETURN_IF_FAILURE(deformation.set(
						updated, static_cast<unsigned int>(vertexIndex)));
				}
			}
		}

		for (std::size_t vertexIndex = 0U;
			 vertexIndex < baseVertexCount; ++vertexIndex) {
			const dommy::geometry::IcosahedronPoint& unitPoint =
				topology.points[vertexIndex];
			MVector generated(
				unitPoint.x * outputRadius,
				unitPoint.y * outputRadius,
				unitPoint.z * outputRadius);
			if (selfDeformValue) {
				generated +=
					deformation[static_cast<unsigned int>(vertexIndex)];
			}
			generated += center;
			const MPoint generatedPoint(generated);
			if (!dommy::maya::isFloatRepresentablePoint(generatedPoint)) {
				return MS::kFailure;
			}
			DOMMY_RETURN_IF_FAILURE(outputVertices.append(MFloatPoint(
				static_cast<float>(generated.x),
				static_cast<float>(generated.y),
				static_cast<float>(generated.z))));
		}

		const std::size_t vertexOffset =
			baseVertexCount * objectIndex;
		for (std::size_t faceIndex = 0U;
			 faceIndex < baseFaceCount; ++faceIndex) {
			const dommy::geometry::IcosahedronTriangle& triangle =
				topology.triangles[faceIndex];
			DOMMY_RETURN_IF_FAILURE(outputFaceCounts.append(3));
			DOMMY_RETURN_IF_FAILURE(outputConnections.append(static_cast<int>(
				vertexOffset + static_cast<std::size_t>(triangle.first))));
			DOMMY_RETURN_IF_FAILURE(outputConnections.append(static_cast<int>(
				vertexOffset + static_cast<std::size_t>(triangle.second))));
			DOMMY_RETURN_IF_FAILURE(outputConnections.append(static_cast<int>(
				vertexOffset + static_cast<std::size_t>(triangle.third))));
		}
	}

	if (outputVertices.length() != totalVertexCount ||
		outputFaceCounts.length() != totalFaceCount ||
		outputConnections.length() != totalConnectionCount) return MS::kFailure;
	MFnMeshData meshDataFunction;
	const MObject newMeshData = meshDataFunction.create(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (newMeshData.isNull()) return MS::kFailure;
	if (totalVertexCount != 0U) {
		MFnMesh meshFunction;
		const MObject generatedMesh = meshFunction.create(
			static_cast<int>(totalVertexCount),
			static_cast<int>(totalFaceCount),
			outputVertices,
			outputFaceCounts,
			outputConnections,
			newMeshData,
			&status);
		CHECK_MSTATUS_AND_RETURN_IT(status);
		if (generatedMesh.isNull()) return MS::kFailure;
	}

	MDataHandle outputHandle = block.outputValue(outPutMesh, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(outputHandle.set(newMeshData));
	return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("foamGenerateNode")


MStatus foamGenerateNode::initialize() try {
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	
	inPosition = gAttr.create("inPosition", "ip", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(
		gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden(true));
	
	radiusArray = gAttr.create("radiusArray", "ra", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(
		gAttr.addDataAccept(MFnData::kFloatArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden(true));
	
	frequency = nAttr.create(
		"frequency", "fq", MFnNumericData::kInt, 1, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	radiusOffset = nAttr.create(
		"radiusOffset", "ro", MFnNumericData::kDouble, 0.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	radiusResultOffset = nAttr.create(
		"radiusResultOffset", "rro",
		MFnNumericData::kDouble, 0.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	//for deform
	selfDeform = nAttr.create(
		"selfDeform", "sd", MFnNumericData::kBoolean, true, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	
	
	outPutMesh = gAttr.create("outPutMesh", "opm", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(inPosition));
	DOMMY_RETURN_IF_FAILURE(addAttribute(radiusArray));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(frequency));
	DOMMY_RETURN_IF_FAILURE(addAttribute(radiusOffset));
	DOMMY_RETURN_IF_FAILURE(addAttribute(radiusResultOffset));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(selfDeform));
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(outPutMesh));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inPosition, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(radiusArray, outPutMesh));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(frequency, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(radiusOffset, outPutMesh));
	DOMMY_RETURN_IF_FAILURE(
		attributeAffects(radiusResultOffset, outPutMesh));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(selfDeform, outPutMesh));
	
	
	return MS::kSuccess;
}
catch (...) {
	return dommy::maya::reportEntryPointException(
		"foamGenerateNode", "initialize", std::current_exception());
}

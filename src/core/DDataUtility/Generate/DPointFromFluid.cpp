//
//  DPointFromFluid.cpp
//  dommyTool
//
//  Created by yuzo on 11/30/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointFromFluid.h"


#include "dpoint_ids.h"
#include "DPData.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MBoundingBox.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDagPath.h>
#include <maya/MFnFluid.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MStatus.h>

#include <cstddef>
#include <utility>



const MTypeId DPointFromFluid::typeId( ID_DP_FROM_FLUID );

MObject DPointFromFluid::aInFluid;
MObject DPointFromFluid::aTime;
MObject DPointFromFluid::outDP;


void* DPointFromFluid::creator()
{
    return ::dommy::maya::createObject("DPointFromFluid", [&]() {
        return new DPointFromFluid();
    });
}


MStatus DPointFromFluid::compute(const MPlug& plug, MDataBlock& data) try
{
	if (plug != outDP) return MS::kUnknownParameter;

	MStatus status;
	data.inputValue(aTime, &status);
	if (!status) return status;

	MPlug messagePlug(thisMObject(), aInFluid);
	MPlug fluidPlugSource = messagePlug.source(&status);
	if (!status || fluidPlugSource.isNull()) {
		MGlobal::displayError("DPointFromFluid requires a connected fluid.");
		return MS::kFailure;
	}

	MObject fluidNode = fluidPlugSource.node(&status);
	if (!status || fluidNode.isNull() || !fluidNode.hasFn(MFn::kFluid)) {
		MGlobal::displayError("DPointFromFluid input is not a valid fluid.");
		return MS::kFailure;
	}

	MFnFluid fluid(fluidNode, &status);
	if (!status || fluid.object().isNull()) return MS::kFailure;
	if (fluid.isAutoResize()) {
		MGlobal::displayError("DPointFromFluid does not support auto-resize fluids.");
		return MS::kFailure;
	}

	unsigned int xResolution = 0U;
	unsigned int yResolution = 0U;
	unsigned int zResolution = 0U;
	status = fluid.getResolution(xResolution, yResolution, zResolution);
	if (!status) return status;

	double xDimension = 0.0;
	double yDimension = 0.0;
	double zDimension = 0.0;
	status = fluid.getDimensions(xDimension, yDimension, zDimension);
	if (!status) return status;

	const MBoundingBox fluidBounds = fluid.boundingBox(&status);
	if (!status) return status;

	dommy::fluid::GridPlan gridPlan;
	if (!dommy::fluid::buildGridPlan(
			xResolution, yResolution, zResolution,
			xDimension, yDimension, zDimension,
			fluidBounds.min().x, fluidBounds.min().y, fluidBounds.min().z,
			1U, false, dommy::generation::kMaxGeneratedPointCount,
			gridPlan)) {
		MGlobal::displayError(
			"Fluid dimensions or resolution exceed the DPoint generation limit.");
		return MS::kFailure;
	}

	const unsigned int scalarGridSize = fluid.gridSize(&status);
	if (!status || static_cast<std::size_t>(scalarGridSize) !=
					   gridPlan.scalar.voxelCount) {
		MGlobal::displayError("Fluid grid size does not match its resolution.");
		return MS::kFailure;
	}

	MDagPath fluidPath;
	status = fluid.getPath(fluidPath);
	if (!status || !fluidPath.isValid()) return MS::kFailure;
	const MMatrix objectToWorld = fluidPath.inclusiveMatrix(&status);
	if (!status || !dommy::maya::isFiniteMatrix(objectToWorld)) {
		MGlobal::displayError("Fluid world transform must contain finite values.");
		return MS::kFailure;
	}

	DPointList pointArray(gridPlan.scalar.voxelCount);
	for (unsigned int x = 0U; x < xResolution; ++x) {
		for (unsigned int y = 0U; y < yResolution; ++y) {
			for (unsigned int z = 0U; z < zResolution; ++z) {
				MPoint point;
				status = fluid.voxelCenterPosition(
					static_cast<int>(x), static_cast<int>(y),
					static_cast<int>(z), point);
				if (!status) return status;

				const int rawIndex = fluid.index(
					static_cast<int>(x), static_cast<int>(y),
					static_cast<int>(z));
				std::size_t pointIndex = 0U;
				if (!dommy::generation::checkedCount(
						rawIndex, pointArray.size() - 1U, pointIndex)) {
					MGlobal::displayError("Fluid returned an invalid voxel index.");
					return MS::kFailure;
				}

				point *= objectToWorld;
				if (!dommy::maya::isFloatRepresentablePoint(point)) {
					MGlobal::displayError(
						"Fluid voxel transformation produced an invalid DPoint position.");
					return MS::kFailure;
				}
				pointArray[pointIndex] = DPoint(point.x, point.y, point.z);
			}
		}
	}

	MFnPluginData pluginData;
	pluginData.create(DPData::id, &status);
	if (!status) return status;
	DPData* const dpData = dommy::maya::pluginDataPointer<DPData>(pluginData, &status);
	if (!status || dpData == NULL) return MS::kFailure;

	// Transfer the generated buffer through DPData's validated ownership
	// boundary without duplicating the potentially large point array.
	if (!dpData->adoptPositions(std::move(pointArray))) {
		return MS::kFailure;
	}
	dpData->updateBBox();

	MDataHandle outputData = data.outputValue(outDP, &status);
	if (!status) return status;
	status = dommy::maya::publishPluginDataObject(
		outputData, pluginData.object(), *dpData);
	if (!status) return status;
	return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPointFromFluid")


MStatus DPointFromFluid::initialize()
{
	MStatus status;
	MFnMessageAttribute messageAttribute;
	aInFluid = messageAttribute.create("InFluid", "infld", &status);
	if (!status) return status;
	status = messageAttribute.setReadable(false);
	if (!status) return status;
	status = messageAttribute.setStorable(false);
	if (!status) return status;
	status = addAttribute(aInFluid);
	if (!status) return status;

	MFnTypedAttribute typedAttribute;
	outDP = typedAttribute.create(
		"outDP", "odp", DPData::id, MObject::kNullObj, &status);
	if (!status) return status;
	status = typedAttribute.setWritable(false);
	if (!status) return status;
	status = typedAttribute.setStorable(false);
	if (!status) return status;
	status = addAttribute(outDP);
	if (!status) return status;

	MFnUnitAttribute unitAttribute;
	aTime = unitAttribute.create(
		"CurrentTime", "time", MFnUnitAttribute::kTime, 0.0, &status);
	if (!status) return status;
	status = addAttribute(aTime);
	if (!status) return status;

	status = attributeAffects(aInFluid, outDP);
	if (!status) return status;
	status = attributeAffects(aTime, outDP);
	if (!status) return status;

	return MS::kSuccess;
}

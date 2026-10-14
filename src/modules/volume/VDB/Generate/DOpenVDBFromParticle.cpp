//
//  DOpenVDBFromParticle.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#include "DOpenVDBFromParticle.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "../Common/DVDBComputeGuard.h"

#include <openvdb/Grid.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/ParticlesToLevelSet.h>




#include <maya/MFnUnitAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MSelectionList.h>
#include <maya/MFnFluid.h>
#include <maya/MFnStringData.h>
#include <maya/MFnPluginData.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MBoundingBox.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>


#include "DOpenVDBData.h"
#include "DPointCommon.h"
#include "DOpenVDBCommon.h"
#include "../Common/DVdbParticleRasterPlan.h"
#include "../Edit/DOpenVDBParticleConversionPolicy.h"
#include "common/utility/DMayaEntryPointGuard.h"

#include "dvdb_ids.h"



MObject DOpenVDBFromParticle::inputPointArray;
MObject DOpenVDBFromParticle::radiusArray;
MObject DOpenVDBFromParticle::velocityArray;

MObject DOpenVDBFromParticle::aVdbOutput;

MObject DOpenVDBFromParticle::levelSet;
MObject DOpenVDBFromParticle::gridName;
MObject DOpenVDBFromParticle::fogVolume;
MObject DOpenVDBFromParticle::fogVolumeGridName;
MObject DOpenVDBFromParticle::maskVolume;
MObject DOpenVDBFromParticle::maskVolumeName;

MObject DOpenVDBFromParticle::voxelSize;//Voxel Size
MObject DOpenVDBFromParticle::worldSpaceUnits;//Use World Space for Band
MObject DOpenVDBFromParticle::bandWidth;//Half-Band Voxels
MObject DOpenVDBFromParticle::bandWidthWS;//Half-Band

MObject DOpenVDBFromParticle::dR;//Particle Radius Scale
MObject DOpenVDBFromParticle::Rmin;//Minimum Radius in Voxels

MObject DOpenVDBFromParticle::prune;//Prune Level Set
MObject DOpenVDBFromParticle::maskWidth;//Mask Width Scale

MObject DOpenVDBFromParticle::footprint;//Particle Footprint

MObject DOpenVDBFromParticle::dV;//Velocity Multiplier
MObject DOpenVDBFromParticle::velocitySpacing;//Velocity Spacing



const MTypeId DOpenVDBFromParticle::typeId( ID_VDB_DOPENVDB_FROM_PARTICLE );


void* DOpenVDBFromParticle::creator()
{
    return ::dommy::maya::createObject("DOpenVDBFromParticle", [&]() {
        return new DOpenVDBFromParticle();
    });
}


MStatus DOpenVDBFromParticle::compute(const MPlug& plug, MDataBlock& block) try {
	
	MStatus stat;
    MDataHandle valueHandle;
	
	if (plug == aVdbOutput) {
		
		//get Attribute
		valueHandle = block.inputValue(levelSet, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool outputLevelSetGrid = valueHandle.asBool();
		valueHandle = block.inputValue(gridName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString gridNameVal = valueHandle.asString();
		valueHandle = block.inputValue(fogVolume, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool outputFogVolumeGrid = valueHandle.asBool();
		valueHandle = block.inputValue(fogVolumeGridName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString fogVolumeGridNameVal = valueHandle.asString();
		valueHandle = block.inputValue(maskVolume, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool outputMaskVolumeGrid = valueHandle.asBool();
		valueHandle = block.inputValue(maskVolumeName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString maskVolumeNameVal = valueHandle.asString();
		
		
		if (!outputFogVolumeGrid && !outputLevelSetGrid && !outputMaskVolumeGrid) {
			MGlobal::displayError("No output selected");
			return MS::kFailure;
		}
		
		valueHandle = block.inputValue(voxelSize, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float mVoxelSize = valueHandle.asFloat();
		valueHandle = block.inputValue(worldSpaceUnits, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool wsUnits = valueHandle.asBool();
		valueHandle = block.inputValue(bandWidth, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float bandWidthVal = valueHandle.asFloat();
		valueHandle = block.inputValue(bandWidthWS, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float bandWidthWSVal = valueHandle.asFloat();
		
		valueHandle = block.inputValue(dR, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dRVal = valueHandle.asFloat();
		valueHandle = block.inputValue(Rmin, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float RminVal = valueHandle.asFloat();
		
		valueHandle = block.inputValue(prune, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool pruneVal = valueHandle.asBool();
		valueHandle = block.inputValue(maskWidth, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float maskWidthVal = valueHandle.asFloat();
		
		
		valueHandle = block.inputValue(footprint, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		short footprintVal = valueHandle.asShort();
        if (footprintVal != 0 && footprintVal != 1) return MS::kInvalidParameter;
		
		valueHandle = block.inputValue(dV, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dVVal = valueHandle.asFloat();
		valueHandle = block.inputValue(velocitySpacing, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dXVal = valueHandle.asFloat();
		
		MFnDependencyNode thiFn(thisMObject(), &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		MPointArray posDataArray;
        DOMMY_RETURN_IF_FAILURE(getPointArrayFromData(block, inputPointArray, posDataArray));
        if (posDataArray.length() > (dommy::vdb::kMaxVdbPayloadBytes / 4U) / 160U)
            return MS::kInvalidParameter;
		
		if (posDataArray.length()==0) {
			MGlobal::displayError(
				"DOpenVDBFromParticle: input point array is empty.");
			return MStatus::kFailure;
		}
		
		
		
		
		MPlug radiusPlug = thiFn.findPlug(radiusArray, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		MDoubleArray radDataArray;
		const bool radiusPlugConnected = radiusPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (radiusPlugConnected) {
            DOMMY_RETURN_IF_FAILURE(getDoubleArrayFromData(block, radiusArray, radDataArray));
			
			if (radDataArray.length()>0 && (radDataArray.length()!=posDataArray.length())) {
				MGlobal::displayError(
					"Particle radius array must have the same length as the position array.");
				return MS::kFailure;
			}
		}
		
		
		MPlug velPlug = thiFn.findPlug(velocityArray, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		MVectorArray velDataArray;
		const bool velPlugConnected = velPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (velPlugConnected) {
            DOMMY_RETURN_IF_FAILURE(getVectorArrayFromData(block, velocityArray, velDataArray));
			
			if (velDataArray.length()>0 && (velDataArray.length()!=posDataArray.length())) {
				MGlobal::displayError(
					"Particle velocity array must have the same length as the position array.");
				return MS::kFailure;
			}
		}

		dommy::vdb::ParticleInputExtrema extrema;
		for (unsigned int index = 0; index < posDataArray.length(); ++index) {
			if (!extrema.addPosition(posDataArray[index])) {
				MGlobal::displayError("Particle positions must contain only finite values.");
				return MS::kFailure;
			}
		}
		if (radDataArray.length() > 0) {
			for (unsigned int index = 0; index < radDataArray.length(); ++index) {
				if (!extrema.addRadius(radDataArray[index])) {
					MGlobal::displayError(
						"Particle radii must be finite and non-negative.");
					return MS::kFailure;
				}
			}
		} else {
			extrema.addRadius(1.0);
		}
		if (velDataArray.length() > 0) {
			for (unsigned int index = 0; index < velDataArray.length(); ++index) {
				if (!extrema.addVelocity(velDataArray[index])) {
					MGlobal::displayError(
						"Particle velocities must contain only finite values.");
					return MS::kFailure;
				}
			}
		}

		dommy::vdb::ParticleRasterParameters rasterParameters;
		rasterParameters.voxelSize = mVoxelSize;
		rasterParameters.halfBandVoxels = wsUnits
			? static_cast<double>(bandWidthWSVal) / mVoxelSize
			: bandWidthVal;
		rasterParameters.radiusScale = dRVal;
		rasterParameters.minimumRadiusVoxels = RminVal;
		rasterParameters.maskWidth = outputMaskVolumeGrid ? maskWidthVal : 0.0;
		rasterParameters.velocityMultiplier = dVVal;
		rasterParameters.velocitySpacing = dXVal;
		rasterParameters.rasterizeTrails =
			footprintVal == 1 && velDataArray.length() > 0;
		rasterParameters.rasterPassCount = 1U;
		if (outputMaskVolumeGrid && maskWidthVal > 0.0f) {
			rasterParameters.rasterPassCount += maskWidthVal < 1.0f ? 2U : 1U;
		}

		dommy::vdb::TreeEditBudget rasterBudget;
            float background = 0;
            if (!dommy::vdb::planParticleConversion(posDataArray.length(), extrema, rasterParameters,
                    1 + (outputLevelSetGrid && outputFogVolumeGrid ? 1 : 0) + (outputMaskVolumeGrid ? 2 : 0), 0,
                    rasterBudget, background)) {
			MGlobal::displayError(
				"Particle-to-VDB input is invalid or exceeds the raster coordinate or resource budget.");
			return MS::kFailure;
		}
		const float rasterMaximumRadiusVoxels = static_cast<float>(
			std::max(static_cast<double>(RminVal),
				extrema.maximumRadius() * dRVal *
					(1.0 + rasterParameters.maskWidth) / mVoxelSize) + 1.0);
		
		
		
		
		ParticleList paList(
			posDataArray, radDataArray, velDataArray, dRVal, dVVal);
		
		Settings settings;
		settings.mRasterizeTrails =  footprintVal == 1;
		settings.mDx = dXVal;
		// get output vdb
		MFnPluginData pluginData;
		pluginData.create(DOpenVDBData::id, &stat);
		if (stat != MS::kSuccess) return stat;
		
        DOpenVDBData* vdb = nullptr;
        stat = dommy::maya::getPluginData(pluginData, vdb);
        if (!stat) return stat;
		if (stat != MS::kSuccess) return stat;
		
		
		openvdb::math::Transform::Ptr transform =
		openvdb::math::Transform::createLinearTransform(mVoxelSize);
            if (!transform) return MS::kFailure;
		
		openvdb::FloatGrid::Ptr outputGrid, minGrid, maxGrid;
		
		
		// Set narrow band half-width
		// background was validated by the shared raster plan.
		
		// Perform the particle conversion.
		
		// Construct a new level set grid.
		if (!outputGrid) outputGrid = openvdb::FloatGrid::create(background);
		if (!outputGrid) return MS::kFailure;
		
		outputGrid->setGridClass(openvdb::GRID_LEVEL_SET);
		outputGrid->setTransform(transform);
		
		convert(outputGrid, paList, settings, RminVal,
			rasterMaximumRadiusVoxels, pruneVal);
		
		if (outputMaskVolumeGrid) {
			
			// Max grid
			if (!maxGrid) maxGrid = openvdb::FloatGrid::create(background);
			if (!maxGrid) return MS::kFailure;
			maxGrid->setGridClass(openvdb::GRID_LEVEL_SET);
			maxGrid->setTransform(transform->copy());
			if (maskWidthVal > 0.0f) {
					if (!paList.scaleRadiusMultiplier(
							1.0f + maskWidthVal)) {
						MGlobal::displayError(
							"Particle radius scaling overflowed.");
						return MS::kFailure;
					}
				convert(maxGrid, paList, settings, RminVal,
					rasterMaximumRadiusVoxels, pruneVal);
				
				// Min grid
				if ( maskWidthVal < 1.0f) {
					if (!minGrid) minGrid = openvdb::FloatGrid::create(background);
					if (!minGrid) return MS::kFailure;
					minGrid->setGridClass(openvdb::GRID_LEVEL_SET);
					minGrid->setTransform(transform->copy());
						if (!paList.scaleRadiusMultiplier(
								(1.0f - maskWidthVal) /
								(1.0f + maskWidthVal))) {
							MGlobal::displayError(
								"Particle radius scaling overflowed.");
							return MS::kFailure;
						}
					convert(minGrid, paList, settings, RminVal,
						rasterMaximumRadiusVoxels, pruneVal);
					
					// CSG difference
					openvdb::tools::csgDifference(*maxGrid, *minGrid);
				}
			}
			
			// Convert to fog volume
			if (!dommy::vdb::convertMeshVolumeToFog(maxGrid)) return MS::kFailure;
			
			// Store the generated fog volume.
			maxGrid->setName(maskVolumeNameVal.asChar());
			
			vdb->insert(maxGrid);
		}// masked volume grid
		
		// Output level set grid
		if (outputLevelSetGrid) {
			outputGrid->setName(gridNameVal.asChar());
			vdb->insert(outputGrid);
		}
		
		// Output fog volume grid
		if ( outputFogVolumeGrid) {
			// If no level set grid is exported the original level set
			// grid is modified in place.
			if (outputLevelSetGrid) outputGrid = outputGrid->deepCopy();
			
			if (!dommy::vdb::convertMeshVolumeToFog(outputGrid)) return MS::kFailure;
			
			outputGrid->setName(fogVolumeGridNameVal.asChar());
			vdb->insert(outputGrid);
			
		}
		
		
		MDataHandle outHandle = block.outputValue(aVdbOutput, &stat);
		if (!stat) return stat;
		stat = dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
			outHandle, pluginData.object(), DOpenVDBData::id);
		if (!stat) return stat;
		
		return block.setClean(plug);
	}
	
	return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFromParticle")



MStatus DOpenVDBFromParticle::initialize() try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	MFnStringData strData;
	MFnGenericAttribute gAttr;
	
	
	inputPointArray = gAttr.create( "inputPointArray", "ipa",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inputPointArray));
	
	
	radiusArray = gAttr.create( "radiusArray", "rar",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(radiusArray));
	
	velocityArray = gAttr.create( "velocityArray", "var",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(velocityArray));
	
	
	
	
	levelSet = nAttr.create("levelSet", "ls", MFnNumericData::kBoolean, 0, &stat);
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(levelSet));
	
	
	const MObject gridNameDefault = strData.create("surface", &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	
	gridName = tAttr.create("gridName", "gn", MFnData::kString, gridNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(gridName));
	
	
	fogVolume = nAttr.create("fogVolume", "fg", MFnNumericData::kBoolean, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(fogVolume));
	
	const MObject fogVolumeGridNameDefault = strData.create("density", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	fogVolumeGridName = tAttr.create("fogVolumeGridName", "fgn", MFnData::kString, fogVolumeGridNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(fogVolumeGridName));

	maskVolume = nAttr.create("maskVolume", "mv", MFnNumericData::kBoolean, 0, &stat);

	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(maskVolume));
	
	const MObject maskVolumeNameDefault = strData.create("mask", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	maskVolumeName = tAttr.create("maskVolumeName", "mvn", MFnData::kString, maskVolumeNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(maskVolumeName));

	voxelSize = nAttr.create("voxelSize", "vs", MFnNumericData::kFloat, 0, &stat);

	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(voxelSize));
	
	dR = nAttr.create("ParticleRadiusScale", "prs", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(dR));
	
	Rmin = nAttr.create("MinimumRadius", "mr", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.5));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(Rmin));
	
	
	worldSpaceUnits = nAttr.create("UseWorldSpace", "uws", MFnNumericData::kBoolean, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(worldSpaceUnits));
	
	bandWidth = nAttr.create("HalfBand", "hb", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(bandWidth));
	
	bandWidthWS = nAttr.create("bandWidthWS", "hbsw", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.3));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(bandWidthWS));
	
	
	prune = nAttr.create("prune", "pru", MFnNumericData::kBoolean, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(prune));
	
	maskWidth = nAttr.create("maskWidth", "mw", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.25));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(maskWidth));
	
	MFnEnumAttribute enumFn;
	footprint = enumFn.create( "footprint", "fp", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Spherical", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Velocity Trail", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(footprint));
	
	dV = nAttr.create("VelocityMultiplier", "vmul", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(dV));
	
	velocitySpacing = nAttr.create("velocitySpacing", "vsp", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.2));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(velocitySpacing));
	
	
	// Setup the output vdb
	aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
	if (stat != MS::kSuccess) return stat;
	
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	stat = addAttribute(aVdbOutput);
	if (stat != MS::kSuccess) return stat;
	
	// Set the attribute dependencies
	DOMMY_RETURN_IF_FAILURE(attributeAffects(message, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inputPointArray, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(radiusArray, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(velocityArray, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(levelSet, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(gridName, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(fogVolume, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(fogVolumeGridName, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(maskVolume, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(maskVolumeName, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(voxelSize, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(worldSpaceUnits, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(bandWidth, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(bandWidthWS, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(dR, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(Rmin, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(prune, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(maskWidth, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(footprint, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(dV, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(velocitySpacing, aVdbOutput));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFromParticle", "initialize", std::current_exception());
}

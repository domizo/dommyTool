// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_To_Polygons.cc

//
//  DOpenVDBConvert.cpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBConvert.h"
#include "modules/mesh/DMesh/common/DMeshNormals.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "../Common/DVDBComputeGuard.h"


#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/Morphology.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Prune.h>
#include <openvdb/tree/ValueAccessor.h>
#include <openvdb/util/NullInterrupter.h>

#include <boost/scoped_ptr.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/math/special_functions/round.hpp>

#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/type_traits/is_const.hpp>

#include <iostream>
#include <cmath>
#include <string>
#include <sstream>
#include <limits>
#include <vector>
#include <list>

//vdb

#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBGeoUtil.h"

#include "DNodeOperation.h"
#include "DMetaDataUtility.h"


#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"
#include "dommyMath.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/vdb/DVdbOp.h"

#include "dvdb_ids.h"


const MTypeId DOpenVDBConvert::typeId( ID_VDB_DOPENVDB_CONVERT );

MObject DOpenVDBConvert::aVdbInput;

MObject DOpenVDBConvert::aVdbAllGridNames;
MObject DOpenVDBConvert::aVdbSelectedGridNames;

MObject DOpenVDBConvert::aReferenceMesh;

MObject DOpenVDBConvert::aIsovalue;
MObject DOpenVDBConvert::aAdaptivity;

MObject DOpenVDBConvert::aComputeNormals;
MObject DOpenVDBConvert::aIsCheckCorner;
MObject DOpenVDBConvert::aConerAngle;
MObject DOpenVDBConvert::aConcernAllPartsNormal;
MObject DOpenVDBConvert::aIsCheckPartsDistance;
MObject DOpenVDBConvert::aCheckTolerance;

MObject DOpenVDBConvert::aInternalAdaptivity;
MObject DOpenVDBConvert::aTransferAttributes;
MObject DOpenVDBConvert::aSharpenFeatures;
MObject DOpenVDBConvert::aEdgeTolerance;

MObject DOpenVDBConvert::aExportColorSet;
MObject DOpenVDBConvert::aExportMetadata;

//group
MObject DOpenVDBConvert::aSurfaceGroupName;
MObject DOpenVDBConvert::aInteriorGroupName;
MObject DOpenVDBConvert::aSeamGroupName;
MObject DOpenVDBConvert::aSeamPointGroupName;


MObject DOpenVDBConvert::outDM;




void* DOpenVDBConvert::creator()
{
    return ::dommy::maya::createObject("DOpenVDBConvert", [&]() {
        return new DOpenVDBConvert();
    });
}




MStatus DOpenVDBConvert::compute(const MPlug& plug, MDataBlock& data) try
{
	MStatus stat;
	if (plug != aVdbAllGridNames && plug != outDM) return MS::kUnknownParameter;
	

	
    //Get input VDB
	const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, inputVdb));
	
	if (plug == aVdbAllGridNames) {
		MString names = getGridNames(*inputVdb).c_str();
		MDataHandle outHandle = data.outputValue(aVdbAllGridNames, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		outHandle.set(names);
		return data.setClean(plug);
	}
	
	// Get selected grids
	MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
	if (stat != MS::kSuccess) return stat;
	std::string names = selectionHandle.asString().asChar();
    GridCPtrVec selectedGrids;
    getGrids(selectedGrids, *inputVdb, names);
    dommy::vdb::MeshingBudget budget;
	
	
	
	// Convert Openvdbs to meshes
	if (plug == outDM) {
		MDataHandle valueHandle = data.inputValue(aComputeNormals, &stat);
		if (!stat) return stat;
		const bool computeNormals = valueHandle.asBool();
		valueHandle = data.inputValue(aAdaptivity, &stat);
		if (!stat) return stat;
		const float adaptivity = valueHandle.asFloat();
		valueHandle = data.inputValue(aIsovalue, &stat);
		if (!stat) return stat;
		const float iso = valueHandle.asFloat();
		if (!std::isfinite(adaptivity) || adaptivity < 0.0f ||
			adaptivity > 1.0f || !std::isfinite(iso)) {
			MGlobal::displayError(
				"OpenVDB conversion requires finite values and adaptivity in [0, 1].");
			return MS::kInvalidParameter;
		}
		
		
		openvdb::tools::VolumeToMesh mesher(iso, adaptivity);
		
		
#pragma mark get referenceMesh info
		MPlug refPlug(thisMObject(), aReferenceMesh);
		
		const bool isRefMesh = refPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
			MFnMesh meshFn;
        if (isRefMesh) DOMMY_RETURN_IF_FAILURE(isMeshData(data, aReferenceMesh, meshFn));
		
		std::vector<meshPrimData> primDataList;
		
		
		
		
		if(isRefMesh) {
			// Collect all level set grids.
			std::list<openvdb::GridBase::ConstPtr> lsGrids;
			std::vector<std::string> nonLevelSetList, nonLinearList;
			
			for (const auto& selected : selectedGrids) {
                if (!selected) return MS::kInvalidParameter;
				const openvdb::GridClass gridClass = selected->getGridClass();
				
				if (gridClass != openvdb::GRID_LEVEL_SET) {
					nonLevelSetList.push_back(selected->getName());
					continue;
				}
				
				if (!selected->transform().isLinear()) {
					nonLinearList.push_back(selected->getName());
					continue;
				}
				
				lsGrids.push_back(selected); // Meshing reads const grids; no deep copy is needed.
			}
			
			if (!nonLevelSetList.empty()) {
				std::string s = "Reference meshing is only supported for "
				"Level Set grids, the following grids were skipped: '" +
				boost::algorithm::join(nonLevelSetList, ", ") + "'.";
				MGlobal::displayWarning(s.c_str());
			}
			
			if (!nonLinearList.empty()) {
				std::string s = "The following grids were skipped: '" +
				boost::algorithm::join(nonLinearList, ", ") +
				"' because they don't have a linear/affine transform.";
				MGlobal::displayWarning(s.c_str());
			}
			
			// Mesh using a reference surface
			if (!lsGrids.empty()) {
                if (!dommy::vdb::planMeshingGrids(GridCPtrVec(lsGrids.begin(), lsGrids.end()), budget))
                    return MS::kInvalidParameter;
				
				if (lsGrids.front()->isType<openvdb::FloatGrid>()) {
					stat = referenceMeshing<openvdb::FloatGrid>(
						lsGrids, mesher, computeNormals, data, primDataList, budget);
				} else if (lsGrids.front()->isType<openvdb::DoubleGrid>()) {
					stat = referenceMeshing<openvdb::DoubleGrid>(
						lsGrids, mesher, computeNormals, data, primDataList, budget);
				} else {
					MGlobal::displayError("Unsupported grid type.");
					return MS::kInvalidParameter;
				}
				if (!stat) return stat;
			}
			
		} else {
			
			
			const auto& grids = selectedGrids;
            if (!dommy::vdb::planMeshingGrids(grids, budget)) return MS::kInvalidParameter;
			
			
			
			
			
			for (size_t n = 0, N = grids.size(); n < N; ++n) {
		
				VDBToMayaMesh converter(mesher, budget);
				
				if (processTypedScalarGrid(grids[n], converter)) {
                    DOMMY_RETURN_IF_FAILURE(converter.status);
					if (converter.mesh.isNull()) return MS::kFailure;
					MFnMesh convertedMesh(converter.mesh, &stat);
					if (!stat) return stat;
					meshPrimData data;
					stat = getPrimDataFromMesh(convertedMesh, data);
					if (!stat) return stat;
                    data.m_obj = converter.mesh;
					
					if(converter.getSurfaceGroup().size()){
						data.surfaceGroup = converter.getSurfaceGroup();
					}
					if(converter.getInteriorGroup().size()){
						data.interiorGroup = converter.getInteriorGroup();
					}
					if(converter.getSeamGroup().size()){
						data.seamGroup = converter.getSeamGroup();
					}
					if(converter.getSeamPointGroup().size()){
						data.seamPointGroup = converter.getSeamPointGroup();
					}
					
					
					primDataList.push_back(data);
				}
			}
			
			
		}
		
		
		
		if (primDataList.empty()) {
			MGlobal::displayError(
				"OpenVDB conversion did not produce any valid mesh data.");
			return MS::kFailure;
		}

		if (primDataList.size()) {
			
			//option
            valueHandle = data.inputValue(aExportColorSet, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool exportColor = valueHandle.asBool();
            valueHandle = data.inputValue(aExportMetadata, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool exportMeta = valueHandle.asBool();
			
            //create output dm data
            MFnPluginData pluginData;
            pluginData.create(DMeshData::id, &stat);
            
            if (stat != MS::kSuccess) {
                EchoWarning(name().asChar(), "Failed to create a new DMeshData object");
                return MS::kFailure;
            }
            
            DMeshData* const outDMeshes(dommy::maya::pluginDataPointer<DMeshData>(pluginData, &stat));
            DOMMY_RETURN_IF_FAILURE(stat);
            
            if (!outDMeshes) {
                EchoWarning(name().asChar(), "Failed to create a Output DMeshData object");
                return MS::kFailure;
            }
            
            std::string seampointGrpName, surfGrpName, interiorGrpName, seamGrpName;
            
            if (exportMeta) {
                valueHandle = data.inputValue(aSeamPointGroupName, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                seampointGrpName = valueHandle.asString().asChar();
                valueHandle = data.inputValue(aSurfaceGroupName, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                surfGrpName = valueHandle.asString().asChar();
                valueHandle = data.inputValue(aInteriorGroupName, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                interiorGrpName = valueHandle.asString().asChar();
                valueHandle = data.inputValue(aSeamGroupName, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                seamGrpName = valueHandle.asString().asChar();
                if (seampointGrpName.empty()) {
                    seampointGrpName = "seamPt";
                }
                if (surfGrpName.empty()) {
                    surfGrpName = "surfGrp";
                }
                if (interiorGrpName.empty()) {
                    interiorGrpName = "interiorGrp";
                }
                if (seamGrpName.empty()) {
                    seamGrpName = "seamGrp";
                }
            }
            
            //convert DM process
            if (primDataList.size() > dommy::generation::kMaxGeneratedMeshCount) return MS::kInvalidParameter;
            DMeshData::DMeshList stagedMeshes;
            stagedMeshes.reserve(primDataList.size());
            dommy::mesh::MeshMergePlan outputPlan;
			for (uint i=0, PN=primDataList.size(); i<PN; i++) {
				
                DMItem newItem = DMItem(new DMeshInternalData(primDataList[i].m_pts, primDataList[i].m_faces,primDataList[i].m_connects));
                if (primDataList[i].hasExplicitNormals)
                    DOMMY_RETURN_IF_FAILURE(dommy::mesh::captureCornerNormals(primDataList[i].m_obj, *newItem));
				
	                if (exportColor &&
                    (primDataList[i].surfaceGroup.size() == primDataList[i].interiorGroup.size() &&
                     primDataList[i].surfaceGroup.size() == primDataList[i].seamGroup.size() &&
                     primDataList[i].surfaceGroup.size() == primDataList[i].m_faces.length())) {
                    
                        
                        DColorList colList(primDataList[i].m_faces.length(), DColor(0.0,0.0,0.0));
                        
                        for (size_t j=0, n=primDataList[i].m_faces.length(); j<n; j++) {
                            float rVal = (float)primDataList[i].surfaceGroup[j];
                            float gVal = (float)primDataList[i].interiorGroup[j];
                            float bVal = (float)primDataList[i].seamGroup[j];
                            colList[j] = DColor(rVal,gVal,bVal);
                        }
                        
                        if (!newItem->addCompAttr(
                                D_COMP_FACE, "frac_color",
                                DPVec3fAttr(
                                    colList, DColor(0.0, 0.0, 0.0)))) {
                            return MS::kFailure;
                        }
                }
				//meta data　情報データの付与
				if (exportMeta) {
					
                    //seamPoint index
                    DPIdList seamPtGrp(primDataList[i].seamPointGroup.size(),false);
                    for (size_t j=0, n=primDataList[i].seamPointGroup.size(); j<n; j++) {
                        if(primDataList[i].seamPointGroup[j])
                            seamPtGrp[j] = true;
                    }
	                    if (!newItem->addCompGrp(
                                D_COMP_POINT, seampointGrpName,
                                DPGroupData(seamPtGrp))) {
                            return MS::kFailure;
                        }
                    
                    
                    //surface index
                    DPIdList surfIdxGrp(primDataList[i].surfaceGroup.size(),false);
                    for (size_t j=0, n=primDataList[i].surfaceGroup.size(); j<n; j++) {
                        if(primDataList[i].surfaceGroup[j])
                            surfIdxGrp[j] = true;
                    }
                    
	                    if (!newItem->addCompGrp(
                                D_COMP_FACE, surfGrpName,
                                DPGroupData(surfIdxGrp))) {
                            return MS::kFailure;
                        }
                    
                    //interior index
                    DPIdList interiorIdxGrp(primDataList[i].interiorGroup.size(),false);
                    for (size_t j=0, n= primDataList[i].interiorGroup.size(); j<n; j++) {
                        if(primDataList[i].interiorGroup[j])
                            interiorIdxGrp[j] = true;
                    }
                    
	                    if (!newItem->addCompGrp(
                                D_COMP_FACE, interiorGrpName,
                                DPGroupData(interiorIdxGrp))) {
                            return MS::kFailure;
                        }
                    
                    //seam index
                    DPIdList seamIdxGrp(primDataList[i].seamGroup.size(),false);
                    for (uint j=0, n=primDataList[i].seamGroup.size(); j<n; j++) {
                        if(primDataList[i].seamGroup[j])
                            seamIdxGrp[j] = true;
                    }
                    
	                    if (!newItem->addCompGrp(
                                D_COMP_FACE, seamGrpName,
                                DPGroupData(seamIdxGrp))) {
                            return MS::kFailure;
                        }
				}
                
                
                if (!dommy::mesh::appendMeshMergePlan(newItem->numberOfPoints(), newItem->numFace(),
                        newItem->numberOfConnections(), newItem->numberOfUVs(), outputPlan) ||
                    !dommy::mesh::validMeshOperationStorage(outputPlan)) return MS::kInvalidParameter;
                stagedMeshes.push_back(newItem);
				
			}
			if (!outDMeshes->insert(stagedMeshes)) return MS::kFailure;
			
            if (outDMeshes->numberOfMesh()) {
                outDMeshes->updateBBox();
            }
			
			
			
			
#pragma mark Create Out mesh
			
#pragma mark Create output
            MDataHandle output = data.outputValue(outDM, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishTypedPluginDataObject<DMeshData>(
                    output, pluginData.object(), DMeshData::id));
            
            return data.setClean(plug);
			
		}
		
		
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBConvert")


MStatus DOpenVDBConvert::initialize() try
{
	MStatus stat;
	
	// attributes
	
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	
	// ----------------------- INPUTS --------------------------
	aVdbInput = tAttr.create( "vdbInput", "input", DOpenVDBData::id, MObject::kNullObj, &stat );
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));
    
	
	aIsovalue = nAttr.create("isovalue", "iso", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsovalue));
	
	
	aAdaptivity = nAttr.create("adaptivity", "adapt", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 2.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAdaptivity));
	
	aComputeNormals = nAttr.create("ComputeNormals", "cnorm", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aComputeNormals));
	
	aIsCheckCorner = nAttr.create("IsCheckCorner", "ichco", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCheckCorner));
	
	aConerAngle = nAttr.create("ConerAngle", "coang", MFnNumericData::kDouble, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConerAngle));
	
	
	aConcernAllPartsNormal = nAttr.create("ConcernAllPartsNormal", "capn", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConcernAllPartsNormal));
    
    aIsCheckPartsDistance = nAttr.create("IsCheckPartsDistance", "ichpd", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCheckPartsDistance));
    
    aCheckTolerance = nAttr.create("CheckTolerance", "chtol", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCheckTolerance));
    
    
    
	
	aInternalAdaptivity = nAttr.create("InternalAdaptivity", "inada", MFnNumericData::kDouble, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInternalAdaptivity));
	
	
	aTransferAttributes = nAttr.create("TransferAttributes", "trattr", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTransferAttributes));
	
	aSharpenFeatures = nAttr.create("SharpenFeatures", "shfea", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSharpenFeatures));
	
	aEdgeTolerance = nAttr.create("EdgeTolerance", "edtol", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeTolerance));
	
	
	
	aExportColorSet = nAttr.create("ExportColorSet", "excols", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportColorSet));
	
	
	
	
	aExportMetadata = nAttr.create("ExportMetadata", "exmeta", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportMetadata));

	MFnStringData fnStringData;
	MObject defaultStringData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	MObject surfaceStringData = fnStringData.create("surface_polygon", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	MObject interiorStringData = fnStringData.create("interior_polygon", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	MObject seamStringData = fnStringData.create("seam_polygon", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	MObject seamPointStringData = fnStringData.create("seam_point", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	
	aSurfaceGroupName = tAttr.create("SurfaceGroupName", "surfgrp", MFnData::kString, surfaceStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceGroupName));
	
	aInteriorGroupName = tAttr.create("InteriorGroupName", "intgrp", MFnData::kString, interiorStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInteriorGroupName));

	aSeamGroupName = tAttr.create("SeamGroupName", "segrp", MFnData::kString, seamStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeamGroupName));

	aSeamPointGroupName = tAttr.create("SeamPointGroupName", "sepgrp", MFnData::kString, seamPointStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeamPointGroupName));
	
	
	aVdbAllGridNames = tAttr.create("vdbAllGridNames", "allgrids", MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbAllGridNames));
	
	
	aVdbSelectedGridNames = tAttr.create("vdbSelectedGridNames", "selectedgrids",
										 MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));
	
	
	aReferenceMesh = gAttr.create( "ReferenceMesh", "reme",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aReferenceMesh));
    
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
	
	
	
	// Setup dependencies
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbAllGridNames));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, outDM));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aReferenceMesh, outDM));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsovalue, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdaptivity, outDM));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aComputeNormals, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsCheckCorner, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aConerAngle, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aConcernAllPartsNormal, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsCheckPartsDistance, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCheckTolerance, outDM));
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aInternalAdaptivity, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aTransferAttributes, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aSharpenFeatures, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeTolerance, outDM));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportColorSet, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportMetadata, outDM));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aSurfaceGroupName, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aInteriorGroupName, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeamGroupName, outDM));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeamPointGroupName, outDM));
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects(aVdbSelectedGridNames, outDM));
	
	return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBConvert", "initialize", std::current_exception());
}







template <class GridType>
MStatus DOpenVDBConvert::referenceMeshing(const std::list<openvdb::GridBase::ConstPtr>& grids,
    openvdb::tools::VolumeToMesh& mesher, bool computeNormals, MDataBlock& data,
    std::vector<meshPrimData>& primDataList, dommy::vdb::MeshingBudget& budget)
{
    MStatus stat;
    if (grids.empty() || !grids.front()) return MS::kInvalidParameter;
    using TreeType = typename GridType::TreeType;
    using ValueType = typename GridType::ValueType;
    using IntGridT = typename GridType::template ValueConverter<openvdb::Int32>::Type;
    MDataHandle valueHandle = data.inputValue(aTransferAttributes, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool transferAttributes = valueHandle.asBool();
    valueHandle = data.inputValue(aSharpenFeatures, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool sharpenFeatures = valueHandle.asBool();
    valueHandle = data.inputValue(aConcernAllPartsNormal, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool concernAllPartsNormal = valueHandle.asBool();
    const auto transform = grids.front()->transform().copy();
    const auto firstGrid = openvdb::gridConstPtrCast<GridType>(grids.front());
    if (!transform || !firstGrid || firstGrid->getGridClass() != openvdb::GRID_LEVEL_SET)
        return MS::kInvalidParameter;
    const ValueType backgroundValue = firstGrid->background();

    auto referenceHandle = data.inputValue(aReferenceMesh, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MObject referenceObject = referenceHandle.data(); // Historical object-space input.
    std::size_t referenceVertices = 0, referenceTriangles = 0;
    DOMMY_RETURN_IF_FAILURE(preflightVdbMeshInput(referenceObject, budget.resources,
                                                referenceVertices, referenceTriangles));
    std::vector<openvdb::Vec3s> pointList;
    std::vector<openvdb::Vec4I> primList;
    std::vector<int> primitiveFaces;
    if (!budget.resources.addBytes(referenceTriangles, sizeof(int))) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(DOpenVDBMakePointList(referenceObject, *transform, pointList));
    DOMMY_RETURN_IF_FAILURE(DOpenVDBMakePrimList(referenceObject, primList, &primitiveFaces));
    if (pointList.size() != referenceVertices || primList.size() > referenceTriangles ||
        primitiveFaces.size() != primList.size())
        return MS::kFailure;
    typename GridType::ConstPtr refGrid;
    typename IntGridT::Ptr indexGrid;
    if (!dommy::vdb::buildReferenceVolume<GridType>(pointList, primList, *transform,
            sharpenFeatures, budget.resources, refGrid, indexGrid)) return MS::kInvalidParameter;
    openvdb::tools::MeshToVoxelEdgeData edgeData;
    if (sharpenFeatures) edgeData.convert(pointList, primList);

	typedef typename TreeType::template ValueConverter<bool>::Type BoolTreeType;
	typename BoolTreeType::Ptr maskTree;
	
	
		MFloatVectorArray refNormals;
		
		if (sharpenFeatures) {
			valueHandle = data.inputValue(aEdgeTolerance, &stat);
		if (!stat) return stat;
		const float edgetolerance = valueHandle.asFloat();
		if (!std::isfinite(edgetolerance)) return MS::kInvalidParameter;
		
		maskTree = typename BoolTreeType::Ptr(new BoolTreeType(false));
		maskTree->topologyUnion(indexGrid->tree());
		openvdb::tree::LeafManager<BoolTreeType> maskLeafs(*maskTree);
        
		
        MFnMesh referenceMesh(referenceObject, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawFaces = referenceMesh.numPolygons(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        std::size_t referenceFaces = 0;
        if (!dommy::generation::checkedCount(rawFaces, dommy::generation::kMaxGeneratedFaceCount,
                referenceFaces) || !budget.resources.addBytes(referenceFaces, 32) ||
            !budget.resources.addWork(referenceFaces, 20)) return MS::kInvalidParameter;
        MItMeshPolygon polyIt(referenceObject, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MFloatVectorArray faceNormals;
        DOMMY_RETURN_IF_FAILURE(faceNormals.setLength(static_cast<unsigned int>(referenceFaces)));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshPolygons(polyIt, referenceFaces,
            [&](MItMeshPolygon& polygon) -> MStatus {
                MVector normal;
                DOMMY_RETURN_IF_FAILURE(polygon.getNormal(normal));
                if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
                const int faceIndex = polygon.index(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (faceIndex < 0 || static_cast<std::size_t>(faceIndex) >= referenceFaces)
                    return MS::kFailure;
                faceNormals[faceIndex] = normal;
                return MS::kSuccess;
            }));
        // MeshToVolume and MeshToVoxelEdgeData index the expanded triangle/quad
        // list, not Maya polygons. Every triangulated n-gon shares its face normal.
        if (!budget.resources.addBytes(primitiveFaces.size(), 32)) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(refNormals.setLength(static_cast<unsigned int>(primitiveFaces.size())));
        for (std::size_t i = 0; i < primitiveFaces.size(); ++i) {
            const int face = primitiveFaces[i];
            if (face < 0 || static_cast<std::size_t>(face) >= referenceFaces) return MS::kFailure;
            refNormals[static_cast<unsigned int>(i)] = faceNormals[face];
        }

		
		
			GenAdaptivityMaskOp<typename IntGridT::TreeType, BoolTreeType>
			op(refNormals, indexGrid->tree(), maskLeafs, edgetolerance);
			op.run(false);
			
			openvdb::tools::pruneInactive(*maskTree);
		
		openvdb::tools::dilateVoxels(*maskTree, 2);
			
			mesher.setAdaptivityMask(maskTree);
		}
	
	
	valueHandle = data.inputValue(aInternalAdaptivity, &stat);
	if (!stat) return stat;
	const double iadaptivity = valueHandle.asDouble();
	if (!std::isfinite(iadaptivity) || iadaptivity < 0 || iadaptivity > 1) return MS::kInvalidParameter;
	mesher.setRefGrid(refGrid, iadaptivity);
		
	std::vector<std::string> badTransformList, badBackgroundList, badTypeList;
	typename std::list<openvdb::GridBase::ConstPtr>::const_iterator it =
		grids.begin();
		
		
		
#pragma mark convert mesh
	for (; it != grids.end(); ++it) {
		if (!*it) {
			badTypeList.push_back("<null>");
			continue;
		}
		typename GridType::ConstPtr grid = openvdb::gridConstPtrCast<GridType>(*it);
		
		if (!grid) {
			badTypeList.push_back((*it)->getName());
			continue;
		}
		
		if (grid->transform() != *transform) {
			badTransformList.push_back(grid->getName());
			continue;
		}
		
		if (!openvdb::math::isApproxEqual(grid->background(), backgroundValue)) {
			badBackgroundList.push_back(grid->getName());
			continue;
		}
		
		
		
		VDBToMayaMesh converter(mesher, budget);
		if (processTypedScalarGrid(grid, converter)) {
            DOMMY_RETURN_IF_FAILURE(converter.status);
			if (converter.mesh.isNull()) {
				MGlobal::displayError(
					"Reference meshing returned an invalid Maya mesh object.");
				return MS::kFailure;
			}
			
			MFnMesh convertedMesh(converter.mesh, &stat);
			if (!stat) return stat;
			meshPrimData primitiveData;
			stat = getPrimDataFromMesh(convertedMesh, primitiveData);
			if (!stat) return stat;
            primitiveData.m_obj = converter.mesh;
            
            
            
			
			if(converter.getSurfaceGroup().size()){
				primitiveData.surfaceGroup = converter.getSurfaceGroup();
			}
			if(converter.getInteriorGroup().size()){
				primitiveData.interiorGroup = converter.getInteriorGroup();
			}
			if(converter.getSeamGroup().size()){
				primitiveData.seamGroup = converter.getSeamGroup();
			}
			if(converter.getSeamPointGroup().size()){
				primitiveData.seamPointGroup = converter.getSeamPointGroup();
			}
			
			
			primDataList.push_back(primitiveData);
		}
		}
	
	if (!badTypeList.empty()) {
		MGlobal::displayWarning((
			"Reference meshing skipped grids with incompatible types: '" +
			boost::algorithm::join(badTypeList, ", ") + "'.").c_str());
	}
	if (!badTransformList.empty()) {
		MGlobal::displayWarning((
			"Reference meshing skipped grids with different transforms: '" +
			boost::algorithm::join(badTransformList, ", ") + "'.").c_str());
	}
	if (!badBackgroundList.empty()) {
		MGlobal::displayWarning((
			"Reference meshing skipped grids with different backgrounds: '" +
			boost::algorithm::join(badBackgroundList, ", ") + "'.").c_str());
	}
	
#pragma mark Sharpen Features
    if (sharpenFeatures) {
        openvdb::tools::MeshToVoxelEdgeData::Accessor acc = edgeData.getAccessor();
        typedef openvdb::tree::ValueAccessor<const openvdb::BoolTree> BoolAccessor;
        boost::scoped_ptr<BoolAccessor> maskAcc;
        
        if (maskTree) {
            maskAcc.reset(new BoolAccessor(*maskTree));
        }
		
        MVector tmpN, tmpP, avgP;
        MBoundingBox cell;
        
        openvdb::Vec3d pos, normal;
        openvdb::Coord ijk;
        
        std::vector<openvdb::Vec3d> points(12), normals(12);
        std::vector<openvdb::Index32> primitives(12);
		
		
        for (size_t i=0,n=primDataList.size(); i<n; i++) {
			const std::size_t faceCount = primDataList[i].m_faces.length();
			const std::size_t pointCount = primDataList[i].m_pts.length();
			if (primDataList[i].surfaceGroup.size() != faceCount ||
				primDataList[i].seamPointGroup.size() != pointCount) {
				MGlobal::displayError(
					"Reference meshing produced inconsistent topology groups.");
				return MS::kFailure;
			}
			MItMeshVertex vIt(primDataList[i].m_obj, &stat);
			if (!stat) return stat;
			
			
			DOMMY_RETURN_IF_FAILURE(vIt.reset());
            std::size_t visited = 0;
            for (; ; stat = vIt.next()) {
                DOMMY_RETURN_IF_FAILURE(stat);
                const bool done = vIt.isDone(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (done) break;
                if (visited++ >= pointCount || !budget.resources.addWork(1, 16))
                    return MS::kInvalidParameter;
				MIntArray faceIds;
				stat = vIt.getConnectedFaces(faceIds);
				if (!stat) return stat;
				bool isSurfaceGrp = false;
				
				for(uint j=0, FL=faceIds.length(); j<FL; j++){
					if (!dommy::memory::indexInRange(
							faceIds[j], primDataList[i].surfaceGroup.size())) {
						return MS::kFailure;
					}
					if(primDataList[i].surfaceGroup[
							static_cast<std::size_t>(faceIds[j])]) {
						isSurfaceGrp = true;
						break;
					}
				}
				
				if (!isSurfaceGrp)
					continue;
				
				
				const int vertexIndex = vIt.index(&stat);
				if (!stat || !dommy::memory::indexInRange(
						vertexIndex, pointCount)) {
					return MS::kFailure;
				}
				tmpP = primDataList[i].m_pts[
					static_cast<unsigned int>(vertexIndex)];
				if (!dommy::maya::isFiniteVector(tmpP)) {
					return MS::kInvalidParameter;
				}
				pos[0] = tmpP.x;
				pos[1] = tmpP.y;
				pos[2] = tmpP.z;
				
				pos = transform->worldToIndex(pos);
				
                for (int axis = 0; axis < 3; ++axis)
                    if (!std::isfinite(pos[axis]) || std::fabs(pos[axis]) > 1e6 ||
                        !dommy::numeric::checkedNumericCast(std::floor(pos[axis]), ijk[axis]))
                        return MS::kInvalidParameter;
				
				
				if (maskAcc && !maskAcc->isValueOn(ijk)) continue;
				
				points.clear();
				normals.clear();
				primitives.clear();
				
				// get voxel-edge intersections
				edgeData.getEdgeData(acc, ijk, points, primitives);
				if (points.size() != primitives.size() || !budget.resources.addWork(points.size(), 16)) {
					MGlobal::displayError(
						"Reference edge data has inconsistent point and face counts.");
					return MS::kFailure;
				}
				
				avgP=MVector(0.0, 0.0, 0.0);
				
				
				
				// Get the normal list.
				for (size_t p = 0, PN = points.size(); p < PN; ++p) {
					
					for (int axis = 0; axis < 3; ++axis)
                            if (!std::isfinite(points[p][axis]) || std::fabs(points[p][axis]) > 1e6)
                                return MS::kInvalidParameter;
                        avgP.x = static_cast<float>(avgP.x + points[p].x());
					avgP.y = static_cast<float>(avgP.y + points[p].y());
					avgP.z = static_cast<float>(avgP.z + points[p].z());
					
					const openvdb::Index32 primitiveIndex = primitives[p];
					if (!dommy::memory::indexInRange(
							primitiveIndex, refNormals.length())) {
						MGlobal::displayError(
							"Reference edge data contains an invalid face index.");
						return MS::kFailure;
					}
					tmpN = refNormals[
						static_cast<unsigned int>(primitiveIndex)];
					if (!dommy::maya::isFiniteVector(tmpN)) {
						return MS::kInvalidParameter;
					}
					
					normal[0] = tmpN.x;
					normal[1] = tmpN.y;
					normal[2] = tmpN.z;
					
					normals.push_back(normal);
				}//iterate point
				
				// Calculate feature point position
				if (points.size() > 1) {
					
					pos = openvdb::tools::findFeaturePoint(points, normals);
					if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) ||
						!std::isfinite(pos[2])) {
						return MS::kFailure;
					}
					
					// Constrain points to stay inside their initial
					// coordinate cell.
					cell = MBoundingBox(MPoint(double(ijk[0]), double(ijk[1]), double(ijk[2])),
										MPoint(double(ijk[0]+1), double(ijk[1]+1), double(ijk[2]+1)));
					
					cell.expand(MPoint(cell.min().x-0.15, cell.min().y-0.15, cell.min().z-0.15));
					cell.expand(MPoint(cell.max().x+0.15, cell.max().y+0.15, cell.max().z+0.15));
					
					
					if (!cell.contains(MPoint(pos[0], pos[1], pos[2]))) {
						
						MVector org(
									   static_cast<float>(pos[0]),
									   static_cast<float>(pos[1]),
									   static_cast<float>(pos[2]));
						
						avgP *= 1.f / float(points.size());
						MVector dir;
						if (!dommy::maya::normalizedVector(avgP - org, dir)) {
							continue;
						}
						MVector hitVec;
						
						
							if (dommy::math::intersectSegmentAabb(
									cell.min(), cell.max(), org, avgP,
									hitVec)) {
								tmpP = hitVec;
							
							pos[0] = tmpP.x;
							pos[1] = tmpP.y;
							pos[2] = tmpP.z;
						}
					}
					
					pos = transform->indexToWorld(pos);
					if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) ||
						!std::isfinite(pos[2])) {
						return MS::kFailure;
					}
					
					tmpP.x = static_cast<float>(pos[0]);
					tmpP.y = static_cast<float>(pos[1]);
					tmpP.z = static_cast<float>(pos[2]);
					
					
					primDataList[i].m_pts[
						static_cast<unsigned int>(vertexIndex)] = tmpP;
				}
			}//vertex iterate
            
            
			stat = primDataList[i].updatePoints();
			if (!stat) return stat;
            
        }//iterate primitive
    }
    

	
// Charge potentially quadratic cross-part normal matching before host queries.
    if (computeNormals && (!budget.resources.addBytes(budget.connects, 48) ||
        !budget.resources.addWork(budget.connects, concernAllPartsNormal ? budget.connects + 32 : 32)))
        return MS::kInvalidParameter;

#pragma mark Compute vertex normals
	if (computeNormals) {
		
		MVector primN, avgN, tmpN;
		
		const float mAngle = (transferAttributes ? -1.0f : 0.7f);
		valueHandle = data.inputValue(aIsCheckPartsDistance, &stat);
		if (!stat) return stat;
        const bool isCheckPartsDistance = valueHandle.asBool();
		valueHandle = data.inputValue(aCheckTolerance, &stat);
		if (!stat) return stat;
        const double checkTolerance = valueHandle.asDouble();
		if (!std::isfinite(checkTolerance) || checkTolerance < 0.0) {
			return MS::kInvalidParameter;
		}
        
        
        
		
		for (size_t i=0, n=primDataList.size(); i<n; i++) {
			if (primDataList[i].seamPointGroup.size()==0) {
				continue;
			}
			const std::size_t faceCount = primDataList[i].m_faces.length();
			const std::size_t pointCount = primDataList[i].m_pts.length();
			if (primDataList[i].surfaceGroup.size() != faceCount ||
				primDataList[i].seamGroup.size() != faceCount ||
				primDataList[i].interiorGroup.size() != faceCount ||
				primDataList[i].seamPointGroup.size() != pointCount) {
				MGlobal::displayError(
					"Reference meshing produced inconsistent normal groups.");
				return MS::kFailure;
			}
			
			// Read normals from the mesh already created by the reference mesher.
			MFnMesh tmpmesh(primDataList[i].m_obj, &stat);
			if (stat!=MS::kSuccess) return stat;
			
			MIntArray totalfaceIds;
			MIntArray totalvtxIds;
			MVectorArray totalNormals;
            //iterate face
			for (size_t j=0, FL=primDataList[i].m_faces.length(); j<FL; j++)
			{
				
				MIntArray vtxIds;
				stat = tmpmesh.getPolygonVertices(
					static_cast<int>(j), vtxIds);
				if (!stat) return stat;
				
				MFloatVectorArray faceVtxNormals;
				stat = tmpmesh.getFaceVertexNormals(
					static_cast<int>(j), faceVtxNormals, MSpace::kObject);
				if (!stat) return stat;
				if (faceVtxNormals.length() != vtxIds.length()) {
					return MS::kFailure;
				}
				
				MVector faceNormal;
				stat = tmpmesh.getPolygonNormal(
					static_cast<int>(j), faceNormal, MSpace::kObject);
				if (!stat) return stat;
				if (!dommy::maya::isFiniteVector(faceNormal)) {
					return MS::kInvalidParameter;
				}
				
				if (!primDataList[i].seamGroup[j] ||
                    primDataList[i].interiorGroup[j]) {
					for (uint k=0; k<vtxIds.length(); k++) {
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(j));
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(vtxIds[k]));
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(faceVtxNormals[k]));
					}
					continue;
				}
				
				for (uint k=0; k<vtxIds.length(); k++) {
					if (!dommy::memory::indexInRange(vtxIds[k], pointCount)) {
						return MS::kFailure;
					}
					const std::size_t vertexIndex =
						static_cast<std::size_t>(vtxIds[k]);
					if (!primDataList[i].seamPointGroup[vertexIndex]) {
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(j));
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(vtxIds[k]));
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(faceVtxNormals[k]));
						continue;
					}
					
					
					MFloatVector finNormal = faceNormal;
					
					
					MPoint baseVtx = primDataList[i].m_pts[
						static_cast<unsigned int>(vertexIndex)];
					if (!dommy::maya::isFinitePoint(baseVtx)) {
						return MS::kInvalidParameter;
					}
					
					//同じオブジェクト内の法線を考慮
					MItMeshVertex vIt(primDataList[i].m_obj, &stat);
					if (!stat) return stat;
					int preId;
					
					stat = vIt.setIndex(vtxIds[k], preId);
					if (!stat) return stat;
					MIntArray vFacesIds;
					stat = vIt.getConnectedFaces(vFacesIds);
					if (!stat) return stat;
					
					for (uint sf=0; sf<vFacesIds.length(); sf++) {
                        if (!budget.resources.addWork(1, 16)) return MS::kInvalidParameter;
						if (!dommy::memory::indexInRange(
								vFacesIds[sf], faceCount)) {
							return MS::kFailure;
						}
						const std::size_t connectedFace =
							static_cast<std::size_t>(vFacesIds[sf]);
						if (j != connectedFace &&
							primDataList[i].surfaceGroup[connectedFace]) {
							MVector sfaceNormal;
							stat = tmpmesh.getPolygonNormal(
								vFacesIds[sf], sfaceNormal, MSpace::kObject);
							if (!stat) return stat;
							if (!dommy::maya::isFiniteVector(sfaceNormal)) {
								return MS::kInvalidParameter;
							}
							
							if ((sfaceNormal*faceNormal) > mAngle) finNormal+=sfaceNormal;
						}
					}
					
					
					
					if (concernAllPartsNormal) {
						//他のオブジェクトの同じ位置にあるポイントのノーマルを計算
						for (uint prim=0; prim<primDataList.size(); prim++) {
							if (i==prim || primDataList[prim].seamPointGroup.size()==0) {
								continue;
							}
							const std::size_t targetFaceCount =
								primDataList[prim].m_faces.length();
							const std::size_t targetPointCount =
								primDataList[prim].m_pts.length();
							if (primDataList[prim].surfaceGroup.size() != targetFaceCount ||
								primDataList[prim].interiorGroup.size() != targetFaceCount ||
								primDataList[prim].seamGroup.size() != targetFaceCount ||
								primDataList[prim].seamPointGroup.size() != targetPointCount) {
								return MS::kFailure;
							}
							MFnMesh targetmesh(primDataList[prim].m_obj, &stat);
							if (stat!=MS::kSuccess) return stat;
							
							//
							for (uint f=0; f<primDataList[prim].m_faces.length(); f++) {
								if (!primDataList[prim].seamGroup[f] || primDataList[prim].interiorGroup[f]) {
									continue;
								}
								MIntArray targetfvtxIds;
								stat = targetmesh.getPolygonVertices(f, targetfvtxIds);
								if (!stat) return stat;
								
								for (uint fv=0; fv<targetfvtxIds.length(); fv++) {
									if (!dommy::memory::indexInRange(
											targetfvtxIds[fv], targetPointCount)) {
										return MS::kFailure;
									}
									const std::size_t targetVertex =
										static_cast<std::size_t>(targetfvtxIds[fv]);
									const MPoint& targetPoint = primDataList[prim].m_pts[
										static_cast<unsigned int>(targetVertex)];
									if (!dommy::maya::isFinitePoint(targetPoint)) {
										return MS::kInvalidParameter;
									}
									if (primDataList[prim].seamPointGroup[targetVertex] &&
										(targetPoint == baseVtx ||
										 (isCheckPartsDistance &&
										  targetPoint.distanceTo(baseVtx) < checkTolerance))) {
										
										MVector tfaceNormal;
										stat = targetmesh.getPolygonNormal(
											f, tfaceNormal, MSpace::kObject);
										if (!stat) return stat;
										if (!dommy::maya::isFiniteVector(tfaceNormal)) {
											return MS::kInvalidParameter;
										}
										
										if ((tfaceNormal*faceNormal) > mAngle) finNormal+=tfaceNormal;
										
										break;
									}
								}
								
								
							}
							
							
						} //target prim iterate
					}
					
					
					MVector normalized;
					if (!dommy::maya::normalizedVector(finNormal, normalized)) {
						return MS::kFailure;
					}
					
					DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(j));
					DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(vtxIds[k]));
					DOMMY_RETURN_IF_FAILURE(totalNormals.append(normalized));
				} //face ver tex iterate
				
				
							} //face iterate
	            stat = primDataList[i].setNormalPerVtxPerFace(
					totalNormals, totalfaceIds, totalvtxIds);
				if (!stat) return stat;
			}//primitive iterate
	}
	
#pragma mark check corner
	
	valueHandle = data.inputValue(aIsCheckCorner, &stat);
	if (!stat) return stat;
	const bool checkCorners = valueHandle.asBool();
	valueHandle = data.inputValue(aConerAngle, &stat);
	if (!stat) return stat;
	const double cornerDotThreshold = valueHandle.asDouble();
	if (!std::isfinite(cornerDotThreshold) ||
		cornerDotThreshold < -1.0 || cornerDotThreshold > 1.0) {
		return MS::kInvalidParameter;
	}
	if(checkCorners) {
        if (!budget.resources.addBytes(budget.connects, 96) ||
            !budget.resources.addWork(budget.connects, 32)) return MS::kInvalidParameter;
		for (uint i=0, n=primDataList.size(); i<n; i++) {
			const std::size_t faceCount = primDataList[i].m_faces.length();
			const std::size_t pointCount = primDataList[i].m_pts.length();
			if (primDataList[i].surfaceGroup.size() != faceCount) {
				return MS::kFailure;
			}
			MItMeshEdge eIt(primDataList[i].m_obj, &stat);
			if (!stat) return stat;
			MFnMesh tmpmesh(primDataList[i].m_obj, &stat);
			if (!stat) return stat;
			
			MIntArray totalfaceIds;
			MIntArray totalvtxIds;
			MVectorArray totalNormals;

			
			DOMMY_RETURN_IF_FAILURE(eIt.reset());
            std::size_t visited = 0;
            for (; ; stat = eIt.next()) {
                DOMMY_RETURN_IF_FAILURE(stat);
                const bool done = eIt.isDone(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (done) break;
                if (visited++ >= budget.connects || !budget.resources.addWork(1, 16))
                    return MS::kInvalidParameter;
				MIntArray fIds;
				const int connectedFaceCount =
					eIt.getConnectedFaces(fIds, &stat);
				if (!stat) return stat;
				if (connectedFaceCount < 0 ||
					static_cast<unsigned int>(connectedFaceCount) !=
						fIds.length()) {
					return MS::kFailure;
				}
				if (fIds.length()==2 &&
					dommy::memory::indexInRange(fIds[0], faceCount) &&
					dommy::memory::indexInRange(fIds[1], faceCount) &&
					primDataList[i].surfaceGroup[
						static_cast<std::size_t>(fIds[0])] &&
					primDataList[i].surfaceGroup[
						static_cast<std::size_t>(fIds[1])]) {
					
					MVector normal1, normal2;
					stat = tmpmesh.getPolygonNormal(fIds[0], normal1);
					if (!stat) return stat;
					stat = tmpmesh.getPolygonNormal(fIds[1], normal2);
					if (!stat) return stat;
					if (!dommy::maya::isFiniteVector(normal1) ||
						!dommy::maya::isFiniteVector(normal2)) {
						return MS::kInvalidParameter;
					}
					if ((normal1 * normal2) <= cornerDotThreshold) {
						const int firstVertex = eIt.index(0, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
						const int secondVertex = eIt.index(1, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
						if (!dommy::memory::indexInRange(firstVertex, pointCount) ||
							!dommy::memory::indexInRange(secondVertex, pointCount)) {
							return MS::kFailure;
						}
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(fIds[0]));
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(fIds[0]));
						
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(firstVertex));
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(secondVertex));
						
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(normal1));
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(normal1));
						
						
						
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(fIds[1]));
						DOMMY_RETURN_IF_FAILURE(totalfaceIds.append(fIds[1]));
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(firstVertex));
						DOMMY_RETURN_IF_FAILURE(totalvtxIds.append(secondVertex));
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(normal2));
						DOMMY_RETURN_IF_FAILURE(totalNormals.append(normal2));
					}
					
					
				}
			}
			stat = primDataList[i].setNormalPerVtxPerFace(
				totalNormals, totalfaceIds, totalvtxIds);
			if (!stat) return stat;
			
		}
	}
	return MS::kSuccess;
}

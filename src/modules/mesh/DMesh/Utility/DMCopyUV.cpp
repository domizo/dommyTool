//
//  DMCopyUV.cpp
//  dommyTool
//
//  Created by yuzo on 5/10/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMCopyUV.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshUVTransfer.h"

#include <utility>



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"



const MTypeId DMCopyUV::typeId( ID_DM_COPY_UV );


MObject DMCopyUV::inDM;

MObject DMCopyUV::aInObject;

MObject DMCopyUV::outDM;



void *DMCopyUV::creator()
{
    return ::dommy::maya::createObject("DMCopyUV", [&]() {
        return new DMCopyUV();
    });
}




MStatus DMCopyUV::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    const DMeshData* inputMeshes = NULL;
    MStatus status = getValidInputDM(inDM, data, inputMeshes);
    if (!status) return status;
    if (!inputMeshes) return MS::kInvalidParameter;

    const std::size_t meshCount = inputMeshes->numberOfMesh();
    if (meshCount > dommy::generation::kMaxGeneratedMeshCount) {
        return MS::kInvalidParameter;
    }

    MObject outputObject;
    DMeshData* outputMeshes = NULL;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!outputMeshes) return MS::kFailure;

    if (meshCount != 0U) {
        MDataHandle referenceHandle = data.inputValue(aInObject, &status);
        if (!status) return status;
        if (referenceHandle.type() != MFnData::kMesh) return MS::kInvalidParameter;
        MFnMesh reference(referenceHandle.data(), &status);
        if (!status) return status;
        const int sourceFaceCount = reference.numPolygons(&status);
        if (!status) return status;
        if (sourceFaceCount <= 0 ||
            static_cast<std::size_t>(sourceFaceCount) >
                dommy::generation::kMaxGeneratedFaceCount) return MS::kInvalidParameter;
        const int sourceUVCount = reference.numUVs(&status);
        if (!status) return status;
        if (sourceUVCount <= 0 ||
            static_cast<std::size_t>(sourceUVCount) >
                dommy::generation::kMaxWorkItemCount / 2U) return MS::kInvalidParameter;

        const int sourceConnectionCount = reference.numFaceVertices(&status);
        if (!status) return status;
        std::size_t destinationFaceCount = 0U, destinationConnectionCount = 0U;
        if (sourceConnectionCount < 0 ||
            !inputMeshes->getCheckedTotalFace(destinationFaceCount) ||
            !inputMeshes->getCheckedTotalConnect(destinationConnectionCount) ||
            destinationFaceCount != static_cast<std::size_t>(sourceFaceCount) ||
            destinationConnectionCount != static_cast<std::size_t>(sourceConnectionCount) ||
            !dommy::mesh::validMeshUVTransferBudget(sourceFaceCount, sourceConnectionCount, sourceUVCount))
            return MS::kInvalidParameter;

        MFloatArray sourceU, sourceV;
        MIntArray sourceCounts, sourceIds;
        status = reference.getUVs(sourceU, sourceV);
        if (!status) return status;
        status = reference.getAssignedUVs(sourceCounts, sourceIds);
        if (!status) return status;
        if (sourceU.length() != static_cast<unsigned int>(sourceUVCount) ||
            sourceIds.length() != static_cast<unsigned int>(sourceConnectionCount) ||
            sourceCounts.length() != static_cast<unsigned int>(sourceFaceCount) ||
            !dommy::mesh::validFullMeshUVArrays(
                sourceCounts, sourceU, sourceV, sourceCounts, sourceIds)) {
            return MS::kInvalidParameter;
        }

        std::vector<std::vector<int>> destinationFaces;
        destinationFaces.reserve(meshCount);
        std::size_t totalFaces = 0U;
        for (auto mesh = inputMeshes->beginMeshes(); mesh != inputMeshes->endMeshes(); ++mesh) {
            if (!*mesh) return MS::kFailure;
            const MIntArray faces = (*mesh)->getFaceList();
            if (faces.length() != static_cast<unsigned int>((*mesh)->numFace()) ||
                !dommy::generation::checkedSum(
                    totalFaces, faces.length(),
                    dommy::generation::kMaxGeneratedFaceCount, totalFaces)) {
                return MS::kFailure;
            }
            std::vector<int> counts;
            counts.reserve(faces.length());
            for (unsigned int face = 0U; face < faces.length(); ++face) {
                counts.push_back(faces[face]);
            }
            destinationFaces.push_back(std::move(counts));
        }

        std::vector<dommy::mesh::UVTransferSlice> slices;
        if (!dommy::mesh::planMeshUVTransfer(
                destinationFaces, sourceCounts, sourceIds.length(), sourceU.length(), slices)) {
            MGlobal::displayError(
                "DMCopyUV: all faces must have matching vertex counts and complete UV assignments.");
            return MS::kInvalidParameter;
        }

        // Edit only an unpublished deep copy after the complete plan succeeds.
        DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
        std::size_t meshIndex = 0U;
        for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes();
             ++mesh, ++meshIndex) {
            if (meshIndex >= slices.size() || !*mesh) return MS::kFailure;
            const dommy::mesh::UVTransferSlice& slice = slices[meshIndex];
            MFloatArray u, v;
            MIntArray counts, ids;
            const unsigned int connectionCount =
                static_cast<unsigned int>(slice.connectionCount);
            status = u.setLength(connectionCount);
            if (!status) return status;
            status = v.setLength(connectionCount);
            if (!status) return status;
            status = ids.setLength(connectionCount);
            if (!status) return status;
            status = counts.setLength(static_cast<unsigned int>(slice.faceCount));
            if (!status) return status;
            if (u.length() != connectionCount || v.length() != connectionCount ||
                ids.length() != connectionCount || counts.length() != slice.faceCount)
                return MS::kFailure;
            for (unsigned int face = 0U; face < counts.length(); ++face) {
                counts[face] = sourceCounts[
                    static_cast<unsigned int>(slice.faceOffset) + face];
            }
            for (unsigned int corner = 0U; corner < connectionCount; ++corner) {
                const int sourceUV = sourceIds[
                    static_cast<unsigned int>(slice.connectionOffset) + corner];
                u[corner] = sourceU[static_cast<unsigned int>(sourceUV)];
                v[corner] = sourceV[static_cast<unsigned int>(sourceUV)];
                ids[corner] = static_cast<int>(corner);
            }
            if (!(*mesh)->setUvData(u, v, counts, ids)) return MS::kFailure;
        }
        if (meshIndex != slices.size()) return MS::kFailure;
    } else {
        DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
    }

    if (!outputMeshes->hasValidMeshTopology()) return MS::kFailure;
    MDataHandle output = data.outputValue(outDM, &status);
    if (!status) return status;
    status = dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, outputObject, DMeshData::id);
    if (!status) return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMCopyUV")









MStatus DMCopyUV::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;
	
	// ----------------------- INPUTS --------------------------
	inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
	
	aInObject = gAttr.create( "inObject", "ino",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));
	
	
	//create custom data
	outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDM ));
	
	
	
	
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMCopyUV", "initialize", std::current_exception());
}

//
//  DMeshUtil.cpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//

#include "DMeshUtil.h"
#include "DMeshNormals.h"
#include "common/utility/DAttributeMathPublication.h"

#include "common/utility/DMayaPluginData.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaMeshDataOutput.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DGroupPattern.h"

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>

#include "dmesh_ids.h"
#include <limits>
#include <stdexcept>

namespace {

bool canCreateMayaMesh(const DMeshInternalData& mesh)
{
    return mesh.hasValidMeshData() &&
        mesh.numberOfPoints() <=
            static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        static_cast<std::size_t>(mesh.getFaceList().length()) <=
            static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        static_cast<std::size_t>(mesh.getConnectList().length()) <=
            static_cast<std::size_t>(std::numeric_limits<int>::max());
}

MStatus meshOutputPoints(const DMeshInternalData& mesh, DMeshPointSpace space,
                         MPointArray& output)
{
    const DPointList& points = *mesh.getPositionPtrConst();
    MStatus status = output.setLength(static_cast<unsigned int>(points.size()));
    if (!status) return status;
    const MMatrix matrix = mesh.getTransformMatrix();
    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (space == DMeshPointSpace::Transformed) {
            std::array<double, 3> transformed;
            if (!dommy::numeric::checkedTransformPoint(
                    points[index], matrix, transformed)) return MS::kInvalidParameter;
            output[static_cast<unsigned int>(index)] =
                MPoint(transformed[0], transformed[1], transformed[2]);
        } else {
            output[static_cast<unsigned int>(index)] = DPToMP(points[index]);
        }
    }
    return MS::kSuccess;
}

bool buildPrimitiveCenters(const DMeshData& data,
                           std::vector<std::array<double, 3>>& centers)
try {
    if (data.numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount) return false;
    centers.reserve(data.numberOfMesh());
    std::size_t totalPoints = 0U;
    for (auto mesh = data.beginMeshes(); mesh != data.endMeshes(); ++mesh) {
        if (!*mesh) return false;
        if (!dommy::generation::checkedSum(totalPoints, (*mesh)->numberOfPoints(),
                dommy::generation::kMaxWorkItemCount, totalPoints)) return false;
        const MVector center = (*mesh)->getCenterDouble();
        centers.push_back({{center[0], center[1], center[2]}});
    }
    return true;
}
catch (const std::invalid_argument&) { return false; }

} // namespace

const DMeshData* getInputDM(const MObject& mesh, MDataBlock& data)
{
	MStatus stat;
	MDataHandle inputHandle = data.inputValue(mesh, &stat);
	if (!stat) return NULL;

	const DMeshData* inputData = NULL;
	stat = dommy::maya::getPluginDataFromHandle(
		inputHandle, DMeshData::id, inputData);
	return stat && inputData && inputData->hasValidMeshTopology()
	           ? inputData
	           : NULL;
}

MStatus getValidInputDM(const MObject& mesh,
                        MDataBlock& data,
                        const DMeshData*& inputData)
{
	inputData = NULL;
	MStatus status;
	const MDataHandle inputHandle = data.inputValue(mesh, &status);
	if (!status) return status;

	status = dommy::maya::getPluginDataFromHandle(
		inputHandle, DMeshData::id, inputData);
	if (!status) return status;
	if (!inputData || !inputData->hasValidMeshTopology()) {
		inputData = NULL;
		return MS::kInvalidParameter;
	}
	return MS::kSuccess;
}

const DMeshData* getInputDMfromPlug(const MPlug &plg, MObject &obj) {
    const DMeshData* inputData = NULL;
    const MStatus status = dommy::maya::getPluginDataFromPlug(
        plg, DMeshData::id, obj, inputData);
    return status && inputData && inputData->hasValidMeshTopology()
               ? inputData
               : NULL;
}


const DMeshData* getInputDMfromHandle(MDataHandle hnd) {
	const DMeshData* inputData = NULL;
	const MStatus status = dommy::maya::getPluginDataFromHandle(
		hnd, DMeshData::id, inputData);
	return status && inputData && inputData->hasValidMeshTopology()
	           ? inputData
	           : NULL;
}


MStatus generateMMeshFromDmesh(const DMeshInternalData &mesh, MObject &newMesh,
                              DMeshPointSpace space) {
    newMesh = MObject::kNullObj;
    if (!canCreateMayaMesh(mesh)) return MS::kFailure;

    MPointArray points;
    MStatus status = meshOutputPoints(mesh, space, points);
    if (status != MS::kSuccess) return status;
    status = dommy::maya::createMeshDataObject(newMesh);
    if (status != MS::kSuccess) return status;

    MFnMesh meshFn;
    const MObject geometry = meshFn.create(
        static_cast<int>(mesh.numberOfPoints()), mesh.numFace(),
        points, mesh.getFaceList(), mesh.getConnectList(),
        newMesh, &status);
    if (status && geometry.isNull()) status = MS::kFailure;
    if (status) status = dommy::mesh::restoreCornerNormals(
        meshFn, mesh, space == DMeshPointSpace::Transformed);
    if (status != MS::kSuccess) newMesh = MObject::kNullObj;
    return status;
}

MStatus generateMMeshFromDmeshWithUV(const DMeshInternalData &mesh, MObject &newMesh,
                                    DMeshPointSpace space) {
    newMesh = MObject::kNullObj;
    if (!canCreateMayaMesh(mesh)) return MS::kFailure;

    if (mesh.hasUV()) {
        MFloatArray ucoords,vcoords;
        MIntArray uvcounts,uvconnects;
        MStatus status = mesh.getUvData(ucoords, vcoords, uvcounts, uvconnects);
        if (status != MS::kSuccess) return status;

        MPointArray points;
        status = meshOutputPoints(mesh, space, points);
        if (status != MS::kSuccess) return status;

        status = dommy::maya::createMeshDataObject(newMesh);
        if (status != MS::kSuccess) return status;
        MFnMesh meshFn;
        const MObject geometry = meshFn.create(static_cast<int>(mesh.numberOfPoints()), mesh.numFace(),
                      points,
                      mesh.getFaceList(),
                      mesh.getConnectList(),
                      ucoords, vcoords,
                      newMesh, &status);
        if (status && geometry.isNull()) status = MS::kFailure;
        if (status != MS::kSuccess) {
            newMesh = MObject::kNullObj;
            return status;
        }
        status = meshFn.assignUVs(uvcounts, uvconnects);
        if (status) status = dommy::mesh::restoreCornerNormals(
            meshFn, mesh, space == DMeshPointSpace::Transformed);
        if (status != MS::kSuccess) newMesh = MObject::kNullObj;
        return status;
    }

    return generateMMeshFromDmesh(mesh, newMesh, space);
}
MStatus DMGeneratePrimitiveCenterList(const DMeshData& dmdata, DPointList& centerList) {
    std::vector<std::array<double, 3>> centers;
    if (!buildPrimitiveCenters(dmdata, centers)) return MS::kInvalidParameter;

    DPointList candidate;
    candidate.reserve(centers.size());
    for (const auto& center : centers) {
        float values[3];
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::checkedFloatCast(center[axis], values[axis])) {
                return MS::kInvalidParameter;
            }
        }
        candidate.push_back(DPoint(values[0], values[1], values[2]));
    }
    centerList.swap(candidate);
    return MS::kSuccess;
}

MStatus DMGeneratePrimitiveCenterList(const DMeshData& dmdata, MVectorArray& centerList) {
    std::vector<std::array<double, 3>> centers;
    if (!buildPrimitiveCenters(dmdata, centers)) return MS::kInvalidParameter;

    return dommy::maya::buildCheckedArray(centers.size(),
        [&](unsigned int index, MVector& value) -> MStatus {
            value = MVector(centers[index][0], centers[index][1], centers[index][2]);
            return MS::kSuccess;
        }, centerList);
}

#pragma mark DMesh Attribute method

void checkDMeshPointAttribute(const std::string attrName,  DMeshData &outputDMData, bool delAttr) {
    
    for (auto cIt=outputDMData.beginMeshes(); cIt!=outputDMData.endMeshes(); ++cIt) {
        
        if ((*cIt)->isExistAttr(attrName) && delAttr) {
            (*cIt)->removeAttr(attrName);
        }
        
    }
    
}

void checkDMeshAttribute(const std::string attrName,  DMeshData &outputDMData, bool delAttr) {
    
    if (outputDMData.isExistAttr(attrName) && delAttr) {
        outputDMData.removeAttr(attrName);
    }
}


#pragma mark DMesh Add Attribute method

MStatus DMAddAttrScalar(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result) {
    return DPublishAttributeValues(dmdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DMAddAttrVec2(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result) {
    return DPublishAttributeValues(dmdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DMAddAttrVec3(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result) {
    return DPublishAttributeValues(dmdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DMPAddAttrScalar(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result) {
    return DPublishAttributeValues(*mesh, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DMPAddAttrVec2(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result) {
    return DPublishAttributeValues(*mesh, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DMPAddAttrVec3(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result) {
    return DPublishAttributeValues(*mesh, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}




#pragma mark dmesh group method

void getDMeshMatchedGroups(const DMeshData &dmesh, const std::string& patternStr, std::vector<std::string> &matchedGrpNames) {
    if (!dommy::group::appendMatchedGroupNames(dmesh, patternStr, matchedGrpNames))
        throw std::invalid_argument("DMesh group pattern is invalid or exceeds its input budget");
}

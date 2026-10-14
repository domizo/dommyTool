//
//  DMToMesh.cpp
//  dommyTool
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMToMesh.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaObjectArrayOutput.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DMeshDataHeaders.h"
#include "DNodeOperation.h"

#include <algorithm>
#include <vector>

namespace {

MStatus convertPrimitive(
    const DMeshInternalData& source, std::vector<MObject>& output)
{
    MObject mesh;
    const MStatus status = generateMMeshFromDmeshWithUV(
        source, mesh, DMeshPointSpace::Transformed);
    if (status != MS::kSuccess || mesh.isNull()) {
        return status == MS::kSuccess ? MStatus(MS::kFailure) : status;
    }
    output.push_back(mesh);
    return MS::kSuccess;
}

} // namespace

const MTypeId DMToMesh::typeId(ID_DM_TO_MESH);

MObject DMToMesh::inDM;
MObject DMToMesh::aConvertType;
MObject DMToMesh::aGroup;
MObject DMToMesh::outMesh;

void* DMToMesh::creator()
{
    return ::dommy::maya::createObject("DMToMesh", [&]() {
        return new DMToMesh();
    });
}

MStatus DMToMesh::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    const MPlug inputPlug(thisMObject(), inDM);
    const bool connected = inputPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) {
        return dommy::maya::setObjectArrayOutput(
            data, outMesh, plug, std::vector<MObject>());
    }
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    const std::size_t meshCount = input->numberOfMesh();
    if (meshCount > static_cast<std::size_t>(
                        std::numeric_limits<unsigned int>::max())) {
        MGlobal::displayError("DMToMesh primitive count exceeds Maya array limits.");
        return MS::kFailure;
    }

    const MDataHandle convertHandle = data.inputValue(aConvertType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short convertType = convertHandle.asShort();
    const MDataHandle groupHandle = data.inputValue(aGroup, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString groupNamesValue = groupHandle.asString();

    std::vector<MObject> outputMeshes;
    if (convertType == 0) {
        if (meshCount != 0U) {
            std::size_t totalPoints = 0U;
            std::size_t totalFaces = 0U;
            std::size_t totalConnections = 0U;
            const std::size_t mayaIndexLimit = static_cast<std::size_t>(
                std::numeric_limits<int>::max());
            if (!input->getCheckedTotalVertex(totalPoints) ||
                !input->getCheckedTotalFace(totalFaces) ||
                !input->getCheckedTotalConnect(totalConnections) ||
                totalPoints > mayaIndexLimit || totalFaces > mayaIndexLimit ||
                totalConnections > mayaIndexLimit) {
                MGlobal::displayError(
                    "DMToMesh merged mesh exceeds Maya index limits.");
                return MS::kFailure;
            }

            std::vector<const DMeshInternalData*> sources;
            sources.reserve(meshCount);
            for (DMeshData::ConstDMeshIter mesh = input->beginMeshes();
                 mesh != input->endMeshes(); ++mesh) {
                sources.push_back(mesh->get());
            }
            DMeshInternalData::Ptr merged;
            status = DMeshInternalData::createMerged(sources, merged);
            if (status != MS::kSuccess) return status;
            status = convertPrimitive(*merged, outputMeshes);
            if (status != MS::kSuccess) return status;
        }
    } else if (convertType == 1) {
        outputMeshes.reserve(meshCount);
        for (DMeshData::ConstDMeshIter mesh = input->beginMeshes();
             mesh != input->endMeshes(); ++mesh) {
            if (!*mesh) return MS::kFailure;
            status = convertPrimitive(**mesh, outputMeshes);
            if (status != MS::kSuccess) return status;
        }
    } else if (convertType == 2) {
        MStringArray groupNames;
        DOMMY_RETURN_IF_FAILURE(groupNamesValue.split(' ', groupNames));
        DPIdList selected(meshCount, false);
        for (unsigned int index = 0U; index < groupNames.length(); ++index) {
            const std::string groupName = groupNames[index].asChar();
            if (groupName.empty() || !input->isExistGrp(groupName)) continue;
            DPGroupData::ConstPtr group = input->getGrp(groupName);
            if (!group || group->getGroupIds().size() != meshCount) {
                MGlobal::displayError("DMToMesh encountered invalid group data.");
                return MS::kFailure;
            }
            DPIdList groupIds = group->getGroupIds();
            mergeUnionIdListOp(&groupIds, selected);
        }

        const std::size_t selectedCount = static_cast<std::size_t>(
            std::count(selected.begin(), selected.end(), true));
        outputMeshes.reserve(selectedCount);
        std::size_t primitive = 0U;
        for (DMeshData::ConstDMeshIter mesh = input->beginMeshes();
             mesh != input->endMeshes(); ++mesh, ++primitive) {
            if (selected[primitive]) {
                if (!*mesh) return MS::kFailure;
                status = convertPrimitive(**mesh, outputMeshes);
                if (status != MS::kSuccess) return status;
            }
        }
    } else {
        MGlobal::displayError("DMToMesh ConvertType is outside the supported range.");
        return MS::kFailure;
    }

    return dommy::maya::setObjectArrayOutput(
        data, outMesh, plug, outputMeshes);
}
DOMMY_COMPUTE_CATCH("DMToMesh")

MStatus DMToMesh::initialize() try {
    MStatus status;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute convertAttribute;
    aConvertType = convertAttribute.create("ConvertType", "cnvty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(convertAttribute.addField("One Mesh", 0));
    DOMMY_RETURN_IF_FAILURE(convertAttribute.addField("Primitive", 1));
    DOMMY_RETURN_IF_FAILURE(convertAttribute.addField("Group", 2));
    DOMMY_RETURN_IF_FAILURE(convertAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConvertType));

    const MObject defaultGroup = stringData.create("", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aGroup = typedAttribute.create(
        "Group", "grp", MFnData::kString, defaultGroup,
        &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroup));

    outMesh = typedAttribute.create(
        "outMesh", "omh", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setArray(true));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConvertType, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroup, outMesh));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMToMesh", "initialize", std::current_exception());
}

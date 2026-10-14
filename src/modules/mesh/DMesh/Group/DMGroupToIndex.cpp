//
//  DMGroupToIndex.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/22.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMGroupToIndex.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"

#include "DMeshDataHeaders.h"

#include <algorithm>
#include <limits>
#include <vector>

MTypeId DMGroupToIndex::typeId(ID_DM_GROUP_TO_INDEX);

MObject DMGroupToIndex::inDM;
MObject DMGroupToIndex::aGroupName;
MObject DMGroupToIndex::outDataArray;

void *DMGroupToIndex::creator() {
    return ::dommy::maya::createObject("DMGroupToIndex", [&]() {
        return new DMGroupToIndex();
    });
}

MStatus DMGroupToIndex::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDataArray) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    MStatus status;
    const MDataHandle groupHandle = data.inputValue(aGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString groupValue = groupHandle.asString();
    if (groupValue.length() > dommy::generation::kMaxGeneratedStringByteCount ||
        input->numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount)
        return MS::kInvalidParameter;
    const std::string groupName = groupValue.asChar();

    DComponentType type = D_COMP_COUNT;
    bool missingGroup = false;
    dommy::mesh::MeshMergePlan storage;
    for (auto mesh = input->beginPrims(); mesh != input->endPrims(); ++mesh) {
        if (!*mesh || (*mesh)->numFace() < 0 ||
            !dommy::mesh::appendMeshMergePlan((*mesh)->numberOfPoints(),
                static_cast<std::size_t>((*mesh)->numFace()),
                (*mesh)->numberOfConnections(), 0U, storage)) return MS::kInvalidParameter;
        // Preserve point/edge/face priority within each primitive, but never
        // concatenate indices from different component domains.
        DComponentType localType = D_COMP_COUNT;
        for (const DComponentType candidate : {D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE}) {
            if ((*mesh)->isExistCompGrp(candidate, groupName)) {
                localType = candidate;
                break;
            }
        }
        if (localType == D_COMP_COUNT) missingGroup = true;
        else if (type == D_COMP_COUNT) type = localType;
        else if (type != localType) {
            MGlobal::displayError("DMGroupToIndex requires one component type across all meshes.");
            return MS::kInvalidParameter;
        }
    }
    if (type == D_COMP_EDGE && !dommy::mesh::validMeshOperationStorage(storage))
        return MS::kInvalidParameter;

    struct Selection {
        std::size_t count;
        DPGroupData::ConstPtr group;
    };
    std::vector<Selection> selections;
    selections.reserve(input->numberOfMesh());
    std::size_t totalCount = 0U, selectedCount = 0U;
    if (type != D_COMP_COUNT) {
        for (auto mesh = input->beginPrims(); mesh != input->endPrims(); ++mesh) {
            std::size_t count = 0U;
            if (type == D_COMP_POINT) count = (*mesh)->numberOfPoints();
            else if (type == D_COMP_FACE) count = static_cast<std::size_t>((*mesh)->numFace());
            else if ((*mesh)->numFace() != 0) {
                // Match the native edge domain used when DMGroup creates masks.
                MObject object;
                DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(**mesh, object));
                if (object.isNull()) return MS::kFailure;
                MFnMesh meshFn(object, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                const int rawCount = meshFn.numEdges(&status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (!dommy::generation::checkedCount(rawCount,
                        dommy::generation::kMaxGeneratedPointCount, count))
                    return MS::kInvalidParameter;
            }
            if (!dommy::generation::checkedSum(totalCount, count,
                    dommy::generation::kMaxGeneratedPointCount, totalCount) ||
                totalCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                return MS::kInvalidParameter;
            DPGroupData::ConstPtr group;
            if ((*mesh)->isExistCompGrp(type, groupName)) {
                group = (*mesh)->getCompGrp(type, groupName);
                if (!group || group->getGroupIds().size() != count) return MS::kInvalidParameter;
                const DPIdList& ids = group->getGroupIds();
                const std::size_t selected = static_cast<std::size_t>(std::count(ids.begin(), ids.end(), true));
                if (!dommy::generation::checkedSum(selectedCount, selected,
                        dommy::generation::kMaxGeneratedPointCount, selectedCount))
                    return MS::kInvalidParameter;
            }
            selections.push_back(Selection{count, group});
        }
    }

    MIntArray indices;
    DOMMY_RETURN_IF_FAILURE(indices.setLength(static_cast<unsigned int>(selectedCount)));
    if (indices.length() != selectedCount) return MS::kFailure;
    std::size_t offset = 0U, write = 0U;
    for (const Selection& selection : selections) {
        if (selection.group) {
            const DPIdList& ids = selection.group->getGroupIds();
            for (std::size_t i = 0U; i < ids.size(); ++i)
                if (ids[i]) indices[static_cast<unsigned int>(write++)] = static_cast<int>(offset + i);
        }
        // Even an absent group occupies its primitive's full index domain.
        offset += selection.count;
    }
    if (write != selectedCount) return MS::kFailure;
    if (missingGroup) EchoWarning(name().asChar(), "The group is missing from one or more meshes.");
    return DOutputDataMayaIntArray(data, outDataArray, outDataArray, indices);
}
DOMMY_COMPUTE_CATCH("DMGroupToIndex")

MStatus DMGroupToIndex::initialize() try {
    MStatus stat;

    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    const MObject defaultGroup = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroupName = tAttr.create("GroupName", "grpn", MFnData::kString, defaultGroup, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

#pragma mark output
    outDataArray = tAttr.create("outDataArray", "out", MFnData::kIntArray, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroupName, outDataArray));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMGroupToIndex", "initialize", std::current_exception());
}

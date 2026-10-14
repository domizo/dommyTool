//
//  DMDelete.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/02.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMDelete.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"

#include "common/utility/DIndexPattern.h"


const MTypeId DMDelete::typeId( ID_DM_DELETE );


MObject DMDelete::inDM;

MObject DMDelete::aDeleteType;
MObject DMDelete::aDeleteGroups;

MObject DMDelete::aPattern;

MObject DMDelete::outDM;



void *DMDelete::creator()
{
    return ::dommy::maya::createObject("DMDelete", [&]() {
        return new DMDelete();
    });
}



MStatus DMDelete::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* inputDMData = nullptr;
    status = getValidInputDM(inDM, data, inputDMData);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle typeHandle = data.inputValue(aDeleteType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const DComponentType type = typeHandle.asShort();
    if (type != D_COMP_PRIM && type != D_COMP_FACE) {
        return MS::kInvalidParameter;
    }

    MDataHandle groupHandle = data.inputValue(aDeleteGroups, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MString groupName = groupHandle.asString();

    MDataHandle patternHandle = data.inputValue(aPattern, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MString pattern = patternHandle.asString();

    MObject outputDataObject;
    DMeshData* outDMeshes = nullptr;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputDataObject, outDMeshes);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));

    const size_t meshCount = outDMeshes->numberOfMesh();
    if (meshCount != 0U) {
        MStringArray groupNameArray;
        DOMMY_RETURN_IF_FAILURE(groupName.split(' ', groupNameArray));

        if (type == D_COMP_PRIM) {
            DPIdList deletePrimitiveIds(meshCount, false);
            for (unsigned int groupIndex = 0U;
                 groupIndex < groupNameArray.length(); ++groupIndex) {
                const char* const name = groupNameArray[groupIndex].asChar();
                if (!outDMeshes->isExistGrp(name)) continue;

                const DPGroupData::ConstPtr group = outDMeshes->getGrp(name);
                if (!group) return MS::kFailure;
                const DPIdList& groupIds = group->getGroupIds();
                if (groupIds.size() != deletePrimitiveIds.size()) {
                    return MS::kFailure;
                }
                for (size_t index = 0U; index < groupIds.size(); ++index) {
                    if (groupIds[index]) deletePrimitiveIds[index] = true;
                }
            }

            DPIndexList patternIndices;
            bool reverse = false;
            if (!getPatternStrIndex(
                    pattern, meshCount, reverse, patternIndices)) {
                EchoWarning(name().asChar(),
                            "Invalid component index pattern.");
                return MS::kInvalidParameter;
            }
            if (!dommy::component::mergeIndexPatternSelection(
                    patternIndices, reverse, deletePrimitiveIds)) {
                return MS::kInvalidParameter;
            }
            if (!outDMeshes->deleteFromIds(deletePrimitiveIds)) {
                return MS::kFailure;
            }
        } else if (type == D_COMP_FACE) {
            // Keep the index in the loop increment so skipped meshes cannot
            // cause later meshes to reuse the wrong deletion flag.
            DPIdList deletePrimitiveIds(meshCount, false);
            bool deletePrimitives = false;
            for (unsigned int groupIndex = 0U;
                 groupIndex < groupNameArray.length(); ++groupIndex) {
                const char* const name = groupNameArray[groupIndex].asChar();
                size_t meshIndex = 0U;
                for (auto mesh = outDMeshes->beginMeshes();
                     mesh != outDMeshes->endMeshes();
                     ++mesh, ++meshIndex) {
                    if (meshIndex >= deletePrimitiveIds.size() || !*mesh) {
                        return MS::kFailure;
                    }
                    if (deletePrimitiveIds[meshIndex]) continue;
                    if (!(*mesh)->isExistCompGrp(D_COMP_FACE, name)) continue;

                    const DPGroupData::ConstPtr group =
                        (*mesh)->getCompGrp(D_COMP_FACE, name);
                    if (!group) return MS::kFailure;
                    const DPIdList& groupIds = group->getGroupIds();
                    if ((*mesh)->numFace() < 0 ||
                        static_cast<size_t>((*mesh)->numFace()) !=
                            groupIds.size()) {
                        return MS::kFailure;
                    }

                    bool allFacesDeleted = false;
                    if (!(*mesh)->deleteFaceFromIds(
                            groupIds, allFacesDeleted)) {
                        return MS::kFailure;
                    }
                    if (allFacesDeleted) {
                        deletePrimitiveIds[meshIndex] = true;
                        deletePrimitives = true;
                    }
                }
                if (meshIndex != deletePrimitiveIds.size()) {
                    return MS::kFailure;
                }
            }
            if (deletePrimitives) {
                if (!outDMeshes->deleteFromIds(deletePrimitiveIds)) {
                    return MS::kFailure;
                }
            }
        }

        outDMeshes->updateBBox();
    }

    if (!outDMeshes->hasValidMeshTopology()) return MS::kFailure;

    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(output.set(outputDataObject));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMDelete")



MStatus DMDelete::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute opEnumFn;
    aDeleteType = opEnumFn.create( "DeleteType", "delty", D_COMP_PRIM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField( "Primitive", D_COMP_PRIM ));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField( "Face", D_COMP_FACE ));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeleteType));

    const MObject aDeleteGroupsDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDeleteGroups = tAttr.create("DeleteGroups", "delgrp", MFnData::kString, aDeleteGroupsDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeleteGroups));

    const MObject aPatternDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPattern = tAttr.create("Pattern", "patt", MFnData::kString, aPatternDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPattern));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDeleteType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDeleteGroups, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPattern, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMDelete", "initialize", std::current_exception());
}

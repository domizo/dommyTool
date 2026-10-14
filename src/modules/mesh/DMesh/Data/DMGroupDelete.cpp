//
//  DMGroupDelete.cpp
//  dommyTool
//
//  Created by yuzo on 5/4/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMGroupDelete.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"


#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"

#include <array>
#include <cstring>
#include <string>


MTypeId DMGroupDelete::typeId( ID_DM_GROUP_DELETE );

MObject DMGroupDelete::inDM;

MObject DMGroupDelete::aIsEnable;
MObject DMGroupDelete::aPrimitiveGroupName;
MObject DMGroupDelete::aPointGroupName;
MObject DMGroupDelete::aFaceGroupName;
MObject DMGroupDelete::aEdgeGroupName;

MObject DMGroupDelete::outDM;




void *DMGroupDelete::creator() {
    return ::dommy::maya::createObject("DMGroupDelete", [&]() {
        return new DMGroupDelete();
    });
}




MStatus DMGroupDelete::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));

    const std::size_t meshCount = input->numberOfMesh();
    std::array<MStringArray, 4> names;
    if (meshCount != 0U) {
        MStatus status;
        const MDataHandle enableHandle = data.inputValue(aIsEnable, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (enableHandle.asBool()) {
            const MObject attributes[] = {
                aPrimitiveGroupName, aPointGroupName, aFaceGroupName, aEdgeGroupName};
            dommy::maya::NameListBudget budget;
            for (std::size_t domain = 0U; domain < names.size(); ++domain) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::readCheckedNameList(
                    data, attributes[domain], domain == 0U ? 1U : meshCount,
                    budget, names[domain]));
            }
        }
    }

    // Parse every requested domain before editing an unpublished copy. Disabled
    // and empty inputs preserve the legacy pass-through behavior.
    MFnPluginData pluginData;
    DMeshData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    for (unsigned int i = 0U; i < names[0].length(); ++i) {
        const std::string name = names[0][i].asChar();
        if (output->isExistGrp(name)) output->removeGrp(name);
    }
    for (auto mesh = output->beginMeshes(); mesh != output->endMeshes(); ++mesh) {
        if (!*mesh) return MS::kFailure;
        for (unsigned int i = 0U; i < names[1].length(); ++i) {
            const std::string name = names[1][i].asChar();
            if ((*mesh)->isExistGrp(name)) (*mesh)->removeGrp(name);
        }
        for (std::size_t domain = 2U; domain < names.size(); ++domain) {
            const DComponentType type = domain == 2U ? D_COMP_FACE : D_COMP_EDGE;
            for (unsigned int i = 0U; i < names[domain].length(); ++i) {
                const std::string name = names[domain][i].asChar();
                if ((*mesh)->isExistCompGrp(type, name) && !(*mesh)->removeCompGrp(type, name))
                    return MS::kFailure;
            }
        }
    }

    if (!output->hasValidMeshTopology()) return MS::kFailure;
    MStatus status;
    MDataHandle outputHandle = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            outputHandle, pluginData.object(), DMeshData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMGroupDelete")


MStatus DMGroupDelete::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;

    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aIsEnable = nAttr.create("IsEnable", "isen", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEnable));

    const MObject defaultPrimitiveGroup = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPrimitiveGroupName = tAttr.create("PrimitiveGroupName", "pgrpnm", MFnData::kString, defaultPrimitiveGroup, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPrimitiveGroupName));

    const MObject defaultPointGroup = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPointGroupName = tAttr.create("PointGroupName", "ptgrpnm", MFnData::kString, defaultPointGroup, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointGroupName));

    const MObject defaultFaceGroup = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFaceGroupName = tAttr.create("FaceGroupName", "fgrpnm", MFnData::kString, defaultFaceGroup, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroupName));

    const MObject defaultEdgeGroup = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aEdgeGroupName = tAttr.create("EdgeGroupName", "egrpnm", MFnData::kString, defaultEdgeGroup, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeGroupName));

    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsEnable, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrimitiveGroupName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointGroupName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFaceGroupName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeGroupName, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMGroupDelete", "initialize", std::current_exception());
}

//
//  DMMerge.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/24.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMMerge.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"
#include "common/utility/DMeshMergePlan.h"




const MTypeId DMMerge::typeId( ID_DM_MERGE );



MObject DMMerge::inDM;

MObject DMMerge::targetDM;


MObject DMMerge::outDM;


void *DMMerge::creator() {
    return ::dommy::maya::createObject("DMMerge", [&]() {
        return new DMMerge();
    });
}




MStatus DMMerge::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    const DMeshData* inputDMData = nullptr;
    const DMeshData* targetDMData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputDMData));
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(targetDM, data, targetDMData));
    if (!inputDMData || !targetDMData) return MS::kInvalidParameter;

    dommy::mesh::MeshMergePlan outputPlan;
    if (!dommy::mesh::appendMeshRangePlan(inputDMData->beginMeshes(), inputDMData->endMeshes(), outputPlan) ||
        !dommy::mesh::appendMeshRangePlan(targetDMData->beginMeshes(), targetDMData->endMeshes(), outputPlan) ||
        !dommy::mesh::validMeshOperationStorage(outputPlan)) return MS::kInvalidParameter;

    MStatus stat;
    MFnPluginData pluginData;
    pluginData.create(DMeshData::id, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DMeshData* const outDMeshes = dommy::maya::pluginDataPointer<DMeshData>(pluginData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!outDMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(
        outDMeshes->copyMergedFrom(*inputDMData, *targetDMData));

    MStatus outputStatus;
    MDataHandle output = data.outputValue(outDM, &outputStatus);
    DOMMY_RETURN_IF_FAILURE(outputStatus);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMMerge")
            
            




MStatus DMMerge::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    targetDM = tAttr.create("targetDM", "tdm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetDM));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( targetDM, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMMerge", "initialize", std::current_exception());
}

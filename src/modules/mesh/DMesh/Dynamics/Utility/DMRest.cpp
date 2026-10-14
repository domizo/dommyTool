//
//  DMRest.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/02.
//

#include "DMRest.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaEntryPointGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"


const MTypeId DMRest::typeId( ID_DM_REST );

MObject DMRest::inDM;

MObject DMRest::aType;

MObject DMRest::outDM;


void *DMRest::creator() {
    return ::dommy::maya::createObject("DMRest", [&]() {
        return new DMRest();
    });
}


MStatus DMRest::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* inputDMData = nullptr;
    status = getValidInputDM(inDM, data, inputDMData);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle typeHandle = data.inputValue(aType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short type = typeHandle.asShort();
    if (type < 0 || type > 1) return MS::kInvalidParameter;

    MObject outputDataObject;
    DMeshData* outDMeshes = nullptr;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputDataObject, outDMeshes);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));

    if (type == 0) {
        for (auto mesh = outDMeshes->beginPrims();
             mesh != outDMeshes->endPrims(); ++mesh) {
            if (!*mesh) return MS::kFailure;
            if (!(*mesh)->isIdentity()) continue;

            const DPoint center = (*mesh)->getCenter();
            for (auto point = (*mesh)->beginPt();
                 point != (*mesh)->endPt(); ++point) {
                *point -= center;
            }

            MTransformationMatrix transform;
            status = transform.setTranslation(
                DPToMV(center), MSpace::kObject);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const MMatrix matrix = transform.asMatrix();
            if (!dommy::maya::isFiniteMatrix(matrix)) {
                return MS::kInvalidParameter;
            }
            (*mesh)->setTransformMatrix(matrix);
        }
    } else if (type == 1) {
        for (auto mesh = outDMeshes->beginPrims();
             mesh != outDMeshes->endPrims(); ++mesh) {
            if (!*mesh) return MS::kFailure;
            if ((*mesh)->isIdentity()) continue;

            const MMatrix matrix = (*mesh)->getTransformMatrix();
            if (!dommy::maya::isFiniteMatrix(matrix)) {
                return MS::kInvalidParameter;
            }
            DOMMY_RETURN_IF_FAILURE((*mesh)->bakeTransform(matrix));
        }
    }

    if (!outDMeshes->hasValidMeshTopology()) return MS::kFailure;

    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(output.set(outputDataObject));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMRest")





MStatus DMRest::initialize() try {
    MStatus stat;
    MFnTypedAttribute tAttr;
    
    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    
    MFnEnumAttribute typeEnumFn;
    aType = typeEnumFn.create("Type", "ty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField("Rest Position", 0));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField("Freeze Position", 1));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));
    
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aType, outDM));
    
    
    
    
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMRest", "initialize", std::current_exception());
}

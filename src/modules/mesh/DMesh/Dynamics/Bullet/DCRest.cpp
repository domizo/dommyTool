//
//  DCRest.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/15.
//

#include "DCRest.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "dmesh_ids.h"


const MTypeId DCRest::typeId( ID_DC_REST );

MObject DCRest::inDC;

MObject DCRest::aType;
MObject DCRest::aRestMatrix;

MObject DCRest::outDC;


void *DCRest::creator() {
    return ::dommy::maya::createObject("DCRest", [&]() {
        return new DCRest();
    });
}


MStatus DCRest::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    const DCurveData* inputDCData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));

    MStatus status;
    MDataHandle typeHandle = data.inputValue(aType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short type = typeHandle.asShort();
    if (type < 0 || type > 1) return MS::kInvalidParameter;

    std::string restMatrixName("restmat");
    if (type == 0) {
        MDataHandle nameHandle = data.inputValue(aRestMatrix, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const std::string requestedName = nameHandle.asString().asChar();
        if (!requestedName.empty()) restMatrixName = requestedName;
    }

    MObject outputDataObject;
    DCurveData* outDCurves = nullptr;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputDataObject, outDCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outDCurves->copyFrom(*inputDCData));

    if (type == 0 && outDCurves->numberOfCurves() != 0U) {
        DMatrixArray restMatrices(outDCurves->numberOfCurves());
        size_t index = 0U;
        for (auto curve = outDCurves->beginPrims();
             curve != outDCurves->endPrims(); ++curve, ++index) {
            if (!*curve) return MS::kFailure;

            const DPoint center = (*curve)->getCenter();
            for (auto point = (*curve)->beginPt();
                 point != (*curve)->endPt(); ++point) {
                (*point) -= center;
            }

            MTransformationMatrix transform;
            status = transform.setTranslation(
                DPToMV(center), MSpace::kObject);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const MMatrix matrix = transform.asMatrix();
            if (!dommy::maya::isFiniteMatrix(matrix)) {
                return MS::kInvalidParameter;
            }
            (*curve)->setTransformMatrix(matrix);
            restMatrices[index] = matrix;
        }

        if (outDCurves->isExistAttr(restMatrixName)) {
            outDCurves->removeAttr(restMatrixName);
        }
        outDCurves->addAttr(restMatrixName, DPMat4Attr(restMatrices));
    } else if (type == 1) {
        for (auto curve = outDCurves->beginPrims();
             curve != outDCurves->endPrims(); ++curve) {
            if (!*curve) return MS::kFailure;
            if ((*curve)->isIdentity()) continue;

            const MMatrix matrix = (*curve)->getTransformMatrix();
            if (!dommy::maya::isFiniteMatrix(matrix)) {
                return MS::kInvalidParameter;
            }
            for (auto point = (*curve)->beginPt();
                 point != (*curve)->endPt(); ++point) {
                const MPoint transformed = DPToMP(*point) * matrix;
                if (!dommy::maya::isFloatRepresentablePoint(transformed)) {
                    return MS::kInvalidParameter;
                }
                *point = MPToDP(transformed);
            }
            (*curve)->setTransformMatrix(MMatrix::identity);
        }
    }

    if (!outDCurves->hasValidCurveTopology()) {
        return MS::kFailure;
    }

    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(output.set(outputDataObject));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCRest")





MStatus DCRest::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    MFnEnumAttribute typeEnumFn;
    aType = typeEnumFn.create( "Type", "ty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField( "Rest Position", 0 ));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField( "Freeze Position", 1 ));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(typeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));
    
    
    const MObject aRestMatrixDefaultData = fnStringData.create("restmat", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRestMatrix = tAttr.create("RestMatrix", "rstmat", MFnData::kString, aRestMatrixDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestMatrix));
    
    
    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestMatrix, outDC ));
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCRest", "initialize", std::current_exception());
}

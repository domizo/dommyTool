//
//  DCToCurves.cpp
//  dommyTool
//
//  Created by yuzo on 2017/03/14.
//
//

#include "DCToCurves.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaObjectArrayOutput.h"



#include "DMayaHeaders.h"

#include "DCurveDataHeaders.h"

#include <vector>




const MTypeId DCToCurves::typeId( ID_DC_CONVERT_TO_CURVE );


MObject DCToCurves::inDC;

MObject DCToCurves::outCurves;


void *DCToCurves::creator()
{
    return ::dommy::maya::createObject("DCToCurves", [&]() {
        return new DCToCurves();
    });
}



MStatus DCToCurves::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outCurves) return MS::kUnknownParameter;

    const DCurveData* inputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));
    if (!inputCurves) return MS::kFailure;

    const std::size_t curveCount = inputCurves->numberOfCurves();
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount) {
        return MS::kFailure;
    }

    std::vector<MObject> outputCurves;
    outputCurves.reserve(curveCount);
    for (auto curve = inputCurves->beginCurves();
         curve != inputCurves->endCurves(); ++curve) {
        if (!*curve || !(*curve)->hasValidCurveTopology()) {
            return MS::kFailure;
        }
        MObject curveObject;
        const MStatus status =
            generateMCurveFromDcurve(*curve, curveObject);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (curveObject.isNull()) return MS::kFailure;
        outputCurves.push_back(curveObject);
    }

    return dommy::maya::setObjectArrayOutput(
        data, outCurves, plug, outputCurves);
}
DOMMY_COMPUTE_CATCH("DCToCurves")

        
        

MStatus DCToCurves::initialize()
try {
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    outCurves = tAttr.create("outCurves", "ocs", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outCurves));
    

    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDC, outCurves ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCToCurves", "initialize", std::current_exception());
}



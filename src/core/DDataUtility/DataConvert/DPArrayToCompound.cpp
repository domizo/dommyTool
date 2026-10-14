//
//  DPArrayToCompound.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/02.
//
//

#include "DPArrayToCompound.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"




#include "DMayaHeaders.h"
#include "dpoint_ids.h"



const MTypeId DPArrayToCompound::typeId( ID_DP_ARRAY_TO_COMPOUND );


MObject DPArrayToCompound::inputData;
MObject DPArrayToCompound::outValue;



void *DPArrayToCompound::creator() {
    return ::dommy::maya::createObject("DPArrayToCompound", [&]() {
        return new DPArrayToCompound();
    });
}


MStatus DPArrayToCompound::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outValue) return MS::kUnknownParameter;
    MStatus status;
    const MPlug inputPlug(thisMObject(), inputData);
    const bool connected = inputPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kUnknownParameter;

    MDataHandle input = data.inputValue(inputData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    // Preserve the archived integer-only implementation, not the advertised
    // but unfinished conversion branches. This node stays default-disabled.
    if (input.type() != MFnData::kIntArray) {
        MGlobal::displayWarning(
            "DPArrayToCompound: the archived prototype only supports integer arrays.");
        return MS::kFailure;
    }
    MFnIntArrayData arrayData(input.data(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MIntArray values = arrayData.array(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (values.length() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    MArrayDataHandle output = data.outputArrayValue(outValue, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataBuilder builder(&data, outValue, values.length(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    for (unsigned int index = 0U; index < values.length(); ++index) {
        MDataHandle element = builder.addElement(index, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        element.set(values[index]);
    }
    DOMMY_RETURN_IF_FAILURE(output.set(builder));
    DOMMY_RETURN_IF_FAILURE(output.setAllClean());
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPArrayToCompound")




MStatus DPArrayToCompound::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    
    
    inputData = gAttr.create( "inputData", "ind",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kStringArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrixArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputData));
    
    
#pragma mark output
    
    outValue = gAttr.create( "outValue", "oval",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNumeric));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outValue));
    
    
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inputData, outValue ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPArrayToCompound", "initialize", std::current_exception());
}

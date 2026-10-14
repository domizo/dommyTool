//
//  DPNull.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/19.
//
//

#include "DPNull.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DNodeOperation.h"



const MTypeId DPNull::typeId( ID_DP_NULL );

MObject DPNull::inDD;

MObject DPNull::outDD;


void *DPNull::creator() {
    return ::dommy::maya::createObject("DPNull", [&]() {
        return new DPNull();
    });
}


MStatus DPNull::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDD) return MS::kUnknownParameter;

    MStatus stat;
    MDataHandle input = data.inputValue(inDD, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (input.data().isNull()) {
        EchoWarning(name().asChar(), "input data is null.");
        return MS::kFailure;
    }

    const unsigned int inputType = input.typeId().id();
    if (inputType == ID_DP_DATA) {
        const DPData* source = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(input, DPData::id, source));
        if (!source) return MS::kFailure;
        if (!dommy::maya::validatePluginData(*source)) return MS::kInvalidParameter;
        stat = dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) { output.setDPData(*source); });
    } else if (inputType == ID_DC_DATA) {
        const DCurveData* source = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(input, DCurveData::id, source));
        if (!source) return MS::kFailure;
        if (!dommy::maya::validatePluginData(*source)) return MS::kInvalidParameter;
        stat = dommy::maya::setPluginDataOutput<DCurveData>(data, outDD, DCurveData::id,
            [&](DCurveData& output) { return output.copyFrom(*source); });
    } else if (inputType == ID_DM_DATA) {
        const DMeshData* source = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(input, DMeshData::id, source));
        if (!source) return MS::kFailure;
        if (!dommy::maya::validatePluginData(*source)) return MS::kInvalidParameter;
        stat = dommy::maya::setPluginDataOutput<DMeshData>(data, outDD, DMeshData::id,
            [&](DMeshData& output) { return output.copyFrom(*source); });
    } else {
        return MS::kInvalidParameter;
    }
    DOMMY_RETURN_IF_FAILURE(stat);
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPNull")



MStatus DPNull::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    MFnGenericAttribute outAttr;
    
    
    // ----------------------- INPUTS --------------------------
    
    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    
    
    //create custom data
    outDD = gAttr.create( "outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPNull", "initialize", std::current_exception());
}

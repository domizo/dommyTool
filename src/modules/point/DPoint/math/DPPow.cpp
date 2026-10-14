//
//  DPPow.cpp
//  dommyTool
//
//  Created by yuzo on 2/18/17.
//
//

#include "DPPow.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "DMayaMathSettings.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DNodeOperation.h"


#include "DPMathUtility.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>



const MTypeId DPPow::typeId( ID_DP_MATH_POW );


MObject DPPow::inDD;
MObject DPPow::aSourceAttribute;
MObject DPPow::inSourceDataArray;



MObject DPPow::aConstantValue;
MObject DPPow::aTargetAttribute;
MObject DPPow::inTargetDataArray;

MObject DPPow::aIsOutAttribute;
MObject DPPow::aOutAttribute;
MObject DPPow::aIsDeleteOriginal;
MObject DPPow::outDD;


MObject DPPow::outDataArray;


void *DPPow::creator() {
    return ::dommy::maya::createObject("DPPow", [&]() {
        return new DPPow();
    });
}



MStatus DPPow::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug == outDD) {
        dommy::maya::MathNodeSettings settings;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::readMathNodeSettings<DPPow>(data, settings));
        const std::string& sourceAttribute = settings.sourceAttribute;
        const std::string& outputAttribute = settings.outputAttribute;
        const bool deleteOriginal = settings.deleteOriginal;
        const bool createOutputAttribute = settings.createOutputAttribute;

        if (createOutputAttribute && outputAttribute.empty()) {
            EchoWarning(name().asChar(), "Output attribute name is empty.");
            return MS::kInvalidParameter;
        }

        return processDDataMathOutput(
            inDD, outDD, name().asChar(), sourceAttribute,
            outputAttribute, createOutputAttribute, deleteOriginal,
            aConstantValue, inTargetDataArray, aTargetAttribute,
            data, plug, D_CAL_POW);
    }

    if (plug == outDataArray) {
        const MPlug sourceDataArrayPlug(thisMObject(), inSourceDataArray);
        MStatus status;
        const bool connected = sourceDataArrayPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected) return MS::kFailure;

        return processMayaNumericMathFunc(
            thisMObject(), name().asChar(), inSourceDataArray,
            outDataArray, aConstantValue, inTargetDataArray,
            data, plug, D_CAL_POW);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPPow")


MStatus DPPow::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    
    
    
    
    aConstantValue = nAttr.create( "ConstantValue", "cnval", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConstantValue));
    
    
    
    MObject emptyAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSourceAttribute = tAttr.create("SourceAttribute", "srcattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceAttribute));
    
    
    aTargetAttribute = tAttr.create("TargetAttribute", "tarattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetAttribute));
    
    
    
    inSourceDataArray = gAttr.create( "inSourceDataArray", "insrcdt",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNumeric));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inSourceDataArray));
    
    
    
    
    
    inTargetDataArray = gAttr.create( "inTargetDataArray", "intardt",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNumeric));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setCached( false ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDataArray));
    
    
    
    aIsOutAttribute = nAttr.create( "IsOutAttribute", "isout", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOutAttribute));
    
    const MObject aOutAttributeDefaultData = fnStringData.create("result", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, aOutAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));
    
    
    aIsDeleteOriginal = nAttr.create( "IsDeleteOriginal", "idor", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeleteOriginal));
    
    //create custom data
    outDD = gAttr.create( "outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
    
    
    outDataArray = gAttr.create( "outDataArray", "out",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNumeric));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kAny));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(MFnNumericData::kDouble));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(MFnNumericData::kFloat));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));
    
    
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstantValue, outDD ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetAttribute, outDD ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDataArray, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutAttribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstantValue, outDataArray ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDataArray, outDataArray ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPPow", "initialize", std::current_exception());
}

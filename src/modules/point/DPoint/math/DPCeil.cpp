//
//  DPCeil.cpp
//  dommyTool
//
//  Created by yuzo on 2/19/17.
//
//

#include "DPCeil.h"
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



const MTypeId DPCeil::typeId( ID_DP_MATH_CEIL );


MObject DPCeil::inDD;
MObject DPCeil::aSourceAttribute;
MObject DPCeil::inSourceDataArray;




MObject DPCeil::aIsOutAttribute;
MObject DPCeil::aOutAttribute;
MObject DPCeil::aIsDeleteOriginal;

MObject DPCeil::outDD;
MObject DPCeil::outDataArray;


void *DPCeil::creator() {
    return ::dommy::maya::createObject("DPCeil", [&]() {
        return new DPCeil();
    });
}



MStatus DPCeil::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug == outDD) {
        dommy::maya::MathNodeSettings settings;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::readMathNodeSettings<DPCeil>(data, settings));
        const std::string& sourceAttribute = settings.sourceAttribute;
        const std::string& outputAttribute = settings.outputAttribute;
        const bool deleteOriginal = settings.deleteOriginal;
        const bool createOutputAttribute = settings.createOutputAttribute;

        if (createOutputAttribute && outputAttribute.empty()) {
            EchoWarning(name().asChar(), "Output attribute name is empty.");
            return MS::kInvalidParameter;
        }

        return processDDataMathOutputSingle(
            inDD, outDD, name().asChar(), sourceAttribute,
            outputAttribute, createOutputAttribute, deleteOriginal,
            data, plug, D_SRC_CEIL);
    }

    if (plug == outDataArray) {
        const MPlug sourceDataArrayPlug(thisMObject(), inSourceDataArray);
        MStatus status;
        const bool connected = sourceDataArrayPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected) return MS::kFailure;

        return processMayaNumericMathFuncSingle(
            thisMObject(), name().asChar(), inSourceDataArray,
            outDataArray, data, plug, D_SRC_CEIL);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPCeil")


MStatus DPCeil::initialize() try {
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
	
	
	MObject emptyAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aSourceAttribute = tAttr.create("SourceAttribute", "srcattr", MFnData::kString, emptyAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceAttribute));
	
	
	
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
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutAttribute, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPCeil", "initialize", std::current_exception());
}

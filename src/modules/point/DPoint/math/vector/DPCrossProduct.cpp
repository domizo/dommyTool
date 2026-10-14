//
//  DPCrossProduct.cpp
//  dommyTool
//
//  Created by yuzo on 2/20/17.
//
//

#include "DPCrossProduct.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "dpoint_ids.h"
#include "DMayaHeaders.h"
#include "math/DMayaVectorNode.h"
#include "common/utility/DComputeGuard.h"



const MTypeId DPCrossProduct::typeId( ID_DP_MATH_CROSS_PRODUCT );


MObject DPCrossProduct::inDD;
MObject DPCrossProduct::aSourceAttribute;
MObject DPCrossProduct::inSourceDataArray;


MObject DPCrossProduct::aType;
MObject DPCrossProduct::aConstantValue;
MObject DPCrossProduct::aTargetAttribute;
MObject DPCrossProduct::inTargetDataArray;


MObject DPCrossProduct::aOutAttribute;

MObject DPCrossProduct::outDD;


MObject DPCrossProduct::outDataArray;


void *DPCrossProduct::creator() {
    return ::dommy::maya::createObject("DPCrossProduct", [&]() {
        return new DPCrossProduct();
    });
}



MStatus DPCrossProduct::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeVectorNode<DPCrossProduct, dommy::math::VectorNodeKind::Cross>(plug, data);
}
DOMMY_COMPUTE_CATCH("DPCrossProduct")


MStatus DPCrossProduct::initialize() try {
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
	
	
	
	MFnEnumAttribute enumFn;
	aType = enumFn.create( "Type", "ty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Constant Value", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Attribute", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From InputDataArray", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aType));
	
	
	
	aConstantValue = nAttr.create( "ConstantValue", "cnval", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
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
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetAttribute));
	
	
	
	inSourceDataArray = gAttr.create( "inSourceDataArray", "insrcdt",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inSourceDataArray));
	
	
	
	
	
	inTargetDataArray = gAttr.create( "inTargetDataArray", "intardt",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDataArray));
	
	
	
	
	
	const MObject aOutAttributeDefaultData = fnStringData.create("cross", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, aOutAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));
	
	
	
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
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));
	
	
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstantValue, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetAttribute, outDD ));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDataArray, outDD ));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aType, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstantValue, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDataArray, outDataArray ));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPCrossProduct", "initialize", std::current_exception());
}

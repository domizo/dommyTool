//
//  DPAttributeDelete.cpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#include "DPAttributeDelete.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaNameList.h"

#include "DMayaHeaders.h"


#include "DPointDataHeaders.h"


const MTypeId DPAttributeDelete::typeId( ID_DP_ATTR_DELETE );


MObject DPAttributeDelete::inDP;
MObject DPAttributeDelete::isEnable;
MObject DPAttributeDelete::attributeName;
MObject DPAttributeDelete::outDP;

void *DPAttributeDelete::creator() {
    return ::dommy::maya::createObject("DPAttributeDelete", [&]() {
        return new DPAttributeDelete();
    });
}

MStatus DPAttributeDelete::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !dommy::maya::validatePluginData(*input))
        return MS::kInvalidParameter;

    const MDataHandle nameHandle = data.inputValue(attributeName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString names = nameHandle.asString();
    const MDataHandle enableHandle = data.inputValue(isEnable, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool enabled = enableHandle.asBool();

    MStringArray attributes;
    if (enabled && names.length() != 0U) {
        dommy::maya::NameListBudget budget;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::splitCheckedNameList(names, 1U, budget, attributes));
    }

    MObject outputObject;
    DPData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, output));
    if (!output)
        return MS::kFailure;
    output->setDPData(*input);
    for (unsigned int index = 0U; index < attributes.length(); ++index) {
        const std::string name = attributes[index].asChar();
        if (output->isExistAttr(name))
            output->removeAttr(name);
    }

    MDataHandle outputHandle = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(outputHandle, outputObject, *output));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAttributeDelete")



MStatus DPAttributeDelete::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	
	
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	isEnable = nAttr.create( "isEnable", "ise", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(isEnable));
	
	
	MFnStringData fnStringData;
	MObject visAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	attributeName = tAttr.create("attributeName", "atn", MFnData::kString, visAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(attributeName));
	
	
	//create custom data
	outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( isEnable, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeName, outDP ));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeDelete", "initialize", std::current_exception());
}

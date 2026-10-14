//
//  DPGroupDelete.cpp
//  dommyTool
//
//  Created by yuzo on 5/26/16.
//
//

#include "DPGroupDelete.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaNameList.h"

#include "DMayaHeaders.h"


#include "DPointDataHeaders.h"



const MTypeId DPGroupDelete::typeId( ID_DP_GROUP_DELETE );


MObject DPGroupDelete::inDP;
MObject DPGroupDelete::aIsEnable;
MObject DPGroupDelete::aGroupName;
MObject DPGroupDelete::outDP;

void *DPGroupDelete::creator() {
    return ::dommy::maya::createObject("DPGroupDelete", [&]() {
        return new DPGroupDelete();
    });
}


MStatus DPGroupDelete::compute(const MPlug& plug, MDataBlock& data) try {
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

    const MDataHandle nameHandle = data.inputValue(aGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString names = nameHandle.asString();
    const MDataHandle enableHandle = data.inputValue(aIsEnable, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool enabled = enableHandle.asBool();

    MStringArray groups;
    if (enabled && names.length() != 0U) {
        dommy::maya::NameListBudget budget;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::splitCheckedNameList(names, 1U, budget, groups));
    }

    MObject outputObject;
    DPData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, output));
    if (!output)
        return MS::kFailure;
    output->setDPData(*input);
    for (unsigned int index = 0U; index < groups.length(); ++index) {
        const std::string name = groups[index].asChar();
        if (output->isExistGrp(name))
            output->removeGrp(name);
    }

    MDataHandle outputHandle = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(outputHandle, outputObject, *output));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPGroupDelete")



MStatus DPGroupDelete::initialize() try {
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
	
	aIsEnable = nAttr.create( "IsEnable", "ise", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEnable));
	
	
	MFnStringData fnStringData;
	
	const MObject aGroupNameDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aGroupName = tAttr.create("GroupName", "grpn", MFnData::kString, aGroupNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));
	
	
	//create custom data
	outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEnable, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupName, outDP ));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPGroupDelete", "initialize", std::current_exception());
}

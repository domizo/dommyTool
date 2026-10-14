//
//  DPAttributeCast.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/08.
//
//

#include "DPAttributeCast.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DAttributeCast.h"




#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"




const MTypeId DPAttributeCast::typeId( ID_DP_ATTR_CAST );


MObject DPAttributeCast::inDP;

MObject DPAttributeCast::aAttributeName;

MObject DPAttributeCast::aCastType;

MObject DPAttributeCast::outDP;



void *DPAttributeCast::creator()
{
    return ::dommy::maya::createObject("DPAttributeCast", [&]() {
        return new DPAttributeCast();
    });
}




MStatus DPAttributeCast::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;

    static_assert(DPA_CAST_TO_INT == static_cast<int>(dommy::attribute::CastTarget::Int) &&
                  DPA_CAST_TO_FLOAT == static_cast<int>(dommy::attribute::CastTarget::Float) &&
                  DPA_CAST_TO_DOUBLE == static_cast<int>(dommy::attribute::CastTarget::Double),
                  "Attribute cast modes must retain their serialized values");
    MStatus status;
    const MDataHandle typeHandle = data.inputValue(aCastType, &status);
    if (!status) return status;
    const int target = typeHandle.asShort();
    const MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
    if (!status) return status;
    const std::string attributeName = nameHandle.asString().asChar();
    if (!dommy::attribute::validCastTarget(target) ||
        !dommy::attribute::validAttributeName(attributeName)) return MS::kInvalidParameter;

    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    if (!status) return status;
    const DPData* input = nullptr;
    status = dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input);
    if (!status) return status;
    if (!input || !dommy::maya::validatePluginData(*input) ||
        !dommy::attribute::validArrayConversionBudget(
            input->numberOfPoints(), sizeof(Imath::V3d))) return MS::kInvalidParameter;

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [&](DPData& output) -> MStatus {
            output.setDPData(*input);
            if (!dommy::attribute::castPointAttribute(output, attributeName, target)) {
                MGlobal::displayError(
                    "DPAttributeCast: invalid attribute, size, or numeric conversion.");
                return MS::kInvalidParameter;
            }
            return MS::kSuccess;
        });
    if (!status) return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAttributeCast")

            
            






MStatus DPAttributeCast::initialize()
try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    
    const MObject aAttributeNameDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("aAttributeName", "attrnm", MFnData::kString, aAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
    
    
    MFnEnumAttribute enumFn;
    aCastType = enumFn.create( "CastType", "cty", DPA_CAST_TO_INT,&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "to int", DPA_CAST_TO_INT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "to float", DPA_CAST_TO_FLOAT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "to double", DPA_CAST_TO_DOUBLE ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCastType));
    
    
    
    
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCastType, outDP ));
    
    
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeCast", "initialize", std::current_exception());
}

//
//  DPAttributeToDataArray.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/10.
//

#include "DPAttributeToDataArray.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "dpoint_ids.h"



const MTypeId DPAttributeToDataArray::typeId( ID_DP_ATTR_TO_MDATA_ARRAY );


MObject DPAttributeToDataArray::inDP;

MObject DPAttributeToDataArray::aAttributeName;

MObject DPAttributeToDataArray::outDataArray;



void *DPAttributeToDataArray::creator()
{
    return ::dommy::maya::createObject("DPAttributeToDataArray", [&]() {
        return new DPAttributeToDataArray();
    });
}



namespace {

template <typename Source, typename Array>
MStatus copyConvertedNumericArray(const std::vector<Source>& source, Array& output)
{
    MStatus status = output.setLength(static_cast<unsigned int>(source.size()));
    if (!status) return status;
    for (unsigned int i = 0U; i < output.length(); ++i) output[i] = source[i];
    return MS::kSuccess;
}

} // namespace

MStatus DPAttributeToDataArray::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDataArray) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
    if (!status) return status;
    const std::string attributeName = nameHandle.asString().asChar();
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kFailure;

    dommy::attribute::ConvertedArray converted;
    if (!dommy::attribute::convertAttributeToArray(
            input->getAttr(attributeName), input->numberOfPoints(), converted)) {
        MGlobal::displayError("DPAttributeToDataArray: invalid attribute, size, or numeric conversion.");
        return MS::kInvalidParameter;
    }

    switch (converted.kind) {
    case dommy::attribute::ArrayKind::Int: {
        MIntArray output;
        DOMMY_RETURN_IF_FAILURE(copyConvertedNumericArray(converted.integers, output));
        return DOutputDataMayaIntArray(data, plug, outDataArray, output);
    }
    case dommy::attribute::ArrayKind::Float: {
        MFloatArray output;
        DOMMY_RETURN_IF_FAILURE(copyConvertedNumericArray(converted.floats, output));
        return DOutputDataMayaFloatArray(data, plug, outDataArray, output);
    }
    case dommy::attribute::ArrayKind::Double: {
        MDoubleArray output;
        DOMMY_RETURN_IF_FAILURE(copyConvertedNumericArray(converted.doubles, output));
        return DOutputDataMayaDoubleArray(data, plug, outDataArray, output);
    }
    case dommy::attribute::ArrayKind::Vector: {
        MVectorArray output;
        DOMMY_RETURN_IF_FAILURE(output.setLength(
            static_cast<unsigned int>(converted.vectors.size())));
        for (unsigned int i = 0U; i < output.length(); ++i) {
            const auto& value = converted.vectors[i];
            output[i] = MVector(value[0], value[1], value[2]);
        }
        return DOutputDataMayaVectorArray(data, plug, outDataArray, output);
    }
    }
    return MS::kInvalidParameter;
}
DOMMY_COMPUTE_CATCH("DPAttributeToDataArray")
            


MStatus DPAttributeToDataArray::initialize()
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
    DOMMY_RETURN_IF_FAILURE(tAttr.setCached(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    
    
    
    MFnStringData strData;
    const MObject aAttributeNameDefaultData = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("AttributeName", "attrnm", MFnData::kString, aAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
    
    
    
    
    //create custom data
    
    outDataArray = gAttr.create( "outDataArray", "out",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kStringArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrixArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDataArray ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDataArray ));
    
    
    
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeToDataArray", "initialize", std::current_exception());
}

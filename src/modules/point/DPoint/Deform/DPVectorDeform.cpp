//
//  DPVectorDeform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/04/01.
//
//

#include "DPVectorDeform.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DPointPayloadBudget.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DPData.h"
#include "DPointDataAccess.h"
#include "dpoint_ids.h"

#include <atomic>
#include <cmath>
#include <vector>
#include <tbb/parallel_for.h>


const MTypeId DPVectorDeform::typeId( ID_DP_VECTOR_DEFORM );


MObject DPVectorDeform::inDP;

MObject DPVectorDeform::aVectorType;
MObject DPVectorDeform::aBaseVector;
MObject DPVectorDeform::aVectorAttributeName;

MObject DPVectorDeform::aMagnitude;

MObject DPVectorDeform::aUseClamp;
MObject DPVectorDeform::aMaxStrength;


MObject DPVectorDeform::aWeightAttributeName;
MObject DPVectorDeform::aWeight;


MObject DPVectorDeform::outDP;


void *DPVectorDeform::creator()
{
    return ::dommy::maya::createObject("DPVectorDeform", [&]() {
        return new DPVectorDeform();
    });
}




MStatus DPVectorDeform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kInvalidParameter;
    MStatus status;
    MDataHandle handle = data.inputValue(aVectorType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short vectorType = handle.asShort();
    handle = data.inputValue(aBaseVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float3& baseValues = handle.asFloat3();
    const Imath::V3d base(baseValues[0], baseValues[1], baseValues[2]);
    handle = data.inputValue(aVectorAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string vectorAttribute = handle.asString().asChar();
    handle = data.inputValue(aMagnitude, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double magnitude = handle.asFloat();
    handle = data.inputValue(aWeightAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::string weightAttribute = handle.asString().asChar();
    handle = data.inputValue(aWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double weight = handle.asFloat();
    handle = data.inputValue(aUseClamp, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool clamp = handle.asBool();
    handle = data.inputValue(aMaxStrength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double maximum = handle.asFloat();
    if (vectorType < 0 || vectorType > 1 || !std::isfinite(magnitude) ||
        !std::isfinite(weight) || !std::isfinite(base[0]) ||
        !std::isfinite(base[1]) || !std::isfinite(base[2]) ||
        !std::isfinite(maximum) || (clamp && maximum < 0.0)) return MS::kInvalidParameter;

    const std::size_t count = input->numberOfPoints();
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*input,
        sizeof(DPoint) + sizeof(Imath::V3d) + sizeof(double), workspace);
    DPointList points(*input->getPositionPtrConst());
    std::vector<Imath::V3d> vectors(count, base);
    std::vector<double> weights(count, 1.0);
    if (vectorType == 1 && input->isExistAttr(vectorAttribute) &&
        !dommy::attribute::convertVector3Attribute(
            input->getAttr(vectorAttribute), count, vectors)) return MS::kInvalidParameter;
    if (input->isExistAttr(weightAttribute) &&
        !dommy::attribute::convertScalarAttribute(
            input->getAttr(weightAttribute), count, weights)) return MS::kInvalidParameter;
    std::atomic<bool> valid(true);
    // Workers touch detached C++ arrays only, never Maya objects or attributes.
    const auto deform = [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
            std::array<double, 3> vector{{vectors[i][0], vectors[i][1], vectors[i][2]}};
            if (clamp && !dommy::point::clampVectorMagnitude(vector, maximum, vector)) {
                valid.store(false, std::memory_order_relaxed);
                continue;
            }
            if (!dommy::point::offsetAlongVector(points[i], vector,
                    magnitude, weights[i], weight, points[i]))
                valid.store(false, std::memory_order_relaxed);
        }
    };
    const tbb::blocked_range<std::size_t> range(0U, count, 4096U);
    if (count < 4096U) deform(range);
    else tbb::parallel_for(range, deform);
    if (!valid.load(std::memory_order_relaxed)) return MS::kInvalidParameter;

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, &points](DPData& output) -> MStatus {
            return output.copyRepeatedFrom(*input, 1U,
                [&points](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                    point = points[index];
                    return MS::kSuccess;
                });
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPVectorDeform")



MStatus DPVectorDeform::initialize()
try {
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    
    MFnEnumAttribute vecEnum;
    aVectorType = vecEnum.create( "VectorType", "vty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(vecEnum.addField( "Base Vector", 0 ));
    DOMMY_RETURN_IF_FAILURE(vecEnum.addField( "From Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(vecEnum.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));
    
    
    
    aBaseVector = nAttr.create( "BaseVector", "bprm", MFnNumericData::k3Float , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,1.0,0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseVector));
    
    
    const MObject aVectorAttributeNameDefaultData = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttributeName = tAttr.create("VectorAttributeName", "vattrn", MFnData::kString, aVectorAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttributeName));
    
    
    
    aMagnitude = nAttr.create( "Magnitude", "mg", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnitude));
    
    
    aUseClamp = nAttr.create( "UseClamp", "uclp", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseClamp));
    
    aMaxStrength = nAttr.create( "MaxStrength", "mxstr", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxStrength));
    
    
    
    const MObject aWeightAttributeNameDefaultData = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttributeName = tAttr.create("WeightAttributeName", "weiattrn", MFnData::kString, aWeightAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttributeName));
    
    aWeight = nAttr.create( "Weight", "wei", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));
    
    
    
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDP, outDP ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorType, outDP ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBaseVector, outDP ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorAttributeName, outDP ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aMagnitude, outDP ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aUseClamp, outDP ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aMaxStrength, outDP ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttributeName, outDP ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDP ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPVectorDeform", "initialize", std::current_exception());
}

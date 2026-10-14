//
//  DCVectorDeform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/23.
//
//

#include "DCVectorDeform.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveDataHeaders.h"


#include <atomic>
#include <cmath>
#include <vector>
#include <tbb/parallel_for.h>


const MTypeId DCVectorDeform::typeId( ID_DC_VECTOR_DEFORM );


MObject DCVectorDeform::inDC;

MObject DCVectorDeform::aVectorType;
MObject DCVectorDeform::aBaseVector;
MObject DCVectorDeform::aVectorAttributeName;

MObject DCVectorDeform::aMagnitude;

MObject DCVectorDeform::aWeightAttributeName;
MObject DCVectorDeform::aWeight;


MObject DCVectorDeform::outDC;


void *DCVectorDeform::creator()
{
    return ::dommy::maya::createObject("DCVectorDeform", [&]() {
        return new DCVectorDeform();
    });
}




MStatus DCVectorDeform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
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
    if (weightAttribute.empty()) weightAttribute = "weight";
    if (vectorType < 0 || vectorType > 1 || !std::isfinite(magnitude) ||
        !std::isfinite(weight) || !std::isfinite(base[0]) ||
        !std::isfinite(base[1]) || !std::isfinite(base[2])) return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*input));
    for (auto curve = outputCurves->beginCurves(); curve != outputCurves->endCurves(); ++curve) {
        DPointList points = (*curve)->getPositionConst();
        const std::size_t count = points.size();
        if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        std::vector<Imath::V3d> vectors(count, base);
        std::vector<double> weights(count, 1.0);
        if (vectorType == 1 && (*curve)->isExistAttr(vectorAttribute) &&
            !dommy::attribute::convertVector3Attribute(
                (*curve)->getAttr(vectorAttribute), count, vectors)) return MS::kInvalidParameter;
        if ((*curve)->isExistAttr(weightAttribute) &&
            !dommy::attribute::convertScalarAttribute(
                (*curve)->getAttr(weightAttribute), count, weights)) return MS::kInvalidParameter;
        std::atomic<bool> valid(true);
        // Workers touch detached C++ arrays only, never Maya objects or attributes.
        const auto deform = [&](const tbb::blocked_range<std::size_t>& range) {
            for (std::size_t i = range.begin(); i != range.end(); ++i) {
                if (!dommy::point::offsetAlongVector(points[i], vectors[i],
                        magnitude, weights[i], weight, points[i]))
                    valid.store(false, std::memory_order_relaxed);
            }
        };
        const tbb::blocked_range<std::size_t> range(0U, count, 4096U);
        if (count < 4096U) deform(range);
        else tbb::parallel_for(range, deform);
        if (!valid.load(std::memory_order_relaxed)) return MS::kInvalidParameter;
        if (!(*curve)->replacePositions(points)) return MS::kFailure;
    }
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCVectorDeform")



MStatus DCVectorDeform::initialize()
try {
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    
    
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
    
    
    
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
   

    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDC, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorType, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBaseVector, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorAttributeName, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aMagnitude, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttributeName, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDC ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCVectorDeform", "initialize", std::current_exception());
}

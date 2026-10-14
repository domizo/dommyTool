//
//  DMVectorDeform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/05.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMVectorDeform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointDeformMath.h"

#include <cmath>
#include <vector>
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"






#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"




const MTypeId DMVectorDeform::typeId( ID_DM_VECTOR_DEFORM );


MObject DMVectorDeform::inDM;

MObject DMVectorDeform::aVectorType;
MObject DMVectorDeform::aBaseVector;
MObject DMVectorDeform::aVectorAttributeName;

MObject DMVectorDeform::aMagnitude;

MObject DMVectorDeform::aWeightAttributeName;
MObject DMVectorDeform::aWeight;


MObject DMVectorDeform::outDM;


void *DMVectorDeform::creator()
{
    return ::dommy::maya::createObject("DMVectorDeform", [&]() {
        return new DMVectorDeform();
    });
}




MStatus DMVectorDeform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    MStatus status = getValidInputDM(inDM, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle handle = data.inputValue(aVectorType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short vectorType = handle.asShort();
    handle = data.inputValue(aBaseVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector base = handle.asVector();
    handle = data.inputValue(aVectorAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string vectorAttribute = handle.asString().asChar();
    handle = data.inputValue(aMagnitude, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double magnitude = handle.asDouble();
    handle = data.inputValue(aWeightAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string weightAttribute = handle.asString().asChar();
    handle = data.inputValue(aWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double weight = handle.asDouble();
    if (vectorType < 0 || vectorType > 2 || weightAttribute.empty() ||
        !dommy::maya::isFiniteVector(base) || !std::isfinite(magnitude) ||
        !std::isfinite(weight)) return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DMeshData* outputMeshes = nullptr;
    status = dommy::maya::createPluginData(pluginData, DMeshData::id, outputMeshes);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*input));
    for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes(); ++mesh) {
        DPointList points = (*mesh)->getPositionConst();
        const std::size_t count = points.size();
        if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        std::vector<Imath::V3d> vectors(count, Imath::V3d(base[0], base[1], base[2]));
        std::vector<double> weights(count, 1.0);
        if (vectorType == 0 && count != 0U) {
            MObject object;
            status = generateMMeshFromDmesh(**mesh, object, DMeshPointSpace::Local);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnMesh function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            for (std::size_t i = 0U; i < count; ++i) {
                MVector normal;
                status = function.getVertexNormal(static_cast<int>(i), normal, MSpace::kObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
                vectors[i] = Imath::V3d(normal[0], normal[1], normal[2]);
            }
        } else if (vectorType == 2 && (*mesh)->isExistAttr(vectorAttribute)) {
            if (!dommy::attribute::convertVector3Attribute(
                    (*mesh)->getAttr(vectorAttribute), count, vectors)) return MS::kInvalidParameter;
        }
        if ((*mesh)->isExistAttr(weightAttribute) &&
            !dommy::attribute::convertScalarAttribute(
                (*mesh)->getAttr(weightAttribute), count, weights)) return MS::kInvalidParameter;
        for (std::size_t i = 0U; i < count; ++i) {
            if (!dommy::point::offsetAlongVector(points[i], vectors[i],
                    magnitude, weights[i], weight, points[i])) return MS::kInvalidParameter;
        }
        if (!(*mesh)->replacePositions(points)) return MS::kFailure;
    }
    outputMeshes->updateBBox();
    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMVectorDeform")





MStatus DMVectorDeform::initialize() try
{
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute vecEnum;
    aVectorType = vecEnum.create("VectorType", "vty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(vecEnum.addField( "Mesh Normal", 0 ));
    DOMMY_RETURN_IF_FAILURE(vecEnum.addField( "Base Vector", 1 ));
    DOMMY_RETURN_IF_FAILURE(vecEnum.addField( "From Attribute", 2 ));
    DOMMY_RETURN_IF_FAILURE(vecEnum.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));

    aBaseVector = nAttr.create( "BaseVector", "bprm", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,1.0,0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseVector));

    const MObject vectorDefault = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttributeName = tAttr.create("VectorAttributeName", "vattrn", MFnData::kString, vectorDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttributeName));

    aMagnitude = nAttr.create( "Magnitude", "mg", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnitude));

    const MObject weightDefault = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttributeName = tAttr.create("WeightAttributeName", "weiattrn", MFnData::kString, weightDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttributeName));
    
    aWeight = nAttr.create( "Weight", "wei", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDM, outDM ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorType, outDM ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBaseVector, outDM ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aVectorAttributeName, outDM ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aMagnitude, outDM ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttributeName, outDM ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDM ) );

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMVectorDeform", "initialize", std::current_exception());
}

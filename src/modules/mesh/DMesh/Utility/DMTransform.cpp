//
//  DMTransform.cpp
//  dommyTool
//
//  Created by yuzo on 5/3/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"

#include "dommyMath.h"


const MTypeId DMTransform::typeId( ID_DM_TRANSFORM );


MObject DMTransform::inDM;

MObject DMTransform::aAxisType;
MObject DMTransform::aAxisVectorAttribute;

MObject DMTransform::aTranslate;
MObject DMTransform::aRotate;
MObject DMTransform::aScale;
MObject DMTransform::aUniformScale;


MObject DMTransform::aTranslateWeight;
MObject DMTransform::aTranslateWeightAttribute;

MObject DMTransform::aRotateWeight;
MObject DMTransform::aRotateWeightAttribute;

MObject DMTransform::aScaleWeight;
MObject DMTransform::aScaleWeightAttribute;

MObject DMTransform::aWeight;
MObject DMTransform::aWeightAttribute;

MObject DMTransform::outDM;



void *DMTransform::creator()
{
    return ::dommy::maya::createObject("DMTransform", [&]() {
        return new DMTransform();
    });
}



MStatus DMTransform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    const std::size_t count = input->numberOfMesh();
    if (!dommy::attribute::validArrayConversionBudget(count,
            sizeof(MMatrix) + 4U * sizeof(double) + sizeof(Imath::V3d)))
        return MS::kInvalidParameter;

    auto readVector = [&](const MObject& attribute, MVector& value) -> MStatus {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        value = handle.asVector();
        return dommy::maya::isFiniteVector(value) ? MS::kSuccess : MS::kInvalidParameter;
    };
    auto readDouble = [&](const MObject& attribute, double& value) -> MStatus {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        value = handle.asDouble();
        return std::isfinite(value) ? MS::kSuccess : MS::kInvalidParameter;
    };
    auto readName = [&](const MObject& attribute, std::string& value) -> MStatus {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        value = handle.asString().asChar();
        return value.empty() || dommy::attribute::validAttributeName(value)
            ? MS::kSuccess : MS::kInvalidParameter;
    };
    MVector translate, rotate, scale;
    double uniformScale = 0.0;
    DOMMY_RETURN_IF_FAILURE(readVector(aTranslate, translate));
    DOMMY_RETURN_IF_FAILURE(readVector(aRotate, rotate));
    DOMMY_RETURN_IF_FAILURE(readVector(aScale, scale));
    DOMMY_RETURN_IF_FAILURE(readDouble(aUniformScale, uniformScale));
    MStatus status;
    const MDataHandle axisHandle = data.inputValue(aAxisType, &status);
    if (!status) return status;
    const short axisType = axisHandle.asShort();
    if (axisType < 0 || axisType > 1) return MS::kInvalidParameter;

    // Preserve the historical multiplicative weight semantics and defaults.
    auto loadWeight = [&](const MObject& control, const MObject& nameAttribute,
                          std::vector<double>& values) -> MStatus {
        double factor = 0.0;
        std::string name;
        DOMMY_RETURN_IF_FAILURE(readDouble(control, factor));
        DOMMY_RETURN_IF_FAILURE(readName(nameAttribute, name));
        values.assign(count, 1.0);
        const auto attribute = input->getAttr(name);
        if (attribute && !dommy::attribute::convertScalarAttribute(attribute, count, values))
            return MS::kInvalidParameter;
        for (double& value : values)
            if (!dommy::numeric::checkedNumericCast(
                    static_cast<long double>(value) * factor, value))
                return MS::kInvalidParameter;
        return MS::kSuccess;
    };
    std::vector<double> translateWeights, rotateWeights, scaleWeights, weights;
    DOMMY_RETURN_IF_FAILURE(loadWeight(aTranslateWeight, aTranslateWeightAttribute, translateWeights));
    DOMMY_RETURN_IF_FAILURE(loadWeight(aRotateWeight, aRotateWeightAttribute, rotateWeights));
    DOMMY_RETURN_IF_FAILURE(loadWeight(aScaleWeight, aScaleWeightAttribute, scaleWeights));
    DOMMY_RETURN_IF_FAILURE(loadWeight(aWeight, aWeightAttribute, weights));
    std::vector<Imath::V3d> axes(count, Imath::V3d(0.0, 1.0, 0.0));
    if (axisType == 1) {
        std::string name;
        DOMMY_RETURN_IF_FAILURE(readName(aAxisVectorAttribute, name));
        const auto attribute = input->getAttr(name);
        if (attribute && !dommy::attribute::convertVector3Attribute(attribute, count, axes))
            return MS::kInvalidParameter;
    }

    std::vector<MMatrix> transforms(count);
    std::size_t index = 0U, work = 0U;
    for (auto it = input->beginMeshes(); it != input->endMeshes(); ++it, ++index) {
        if (!*it || index >= count || !dommy::generation::checkedSum(work,
                (*it)->numberOfPoints(), dommy::generation::kMaxWorkItemCount, work))
            return MS::kInvalidParameter;
        const MVector center = (*it)->getCenterDouble();
        double scaling[3], rotation[3];
        MVector translation;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::checkedNumericCast(static_cast<long double>(scale[axis]) *
                    uniformScale * scaleWeights[index] * weights[index], scaling[axis]) ||
                !dommy::numeric::checkedNumericCast(
                    static_cast<long double>(dommy::math::degreesToRadians(rotate[axis])) *
                    rotateWeights[index] * weights[index], rotation[axis]) ||
                !dommy::numeric::checkedNumericCast(static_cast<long double>(translate[axis]) *
                    translateWeights[index] * weights[index], translation[axis]))
                return MS::kInvalidParameter;
        }
        MTransformationMatrix operation;
        DOMMY_RETURN_IF_FAILURE(operation.addScale(scaling, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(operation.addRotation(rotation, MTransformationMatrix::kZYX, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(operation.addTranslation(translation, MSpace::kObject));
        MMatrix worldOperation = operation.asMatrix();
        if (axisType == 1) {
            const Imath::V3d& axis = axes[index];
            const double magnitude = std::max(std::fabs(axis[0]),
                std::max(std::fabs(axis[1]), std::fabs(axis[2])));
            if (!std::isfinite(magnitude) || magnitude == 0.0) return MS::kInvalidParameter;
            MVector direction;
            if (!dommy::maya::normalizedVector(
                    MVector(axis[0] / magnitude, axis[1] / magnitude, axis[2] / magnitude), direction))
                return MS::kInvalidParameter;
            const MMatrix frame = MVector(0.0, 1.0, 0.0).rotateTo(direction).asMatrix();
            // A rotation frame's inverse is its transpose; never invert the primitive.
            worldOperation = frame.transpose() * worldOperation * frame;
        }
        dommy::math::Bounds3d bounds;
        if (!dommy::numeric::composeWorldPivotTransform(
                (*it)->getTransformMatrix(), center, worldOperation, transforms[index]) ||
            !dommy::numeric::appendTransformedBounds(
                *(*it)->getPositionPtrConst(), transforms[index], bounds))
            return MS::kInvalidParameter;
    }
    if (index != count) return MS::kFailure;

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DMeshData>(
        data, outDM, DMeshData::id, [&](DMeshData& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
            std::size_t index = 0U;
            for (auto it = output.beginMeshes(); it != output.endMeshes(); ++it, ++index) {
                if (!*it || index >= transforms.size()) return MS::kFailure;
                (*it)->setTransformMatrix(transforms[index]);
            }
            if (index != transforms.size()) return MS::kFailure;
            output.updateBBox();
            return MS::kSuccess;
        }));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMTransform")



			
			



MStatus DMTransform::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute axEnumFn;
    aAxisType = axEnumFn.create( "AxisType", "axty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(axEnumFn.addField( "World", 0 ));
    DOMMY_RETURN_IF_FAILURE(axEnumFn.addField( "Vector Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(axEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisType));

    const MObject aAxisVectorAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAxisVectorAttribute = tAttr.create("AxisVectorAttribute", "axvecattr", MFnData::kString, aAxisVectorAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisVectorAttribute));

    aTranslate = nAttr.create( "Translate", "t", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslate));

    aRotate = nAttr.create( "Rotate", "rot", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotate));

    aScale = nAttr.create( "Scale", "scl", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aUniformScale = nAttr.create( "UniformScale", "uscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));

    aTranslateWeight = nAttr.create( "TranslateWeight", "twgt", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateWeight));

    const MObject aTranslateWeightAttributeDefault = fnStringData.create("tw", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTranslateWeightAttribute = tAttr.create("TranslateWeightAttribute", "twgtattr", MFnData::kString, aTranslateWeightAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateWeightAttribute));

    aRotateWeight = nAttr.create( "RotateWeight", "rwgt", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateWeight));

    const MObject aRotateWeightAttributeDefault = fnStringData.create("rw", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRotateWeightAttribute = tAttr.create("RotateWeightAttribute", "rwgtattr", MFnData::kString, aRotateWeightAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateWeightAttribute));

    aScaleWeight = nAttr.create( "ScaleWeight", "swgt", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScaleWeight));

    const MObject aScaleWeightAttributeDefault = fnStringData.create("sw", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aScaleWeightAttribute = tAttr.create("ScaleWeightAttribute", "swgtattr", MFnData::kString, aScaleWeightAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScaleWeightAttribute));

    aWeight = nAttr.create( "Weight", "wgt", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    const MObject aWeightAttributeDefault = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttribute = tAttr.create("WeightAttribute", "wgtattr", MFnData::kString, aWeightAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttribute));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisVectorAttribute, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformScale, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateWeight, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateWeightAttribute, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateWeight, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateWeightAttribute, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScaleWeight, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScaleWeightAttribute, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWeight, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWeightAttribute, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMTransform", "initialize", std::current_exception());
}

//
//  DMCopyTransform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMCopyTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dmesh_ids.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMeshCopyBudget.h"

#include "dommyMath.h"

#include <cmath>
#include <cstddef>


const MTypeId DMCopyTransform::typeId( ID_DM_COPY_TRANSFORM );


MObject DMCopyTransform::inDM;

MObject DMCopyTransform::aCount;


MObject DMCopyTransform::aTranslate;
MObject DMCopyTransform::aRotate;
MObject DMCopyTransform::aScale;
MObject DMCopyTransform::aPivot;
MObject DMCopyTransform::aUniformScale;

MObject DMCopyTransform::aRotateOrder;

MObject DMCopyTransform::aFreezeTransform;


MObject DMCopyTransform::outDM;



void *DMCopyTransform::creator()
{
    return ::dommy::maya::createObject("DMCopyTransform", [&]() {
        return new DMCopyTransform();
    });
}


MStatus DMCopyTransform::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* inputDMData = nullptr;
    MStatus stat = getValidInputDM(inDM, data, inputDMData);
    if (!stat) return stat;
    if (!inputDMData) return MS::kFailure;
    const MDataHandle rawCountHandle = data.inputValue(aCount, &stat);
    if (!stat) return stat;
    const int rawCount = rawCountHandle.asInt();
    std::size_t count = 0U;
    if (!dommy::generation::checkedCount(rawCount,
            dommy::generation::kMaxGeneratedTransformCount, count)) return MS::kInvalidParameter;
    const std::size_t meshCount = inputDMData->numberOfMesh();
    dommy::mesh::MeshMergePlan sourcePlan, outputPlan;
    if (!dommy::mesh::appendMeshRangePlan(inputDMData->beginMeshes(), inputDMData->endMeshes(), sourcePlan) ||
        !dommy::mesh::repeatMeshMergePlan(sourcePlan, count == 0U ? 1U : count, outputPlan) ||
        !dommy::mesh::validMeshOperationStorage(outputPlan))
        return MS::kInvalidParameter;

    // Admit repeated metadata before the plugin factory or any payload clone.
    const std::size_t copies = count == 0U ? 1U : count;
    if (!dommy::mesh::validateRepeatedMeshMetadata(*inputDMData, copies, outputPlan))
        return MS::kInvalidParameter;
    DMeshData::MeshCopyOperation operation;
    if (meshCount != 0U && count != 0U) {
        const MDataHandle transVecHandle = data.inputValue(aTranslate, &stat);
        if (!stat) return stat;
        const MVector transVec = transVecHandle.asVector();
        const MDataHandle rotVecHandle = data.inputValue(aRotate, &stat);
        if (!stat) return stat;
        const MVector rotVec = rotVecHandle.asVector();
        const MDataHandle scaleVecHandle = data.inputValue(aScale, &stat);
        if (!stat) return stat;
        const MVector scaleVec = scaleVecHandle.asVector();
        const MDataHandle pivotVecHandle = data.inputValue(aPivot, &stat);
        if (!stat) return stat;
        const MVector pivotVec = pivotVecHandle.asVector();
        const MDataHandle uniScaleHandle = data.inputValue(aUniformScale, &stat);
        if (!stat) return stat;
        const double uniScale = uniScaleHandle.asDouble();
        const MDataHandle freezeHandle = data.inputValue(aFreezeTransform, &stat);
        if (!stat) return stat;
        const bool freeze = freezeHandle.asBool();
        if (!dommy::maya::isFiniteVector(transVec) ||
            !dommy::maya::isFiniteVector(rotVec) ||
            !dommy::maya::isFiniteVector(scaleVec) ||
            !dommy::maya::isFiniteVector(pivotVec) ||
            !std::isfinite(uniScale)) {
            return MS::kFailure;
        }

        const MDataHandle rotateOrderTypeHandle = data.inputValue(aRotateOrder, &stat);
        if (!stat) return stat;
        const short rotateOrderType = rotateOrderTypeHandle.asShort();
        MTransformationMatrix::RotationOrder rotOrder;
        switch ( rotateOrderType ) {
            case ROTATE_ORDER_XYZ:
                rotOrder = MTransformationMatrix::kXYZ; break;
            case ROTATE_ORDER_YZX:
                rotOrder = MTransformationMatrix::kYZX; break;
            case ROTATE_ORDER_ZXY:
                rotOrder = MTransformationMatrix::kZXY; break;
            case ROTATE_ORDER_XZY:
                rotOrder = MTransformationMatrix::kXZY; break;
            case ROTATE_ORDER_YXZ:
                rotOrder = MTransformationMatrix::kYXZ; break;
            case ROTATE_ORDER_ZYX:
                rotOrder = MTransformationMatrix::kZYX; break;
            default:
                return MS::kFailure;
        }


        MVector scale(scaleVec.x*uniScale, scaleVec.y*uniScale, scaleVec.z*uniScale);
        if (!dommy::maya::isFiniteVector(scale)) {
            return MS::kFailure;
        }

        MVector prescale(1.0, 1.0, 1.0);
        operation = [=](DMeshInternalData& copied, std::size_t i, std::size_t index) mutable -> MStatus {
            if (index == 0U)
                prescale = MVector(prescale[0]*scale[0], prescale[1]*scale[1], prescale[2]*scale[2]);
            const MVector movVec = transVec * static_cast<double>(i);
            const MVector pivot = movVec + pivotVec;
            const double3 finRotVal = {
                dommy::math::degreesToRadians(rotVec[0])*static_cast<double>(i),
                dommy::math::degreesToRadians(rotVec[1])*static_cast<double>(i),
                dommy::math::degreesToRadians(rotVec[2])*static_cast<double>(i)};
            if (!dommy::maya::isFiniteVector(prescale) || !dommy::maya::isFiniteVector(movVec) ||
                !dommy::maya::isFiniteVector(pivot) ||
                !std::isfinite(finRotVal[0]) || !std::isfinite(finRotVal[1]) || !std::isfinite(finRotVal[2]))
                return MS::kInvalidParameter;
            MTransformationMatrix transMat(copied.getTransformMatrix());
            MTransformationMatrix newMat;
            DOMMY_RETURN_IF_FAILURE(newMat.setRotatePivot(pivot, MSpace::kObject, false));
            DOMMY_RETURN_IF_FAILURE(newMat.setScalePivot(pivot, MSpace::kObject, false));
            const double3 finScaleVal = {prescale.x, prescale.y, prescale.z};
            DOMMY_RETURN_IF_FAILURE(newMat.addScale(finScaleVal, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(newMat.addRotation(finRotVal, rotOrder, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(newMat.addTranslation(movVec, MSpace::kObject));
            const MMatrix finMat = transMat.asMatrix() * newMat.asMatrix();
            if (!dommy::maya::isFiniteMatrix(finMat)) return MS::kInvalidParameter;
            if (freeze) {
                DOMMY_RETURN_IF_FAILURE(copied.bakeTransform(finMat));
            } else {
                copied.setTransformMatrix(finMat);
            }
            return MS::kSuccess;
        };
    }
    MFnPluginData pluginData;
    DMeshData* outDMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, outDMeshes));
    if (!outDMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outDMeshes->copyRepeatedFrom(*inputDMData, copies, operation));
    MDataHandle output = data.outputValue(outDM, &stat);
    if (!stat) return stat;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMCopyTransform")
            
            
            
            


MStatus DMCopyTransform::initialize() try
{
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aCount = nAttr.create( "Count", "cnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

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

    aPivot = nAttr.create( "Pivot", "pvt", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPivot));

    aUniformScale = nAttr.create( "UniformScale", "uscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));

    MFnEnumAttribute rotEnumAttr;
    aRotateOrder = rotEnumAttr.create( "RotateOrder", "ro", ROTATE_ORDER_XYZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xyz", ROTATE_ORDER_XYZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yzx", ROTATE_ORDER_YZX));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zxy", ROTATE_ORDER_ZXY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xzy", ROTATE_ORDER_XZY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yxz", ROTATE_ORDER_YXZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zyx", ROTATE_ORDER_ZYX));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateOrder));

    aFreezeTransform = nAttr.create( "FreezeTransform", "frztrans", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFreezeTransform));

    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCount, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformScale, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateOrder, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFreezeTransform, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPivot, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMCopyTransform", "initialize", std::current_exception());
}

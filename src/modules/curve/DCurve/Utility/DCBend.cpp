//
//  DCBend.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/17.
//

#include "DCBend.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveUtil.h"
#include "DPAttributeCommon.h"
#include "dommyMath.h"
#include "DPoint/math/DVectorMath.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DCurveBendPlan.h"

#include <cmath>




const MTypeId DCBend::typeId( ID_DC_BEND );

MObject DCBend::inDC;

MObject DCBend::aBend;

MObject DCBend::aBendVectorType;
MObject DCBend::aRotateVector;
MObject DCBend::aRotateVectorAttribute;
MObject DCBend::aReverse;

MObject DCBend::aWeightAttriute;
MObject DCBend::aWeight;

MObject DCBend::outDC;


void *DCBend::creator()
{
    return ::dommy::maya::createObject("DCBend", [&]() {
        return new DCBend();
    });
}




MStatus DCBend::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    if (!input) return MS::kInvalidParameter;

    MStatus status;
    MDataHandle handle = data.inputValue(aBend, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double bend = handle.asDouble();
    handle = data.inputValue(aBendVectorType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short angleType = handle.asShort();
    handle = data.inputValue(aRotateVector, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector rotateVector = handle.asVector();
    handle = data.inputValue(aRotateVectorAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string axisAttribute = handle.asString().asChar();
    handle = data.inputValue(aReverse, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool reverse = handle.asBool();
    handle = data.inputValue(aWeightAttriute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string weightAttribute = handle.asString().asChar();
    handle = data.inputValue(aWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double weight = handle.asDouble();
    const double angle = dommy::math::degreesToRadians(bend) * weight;
    if (angleType < 0 || angleType > 2 || !std::isfinite(bend) ||
        !std::isfinite(weight) || !std::isfinite(angle) ||
        !dommy::maya::isFiniteVector(rotateVector)) return MS::kInvalidParameter;

    const auto normalize = [](MVector& value) {
        MVector unit;
        if (!dommy::math::normalized3OrZero(value.x, value.y, value.z,
                unit.x, unit.y, unit.z, 1.0e-12)) return false;
        value = unit;
        return true;
    };

    std::size_t totalWork = 0U;
    if (angle != 0.0) {
        for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve) {
            if (!dommy::curve::appendBendWork((*curve)->numberOfPoints(), angleType == 0, totalWork)) {
                MGlobal::displayError("DCBend: curve deformation exceeds the safe work budget.");
                return MS::kInvalidParameter;
            }
        }
    }

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*input));

    // A global zero bend is a true no-op, including straight/degenerate normals.
    if (angle != 0.0) {
        for (auto curve = outputCurves->beginCurves();
             curve != outputCurves->endCurves(); ++curve) {
            const std::size_t pointCount = (*curve)->numberOfPoints();
            if (pointCount < 2U) return MS::kInvalidParameter;
            MPointArray points;
            status = dommy::geometry::copyPointsToMayaArray(
                *(*curve)->getPositionPtrConst(), points);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MVectorArray axes;
            status = dommy::maya::buildCheckedArray(pointCount,
                [&](unsigned int, MVector& value) {
                    value = rotateVector;
                    return MS::kSuccess;
                }, axes);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (angleType == 2 && (*curve)->isExistAttr(axisAttribute)) {
                if (!isVector3DAttrType((*curve)->getAttr(axisAttribute)->getAttrType()))
                    return MS::kInvalidParameter;
                getDCPAttributeMVectorArrayFromVec3(*curve, axisAttribute, axes);
            }
            MDoubleArray weights;
            status = dommy::maya::buildCheckedArray(pointCount,
                [](unsigned int, double& value) {
                    value = 1.0;
                    return MS::kSuccess;
                }, weights);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if ((*curve)->isExistAttr(weightAttribute))
                getDCPAttributeMDoubleArray(*curve, weightAttribute, weights);
            if (points.length() != pointCount || axes.length() != pointCount ||
                weights.length() != pointCount) return MS::kFailure;
            for (unsigned int index = 0U; index < points.length(); ++index) {
                if (!dommy::maya::isFloatRepresentablePoint(points[index]) ||
                    !std::isfinite(weights[index]) ||
                    !dommy::maya::isFiniteVector(axes[index])) return MS::kInvalidParameter;
            }

            MFnNurbsCurve curveFunction;
            MObject curveObject;
            if (angleType == 0) {
                status = generateMCurveFromDcurve(*curve, curveObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                status = curveFunction.setObject(curveObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
            }
            MVector previousAxis;
            for (std::size_t step = 0U; step + 1U < pointCount; ++step) {
                const unsigned int pivotIndex = static_cast<unsigned int>(
                    reverse ? pointCount - 1U - step : step);
                const MPoint pivot = points[pivotIndex];
                MVector axis;
                if (angleType == 0) {
                    // No initial parameter guess is available for this pivot.
                    double parameter = 0.0;
                    const MPoint closest = curveFunction.closestPoint(
                        pivot, &parameter, kMFnNurbsEpsilon, MSpace::kObject, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    if (!std::isfinite(parameter) || !dommy::maya::isFinitePoint(closest))
                        return MS::kInvalidParameter;
                    MVector tangent = curveFunction.tangent(parameter, MSpace::kObject, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    if (!normalize(tangent)) return MS::kInvalidParameter;
                    MVector normal;
                    if (step == 0U) {
                        normal = curveFunction.normal(parameter, MSpace::kObject, &status);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                    } else {
                        normal = previousAxis ^ tangent;
                    }
                    if (!normalize(normal)) return MS::kInvalidParameter;
                    axis = tangent ^ normal;
                } else {
                    axis = angleType == 1 ? rotateVector : axes[pivotIndex];
                }
                if (!normalize(axis)) return MS::kInvalidParameter;
                const double rotationAngle = angle * weights[pivotIndex];
                if (!std::isfinite(rotationAngle)) return MS::kInvalidParameter;
                MTransformationMatrix rotation;
                status = rotation.setRotatePivot(pivot, MSpace::kObject, true);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                status = rotation.setToRotationAxis(axis, rotationAngle);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                const MMatrix matrix = rotation.asMatrix();
                if (!dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
                for (std::size_t offset = step + 1U; offset < pointCount; ++offset) {
                    const unsigned int index = static_cast<unsigned int>(
                        reverse ? pointCount - 1U - offset : offset);
                    const MPoint transformed = points[index] * matrix;
                    if (!dommy::maya::isFloatRepresentablePoint(transformed))
                        return MS::kInvalidParameter;
                    points[index] = transformed;
                    if (angleType == 2) {
                        const MVector transformedAxis = axes[index].transformAsNormal(matrix);
                        if (!dommy::maya::isFiniteVector(transformedAxis))
                            return MS::kInvalidParameter;
                        axes[index] = transformedAxis;
                    }
                }
                if (angleType == 0) {
                    status = curveFunction.setCVs(points, MSpace::kObject);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    status = curveFunction.updateCurve();
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                }
                previousAxis = axis;
            }
            if (!(*curve)->replacePositions(points)) return MS::kFailure;
        }
    }
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCBend")
        





MStatus DCBend::initialize()
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
    
    
    
    aBend = nAttr.create( "Bend", "b", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( -180.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 180.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBend));
    
    
    MFnEnumAttribute dirEnumFn;
    aBendVectorType = dirEnumFn.create( "BendVectorType", "bvecty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField( "Curve Normal", 0 ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField( "Use Vector", 1 ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField( "From Attribute", 2 ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBendVectorType));
    
    
    aRotateVector = nAttr.create( "RotateVector", "rvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 1.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateVector));
    
    
    const MObject aRotateVectorAttributeDefaultData = fnStringData.create("ang", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRotateVectorAttribute = tAttr.create("RotateVectorAttribute", "rattrn", MFnData::kString, aRotateVectorAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateVectorAttribute));
    
    aReverse = nAttr.create( "Reverse", "rev", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aReverse));
    
    
    
    
    aWeight = nAttr.create( "Weight", "w", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));
    
    
    const MObject aWeightAttriuteDefaultData = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttriute = tAttr.create("WeightAttriute", "wattr", MFnData::kString, aWeightAttriuteDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttriute));
    
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDC, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBend, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBendVectorType, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRotateVector, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRotateVectorAttribute, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aReverse, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttriute, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDC ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCBend", "initialize", std::current_exception());
}

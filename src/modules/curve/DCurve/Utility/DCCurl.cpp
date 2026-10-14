//
//  DCCurl.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/18.
//

#include "DCCurl.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveDataHeaders.h"
#include "dommyMath.h"
#include <cmath>


const MTypeId DCCurl::typeId( ID_DC_CURL );

MObject DCCurl::inDC;

MObject DCCurl::aRadius;
MObject DCCurl::aRadiusAttribute;
MObject DCCurl::aArcStep;
MObject DCCurl::aArcStepAttribute;
MObject DCCurl::aSpiralOffset;
MObject DCCurl::aSpiralOffsetAttribute;


MObject DCCurl::aWeightAttriute;
MObject DCCurl::aWeight;

MObject DCCurl::outDC;


void *DCCurl::creator()
{
    return ::dommy::maya::createObject("DCCurl", [&]() {
        return new DCCurl();
    });
}



MStatus DCCurl::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aWeightAttriute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string weightAttribute = handle.asString().asChar();
    handle = data.inputValue(aWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double weight = handle.asDouble();
    handle = data.inputValue(aRadius, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double radius = handle.asDouble();
    handle = data.inputValue(aArcStep, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double arcStep = handle.asDouble();
    handle = data.inputValue(aSpiralOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double spiralOffset = handle.asDouble();
    handle = data.inputValue(aRadiusAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string radiusAttribute = handle.asString().asChar();
    handle = data.inputValue(aArcStepAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string arcAttribute = handle.asString().asChar();
    handle = data.inputValue(aSpiralOffsetAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string spiralAttribute = handle.asString().asChar();
    if (!std::isfinite(weight) || !std::isfinite(radius) ||
        !std::isfinite(arcStep) || !std::isfinite(spiralOffset))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*input));

    // Preserve the historical non-positive global weight no-op.
    if (weight > 0.0) {
        const std::size_t curveCount = outputCurves->numberOfCurves();
        const auto readCurveScalars = [&](const std::string& name, double fallback,
                                          std::vector<double>& values) {
            values.assign(curveCount, fallback);
            if (input->isExistAttr(name)) {
                if (!dommy::attribute::convertScalarAttribute(input->getAttr(name), curveCount, values))
                    return false;
            }
            if (values.size() != curveCount) return false;
            for (double value : values) if (!std::isfinite(value)) return false;
            return true;
        };
        std::vector<double> radii, arcs, spirals;
        if (!readCurveScalars(radiusAttribute, radius, radii) ||
            !readCurveScalars(arcAttribute, arcStep, arcs) ||
            !readCurveScalars(spiralAttribute, spiralOffset, spirals))
            return MS::kInvalidParameter;

        std::size_t curveIndex = 0U;
        for (auto curve = outputCurves->beginCurves();
             curve != outputCurves->endCurves(); ++curve, ++curveIndex) {
            const std::size_t pointCount = (*curve)->numberOfPoints();
            std::vector<double> weights(pointCount, 1.0);
            if ((*curve)->isExistAttr(weightAttribute) &&
                !dommy::attribute::convertWeightAttribute(
                    (*curve)->getAttr(weightAttribute), pointCount, weights))
                return MS::kInvalidParameter;
            if (weights.size() != pointCount) return MS::kInvalidParameter;

            MPointArray points;
            status = dommy::geometry::copyPointsToMayaArray(
                *(*curve)->getPositionPtrConst(), points);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (points.length() != pointCount) return MS::kFailure;
            MObject curveObject;
            status = generateMCurveFromDcurve(*curve, curveObject);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnNurbsCurve curveFunction(curveObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MVector previousBinormal;
            MPointArray curledPoints;
            status = dommy::maya::buildCheckedArray(pointCount,
                [&](unsigned int i, MPoint& result) -> MStatus {
                    MVector tangent, normal, binormal;
                    MStatus frameStatus = dommy::curve::frameAtClosestPoint(
                        curveFunction, points[i], i == 0U ? nullptr : &previousBinormal,
                        kMFnNurbsEpsilon, MSpace::kObject, tangent, normal, binormal);
                    if (!frameStatus) return frameStatus;
                    const MQuaternion yRotation = MVector(0.0, 1.0, 0.0).rotateTo(tangent);
                    const MQuaternion xRotation =
                        MVector(1.0, 0.0, 0.0).rotateBy(yRotation).rotateTo(normal);
                    if (!dommy::maya::isFiniteQuaternion(xRotation) ||
                        !dommy::maya::isFiniteQuaternion(yRotation)) return MS::kInvalidParameter;
                    MTransformationMatrix transform;
                    frameStatus = transform.addRotationQuaternion(
                        xRotation.x, xRotation.y, xRotation.z, xRotation.w, MSpace::kObject);
                    if (!frameStatus) return frameStatus;
                    frameStatus = transform.addRotationQuaternion(
                        yRotation.x, yRotation.y, yRotation.z, yRotation.w, MSpace::kObject);
                    if (!frameStatus) return frameStatus;
                    frameStatus = transform.addTranslation(MVector(points[i]), MSpace::kWorld);
                    if (!frameStatus) return frameStatus;
                    const MMatrix matrix = transform.asMatrix();
                    const double localRadius = radii[curveIndex] * weight * weights[i];
                    const double radians = dommy::math::degreesToRadians(
                        arcs[curveIndex] * static_cast<double>(i) + spirals[curveIndex]);
                    if (!std::isfinite(localRadius) || !std::isfinite(radians) ||
                        !dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
                    const MPoint candidate =
                        MPoint(MVector(0.0, 0.0, localRadius).rotateBy(MVector::kYaxis, radians)) * matrix;
                    if (!dommy::maya::isFloatRepresentablePoint(candidate))
                        return MS::kInvalidParameter;
                    previousBinormal = binormal;
                    result = candidate;
                    return MS::kSuccess;
                }, curledPoints);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!(*curve)->replacePositions(curledPoints)) return MS::kFailure;
        }
    }
    outputCurves->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCCurl")


        



MStatus DCCurl::initialize()
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
    
    
    
    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 4.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 20.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    const MObject aRadiusAttributeDefaultData = fnStringData.create("radius", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "radattr", MFnData::kString, aRadiusAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
    
    
    aArcStep = nAttr.create( "ArcStep", "ars", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 45.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aArcStep));
    
    const MObject aArcStepAttributeDefaultData = fnStringData.create("arc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aArcStepAttribute = tAttr.create("ArcStepAttribute", "arcattr", MFnData::kString, aArcStepAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aArcStepAttribute));
    
    aSpiralOffset = nAttr.create( "SpiralOffset", "spo", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpiralOffset));
    
    const MObject aSpiralOffsetAttributeDefaultData = fnStringData.create("spiral", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSpiralOffsetAttribute = tAttr.create("SpiralOffsetAttribute", "sprlattr", MFnData::kString, aSpiralOffsetAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpiralOffsetAttribute));
    
    
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
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRadius, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aArcStep, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSpiralOffset, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRadiusAttribute, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aArcStepAttribute, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSpiralOffsetAttribute, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttriute, outDC ) );
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCCurl", "initialize", std::current_exception());
}

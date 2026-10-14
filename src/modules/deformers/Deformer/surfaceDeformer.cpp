#include "surfaceDeformer.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DMayaDeformerGeometry.h"

#include <maya/MPlugArray.h>
#include <maya/MItGeometry.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MMatrix.h>

#include <algorithm>
#include <cmath>

MTypeId surfaceDeformer::typeId(0x0034A);
MString surfaceDeformer::typeName("surfD");

// Attributes
MObject surfaceDeformer::posU;
MObject surfaceDeformer::posV;

MObject surfaceDeformer::offsetN;

MObject surfaceDeformer::scaleU;
MObject surfaceDeformer::scaleV;

MObject surfaceDeformer::scaleN;

MObject surfaceDeformer::minU;
MObject surfaceDeformer::maxU;
MObject surfaceDeformer::minV;
MObject surfaceDeformer::maxV;

MObject surfaceDeformer::baseSurface;

void* surfaceDeformer::creator() {
    return ::dommy::maya::createObject("surfaceDeformer", [&]() {
        return new surfaceDeformer();
    });
}

MStatus surfaceDeformer::deform(MDataBlock& block, MItGeometry& iter, const MMatrix& localToWorld,
                                unsigned int geomIndex) try {
    (void)localToWorld;
    MStatus stat;

    MDataHandle envHandle = block.inputValue(envelope, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0) // Deformer has no effect
        return MS::kSuccess;

    MDataHandle posUValHandle = block.inputValue(posU, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double posUVal = posUValHandle.asDouble();
    MDataHandle posVValHandle = block.inputValue(posV, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double posVVal = posVValHandle.asDouble();
    MDataHandle offsetNValHandle = block.inputValue(offsetN, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double offsetNVal = offsetNValHandle.asDouble();
    MDataHandle scaleUValHandle = block.inputValue(scaleU, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double scaleUVal = scaleUValHandle.asDouble();
    MDataHandle scaleVValHandle = block.inputValue(scaleV, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double scaleVVal = scaleVValHandle.asDouble();
    MDataHandle scaleNValHandle = block.inputValue(scaleN, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double scaleNVal = scaleNValHandle.asDouble();
    MDataHandle minUValHandle = block.inputValue(minU, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double minUVal = minUValHandle.asDouble();
    MDataHandle maxUValHandle = block.inputValue(maxU, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double maxUVal = maxUValHandle.asDouble();
    MDataHandle minVValHandle = block.inputValue(minV, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double minVVal = minVValHandle.asDouble();
    MDataHandle maxVValHandle = block.inputValue(maxV, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double maxVVal = maxVValHandle.asDouble();

    const double parameters[] = {posUVal,   posVVal, offsetNVal, scaleUVal, scaleVVal,
                                 scaleNVal, minUVal, maxUVal,    minVVal,   maxVVal};
    for (std::size_t index = 0; index < sizeof(parameters) / sizeof(parameters[0]); ++index) {
        if (!std::isfinite(parameters[index])) {
            return MS::kFailure;
        }
    }
    if (minUVal > maxUVal || minVVal > maxVVal) {
        return MS::kFailure;
    }

    MDataHandle baseSurfaceHnd = block.inputValue(baseSurface, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    if (baseSurfaceHnd.type() != MFnData::kNurbsSurface) {
        return MS::kSuccess;
    }

    MFnNurbsSurface surfaceFn(baseSurfaceHnd.data(), &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);

    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int pointIndex, const MPoint& pt, MPoint& result) -> MStatus {
            const float weight = weightValue(block, geomIndex, pointIndex);
            if (!std::isfinite(weight)) {
                return MS::kFailure;
            }
            if (weight == 0.0f)
                return MS::kSuccess;

            const double rawU = ((pt.x + 1.0) * 0.5) * scaleUVal + posUVal;
            const double rawV = ((pt.z + 1.0) * 0.5) * scaleVVal + posVVal;
            if (!std::isfinite(rawU) || !std::isfinite(rawV))
                return MS::kInvalidParameter;
            const double offsetU = std::max(minUVal, std::min(maxUVal, rawU));
            const double offsetV = std::max(minVVal, std::min(maxVVal, rawV));

            MPoint surfacePoint;
            stat = surfaceFn.getPointAtParam(offsetU, offsetV, surfacePoint, MSpace::kObject);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            MVector surfaceNormal = surfaceFn.normal(offsetU, offsetV, MSpace::kObject, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!dommy::deformer::normalizeVector(surfaceNormal))
                return MS::kInvalidParameter;
            if (surfaceNormal.length() <= 1.0e-12) {
                return MS::kFailure;
            }
            MPoint target;
            if (!dommy::deformer::offsetPoint(surfacePoint, surfaceNormal, pt.y + offsetNVal,
                                              scaleNVal, 1.0, 1.0, 1.0, target))
                return MS::kInvalidParameter;
            return dommy::deformer::blendPoint(pt, target, double(weight) * double(env), result)
                       ? MS::kSuccess
                       : MS::kInvalidParameter;
        });
}
DOMMY_DEFORMER_CATCH("surfaceDeformer")

MStatus surfaceDeformer::initialize() try {
    MStatus status;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	
	posU = nAttr.create( "posU", "pu", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	posV = nAttr.create( "posV", "pv", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	offsetN = nAttr.create( "offsetN", "on", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	scaleU = nAttr.create( "scaleU", "su", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	scaleV = nAttr.create( "scaleV", "sv", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	scaleN = nAttr.create( "scaleN", "sn", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	minU = nAttr.create( "minU", "miu", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	maxU = nAttr.create( "maxU", "mau", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	minV = nAttr.create( "minV", "miv", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	maxV = nAttr.create( "maxV", "mav", MFnNumericData::kDouble , 0, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	
	
	
	
	
	
	
	baseSurface = gAttr.create( "baseSurface", "bs" , &status);
	DOMMY_RETURN_IF_FAILURE(status);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden( true ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( posU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( posV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( offsetN ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleV ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleN ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( minU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( maxU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( minV ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( maxV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( baseSurface ));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( posU, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( posV, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( offsetN, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( scaleU, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( scaleV, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( scaleN, outputGeom ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( minU, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( maxU, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( minV, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( maxV, outputGeom ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( baseSurface, outputGeom ));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "surfaceDeformer", "initialize", std::current_exception());
}

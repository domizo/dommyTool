


#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"


#include "DMayaHeaders.h"

#include "dommyMath.h"

#include "DSpiralCurve.h"
#include "DSpiralCurvePlan.h"

#include <cmath>
#include <cstddef>
#include <iostream>


const MTypeId DSpiralCurve::typeId( 0x0045A );

// Attributes
MObject DSpiralCurve::inputCurve;


MObject DSpiralCurve::inputTangentCurve;

MObject DSpiralCurve::curveLength;
MObject DSpiralCurve::sampling;
MObject DSpiralCurve::offset;
MObject DSpiralCurve::spiralOffset;

MObject DSpiralCurve::radius;
MObject DSpiralCurve::arcStep;

MObject DSpiralCurve::outputCurve;



void *DSpiralCurve::creator()
{
    return ::dommy::maya::createObject("DSpiralCurve", [&]() {
        return new DSpiralCurve();
    });
}


MStatus DSpiralCurve::compute( const MPlug& plug, MDataBlock& data ) try
{
	MStatus stat;

    
    
	if ( plug == outputCurve )
    {
        
        MDataHandle control = data.inputValue(curveLength, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double curveLengthVal = control.asDouble();
		control = data.inputValue(sampling, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		const int rawSampling = control.asInt();
        control = data.inputValue(offset, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double offsetVal = control.asDouble();
        control = data.inputValue(spiralOffset, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double spiralOffsetVal = control.asDouble();
        control = data.inputValue(radius, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double radiusVal = control.asDouble();
        control = data.inputValue(arcStep, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double arcStepVal = control.asDouble();

		dommy::curve::SpiralCurvePlan plan;
		if (!dommy::curve::buildSpiralCurvePlan(rawSampling, plan) || !std::isfinite(curveLengthVal) ||
			curveLengthVal < 0.0 || !std::isfinite(offsetVal) ||
			!std::isfinite(spiralOffsetVal) || !std::isfinite(radiusVal) ||
			radiusVal < 0.0 || !std::isfinite(arcStepVal)) {
			MGlobal::displayError("DSpiralCurve: invalid or excessive input value.");
			return MS::kInvalidParameter;
		}
		const std::size_t totalSampling = plan.samples;
		
        
        
        
        
#pragma mark Get input object 入力オブジェクトの取得
        MDataHandle dataHnd = data.inputValue(inputCurve,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        MPlug inCrvPlug(thisMObject(), inputCurve);
        const bool inputConnected = inCrvPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!inputConnected) {
            MGlobal::displayWarning("DSpiralCurve :Input curve is not connected.");
            return MS::kFailure;
        } else if (dataHnd.type() != MFnData::kNurbsCurve) {
            MGlobal::displayWarning("DSpiralCurve :Input is not curve.");
            return MS::kFailure;
        }
        
        MObject crvObj = dataHnd.data();
        
        MFnNurbsCurve crvFn;
		DOMMY_RETURN_IF_FAILURE(crvFn.setObject(crvObj));
        
        
#pragma mark Get input object 入力オブジェクトの取得
        MDataHandle tanHnd = data.inputValue(inputTangentCurve,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        MPlug tangentPlug(thisMObject(), inputTangentCurve);
        
        MFnNurbsCurve tanCrvFn;
        bool useTangent = false;
        const bool tangentConnected = tangentPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (tangentConnected) {
            if (tanHnd.type() != MFnData::kNurbsCurve) {
                MGlobal::displayWarning("DSpiralCurve :Input tangent is not curve.");
                return MS::kFailure;
            }
            
            
            
			DOMMY_RETURN_IF_FAILURE(tanCrvFn.setObject(tanHnd.data()));
            
            useTangent =true;
        }
        
        
        
        
        
        
        
        
        
        //Get curve info
        double start, end;
        stat = crvFn.getKnotDomain (start, end);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double length = crvFn.length(kMFnNurbsEpsilon, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        length *= curveLengthVal;
		if (!std::isfinite(start) || !std::isfinite(end) || start > end ||
			!std::isfinite(length) || length < 0.0) {
			MGlobal::displayError("DSpiralCurve: curve domain or length is invalid.");
			return MS::kInvalidParameter;
		}
        
        
        
        
        
        
        
		const double stepLength =
			length / static_cast<double>(totalSampling - 1U);
        
        
        
        
        MVector oldBiNormal;
        
        //create transform info
        MQuaternion xQuat,yQuat;//,zQuat;
        MVector xAxisVec(1.0,0.0,0.0);
        MVector yAxisVec(0.0,1.0,0.0);
        
        std::vector<MQuaternion> xQuatArray;
        std::vector<MQuaternion> yQuatArray;
        std::vector<MPoint> dupPointArray;
		xQuatArray.reserve(totalSampling);
		yQuatArray.reserve(totalSampling);
		dupPointArray.reserve(totalSampling);
        
//        //twist and width
//        MFloatArray twistValueArray,tubeWidthValueArray;
//        twistRampAttr.sampleValueRamp(totalSampling, twistValueArray,&stat);
//        CHECK_MSTATUS(stat);
//        tubeWidthRampAttr.sampleValueRamp(totalSampling, tubeWidthValueArray,&stat);
//        CHECK_MSTATUS(stat);
        
        
        
		for (std::size_t c=0U; c<totalSampling; ++c) {
            MPoint cv;
			double curveParam = crvFn.findParamFromLength(
				stepLength * static_cast<double>(c), &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            double offsetedParam = curveParam+offsetVal;
            
            offsetedParam = offsetedParam<start?start:offsetedParam;
            offsetedParam = offsetedParam>end?end:offsetedParam;
            
            
			DOMMY_RETURN_IF_FAILURE(
				crvFn.getPointAtParam(offsetedParam, cv, MSpace::kWorld));
			if (!dommy::maya::isFinitePoint(cv)) return MS::kFailure;
            
            MVector	normal;
            MVector	tangent = crvFn.tangent(offsetedParam , MSpace::kWorld , &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            if (useTangent) {
                MPoint targetCV;
				DOMMY_RETURN_IF_FAILURE(
					tanCrvFn.getPointAtParam(
						offsetedParam, targetCV, MSpace::kWorld));
                normal = targetCV-cv;
            } else {
                if (c==0) {
					normal= crvFn.normal(
						offsetedParam, MSpace::kWorld, &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
                } else {
                    normal = oldBiNormal ^ tangent;
                }
            }
            
			MVector normalizedTangent;
			if (!dommy::maya::normalizedVector(tangent, normalizedTangent)) {
				MGlobal::displayError("DSpiralCurve: curve tangent is degenerate.");
				return MS::kFailure;
			}
			const MVector orthogonalNormal =
				normal - normalizedTangent * (normal * normalizedTangent);
			MVector normalizedNormal;
			if (!dommy::maya::normalizedVector(
					orthogonalNormal, normalizedNormal)) {
				MGlobal::displayError("DSpiralCurve: orientation is degenerate.");
				return MS::kFailure;
			}
			MVector biNormal;
			if (!dommy::maya::normalizedVector(
					normalizedTangent ^ normalizedNormal, biNormal)) {
				return MS::kFailure;
			}
            
			yQuat = yAxisVec.rotateTo(normalizedTangent);
            MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
			xQuat = xPostPosVec.rotateTo(normalizedNormal);
            
//            xQuatArray[c] = xQuat;
//            yQuatArray[c] = yQuat;
//            dupPointArray[c] = cv;
            
            xQuatArray.push_back(xQuat);
            yQuatArray.push_back(yQuat);
            dupPointArray.push_back(cv);
            
            oldBiNormal = biNormal;
        }
        
        
        
        
//        if (VERBOSE) cout << "twistValueArray:" << twistValueArray.length() <<  endl;
        
        
        const MVector basePt(0.0,0.0,1.0*radiusVal);
        
        MPointArray cvs;
        MDoubleArray kv;
        
		for (std::size_t c=0U; c<totalSampling; ++c) {
            
			MVector newPt = basePt.rotateBy(
				MVector::kYaxis,
				dommy::math::degreesToRadians(arcStepVal * static_cast<double>(c) + spiralOffsetVal));
//            if (VERBOSE) cout << "newPt:" << newPt <<  endl;
            
            newPt = newPt.rotateBy(yQuatArray[c]).rotateBy(xQuatArray[c]);
            MPoint finPt(newPt.x+dupPointArray[c].x, newPt.y+dupPointArray[c].y, newPt.z+dupPointArray[c].z);
			if (!dommy::maya::isFinitePoint(finPt)) {
				MGlobal::displayError("DSpiralCurve: generated point is non-finite.");
				return MS::kFailure;
			}
            
			DOMMY_RETURN_IF_FAILURE(cvs.append(finPt));
//            knots.append(double(c));
        }
        
        
       
		const unsigned int degree = 3U;
		const std::size_t cvCount = static_cast<std::size_t>(cvs.length());
		const std::size_t knotCount = plan.knots;
		if (cvCount != totalSampling || cvCount <= degree) {
			return MS::kFailure;
		}
		for (std::size_t knotIndex = 0U;
			 knotIndex < knotCount; ++knotIndex) {
			DOMMY_RETURN_IF_FAILURE(
				kv.append(static_cast<double>(knotIndex)));
		}
        
        
//        if (VERBOSE) cout << "cvs:" << cvs.length() << endl;
//        if (VERBOSE) cout << "kv:" << kv.length() << endl;
        
        MFnNurbsCurveData dataCreator;
        MObject newCurveData = dataCreator.create( &stat );
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		if (newCurveData.isNull()) return MS::kFailure;
        
        MFnNurbsCurve curveFn;
		const MObject curveObject = curveFn.create(
			cvs,kv,degree,MFnNurbsCurve::kOpen,false,false,newCurveData,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		if (curveObject.isNull()) return MS::kFailure;
        
		MDataHandle curveHandle = data.outputValue(
			DSpiralCurve::outputCurve, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        
		DOMMY_RETURN_IF_FAILURE(curveHandle.set(newCurveData));
		curveHandle.setClean();
        
        return data.setClean(plug);
        
	}
	else {
		return MS::kUnknownParameter;
	}
	
	return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DSpiralCurve")




MStatus DSpiralCurve::initialize() try
{
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MStatus stat;
    
    inputCurve = gAttr.create( "inputCurve", "inc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputCurve));
    
    
    
    inputTangentCurve = gAttr.create( "inputTangentCurve", "intc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputTangentCurve));
    
    
    curveLength = nAttr.create( "curveLength", "cl", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));
    
    sampling = nAttr.create( "sampling", "sa", MFnNumericData::kInt , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 50 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 2 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(
		dommy::generation::kMaxGeneratedPointCount - 2U)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));
    
    offset = nAttr.create( "offset", "off", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
    
    spiralOffset = nAttr.create( "spiralOffset", "spo", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(spiralOffset));
    
    
    

    radius = nAttr.create( "radius", "rad", MFnNumericData::kDouble, 1.0 , &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(radius));
    
    
    arcStep = nAttr.create( "arcStep", "ars", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 45.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(arcStep));
    
    
    
    outputCurve = tAttr.create("outputCurve", "oc", MFnNurbsCurveData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputCurve));
    
    
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inputCurve, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inputTangentCurve, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(curveLength, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(spiralOffset, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(radius, outputCurve));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(arcStep, outputCurve));
    

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DSpiralCurve", "initialize", std::current_exception());
}

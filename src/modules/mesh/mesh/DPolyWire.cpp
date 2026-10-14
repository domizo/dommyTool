//
//  DPolyWire.cpp
//  dommyTool
//
//  Created by yuzo on 9/25/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolyWire.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaWireTopology.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include <vector>
#include <SeExpression.h>

#include "dommyMath.h"
#include "DCurveSeExpr.h"

#include <cmath>
#include <cstddef>


const MTypeId DPolyWire::typeId( 0x00332 );

// Attributes
MObject DPolyWire::inCurve;
MObject DPolyWire::inTangentCurve;


MObject DPolyWire::sampling;

MObject DPolyWire::profileMode;
MObject DPolyWire::profileCurve;

MObject DPolyWire::circleRadius;
MObject DPolyWire::circleSampling;


MObject DPolyWire::curveLength;
MObject DPolyWire::curveOffset;

MObject DPolyWire::tangentMode;
MObject DPolyWire::tangentVector;

MObject DPolyWire::twistRamp;
MObject DPolyWire::widthRamp;

MObject DPolyWire::globalRotate;
MObject DPolyWire::globalScale;

MObject DPolyWire::aExpression;

MObject DPolyWire::aUVTranslate;
MObject DPolyWire::aUVScale;

MObject DPolyWire::outMesh;


void *DPolyWire::creator()
{
    return ::dommy::maya::createObject("DPolyWire", [&]() {
        return new DPolyWire();
    });
}

void DPolyWire::postConstructor() {
    dommy::maya::initializeNodeRamps("DPolyWire", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), twistRamp,
                                                                    {0.0f, 0.0f}, {0.0f, 0.0f}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), widthRamp, {1.0f}, {0.0f}, {MRampAttribute::kLinear}));
        return MS::kSuccess;
    });
}






MStatus DPolyWire::compute( const MPlug& plug, MDataBlock& data ) try
{
	MStatus stat;

	
	
	if ( plug == outMesh )
	{
		
		
		const MDataHandle profileModeValHandle = data.inputValue(profileMode, &stat);
		if (!stat) return stat;
		const int profileModeVal = profileModeValHandle.asInt();
		const MDataHandle curveLengthValHandle = data.inputValue(curveLength, &stat);
		if (!stat) return stat;
		const double curveLengthVal = curveLengthValHandle.asDouble();
		const MDataHandle curveOffsetValHandle = data.inputValue(curveOffset, &stat);
		if (!stat) return stat;
		const double curveOffsetVal = curveOffsetValHandle.asDouble();
		const MDataHandle rawSamplingHandle = data.inputValue(sampling, &stat);
		if (!stat) return stat;
		const int rawSampling = rawSamplingHandle.asInt();
		
		const MDataHandle circleRadiusValHandle = data.inputValue(circleRadius, &stat);
		if (!stat) return stat;
		const double circleRadiusVal = circleRadiusValHandle.asDouble();
		const MDataHandle circleSamplingValHandle = data.inputValue(circleSampling, &stat);
		if (!stat) return stat;
		const int circleSamplingVal = circleSamplingValHandle.asInt();
		
		const MDataHandle tangentModeValHandle = data.inputValue(tangentMode, &stat);
		if (!stat) return stat;
		const int tangentModeVal = tangentModeValHandle.asInt();
		const MDataHandle tangentVecValHandle = data.inputValue(tangentVector, &stat);
		if (!stat) return stat;
		const double3 &tangentVecVal = tangentVecValHandle.asDouble3();
		
		const MDataHandle gRotateValHandle = data.inputValue(globalRotate, &stat);
		if (!stat) return stat;
		const double gRotateVal = gRotateValHandle.asDouble();
		const MDataHandle gScaleValHandle = data.inputValue(globalScale, &stat);
		if (!stat) return stat;
		const double gScaleVal = gScaleValHandle.asDouble();
        
        MPlug twistPlug( thisMObject(), twistRamp );
        MRampAttribute twistRampAttr( twistPlug , &stat );
        if (!stat) return stat;
        
        MPlug widthPlug( thisMObject(), widthRamp );
        MRampAttribute widthRampAttr( widthPlug , &stat );
        if (!stat) return stat;
        
		const MDataHandle uvTransValHandle = data.inputValue(aUVTranslate, &stat);
		if (!stat) return stat;
		const double2 &uvTransVal = uvTransValHandle.asDouble2();
		const MDataHandle uvScaleValHandle = data.inputValue(aUVScale, &stat);
		if (!stat) return stat;
		const double2 &uvScaleVal = uvScaleValHandle.asDouble2();

		std::size_t samplingVal = 0U;
		std::size_t checkedCircleSampling = 0U;
		if ((profileModeVal != 0 && profileModeVal != 1) ||
			(tangentModeVal < 0 || tangentModeVal > 2) ||
			!dommy::generation::checkedCount(
				rawSampling, dommy::generation::kMaxGeneratedPointCount,
				samplingVal) || samplingVal < 2U ||
			!dommy::generation::checkedCount(
				circleSamplingVal, dommy::generation::kMaxGeneratedPointCount,
				checkedCircleSampling) || checkedCircleSampling < 3U ||
			!std::isfinite(curveLengthVal) || curveLengthVal < 0.0 ||
			!std::isfinite(curveOffsetVal) ||
			!std::isfinite(circleRadiusVal) || circleRadiusVal < 0.0 ||
			!std::isfinite(tangentVecVal[0]) ||
			!std::isfinite(tangentVecVal[1]) ||
			!std::isfinite(tangentVecVal[2]) ||
			!std::isfinite(gRotateVal) || !std::isfinite(gScaleVal) ||
			!std::isfinite(uvTransVal[0]) || !std::isfinite(uvTransVal[1]) ||
			!std::isfinite(uvScaleVal[0]) || !std::isfinite(uvScaleVal[1])) {
			MGlobal::displayError("DPolyWire: invalid or excessive input value.");
			return MS::kInvalidParameter;
		}
		
		
        
#pragma mark thisNode
		MFnDependencyNode thisFn(thisMObject());
		
#pragma mark Get input curve 入力カーブの取得
		MDataHandle dataHnd = data.inputValue(inCurve,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		MPlug inCrvPlug(thisMObject(), inCurve);
		const bool curveConnected = inCrvPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (!curveConnected) {
			MGlobal::displayWarning("DCurveDuplicator :Input curve is not connected.");
			return MS::kFailure;
		} else if (dataHnd.type() != MFnData::kNurbsCurve) {
			MGlobal::displayWarning("DCurveDuplicator :Input is not curve.");
			return MS::kFailure;
		}
		
		MObject crvObj = dataHnd.data();
		
		MFnNurbsCurve crvFn;
		DOMMY_RETURN_IF_FAILURE(crvFn.setObject(crvObj));
		
#pragma mark Get input object 入力タンジェントカーブの取得
		MDataHandle tanHnd = data.inputValue(inTangentCurve,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		MPlug tangentPlug(thisMObject(), inTangentCurve);
		
		MFnNurbsCurve tanCrvFn;
		bool useTangent = false;
		const bool tangentConnected = tangentPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (tangentConnected) {
			if (tanHnd.type() != MFnData::kNurbsCurve) {
				MGlobal::displayWarning("DCurveDuplicator :Input tangent is not curve.");
				return MS::kFailure;
			}
			
			
			
			DOMMY_RETURN_IF_FAILURE(tanCrvFn.setObject(tanHnd.data()));
			
			useTangent =true;
		}
		
		
		
		
		
        // Read only scalar profile counts before allocating geometry arrays.
        MPointArray profPointArray;
        MFnNurbsCurve profCrvFn;
        std::size_t profilePointCount = checkedCircleSampling;
        bool useProfile = false;
        if (profileModeVal == 1) {
            MDataHandle profCrvHnd = data.inputValue(profileCurve, &stat);
            if (!stat) return stat;
            MPlug profCrvPlug(thisMObject(), profileCurve);
            const bool profileConnected = profCrvPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (!profileConnected || profCrvHnd.type() != MFnData::kNurbsCurve)
                return MS::kInvalidParameter;
            DOMMY_RETURN_IF_FAILURE(profCrvFn.setObject(profCrvHnd.data()));
            useProfile = true;
        }
        if (useProfile) {
            const int declaredCvs = profCrvFn.numCVs(&stat);
            if (!stat) return stat;
            if (!dommy::generation::checkedCount(declaredCvs,
                    dommy::generation::kMaxGeneratedPointCount, profilePointCount))
                return MS::kInvalidParameter;
        }
        dommy::mesh::WireTopologyPlan topologyPlan;
        if (!dommy::mesh::checkedWireTopologyCounts(samplingVal, profilePointCount,
                dommy::generation::kMaxGeneratedPointCount,
                dommy::generation::kMaxGeneratedFaceCount,
                dommy::generation::kMaxWorkItemCount, topologyPlan) ||
            !dommy::mesh::validWireGenerationBudget(topologyPlan, 1U))
            return MS::kInvalidParameter;
        if (useProfile) {
            DOMMY_RETURN_IF_FAILURE(profCrvFn.getCVs(profPointArray, MSpace::kWorld));
        } else {
            const MVector basePt(0.0, 0.0, -circleRadiusVal);
            const double step = 360.0 / static_cast<double>(checkedCircleSampling);
            for (std::size_t i = 0U; i < checkedCircleSampling; ++i) {
                const MVector rotated = basePt.rotateBy(MVector::kYaxis,
                    dommy::math::degreesToRadians(step * static_cast<double>(i)));
                DOMMY_RETURN_IF_FAILURE(profPointArray.append(MPoint(rotated)));
            }
        }
        if (profPointArray.length() != profilePointCount) return MS::kFailure;
        for (unsigned int i = 0U; i < profPointArray.length(); ++i)
            if (!dommy::maya::isFinitePoint(profPointArray[i])) return MS::kInvalidParameter;
        if (!dommy::mesh::checkedWireTopologyPlan(samplingVal, profilePointCount,
                uvTransVal[0], uvTransVal[1], uvScaleVal[0], uvScaleVal[1],
                dommy::generation::kMaxGeneratedPointCount,
                dommy::generation::kMaxGeneratedFaceCount,
                dommy::generation::kMaxWorkItemCount, topologyPlan)) return MS::kInvalidParameter;

#pragma mark Get curve info
		double start, end;
		stat = crvFn.getKnotDomain (start, end);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double length = crvFn.length(kMFnNurbsEpsilon, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		length *= curveLengthVal;
		if (!std::isfinite(start) || !std::isfinite(end) || start > end ||
			!std::isfinite(length) || length < 0.0) {
			MGlobal::displayError("DPolyWire: curve domain or length is invalid.");
			return MS::kInvalidParameter;
		}
		
		
		
		
		
		const std::size_t totalSampling = samplingVal;
		const double stepLength =
			length / static_cast<double>(totalSampling - 1U);
		
		
		
		
		MVector oldBiNormal;
		
#pragma mark create transform info
		MQuaternion xQuat,yQuat;//,zQuat;
		MVector xAxisVec(1.0,0.0,0.0);
		MVector yAxisVec(0.0,1.0,0.0);
		
		std::vector<MQuaternion> xQuatArray;
		std::vector<MQuaternion> yQuatArray;
		std::vector<MPoint> dupPointArray;
		xQuatArray.reserve(totalSampling);
		yQuatArray.reserve(totalSampling);
		dupPointArray.reserve(totalSampling);
		
		
		for (std::size_t c=0U; c<totalSampling; ++c) {
			MPoint cv;
			double curveParam = crvFn.findParamFromLength(
				stepLength * static_cast<double>(c), &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			
			double offsetedParam = curveParam+curveOffsetVal;
			
			offsetedParam = offsetedParam<start?start:offsetedParam;
			offsetedParam = offsetedParam>end?end:offsetedParam;
			
			
			DOMMY_RETURN_IF_FAILURE(
				crvFn.getPointAtParam(offsetedParam, cv, MSpace::kWorld));
			if (!dommy::maya::isFinitePoint(cv)) return MS::kFailure;
			
			MVector	normal;
			MVector	tangent = crvFn.tangent(offsetedParam , MSpace::kWorld , &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			
			if (tangentModeVal==1) {//Simple vector
				normal= MVector(tangentVecVal);
				
				//				if (c==0) {
				//					normal= MVector(tangentVecVal);
				//				} else {
				//					normal = oldBiNormal ^ tangent;
				//				}
			} else if (tangentModeVal==2 && useTangent) {//input curve
				MPoint targetCV;
				double targetParam;
				tanCrvFn.closestPoint(
					cv,&targetParam,kMFnNurbsEpsilon,MSpace::kWorld,&stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
				DOMMY_RETURN_IF_FAILURE(
					tanCrvFn.getPointAtParam(targetParam, targetCV, MSpace::kWorld));
				normal = targetCV-cv;
			} else {
				if (c==0) {
					normal= crvFn.normal( offsetedParam , MSpace::kWorld , &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
				} else {
					normal = oldBiNormal ^ tangent;
				}
			}
			
			MVector normalizedTangent;
			if (!dommy::maya::normalizedVector(tangent, normalizedTangent)) {
				MGlobal::displayError("DPolyWire: curve tangent is degenerate.");
				return MS::kFailure;
			}
			const MVector orthogonalNormal =
				normal - normalizedTangent * (normal * normalizedTangent);
			MVector normalizedNormal;
			if (!dommy::maya::normalizedVector(
					orthogonalNormal, normalizedNormal)) {
				MGlobal::displayError("DPolyWire: profile orientation is degenerate.");
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
			
			xQuatArray.push_back(xQuat);
			yQuatArray.push_back(yQuat);
			dupPointArray.push_back(cv);
			
			oldBiNormal = biNormal;
		}
		
		
		
		
		
		
	#pragma mark twist and width
        MFloatArray twistValueArray,widthValueArray;
	        twistRampAttr.sampleValueRamp(
			static_cast<unsigned int>(totalSampling), twistValueArray,&stat);
	        CHECK_MSTATUS_AND_RETURN_IT(stat);
	        widthRampAttr.sampleValueRamp(
			static_cast<unsigned int>(totalSampling), widthValueArray,&stat);
	        CHECK_MSTATUS_AND_RETURN_IT(stat);
		if (twistValueArray.length() != totalSampling ||
			widthValueArray.length() != totalSampling) {
			return MS::kFailure;
		}
		for (std::size_t sampleIndex = 0U;
			 sampleIndex < totalSampling; ++sampleIndex) {
			if (!std::isfinite(twistValueArray[sampleIndex]) ||
				!std::isfinite(widthValueArray[sampleIndex])) {
				MGlobal::displayError("DPolyWire: ramp produced a non-finite value.");
				return MS::kFailure;
			}
		}
		
		
		
#pragma mark eval expression
//		std::string exprStr = "$val=.5*PI*param;7*sin(val)/val";
		const MDataHandle exprStrHandle = data.inputValue(aExpression, &stat);
		if (!stat) return stat;
		const std::string exprStr = exprStrHandle.asString().asChar();
		
		//create expression
		DCurveExpr expr(exprStr);
		MDoubleArray paramExpArray;
		
		if(expr.isValid()){
			for (std::size_t c=0U; c<totalSampling; ++c) {
				double curveParam = crvFn.findParamFromLength(
					stepLength * static_cast<double>(c), &stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
				
				double offsetedParam = curveParam+curveOffsetVal;
				
				offsetedParam = offsetedParam<start?start:offsetedParam;
				offsetedParam = offsetedParam>end?end:offsetedParam;
				
				expr.setParam(offsetedParam);
				expr.setLength(stepLength * static_cast<double>(c));
				
				SeVec3d val=expr.evaluate();
				double y=val[0];
				if (!std::isfinite(y)) {
					MGlobal::displayError("DPolyWire: expression produced a non-finite value.");
					return MS::kFailure;
				}
				
				DOMMY_RETURN_IF_FAILURE(paramExpArray.append(y));
			}
		} else if (exprStr.size()>0) {
			DOMMY_MAYA_WARNING(
				"DPolyWire", "expression failed: " << expr.parseError());
		}
		
		
		
		
#pragma mark Create Wire
		MPointArray vertexArray;
		MIntArray polygonCount,polygonConnect;
		
		//create vertex
		for (std::size_t i=0U; i<totalSampling; ++i) {
			for (unsigned int p=0U; p<profPointArray.length(); ++p) {
				MVector newVec(profPointArray[p]);
                
                newVec*=widthValueArray[i];
				if(paramExpArray.length()==totalSampling) newVec*=paramExpArray[i];
                newVec*=gScaleVal;
                
                newVec=newVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(twistValueArray[i]*360.0f+gRotateVal));
                
				newVec = newVec.rotateBy(yQuatArray[i]).rotateBy(xQuatArray[i]);
				MPoint finPt(newVec.x+dupPointArray[i].x, newVec.y+dupPointArray[i].y, newVec.z+dupPointArray[i].z);
				if (!dommy::maya::isFinitePoint(finPt)) {
					MGlobal::displayError("DPolyWire: generated vertex is non-finite.");
					return MS::kFailure;
				}
				
				DOMMY_RETURN_IF_FAILURE(vertexArray.append(finPt));
			}
            
		}
		
		
		MFloatArray uCoords, vCoords;
		MIntArray uvCountsArray,uvIdsArray;
		DOMMY_RETURN_IF_FAILURE(dommy::maya::copyWireTopologyToMaya(
			topologyPlan, polygonCount, polygonConnect, uCoords, vCoords,
			uvCountsArray, uvIdsArray));
		if (static_cast<std::size_t>(vertexArray.length()) !=
			topologyPlan.vertexCount) {
			MGlobal::displayError("DPolyWire: generated vertex count is inconsistent.");
			return MS::kFailure;
		}
		
		
#pragma mark set Create Output mesh data
		MFnMeshData dataCreator;
		MObject newMeshData = dataCreator.create( &stat );
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		MFnMesh newMeshFn;
		newMeshFn.create(vertexArray.length(),polygonCount.length(),vertexArray,polygonCount,polygonConnect,uCoords, vCoords,newMeshData,&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		//assign uv
		DOMMY_RETURN_IF_FAILURE(newMeshFn.assignUVs(uvCountsArray,uvIdsArray));
		
		
		MDataHandle outHandle = data.outputValue( DPolyWire::outMesh, &stat );
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		DOMMY_RETURN_IF_FAILURE(outHandle.set(newMeshData));
		
		return data.setClean(plug);
		
		
		
	} else {
		return MS::kUnknownParameter;
	}
	
}
DOMMY_COMPUTE_CATCH("DPolyWire")





MStatus DPolyWire::initialize() try
{
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute enumFn;

    inCurve = gAttr.create( "inCurve", "inc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));

    inTangentCurve = gAttr.create( "inTangentCurve", "intc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTangentCurve));

    profileMode = enumFn.create( "profileMode", "prfm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Simple Circle", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Profile", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(profileMode));

    profileCurve = gAttr.create( "profileCurve", "pcrv",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(profileCurve));

    circleRadius = nAttr.create( "circleRadius", "cr", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(circleRadius));

    circleSampling = nAttr.create( "circleSampling", "cs", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 3 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(circleSampling));

    sampling = nAttr.create( "sampling", "sa", MFnNumericData::kLong, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 2 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    curveLength = nAttr.create( "curveLength", "cl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));

    curveOffset = nAttr.create( "curveOffset", "off", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveOffset));

    tangentMode = enumFn.create( "tangentMode", "tam", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Curve", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Simple Vector", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "input Curve", 2 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(tangentMode));

    tangentVector = nAttr.create( "tangentVector", "tanvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(tangentVector));

    twistRamp = MRampAttribute::createCurveRamp("twistRamp", "twr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(twistRamp));

    widthRamp = MRampAttribute::createCurveRamp("widthRamp", "wir", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(widthRamp));

    globalRotate = nAttr.create( "globalRotate", "grot", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(globalRotate));

    globalScale = nAttr.create( "globalScale", "gscal", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(globalScale));

    MFnStringData fnStringData;
    MObject expAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExpression = tAttr.create("Expression", "expr", MFnData::kString, expAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));

    aUVTranslate = nAttr.create( "UVTranslate", "uvt", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVTranslate));

    aUVScale = nAttr.create( "UVScale", "uvs", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0, 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVScale));

    outMesh = tAttr.create("outMesh", "om", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE( attributeAffects( inCurve, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTangentCurve, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( sampling, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( profileMode, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( profileCurve, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( circleRadius, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( circleSampling, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( curveLength, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( curveOffset, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( tangentMode, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( tangentVector, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( twistRamp, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( widthRamp, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( globalRotate, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( globalScale, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( aExpression, outMesh ) );

    DOMMY_RETURN_IF_FAILURE( attributeAffects( aUVTranslate, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aUVScale, outMesh ) );

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyWire", "initialize", std::current_exception());
}

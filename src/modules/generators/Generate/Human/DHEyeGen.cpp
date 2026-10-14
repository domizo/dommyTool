//
//  DHEyeGen.cpp
//  dommyTool
//
//  Created by yuzo on 5/7/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DHEyeGen.h"
#include "common/utility/DMayaDefaultRamp.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MRampAttribute.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>

#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DProceduralMeshPlans.h"
#include "dommyMath.h"

namespace {

MStatus publishEmptyMesh(MDataBlock& data, const MPlug& plug, const MObject& outputAttribute)
{
    MStatus status;
    MFnMeshData meshData;
    const MObject emptyMesh = meshData.create(&status);
    if (status != MS::kSuccess) {
        return status;
    }

    MDataHandle output = data.outputValue(outputAttribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = output.set(emptyMesh);
    if (status != MS::kSuccess) {
        return status;
    }
    return data.setClean(plug);
}

MStatus publishGeneratedMesh(MDataBlock& data,
                             const MPlug& plug,
                             const MObject& outputAttribute,
                             const dommy::generation::EyePartPlan& plan,
                             const MFloatPointArray& points,
                             const MIntArray& polygonCounts,
                             const MIntArray& polygonConnections,
                             const MFloatArray& uCoordinates,
                             const MFloatArray& vCoordinates,
                             const MIntArray& uvCounts,
                             const MIntArray& uvIds)
{
    if (points.length() != uCoordinates.length() ||
        points.length() != vCoordinates.length() ||
        polygonCounts.length() != uvCounts.length() ||
        !dommy::generation::matchesEyePartPlan(
            plan, points.length(), polygonCounts.length(),
            polygonConnections.length())) {
        return MS::kInvalidParameter;
    }

    for (unsigned int point = 0U; point < points.length(); ++point) {
        if (!std::isfinite(points[point].x) ||
            !std::isfinite(points[point].y) ||
            !std::isfinite(points[point].z) ||
            !std::isfinite(uCoordinates[point]) ||
            !std::isfinite(vCoordinates[point])) {
            return MS::kInvalidParameter;
        }
    }

    std::size_t expectedConnections = 0U;
    std::size_t expectedUvIds = 0U;
    for (unsigned int face = 0U; face < polygonCounts.length(); ++face) {
        if (polygonCounts[face] < 3 || uvCounts[face] < 3 ||
            !dommy::generation::checkedSum(
                expectedConnections,
                static_cast<std::size_t>(polygonCounts[face]),
                plan.connectionCount, expectedConnections) ||
            !dommy::generation::checkedSum(
                expectedUvIds,
                static_cast<std::size_t>(uvCounts[face]),
                plan.connectionCount, expectedUvIds)) {
            return MS::kInvalidParameter;
        }
    }
    if (expectedConnections != polygonConnections.length() ||
        expectedUvIds != uvIds.length()) {
        return MS::kInvalidParameter;
    }
    for (unsigned int index = 0U;
         index < polygonConnections.length(); ++index) {
        if (polygonConnections[index] < 0 ||
            static_cast<unsigned int>(polygonConnections[index]) >=
                points.length()) {
            return MS::kInvalidParameter;
        }
    }
    for (unsigned int index = 0U; index < uvIds.length(); ++index) {
        if (uvIds[index] < 0 ||
            static_cast<unsigned int>(uvIds[index]) >=
                uCoordinates.length()) {
            return MS::kInvalidParameter;
        }
    }

    MStatus status;
    MFnMeshData meshData;
    const MObject outputData = meshData.create(&status);
    if (!status || outputData.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }

    MFnMesh mesh;
    const MObject meshObject = mesh.create(
        points.length(),
        polygonCounts.length(),
        points,
        polygonCounts,
        polygonConnections,
        uCoordinates,
        vCoordinates,
        outputData,
        &status);
    if (!status || meshObject.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }

    status = mesh.assignUVs(uvCounts, uvIds);
    if (!status) return status;

    const int edgeCount = mesh.numEdges(&status);
    if (!status) return status;
    for (int edge = 0; edge < edgeCount; ++edge) {
        status = mesh.setEdgeSmoothing(edge, true);
        if (!status) return status;
    }

    MDataHandle output = data.outputValue(outputAttribute, &status);
    if (!status) return status;
    status = output.set(outputData);
    if (!status) return status;
    return data.setClean(plug);
}

bool checkedEyeResolution(int rawDivideU,
                          int rawDivideV,
                          int rawIrisDivide,
                          bool capPupil,
                          unsigned long& divideU,
                          unsigned long& divideV,
                          unsigned long& irisDivide,
                          dommy::generation::EyeMeshPlan& plan)
{
    if (!dommy::generation::buildEyeMeshPlan(
            rawDivideU, rawDivideV, rawIrisDivide, capPupil, plan)) {
        return false;
    }
    divideU = static_cast<unsigned long>(plan.divideU);
    divideV = static_cast<unsigned long>(plan.divideV);
    irisDivide = static_cast<unsigned long>(plan.irisDivide);
    return true;
}

bool areFiniteEyeInputs(double radius,
                        double innerRadius,
                        double lensGap,
                        double lensOffset,
                        double irisSize,
                        double irisOffset,
                        double pupilSize,
                        double pupilDepth,
                        double pupilMotion,
                        double uvPadding)
{
    return std::isfinite(radius) && std::isfinite(innerRadius) &&
           std::isfinite(lensGap) && std::isfinite(lensOffset) &&
           std::isfinite(irisSize) && std::isfinite(irisOffset) &&
           std::isfinite(pupilSize) && std::isfinite(pupilDepth) &&
           std::isfinite(pupilMotion) && std::isfinite(uvPadding);
}

} // namespace

MTypeId DHEyeGen::typeId( 0x0051A );


MObject DHEyeGen::divideU;
MObject DHEyeGen::divideV;
MObject DHEyeGen::radius;
MObject DHEyeGen::radiusRamp;
MObject DHEyeGen::innerRadius;

MObject DHEyeGen::lenzGap;
MObject DHEyeGen::lenzRamp;
MObject DHEyeGen::lenzsOffset;

MObject DHEyeGen::irisSize;
MObject DHEyeGen::irisDivide;
MObject DHEyeGen::irisRamp;
MObject DHEyeGen::irisOffset;

MObject DHEyeGen::pupilSize;
MObject DHEyeGen::pupilDepth;
MObject DHEyeGen::pupilMotion;
MObject DHEyeGen::isCapPupil;

MObject DHEyeGen::uvPadding;

MObject DHEyeGen::outLenzMesh;
MObject DHEyeGen::outIrisMesh;




void *DHEyeGen::creator()
{
    return ::dommy::maya::createObject("DHEyeGen", [&]() {
        return new DHEyeGen();
    });
}



void DHEyeGen::postConstructor() {
    dommy::maya::initializeNodeRamps("DHEyeGen", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), irisRamp, {0.0f, 1.0f}, {0.0f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), lenzRamp, {0.0f, 0.180f, 0.640f, 0.78f}, {0.0f, 0.243f, 0.591f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline,
             MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), radiusRamp, {1.0f}, {0.0f}, {MRampAttribute::kLinear}));
        return MS::kSuccess;
    });
}




MStatus DHEyeGen::compute( const MPlug& plug, MDataBlock& data )
{
	try {
		MStatus stat;

	
	
	if(plug == outLenzMesh ||  plug == outIrisMesh)
	{
		unsigned long divideUVal = 0;
		unsigned long divideVVal = 0;
		unsigned long irisDivideVal = 0;
			const MDataHandle rawDivideUHandle = data.inputValue(divideU, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const int rawDivideU = rawDivideUHandle.asInt();
			const MDataHandle rawDivideVHandle = data.inputValue(divideV, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const int rawDivideV = rawDivideVHandle.asInt();
			const MDataHandle rawIrisDivideHandle = data.inputValue(irisDivide, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const int rawIrisDivide = rawIrisDivideHandle.asInt();
			const MDataHandle isCapPupilValHandle = data.inputValue(isCapPupil, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const bool isCapPupilVal = isCapPupilValHandle.asBool();
			dommy::generation::EyeMeshPlan meshPlan;
			if (!checkedEyeResolution(rawDivideU, rawDivideV, rawIrisDivide,
			                         isCapPupilVal,
			                         divideUVal, divideVVal, irisDivideVal,
			                         meshPlan)) {
			MGlobal::displayError(
				"DHEyeGen: subdivision count exceeds the generation budget.");
			return MS::kFailure;
		}
		const MDataHandle radiusValHandle = data.inputValue(radius, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double radiusVal = radiusValHandle.asDouble();
		const MDataHandle innerRadiusValHandle = data.inputValue(innerRadius, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double innerRadiusVal = innerRadiusValHandle.asDouble();
		
		const MDataHandle lenzGapValHandle = data.inputValue(lenzGap, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double lenzGapVal = lenzGapValHandle.asDouble();
		const MDataHandle lenzOffsetValHandle = data.inputValue(lenzsOffset, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double lenzOffsetVal = lenzOffsetValHandle.asDouble();
		
		const MDataHandle irisSizeValHandle = data.inputValue(irisSize, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double irisSizeVal = irisSizeValHandle.asDouble();
		const MDataHandle irisOffsetValHandle = data.inputValue(irisOffset, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double irisOffsetVal = irisOffsetValHandle.asDouble();
		
		
		const MDataHandle pupilSizeValHandle = data.inputValue(pupilSize, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double pupilSizeVal = pupilSizeValHandle.asDouble();
        const MDataHandle pupilDepthValHandle = data.inputValue(pupilDepth, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double pupilDepthVal = pupilDepthValHandle.asDouble();
        const MDataHandle pupilMotionValHandle = data.inputValue(pupilMotion, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double pupilMotionVal = pupilMotionValHandle.asDouble();
        
	        const MDataHandle uvPaddingValHandle = data.inputValue(uvPadding, &stat);
	        DOMMY_RETURN_IF_FAILURE(stat);
	        const double uvPaddingVal = uvPaddingValHandle.asDouble();

		if (!areFiniteEyeInputs(radiusVal, innerRadiusVal, lenzGapVal,
		                       lenzOffsetVal, irisSizeVal, irisOffsetVal,
		                       pupilSizeVal, pupilDepthVal, pupilMotionVal,
		                       uvPaddingVal) ||
			innerRadiusVal < 0.0 || innerRadiusVal > 180.0 ||
			lenzGapVal < 0.0 || lenzGapVal >= 1.0 ||
			irisSizeVal < 0.0 || irisSizeVal > 1.0 ||
			pupilSizeVal < 0.0 || pupilSizeVal > 1.0 ||
			pupilDepthVal < 0.0 || pupilDepthVal > 1.0 ||
			uvPaddingVal < 0.0 ||
			radiusVal > static_cast<double>(
				std::numeric_limits<float>::max())) {
			MGlobal::displayError(
				"DHEyeGen: inputs are outside the finite geometry range.");
			return MS::kInvalidParameter;
		}
		if (radiusVal <= 0.0) {
			return publishEmptyMesh(
				data, plug, plug == outLenzMesh ? outLenzMesh : outIrisMesh);
		}
        
        
		MRampAttribute irisRampAttr(
			MPlug(thisMObject(), irisRamp), &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		MRampAttribute lenzRampAttr(
			MPlug(thisMObject(), lenzRamp), &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        
		MRampAttribute radiusRampAttr(
			MPlug(thisMObject(), radiusRamp), &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
        
		
		
        
		
		
		
		
		
        
        
        
        
        
        
		
		
			dommy::maya::CheckedArray<MFloatPointArray>
				hakuVtxArray(meshPlan.iris.pointCount);
			dommy::maya::CheckedArray<MIntArray>
				finPolyCountArray(meshPlan.iris.faceCount),
				finPolyConnectArray(meshPlan.iris.connectionCount);
			
			dommy::maya::CheckedArray<MFloatPointArray>
				LenzVtxArray(meshPlan.lens.pointCount);
			dommy::maya::CheckedArray<MIntArray>
				LenzPolyCountArray(meshPlan.lens.faceCount),
				LenzPolyConnectArray(meshPlan.lens.connectionCount);
		
		
#pragma mark init UV data
	        dommy::maya::CheckedArray<MFloatArray>
	        	lenzUCoords(meshPlan.lens.pointCount),
	        	lenzVCoords(meshPlan.lens.pointCount);
	        dommy::maya::CheckedArray<MIntArray>
	        	lenzUvCountsArray(meshPlan.lens.faceCount),
	        	lenzUvIdsArray(meshPlan.lens.connectionCount);
			
			dommy::maya::CheckedArray<MFloatArray>
				hakuUCoords(meshPlan.iris.pointCount),
				hakuVCoords(meshPlan.iris.pointCount);
			dommy::maya::CheckedArray<MIntArray>
				hakuUvCountsArray(meshPlan.iris.faceCount),
				hakuUvIdsArray(meshPlan.iris.connectionCount);
#pragma mark Eye geometry generation
		
		float irisAngle = 90.0*irisSizeVal;
		
		float innerStartAngle = innerRadiusVal;
		
		if (180.0-innerStartAngle < irisAngle) {
			innerStartAngle = irisAngle;
		}
		
		float stepDegreeU = (180.0-irisAngle)/(float)(divideUVal);
		float stepInnerDegreeU = ((180.0-innerStartAngle)-irisAngle)/(float)(divideUVal);
		float stepDegreeV = 360.0/(float)(divideVVal);
		
		
		
		
		
		
		
		
		unsigned long irisDivStartIndex=0;
		unsigned long pupilStartIndex=0;
		unsigned long DeepPupilStartIndex=0;
		
			double eyeRadius = radiusVal-(radiusVal*lenzGapVal);
		
		double startoffset = 45.0;
		
		hakuVtxArray.append(MFloatPoint(0.0,0.0,-eyeRadius));//目のウラの頂点
		LenzVtxArray.append(MFloatPoint(0.0,0.0,-radiusVal));//レンズのウラの頂点
        
        hakuUCoords.append(0.0);hakuVCoords.append(0.0);
        lenzUCoords.append(0.0);lenzVCoords.append(0.0);
		
#pragma mark lenz and haku
		for (unsigned long u=1; u<divideUVal+1; u++) {
			
			//白目
			MVector zVec(0.0, 0.0, -eyeRadius);
			MVector rotatedXVec = zVec.rotateBy(MVector::kXaxis, dommy::math::degreesToRadians(innerStartAngle+stepInnerDegreeU*(float)u));
			
			//レンズ
			MVector LenzZVec(0.0, 0.0, -radiusVal);
			MVector rotLenzXVec = LenzZVec.rotateBy(MVector::kXaxis, dommy::math::degreesToRadians(stepDegreeU*(float)u));
			
			
			for (unsigned long v=0; v<divideVVal; v++) {
				
				//白目
				MVector rotatedZVec = rotatedXVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
				hakuVtxArray.append(MFloatPoint(rotatedZVec.x, rotatedZVec.y, rotatedZVec.z));
				//uv
                hakuUCoords.append(rotatedZVec.x);hakuVCoords.append(rotatedZVec.y);
                
				//レンズ
				MVector rotLenzZVec = rotLenzXVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
				LenzVtxArray.append(MFloatPoint(rotLenzZVec.x, rotLenzZVec.y, rotLenzZVec.z));
				//uv
                lenzUCoords.append(rotLenzZVec.x);lenzVCoords.append(rotLenzZVec.y);
			}
			
			
			if (u==divideUVal) {//last pupil
				
				float pupilRadius = eyeRadius * pupilSizeVal;
				pupilRadius = rotatedXVec.y<pupilRadius?rotatedXVec.y:pupilRadius;
                float pupilMotionRadius = pupilRadius*pupilMotionVal;
                pupilMotionRadius = rotatedXVec.y<pupilMotionRadius?rotatedXVec.y:pupilMotionRadius;
                
				
				
                
				float dOffset = eyeRadius * irisOffsetVal;
				float lenzOffset = radiusVal * lenzOffsetVal;
				
				//set ramp samplig value
					dommy::maya::CheckedArray<MFloatArray> irisRampValueArray;
				
				//divide iris
					if (irisDivideVal) {
						irisRampAttr.sampleValueRamp(irisDivideVal+1, irisRampValueArray,&stat);
						
						CHECK_MSTATUS_AND_RETURN_IT(stat);
						if (irisRampValueArray.length() != irisDivideVal + 1U) {
							return MS::kFailure;
						}
						for (unsigned int rampIndex = 0U;
							 rampIndex < irisRampValueArray.length(); ++rampIndex) {
							if (!std::isfinite(irisRampValueArray[rampIndex])) {
								return MS::kInvalidParameter;
							}
						}
					
					
					float pupilStep = (rotatedXVec.y-pupilMotionRadius)/(float)(irisDivideVal+1);
                    float pupilUVStep = (rotatedXVec.y-pupilRadius)/(float)(irisDivideVal+1);
                    
					irisDivStartIndex = hakuVtxArray.length();
					
					for (unsigned long d=1; d<irisDivideVal+1; d++) {
						
                        //base vector
						MVector irisDivVec(0.0, rotatedXVec.y-pupilStep*(float)d, rotatedXVec.z-dOffset*irisRampValueArray[d-1]);
						MVector irisDivUVVec(0.0, rotatedXVec.y-pupilUVStep*(float)d, rotatedXVec.z-dOffset*irisRampValueArray[d-1]);
                        
                        
						for (unsigned long v=0; v<divideVVal; v++) {
							MVector rotatedIrisDivVec = irisDivVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
							
							hakuVtxArray.append(MFloatPoint(rotatedIrisDivVec.x, rotatedIrisDivVec.y, rotatedIrisDivVec.z));
                            //uv
                            MVector rotatedIrisDivUVVec = irisDivUVVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
                            
							hakuUCoords.append(rotatedIrisDivUVVec.x);
                            hakuVCoords.append(rotatedIrisDivUVVec.y);
						}
					}
				} else {
						float lastValue = 0.0f;
						if (!dommy::maya::sampleFiniteRamp(
								irisRampAttr, 1.0, lastValue)) {
							return MS::kFailure;
						}
						irisRampValueArray.append(lastValue);
					}
						//set ramp sampling value
						dommy::maya::CheckedArray<MFloatArray> lenzRampValueArray;
						float lastValue = 0.0f;
						if (!dommy::maya::sampleFiniteRamp(
								lenzRampAttr, 1.0, lastValue)) {
							return MS::kFailure;
						}
					lenzRampValueArray.append(lastValue);
					
					//pupil index
				pupilStartIndex = hakuVtxArray.length();
				
				
				MVector pupilVec(0.0, pupilMotionRadius, rotatedXVec.z-dOffset*irisRampValueArray[irisRampValueArray.length()-1]);
                MVector pupilUVVec(0.0, pupilRadius, rotatedXVec.z-dOffset*irisRampValueArray[irisRampValueArray.length()-1]);
				
				for (unsigned long v=0; v<divideVVal; v++) {
					
					MVector rotatedPupilVec = pupilVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
					
					hakuVtxArray.append(MFloatPoint(rotatedPupilVec.x, rotatedPupilVec.y, rotatedPupilVec.z));
					
                    //uv
                    MVector rotatedPupilUVVec = pupilUVVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
                    hakuUCoords.append(rotatedPupilUVVec.x);
                    hakuVCoords.append(rotatedPupilUVVec.y);
				}
				
				//last pupil depth
				
				MVector innerPupilVec(0.0, pupilMotionRadius, rotatedXVec.z-(eyeRadius*pupilDepthVal));
                MVector innerPupilUVVec(0.0, pupilRadius, rotatedXVec.z-(eyeRadius*pupilDepthVal));
				DeepPupilStartIndex = hakuVtxArray.length();
				
				
				
				
				for (unsigned long v=0; v<divideVVal; ++v) {
					
					MVector rotatedDPupilVec = innerPupilVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
					
					hakuVtxArray.append(MFloatPoint(rotatedDPupilVec.x, rotatedDPupilVec.y, rotatedDPupilVec.z));
					
                    //uv
                    MVector rotatedDPupilUVVec = innerPupilUVVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-stepDegreeV*(float)v+startoffset));
                    
                    hakuUCoords.append(rotatedDPupilUVVec.x);
                    hakuVCoords.append(rotatedDPupilUVVec.y);
				}
				
				
				//add cap pupil
				if (isCapPupilVal) {
					hakuVtxArray.append(MFloatPoint(0, 0, rotatedXVec.z-(eyeRadius*pupilDepthVal)));
                    
                    //uv
                    hakuUCoords.append(0.0);hakuVCoords.append(0.0);
				}
#pragma mark add Lenz last point
				if (divideVVal>4) {
                    int quadDiv = (divideVVal/4)+1;
                    
                    
					uint leftTopIdx = LenzVtxArray.length()-divideVVal;
                    MPoint leftTop = LenzVtxArray[leftTopIdx];
                    
					
					uint rightTopIdx = LenzVtxArray.length()-divideVVal+(quadDiv-1);
					
					uint leftBottomIdx = LenzVtxArray.length()-divideVVal+(quadDiv-1)*3;
					
					double xstart,xend;
					double ystart, yend;
					
                    double zstart = leftTop.z;
					double zend = radiusVal+lenzOffset*lenzRampValueArray[lenzRampValueArray.length()-1];
					double zdist = std::abs(zend-zstart);
					MPoint lenzTipPoint(0.0,0.0,radiusVal+lenzOffset*lenzRampValueArray[lenzRampValueArray.length()-1]);
					double circleDist = leftTop.distanceTo(lenzTipPoint);
					
					
					float xwidth,ywidth;
					float xstep, ystep;
					
					
					
					for (int y=0; y<quadDiv; y++) {
						
						for (int x=0; x<quadDiv; x++) {
							//calc x start position
							
							if (y==0) {
								// out top edge points
								MPoint outPoint = LenzVtxArray[leftTopIdx+x];
								LenzVtxArray.append(outPoint);
                                //uv
                                lenzUCoords.append(outPoint.x);lenzVCoords.append(outPoint.y);
							} else if (x==0) {
								// out left edge points
								MPoint outPoint = LenzVtxArray[leftTopIdx+(divideVVal)-y];
								LenzVtxArray.append(outPoint);
                                //uv
                                lenzUCoords.append(outPoint.x);lenzVCoords.append(outPoint.y);
							} else if (x==quadDiv-1) {
								// out right edge points
								MPoint outPoint = LenzVtxArray[rightTopIdx+y];
								LenzVtxArray.append(outPoint);
                                //uv
                                lenzUCoords.append(outPoint.x);lenzVCoords.append(outPoint.y);
							} else if (y==quadDiv-1) {
								// out bottom edge points
								MPoint outPoint = LenzVtxArray[leftBottomIdx-x];
								LenzVtxArray.append(outPoint);
                                //uv
                                lenzUCoords.append(outPoint.x);lenzVCoords.append(outPoint.y);
							} else {
								
								//inner point
								
								xstart = LenzVtxArray[leftTopIdx+(divideVVal)-y].x;
								xend = LenzVtxArray[rightTopIdx+y].x;
								
								ystart = LenzVtxArray[leftTopIdx+x].y;
								yend = LenzVtxArray[leftBottomIdx-x].y;
								
								
								xwidth = std::abs(xend-xstart);
								ywidth = std::abs(yend-ystart);
								
								
								
								
								xstep = xwidth/(double)(quadDiv-1);
								ystep = ywidth/(double)(quadDiv-1);
								
								
								
								
								
								MPoint newPt((xstep*x)+xstart,ystart-(ystep*y),0.0);
								MPoint centerPt(0.0,0.0,0.0);
								
								double dist = newPt.distanceTo(centerPt);
								
								
								
								if (!std::isfinite(circleDist) || circleDist <= 1.0e-12) {
									return MS::kInvalidParameter;
								}
								double distRatio = dist/circleDist;
								if (!std::isfinite(distRatio)) {
									return MS::kInvalidParameter;
								}
								
								
								const double rampPosition = std::max(
									0.0, std::min(1.0, 1.0-distRatio));
								float offsetValue = 0.0f;
								if (!dommy::maya::sampleFiniteRamp(
										lenzRampAttr, rampPosition, offsetValue)) {
									return MS::kFailure;
								}
								LenzVtxArray.append(MFloatPoint(newPt.x,newPt.y,zstart+(zdist*offsetValue)));
                                //uv
                                lenzUCoords.append(newPt.x);lenzVCoords.append(newPt.y);
							}
							
                        }
                    }
					
					}
				
			}
		}
		
		
		
#pragma mark add lenz and hakukyu back face  index and UV
		for (unsigned long i=0; i<divideVVal; i++) {//back triangle
			if (i==(divideVVal-1)) {//lastShare
				//eye
				finPolyCountArray.append(3);
				finPolyConnectArray.append(0);
				finPolyConnectArray.append(i+1);
				finPolyConnectArray.append(1);
				//lenz
				LenzPolyCountArray.append(3);
				LenzPolyConnectArray.append(0);
				LenzPolyConnectArray.append(i+1);
				LenzPolyConnectArray.append(1);
				
				
				//haku uv
				hakuUvCountsArray.append(3);
				hakuUvIdsArray.append(0);
				hakuUvIdsArray.append(i+1);
				hakuUvIdsArray.append(1);
                
                
                //lenz uv
                lenzUvCountsArray.append(3);
                lenzUvIdsArray.append(0);
                lenzUvIdsArray.append(i+1);
                lenzUvIdsArray.append(1);
				
			} else {
				//eye
				finPolyCountArray.append(3);
				finPolyConnectArray.append(0);
				finPolyConnectArray.append(i+1);
				finPolyConnectArray.append(i+2);
				//lenz
				LenzPolyCountArray.append(3);
				LenzPolyConnectArray.append(0);
				LenzPolyConnectArray.append(i+1);
				LenzPolyConnectArray.append(i+2);
				
                
                //haku uv
                hakuUvCountsArray.append(3);
                hakuUvIdsArray.append(0);
                hakuUvIdsArray.append(i+1);
                hakuUvIdsArray.append(i+2);
                
                //lenz uv
                lenzUvCountsArray.append(3);
                lenzUvIdsArray.append(0);
                lenzUvIdsArray.append(i+1);
                lenzUvIdsArray.append(i+2);
				
			}
		}
		
#pragma mark add lenz and hakukyu face
		for (unsigned long u=0; u<divideUVal-1; u++) {
			for (unsigned long v=1; v< divideVVal+1; v++) {
				if (v==divideVVal) {//lastShare
					//eye
					finPolyCountArray.append(4);
					finPolyConnectArray.append(1+(u*divideVVal));
					finPolyConnectArray.append(v+(u*divideVVal));
					finPolyConnectArray.append(v+divideVVal+(u*divideVVal));
					finPolyConnectArray.append((1+divideVVal)+(u*divideVVal));
                    
					
					//lenz
					LenzPolyCountArray.append(4);
					LenzPolyConnectArray.append(1+(u*divideVVal)				);
					LenzPolyConnectArray.append(v+(u*divideVVal)				);
					LenzPolyConnectArray.append(v+divideVVal+(u*divideVVal)		);
					LenzPolyConnectArray.append((1+divideVVal)+(u*divideVVal)	);
					
                    //haku uv
                    hakuUvCountsArray.append(4);
                    hakuUvIdsArray.append(1+(u*divideVVal));
                    hakuUvIdsArray.append(v+(u*divideVVal));
                    hakuUvIdsArray.append(v+divideVVal+(u*divideVVal));
                    hakuUvIdsArray.append((1+divideVVal)+(u*divideVVal));
                    
                    
                    //lenz uv
                    lenzUvCountsArray.append(4);
                    lenzUvIdsArray.append(1+(u*divideVVal)                );
                    lenzUvIdsArray.append(v+(u*divideVVal)                );
                    lenzUvIdsArray.append(v+divideVVal+(u*divideVVal)        );
                    lenzUvIdsArray.append((1+divideVVal)+(u*divideVVal)    );
				} else {
					//eye
					finPolyCountArray.append(4);
					finPolyConnectArray.append((v+1)+(u*divideVVal));
					finPolyConnectArray.append(v+(u*divideVVal));
					finPolyConnectArray.append(v+divideVVal+(u*divideVVal));
					finPolyConnectArray.append((v+divideVVal+1)+(u*divideVVal));
					
					//lenz
					LenzPolyCountArray.append(4);
					LenzPolyConnectArray.append((v+1)+(u*divideVVal)			);
					LenzPolyConnectArray.append(v+(u*divideVVal)				);
					LenzPolyConnectArray.append(v+divideVVal+(u*divideVVal)		);
					LenzPolyConnectArray.append((v+divideVVal+1)+(u*divideVVal) );
                    
                    
                    //haku uv
                    hakuUvCountsArray.append(4);
                    hakuUvIdsArray.append((v+1)+(u*divideVVal));
                    hakuUvIdsArray.append(v+(u*divideVVal));
                    hakuUvIdsArray.append(v+divideVVal+(u*divideVVal));
                    hakuUvIdsArray.append((v+divideVVal+1)+(u*divideVVal));
                    
                    
                    //lenz uv
                    lenzUvCountsArray.append(4);
                    lenzUvIdsArray.append((v+1)+(u*divideVVal)            );
                    lenzUvIdsArray.append(v+(u*divideVVal)                );
                    lenzUvIdsArray.append(v+divideVVal+(u*divideVVal)        );
                    lenzUvIdsArray.append((v+divideVVal+1)+(u*divideVVal) );
				}
				
			}
		}
		
		
		//iris divide Face
		if (irisDivideVal) {
			
			unsigned long hakukyuLastIndex = irisDivStartIndex-divideVVal;
			for (unsigned long u=0; u<irisDivideVal; u++) {
				for (unsigned long v=0; v< divideVVal; v++) {
					if (v==divideVVal-1) {//lastShare
						finPolyCountArray.append(4);
						finPolyConnectArray.append(hakukyuLastIndex+(u*divideVVal));
						finPolyConnectArray.append(hakukyuLastIndex+v+(u*divideVVal));
						finPolyConnectArray.append(hakukyuLastIndex+v+divideVVal+(u*divideVVal));
						finPolyConnectArray.append((hakukyuLastIndex+divideVVal)+(u*divideVVal));
						
                        //haku uv
                        hakuUvCountsArray.append(4);
                        hakuUvIdsArray.append(hakukyuLastIndex+(u*divideVVal));
                        hakuUvIdsArray.append(hakukyuLastIndex+v+(u*divideVVal));
                        hakuUvIdsArray.append(hakukyuLastIndex+v+divideVVal+(u*divideVVal));
                        hakuUvIdsArray.append((hakukyuLastIndex+divideVVal)+(u*divideVVal));
					} else {
						finPolyCountArray.append(4);
						finPolyConnectArray.append((hakukyuLastIndex+v+1)+(u*divideVVal));
						finPolyConnectArray.append(hakukyuLastIndex+v+(u*divideVVal));
						finPolyConnectArray.append(hakukyuLastIndex+v+divideVVal+(u*divideVVal));
						finPolyConnectArray.append((hakukyuLastIndex+v+divideVVal+1)+(u*divideVVal));
						
                        //haku uv
                        hakuUvCountsArray.append(4);
                        hakuUvIdsArray.append((hakukyuLastIndex+v+1)+(u*divideVVal));
                        hakuUvIdsArray.append(hakukyuLastIndex+v+(u*divideVVal));
                        hakuUvIdsArray.append(hakukyuLastIndex+v+divideVVal+(u*divideVVal));
                        hakuUvIdsArray.append((hakukyuLastIndex+v+divideVVal+1)+(u*divideVVal));
					}
					
				}
			}
		}
		
			//pupil face
		//hakukyu index
		
		unsigned long irisLastIndex = pupilStartIndex-divideVVal;
		for (unsigned long v=0; v< divideVVal; v++) {
			if (v==divideVVal-1) {//lastShare
				finPolyCountArray.append(4);
				finPolyConnectArray.append(irisLastIndex);
				finPolyConnectArray.append(irisLastIndex+v);
				finPolyConnectArray.append(irisLastIndex+v+divideVVal);
				finPolyConnectArray.append((irisLastIndex+divideVVal));
				
                //haku uv
                hakuUvCountsArray.append(4);
                hakuUvIdsArray.append(irisLastIndex);
                hakuUvIdsArray.append(irisLastIndex+v);
                hakuUvIdsArray.append(irisLastIndex+v+divideVVal);
                hakuUvIdsArray.append((irisLastIndex+divideVVal));
			} else {
				finPolyCountArray.append(4);
				finPolyConnectArray.append((irisLastIndex+v+1));
				finPolyConnectArray.append(irisLastIndex+v);
				finPolyConnectArray.append(irisLastIndex+v+divideVVal);
				finPolyConnectArray.append((irisLastIndex+v+divideVVal+1));
				
                
                //haku uv
                hakuUvCountsArray.append(4);
                hakuUvIdsArray.append(irisLastIndex+v+1);
                hakuUvIdsArray.append(irisLastIndex+v);
                hakuUvIdsArray.append(irisLastIndex+v+divideVVal);
                hakuUvIdsArray.append((irisLastIndex+v+divideVVal+1));
			}
			
		}
		
		
		//pupil deep
		
		unsigned long pupilLastIndex = DeepPupilStartIndex-divideVVal;
		for (unsigned long v=0; v< divideVVal; v++) {
			if (v==divideVVal-1) {//lastShare
				finPolyCountArray.append(4);
				finPolyConnectArray.append(pupilLastIndex);
				finPolyConnectArray.append(pupilLastIndex+v);
				finPolyConnectArray.append(pupilLastIndex+v+divideVVal);
				finPolyConnectArray.append((pupilLastIndex+divideVVal));
				
                //haku uv
                hakuUvCountsArray.append(4);
                hakuUvIdsArray.append(pupilLastIndex);
                hakuUvIdsArray.append(pupilLastIndex+v);
                hakuUvIdsArray.append(pupilLastIndex+v+divideVVal);
                hakuUvIdsArray.append((pupilLastIndex+divideVVal));
			} else {
				finPolyCountArray.append(4);
				finPolyConnectArray.append((pupilLastIndex+v+1));
				finPolyConnectArray.append(pupilLastIndex+v);
				finPolyConnectArray.append(pupilLastIndex+v+divideVVal);
				finPolyConnectArray.append((pupilLastIndex+v+divideVVal+1));
				
                //haku uv
                hakuUvCountsArray.append(4);
                hakuUvIdsArray.append(pupilLastIndex+v+1);
                hakuUvIdsArray.append(pupilLastIndex+v);
                hakuUvIdsArray.append(pupilLastIndex+v+divideVVal);
                hakuUvIdsArray.append((pupilLastIndex+v+divideVVal+1));
			}
			
		}
		
		//add cap pupil
		if (isCapPupilVal) {
			unsigned long lastIndex = hakuVtxArray.length()-1;
			for (unsigned long i=0; i<divideVVal; i++) {//front triangle
				if (i==(divideVVal-1)) {//lastShare
					finPolyCountArray.append(3);
					finPolyConnectArray.append(lastIndex);
					finPolyConnectArray.append(lastIndex-divideVVal);
					finPolyConnectArray.append(lastIndex-divideVVal+i);
                    
                    //haku uv
                    hakuUvCountsArray.append(3);
                    hakuUvIdsArray.append(lastIndex);
                    hakuUvIdsArray.append(lastIndex-divideVVal);
                    hakuUvIdsArray.append(lastIndex-divideVVal+i);
                    
                    
				} else {
					finPolyCountArray.append(3);
					finPolyConnectArray.append(lastIndex);
					finPolyConnectArray.append(lastIndex-divideVVal+i+1);
					finPolyConnectArray.append(lastIndex-divideVVal+i);
                    
                    
                    //haku uv
                    hakuUvCountsArray.append(3);
                    hakuUvIdsArray.append(lastIndex);
                    hakuUvIdsArray.append(lastIndex-divideVVal+i+1);
                    hakuUvIdsArray.append(lastIndex-divideVVal+i);
				}
			}
		}
		
#pragma mark lenz tip index
			unsigned long lenzLastIndex = LenzVtxArray.length()-1;
			
			if (divideVVal>4) {
            int quadDiv = (divideVVal/4);
            const std::size_t totalvtx = meshPlan.lensTipPointCount;
			
			
            unsigned long quadFirstIndex = LenzVtxArray.length()-totalvtx;
			
			
			
            for (int y=0; y<quadDiv; y++) {
                for (int x=0; x<quadDiv; x++) {
                    LenzPolyCountArray.append(4);
                    
                    LenzPolyConnectArray.append(quadFirstIndex+x+(y*(quadDiv+1)));
					LenzPolyConnectArray.append(quadFirstIndex+x+(y*(quadDiv+1))+(quadDiv+1));
					LenzPolyConnectArray.append(quadFirstIndex+x+1+(y*(quadDiv+1))+(quadDiv+1));
                    LenzPolyConnectArray.append(quadFirstIndex+x+1+(y*(quadDiv+1)));
                    
                    
                    
                    //lenz uv
                    lenzUvCountsArray.append(4);
                    lenzUvIdsArray.append(quadFirstIndex+x+(y*(quadDiv+1)));
                    lenzUvIdsArray.append(quadFirstIndex+x+(y*(quadDiv+1))+(quadDiv+1));
                    lenzUvIdsArray.append(quadFirstIndex+x+1+(y*(quadDiv+1))+(quadDiv+1));
                    lenzUvIdsArray.append(quadFirstIndex+x+1+(y*(quadDiv+1)));
                    
                }
            }
			
			}else {
			LenzPolyCountArray.append(4);
			
			LenzPolyConnectArray.append(lenzLastIndex);
			LenzPolyConnectArray.append(lenzLastIndex-1);
			LenzPolyConnectArray.append(lenzLastIndex-2);
			LenzPolyConnectArray.append(lenzLastIndex-3);
			
            
            //lenz uv
            lenzUvCountsArray.append(4);
            lenzUvIdsArray.append(lenzLastIndex);
            lenzUvIdsArray.append(lenzLastIndex-1);
            lenzUvIdsArray.append(lenzLastIndex-2);
            lenzUvIdsArray.append(lenzLastIndex-3);
			}
#pragma mark remap uv value
	        double diameter = radiusVal*2.0;
	        diameter += (uvPaddingVal*radiusVal*2.0);
	        if (!std::isfinite(diameter) || diameter <= 1.0e-12) {
	            return MS::kInvalidParameter;
	        }
        double offset = 0.5;
        for (size_t i=0,N=hakuUCoords.length(); i<N; ++i) {
            
            hakuUCoords[i] = hakuUCoords[i]/diameter+offset;
            hakuVCoords[i] = hakuVCoords[i]/diameter+offset;
        }
        
        for (size_t i=0,N=lenzUCoords.length(); i<N; ++i) {
            
            lenzUCoords[i] = lenzUCoords[i]/diameter+offset;
            lenzVCoords[i] = lenzVCoords[i]/diameter+offset;
        }
        
#pragma mark radius ramp calc
        
        for (size_t i=0, N=LenzVtxArray.length(); i<N; ++i) {
            
            MFloatPoint *p = &LenzVtxArray[i];
	            double ratio = (p->z+radiusVal)/(radiusVal*2.0);
	            if (!std::isfinite(ratio)) return MS::kInvalidParameter;
	            ratio = std::max(0.0, std::min(1.0, ratio));
	            float val = 0.0f;
	            if (!dommy::maya::sampleFiniteRamp(
	                    radiusRampAttr, 1.0-ratio, val)) {
	                return MS::kFailure;
	            }
            
            p->x = p->x*val;
            p->y = p->y*val;
            
            
        }
        
        for (size_t i=0, N=hakuVtxArray.length(); i<N; ++i) {
            
            MFloatPoint *p = &hakuVtxArray[i];
	            double ratio = (p->z+eyeRadius)/(eyeRadius*2.0);
	            if (!std::isfinite(ratio)) return MS::kInvalidParameter;
	            ratio = std::max(0.0, std::min(1.0, ratio));
	            float val = 0.0f;
	            if (!dommy::maya::sampleFiniteRamp(
	                    radiusRampAttr, 1.0-ratio, val)) {
	                return MS::kFailure;
	            }
            
            p->x = p->x*val;
            p->y = p->y*val;
            
            
        }
#pragma mark set output mesh
		
		if (plug == outLenzMesh) {
	            return publishGeneratedMesh(
	                data, plug, outLenzMesh, meshPlan.lens,
                LenzVtxArray, LenzPolyCountArray, LenzPolyConnectArray,
                lenzUCoords, lenzVCoords, lenzUvCountsArray, lenzUvIdsArray);
			
		}
		if (plug == outIrisMesh) {
	            return publishGeneratedMesh(
	                data, plug, outIrisMesh, meshPlan.iris,
                hakuVtxArray, finPolyCountArray, finPolyConnectArray,
                hakuUCoords, hakuVCoords, hakuUvCountsArray, hakuUvIdsArray);
		}
		
		
		
		
		
        
        
        
		
		
		return MS::kUnknownParameter;
		
	}
	
		return MS::kUnknownParameter;
	} DOMMY_COMPUTE_CATCH("DHEyeGen")
		
}





MStatus DHEyeGen::initialize() try
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute   tAttr;
	
	MStatus				stat;
	
	divideU = nAttr.create( "divideU", "divu", MFnNumericData::kLong, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 9 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(60));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(divideU));
	
	divideV= nAttr.create( "divideV", "divv", MFnNumericData::kLong, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 9 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(60));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(divideV));
	
	radius = nAttr.create( "radius", "rad", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 4.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(radius));
    
    radiusRamp = MRampAttribute::createCurveRamp("radiusRamp", "radrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(radiusRamp));
	
	innerRadius = nAttr.create( "innerRadius", "inrad", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(180.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(innerRadius));
	
	lenzGap = nAttr.create( "lenzGap", "leg", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.01 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(lenzGap));
	
		lenzRamp = MRampAttribute::createCurveRamp("lenzRamp", "lera", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(lenzRamp));
	
	
	lenzsOffset = nAttr.create( "lenzsOffset", "leof", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.097 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(lenzsOffset));
	
	irisSize = nAttr.create( "irisSize", "irs", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.25 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(irisSize));
    
    irisDivide = nAttr.create( "irisDivide", "ird", MFnNumericData::kLong, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 4 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(irisDivide));
    
    
    irisRamp = MRampAttribute::createCurveRamp("irisRamp", "irra", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(irisRamp));
    
    irisOffset = nAttr.create( "irisOffset", "irom", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( -0.020 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(irisOffset));
    
	pupilSize = nAttr.create( "pupilSize", "pus", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(pupilSize));
    
    pupilDepth = nAttr.create( "pupilDepth", "pupd", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.4 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(pupilDepth));
    
    pupilMotion = nAttr.create( "pupilMotion", "pupm", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(pupilMotion));
    
    
    isCapPupil = nAttr.create( "isCapPupil", "isp", MFnNumericData::kBoolean, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isCapPupil));
    
    
    uvPadding= nAttr.create( "uvPadding", "uvpad", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.01 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(uvPadding));
    
	
	outLenzMesh = tAttr.create("outLenzMesh", "olm", MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outLenzMesh));
	
	outIrisMesh = tAttr.create("outIrisMesh", "oim", MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outIrisMesh));
		DOMMY_RETURN_IF_FAILURE(attributeAffects( divideU, outLenzMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( divideV, outLenzMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( radius, outLenzMesh ));
	    DOMMY_RETURN_IF_FAILURE(attributeAffects( radiusRamp, outLenzMesh ));
		DOMMY_RETURN_IF_FAILURE(attributeAffects( innerRadius, outLenzMesh ));
		
		DOMMY_RETURN_IF_FAILURE(attributeAffects( lenzGap, outLenzMesh ));
		DOMMY_RETURN_IF_FAILURE(attributeAffects( lenzRamp, outLenzMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( lenzsOffset, outLenzMesh ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( uvPadding, outLenzMesh ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( irisSize, outLenzMesh ));
		DOMMY_RETURN_IF_FAILURE(attributeAffects( divideU, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( divideV, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( radius, outIrisMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( radiusRamp, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( innerRadius, outIrisMesh ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( lenzGap, outIrisMesh ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( irisSize, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( irisDivide, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( irisRamp, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( irisOffset, outIrisMesh ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( pupilSize, outIrisMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( pupilDepth, outIrisMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( pupilMotion, outIrisMesh ));
   
	DOMMY_RETURN_IF_FAILURE(attributeAffects( isCapPupil, outIrisMesh ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( uvPadding, outIrisMesh ));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DHEyeGen", "initialize", std::current_exception());
}

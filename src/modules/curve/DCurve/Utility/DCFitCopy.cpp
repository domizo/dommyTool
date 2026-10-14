//
//  DCFitCopy.cpp
//  dommyTool
//
//  Created by yuzo on 3/5/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCFitCopy.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "DCurveCopyBudget.h"
#include "DCurveCopyMath.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DRandomValueGeneration.h"
#include "DLogConfig.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "DNodeOperation.h"

#include "dommyMath.h"

#include <cmath>

namespace {

void DGenerateRandomValue(int seed,
                          float minimum,
                          float maximum,
                          std::size_t count,
                          DScalarDataList& output)
{
    dommy::random::generateFloatValues(
        static_cast<std::uint32_t>(seed), minimum, maximum, count,
        output);
}

} // namespace


const MTypeId DCFitCopy::typeId( ID_DC_FIT_COPY );

MObject DCFitCopy::inCopyObject;


MObject DCFitCopy::inObject;

MObject DCFitCopy::inReferenceObject;

MObject DCFitCopy::aCountU;
MObject DCFitCopy::aCountV;


MObject DCFitCopy::aPadding;
MObject DCFitCopy::aRandomPadding;

MObject DCFitCopy::aConnectType;
MObject DCFitCopy::aConnectUVSwitch;

MObject DCFitCopy::aTangentMode;
MObject DCFitCopy::aTangentVector;

MObject DCFitCopy::aUniformScale;
MObject DCFitCopy::aScale;

MObject DCFitCopy::aSeed;
MObject DCFitCopy::aRandomScale;
MObject DCFitCopy::aRandomRotate;
MObject DCFitCopy::aIsRandScaleAbsolute;
MObject DCFitCopy::aIsRandRotateabsolute;

MObject DCFitCopy::outDC;


void *DCFitCopy::creator()
{
    return ::dommy::maya::createObject("DCFitCopy", [&]() {
        return new DCFitCopy();
    });
}



MStatus DCFitCopy::compute(const MPlug& plug, MDataBlock& data) try
{
	
	MStatus stat;
	
	if (plug == outDC) {
		
		
		
		const bool VERBOSE = DOMMY_ENABLE_DEBUG_LOGS != 0;
		
		if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "DCFitCopy::compute");
		
		MPlug inCopyObjPlug(thisMObject(), inCopyObject);
		
		const bool copyConnected = inCopyObjPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (!copyConnected) {
			EchoWarning(name().asChar(), "Not connected inCopyObject plug. Please connect nurbsCurve data.");
			return MS::kFailure;
		}
        
        MObject copyCrvObj;
        
		stat = DGetMayaCurveFromPlug(inCopyObject, data, copyCrvObj);
		if (!stat) {
			EchoWarning(name().asChar(), "inCopyObject suppport nurbs curve only.");
			return stat;
		}
		
		MFnNurbsCurve copyCrvFn(copyCrvObj, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		//copy object data
		MPointArray copyCvs;
		MDoubleArray copyKnots;
		const int copyDegree = copyCrvFn.degree(&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		DOMMY_RETURN_IF_FAILURE(
			copyCrvFn.getCVs(copyCvs, MSpace::kObject));
		DOMMY_RETURN_IF_FAILURE(copyCrvFn.getKnots(copyKnots));
		if (copyCvs.length() == 0) {
			EchoWarning(name().asChar(), "The copy curve has no control vertices.");
			return MS::kFailure;
		}
		const DCurveInternalData copyCurveData(
			copyCvs, copyKnots, copyDegree);
		if (!copyCurveData.hasValidCurveTopology()) {
			return MS::kInvalidParameter;
		}
		
        std::vector<std::array<double, 2> > copyParamsUV;
        if (!dommy::curve::normalizedCopyCoordinates(copyCvs, copyParamsUV))
            return MS::kInvalidParameter;

#pragma mark attribute
		
		const MDataHandle countUHandle = data.inputValue(aCountU, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const int countU = countUHandle.asInt();
		const MDataHandle countVHandle = data.inputValue(aCountV, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const int countV = countVHandle.asInt();
		
		const MDataHandle paddingUVHandle = data.inputValue(aPadding, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float2 &paddingUV = paddingUVHandle.asFloat2();
		const MDataHandle randPaddingUVHandle = data.inputValue(aRandomPadding, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float2 &randPaddingUV = randPaddingUVHandle.asFloat2();
		
		const MDataHandle connectTypeHandle = data.inputValue(aConnectType, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const short connectType = connectTypeHandle.asShort();
		const MDataHandle uvSwitchHandle = data.inputValue(aConnectUVSwitch, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const bool uvSwitch = uvSwitchHandle.asBool();
		
		const MDataHandle tangentModeHandle = data.inputValue(aTangentMode, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const short tangentMode = tangentModeHandle.asShort();
		const MDataHandle tangentVecHandle = data.inputValue(aTangentVector, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float3 &tangentVec = tangentVecHandle.asFloat3();
		
		const MDataHandle uniScaleHandle = data.inputValue(aUniformScale, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float uniScale = uniScaleHandle.asFloat();
		const MDataHandle scaleHandle = data.inputValue(aScale, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float3 &scale = scaleHandle.asFloat3();
		
		const MDataHandle rawSeedHandle = data.inputValue(aSeed, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const int rawSeed = rawSeedHandle.asInt();
        const int seed = dommy::random::requireOffsetNonnegativeSeed(rawSeed, 0U);
        const int seedY = dommy::random::requireOffsetNonnegativeSeed(seed, 1U);
        const int seedZ = dommy::random::requireOffsetNonnegativeSeed(seed, 2U);
		const MDataHandle rndScaleHandle = data.inputValue(aRandomScale, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float3 &rndScale = rndScaleHandle.asFloat3();
		const MDataHandle rndRotHandle = data.inputValue(aRandomRotate, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const float3 &rndRot = rndRotHandle.asFloat3();
		const MDataHandle israndSclAbsHandle = data.inputValue(aIsRandScaleAbsolute, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const bool israndSclAbs = israndSclAbsHandle.asBool();
		const MDataHandle israndRotAbsHandle = data.inputValue(aIsRandRotateabsolute, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const bool israndRotAbs = israndRotAbsHandle.asBool();
		std::size_t checkedCountU = 0U;
		std::size_t checkedCountV = 0U;
		std::size_t gridInstanceCount = 0U;
		if (!dommy::curve::checkedCurveCopyCounts(
				countU, countV, checkedCountU, checkedCountV,
				gridInstanceCount) ||
			(connectType != 0 && connectType != 1) || tangentMode < 0 ||
			tangentMode > 2 || !std::isfinite(uniScale)) {
			return MS::kInvalidParameter;
		}
		for (unsigned int axis = 0U; axis < 3U; ++axis) {
			if (!std::isfinite(tangentVec[axis]) ||
				!std::isfinite(scale[axis]) ||
				!std::isfinite(rndScale[axis]) ||
				!std::isfinite(rndRot[axis])) {
				return MS::kInvalidParameter;
			}
		}
		for (unsigned int axis = 0U; axis < 2U; ++axis) {
			if (!std::isfinite(paddingUV[axis]) ||
				!std::isfinite(randPaddingUV[axis])) {
				return MS::kInvalidParameter;
			}
		}
		if (tangentMode == 1 &&
			MVector(tangentVec).length() <= 1.0e-12) {
			return MS::kInvalidParameter;
		}
		if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "countU:" << countU);
		if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "countV:" << countV);
		if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "connectType:" << connectType);
		if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "scale:" << scale[0] << " " << scale[1] << " " << scale[2]);
		
		
		
		
		//random scale
		bool useRandSX,useRandSY,useRandSZ;
		useRandSX=useRandSY=useRandSZ = false;
		if(rndScale[0]) useRandSX = true;
		if(rndScale[1]) useRandSY = true;
		if(rndScale[2]) useRandSZ = true;
		
		//random rotate
		bool useRandRX,useRandRY,useRandRZ;
		useRandRX=useRandRY=useRandRZ = false;
		if(rndRot[0]) useRandRX = true;
		if(rndRot[1]) useRandRY = true;
		if(rndRot[2]) useRandRZ = true;
		
		
		
		//get inObject for copy
		MPlug inObjPlug(thisMObject(), inObject);
		
		
#pragma mark checkReference
		MPlug refObjPlug(thisMObject(), inReferenceObject);
		bool useCrvRefObj = false;
		bool useSrfRefObj = false;
		MFnNurbsSurface refSrfRef;
		MFnNurbsCurve refCrvRef;
		const bool referenceConnected = refObjPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (referenceConnected) {
			
			MDataHandle refDataHnd = data.inputValue(inReferenceObject,&stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			if (!dommy::maya::isFiniteMatrix(refDataHnd.geometryTransformMatrix())) return MS::kInvalidParameter;
			if (refDataHnd.type() == MFnData::kNurbsCurve) {
				const MObject object = refDataHnd.asNurbsCurveTransformed();
				if (object.isNull()) return MS::kInvalidParameter;
				stat = refCrvRef.setObject(object);
				DOMMY_RETURN_IF_FAILURE(stat);
				useCrvRefObj = true;
			} else if (refDataHnd.type() == MFnData::kNurbsSurface) {
				const MObject object = refDataHnd.asNurbsSurfaceTransformed();
				if (object.isNull()) return MS::kInvalidParameter;
				stat = refSrfRef.setObject(object);
				DOMMY_RETURN_IF_FAILURE(stat);
				useSrfRefObj = true;
			} else {
				return MS::kInvalidParameter;
			}
		}
		
		
#pragma mark create output data
		MFnPluginData pluginData;
		DCurveData* outDCrvs = NULL;
		DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
			pluginData, DCurveData::id, outDCrvs));
        
        
        if (outDCrvs) {
            DCurveOutputBatch generatedCurves;
            
            
            const bool inputConnected = inObjPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (inputConnected && countU) {
                
#pragma mark get -----------------------input object-----------------------
                
#pragma mark nurbs curve
                MDataHandle dataHnd = data.inputValue(inObject,&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!dommy::maya::isFiniteMatrix(dataHnd.geometryTransformMatrix()) ||
                    (dataHnd.type() != MFnData::kNurbsCurve && dataHnd.type() != MFnData::kNurbsSurface))
                    return MS::kInvalidParameter;
                if (dataHnd.type() == MFnData::kNurbsCurve) {
					std::size_t maximumOutputCvCount = 0U;
					if (!dommy::curve::checkedCurveCopyBudget(
							checkedCountU, copyCvs.length(),
							maximumOutputCvCount)) {
						return MS::kInvalidParameter;
					}
                    
                    MFnNurbsCurve crvFn;
                    const MObject curveObject = dataHnd.asNurbsCurveTransformed();
                    if (curveObject.isNull()) return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(crvFn.setObject(curveObject));
                    
                    //                double curveLength = crvFn.length();
                    //                if(VERBOSE) cout << "curveLength:" << curveLength << endl;
                    
                    double start = 0.0, end = 0.0;
                    DOMMY_RETURN_IF_FAILURE(crvFn.getKnotDomain(start, end));
                    if (!dommy::curve::validCopyParameterDomain(start, end)) return MS::kInvalidParameter;
                    
                    if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "start:" << start << " end:" << end);

                    
                    
#pragma mark create random
                    
                    //random padding
                    DScalarDataList rndPaddingValues(countU,randPaddingUV[0]);
                    if(randPaddingUV[0]!=0.0) DGenerateRandomValue(seed, -randPaddingUV[0], randPaddingUV[0], countU, rndPaddingValues);
                    
                    
                    //randam scale
                    DScalarDataList rndScaleXValues(countU,0.0);
                    DScalarDataList rndScaleYValues(countU,0.0);
                    DScalarDataList rndScaleZValues(countU,0.0);
                    
                    if(useRandSX && rndScale[0]!=0.0) DGenerateRandomValue(seed, israndSclAbs?0.0f:-rndScale[0], rndScale[0], countU, rndScaleXValues);
                    if(useRandSY && rndScale[1]!=0.0) DGenerateRandomValue(seedY, israndSclAbs?0.0f:-rndScale[1], rndScale[1], countU, rndScaleYValues);
                    if(useRandSZ && rndScale[2]!=0.0) DGenerateRandomValue(seedZ, israndSclAbs?0.0f:-rndScale[2], rndScale[2], countU, rndScaleZValues);
                    
                    //random rotate
                    DScalarDataList rndRotXValues(countU,0.0);
                    DScalarDataList rndRotYValues(countU,0.0);
                    DScalarDataList rndRotZValues(countU,0.0);
                    
                    if(useRandRX && rndRot[0]!=0.0) DGenerateRandomValue(seed, israndRotAbs?0.0f:-rndRot[0], rndRot[0], countU, rndRotXValues);
                    if(useRandRY && rndRot[1]!=0.0) DGenerateRandomValue(seedY, israndRotAbs?0.0f:-rndRot[1], rndRot[1], countU, rndRotYValues);
                    if(useRandRZ && rndRot[2]!=0.0) DGenerateRandomValue(seedZ, israndRotAbs?0.0f:-rndRot[2], rndRot[2], countU, rndRotZValues);
                    
                    
#pragma mark create transform info
                    MVector oldBiNormal;
                    MQuaternion xQuat,yQuat;
                    
                    std::vector<std::vector<MMatrix> > matrixArray(countU);
                    
                    const MVector xAxisVec(0.0,1.0,0.0);
                    const MVector yAxisVec(1.0,0.0,0.0);
                    
                    //                double offset = 0.0;
                    
                    for (std::size_t c = 0U; c < checkedCountU; ++c) {

                        std::vector<MMatrix> crvMatArray(copyParamsUV.size());
                        
                        for(size_t p=0;p<copyParamsUV.size();++p){
                            double offsetedParam = 0.0;
                            if (!dommy::curve::checkedCopyParameter(
                                    start, end, checkedCountU, c, paddingUV[0],
                                    rndPaddingValues[c], copyParamsUV[p][0], offsetedParam))
                                return MS::kInvalidParameter;

                            MPoint cv;
                            DOMMY_RETURN_IF_FAILURE(crvFn.getPointAtParam(offsetedParam, cv, MSpace::kObject));
                            if (!dommy::maya::isFinitePoint(cv)) return MS::kInvalidParameter;
                            
                            MVector    normal;
                            MVector    tangent = crvFn.tangent(offsetedParam , MSpace::kObject , &stat);
                            DOMMY_RETURN_IF_FAILURE(stat);
                            
                            if (tangentMode==1) {//Simple vector
                                normal= MVector(tangentVec);
                                
                            } else if (tangentMode==2 && useCrvRefObj) {//input Curve
                                MPoint targetCV;
                                double targetParam = 0.0;
                                refCrvRef.closestPoint(cv,&targetParam,kMFnNurbsEpsilon,MSpace::kObject,&stat);
                                DOMMY_RETURN_IF_FAILURE(stat);
                                if (!std::isfinite(targetParam)) return MS::kInvalidParameter;
                                DOMMY_RETURN_IF_FAILURE(refCrvRef.getPointAtParam(targetParam, targetCV, MSpace::kObject));
                                normal = targetCV-cv;
                            } else if (tangentMode==2 && useSrfRefObj) {//input Surface
                                MPoint targetCV;
                                double targetParamU = 0.0;
                                double targetParamV = 0.0;
                                refSrfRef.closestPoint(cv, &targetParamU, &targetParamV, false,
                                        kMFnNurbsEpsilon, MSpace::kObject, &stat);
                                DOMMY_RETURN_IF_FAILURE(stat);
                                if (!std::isfinite(targetParamU) || !std::isfinite(targetParamV))
                                    return MS::kInvalidParameter;
                                normal = refSrfRef.normal(targetParamU, targetParamV, MSpace::kObject, &stat);
                                DOMMY_RETURN_IF_FAILURE(stat);
                                
                            } else {
                                if (c==0 && p==0) {
                                    normal= crvFn.normal( offsetedParam , MSpace::kObject , &stat );
                                    DOMMY_RETURN_IF_FAILURE(stat);
                                } else {
                                    normal = oldBiNormal ^ tangent;
                                }
                            }
                            
                            if (!dommy::maya::isFiniteVector(normal) ||
                                !dommy::maya::isFiniteVector(tangent)) return MS::kInvalidParameter;
                            DOMMY_RETURN_IF_FAILURE(normal.normalize());
                            DOMMY_RETURN_IF_FAILURE(tangent.normalize());
                            
                            MVector biNormal((tangent ^ normal));//targetZ
                            DOMMY_RETURN_IF_FAILURE(biNormal.normalize());
                            
                            yQuat = yAxisVec.rotateTo(tangent);
                            MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
                            xQuat = xPostPosVec.rotateTo(normal);
                            
                            MTransformationMatrix transMat;
                            //                        double scaleD[3] = {(scale[0]*uniScale+(*rndScaleXValues)[c]),
                            //                                            (scale[1]*uniScale+(*rndScaleYValues)[c]),
                            //                                            (scale[2]*uniScale+(*rndScaleZValues)[c])};
                            //
                            //                        transMat.addScale(scaleD, MSpace::kObject);
                            
                            //add rand rot
                            //                        double randRotXYZ[3] = {dommy::math::degreesToRadians((*rndRotXValues)[c]), dommy::math::degreesToRadians((*rndRotYValues)[c]), dommy::math::degreesToRadians((*rndRotZValues)[c])};
                            //                        transMat.addRotation(randRotXYZ, MTransformationMatrix::kXYZ, MSpace::kObject);
                            //                        preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kYZX);
                            //                        pt.setCast(preRot);
                            
                            //add align curve
                            DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(xQuat.x, xQuat.y, xQuat.z, xQuat.w, MSpace::kObject));
                            DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(yQuat.x, yQuat.y, yQuat.z, yQuat.w, MSpace::kObject));
                            
                            //add move curve position
                            if (!dommy::maya::isFinitePoint(cv)) return MS::kInvalidParameter;
                            DOMMY_RETURN_IF_FAILURE(transMat.addTranslation(MVector(cv), MSpace::kWorld));
                            if (!dommy::maya::isFiniteMatrix(transMat.asMatrix())) return MS::kInvalidParameter;
                            
                            crvMatArray[p] = transMat.asMatrix();
                            
                            oldBiNormal = biNormal;
                        }
                        
                        matrixArray[c] = crvMatArray;
                    }
                    
                    
                    if (matrixArray.size()) {
                        
                        size_t indexOffset = 0;
                        
                        if(connectType==0) {
                            
                            //split mode
                            
                            for(uint i=0;i<matrixArray.size();i++)
                            {
                                MPointArray cvs;
                                DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(copyCvs, cvs));
                                
                                if (matrixArray[i].size() != cvs.length()) return MS::kFailure;
                                if(matrixArray[i].size()==cvs.length()){
                                    for(uint p=0;p<cvs.length();p++)
                                    {
                                        MFloatPoint pt = cvs[p];
                                        
                                        //set scale
                                        pt.x *= scale[0] * uniScale + (rndScaleXValues)[indexOffset];
                                        pt.y *= scale[1] * uniScale + (rndScaleYValues)[indexOffset];
                                        pt.z *= scale[2] * uniScale + (rndScaleZValues)[indexOffset];
                                        
                                        
                                        //set rotate
                                        MVector preRot(pt);
                                        
                                        double randRotXYZ[3] = {dommy::math::degreesToRadians((rndRotXValues)[indexOffset]),dommy::math::degreesToRadians((rndRotYValues)[indexOffset]),dommy::math::degreesToRadians((rndRotZValues)[indexOffset])};
                                        preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kYZX);
                                        pt.setCast(preRot);
                                        
                                        
                                        cvs[p] = MPoint(pt)*matrixArray[i][p];
                                    }
                                }
                                
                                indexOffset++;
                                
                                if (!generatedCurves.append(DCItem(new DCurveInternalData(cvs,copyKnots,copyDegree)))) return MS::kFailure;
                                
                            }
                            
                            
                        } else {
                            
                            //connect mode
                            
                            MPointArray totalCvs;
                            
                            for(uint i=0;i<matrixArray.size();i++)
                            {
                                
                                if (matrixArray[i].size() != copyCvs.length()) return MS::kFailure;
                                if(matrixArray[i].size()==copyCvs.length()){
                                    for(uint p=0;p<copyCvs.length();p++)
                                    {
                                        MFloatPoint pt = copyCvs[p];
                                        
                                        //set scale
                                        pt.x *= scale[0] * uniScale + (rndScaleXValues)[indexOffset];
                                        pt.y *= scale[1] * uniScale + (rndScaleYValues)[indexOffset];
                                        pt.z *= scale[2] * uniScale + (rndScaleZValues)[indexOffset];
                                        
                                        //set rotate
                                        MVector preRot(pt);
                                        
                                        double randRotXYZ[3] = {dommy::math::degreesToRadians((rndRotXValues)[indexOffset]),dommy::math::degreesToRadians((rndRotYValues)[indexOffset]),dommy::math::degreesToRadians((rndRotZValues)[indexOffset])};
                                        preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kYZX);
                                        pt.setCast(preRot);
                                        
                                        
                                        
                                        MPoint finPt = MPoint(pt)*matrixArray[i][p];
                                        
                                        DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));
                                        
                                        if (i>0 && p==0) DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));//first multi point
                                        
                                        if (i + 1U != matrixArray.size() &&
										p == (copyCvs.length()-1)) {
                                            DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));
                                        }
                                    }
                                }
                                
                                indexOffset++;
                            }
                            
                            MDoubleArrayPtr knots=MDoubleArrayPtr(new MDoubleArray());
                            GenerateDCurveKnots(totalCvs.length(), copyDegree, *knots);
                            
                            if (!generatedCurves.append(DCItem(new DCurveInternalData(totalCvs,*knots,copyDegree)))) return MS::kFailure;
                            
                        }//split or connect type
                        
                    }
                    
                    
                    
                } else if ((dataHnd.type() == MFnData::kNurbsSurface) && gridInstanceCount != 0U) {
					std::size_t maximumOutputCvCount = 0U;
					if (!dommy::curve::checkedCurveCopyBudget(
							gridInstanceCount, copyCvs.length(),
							maximumOutputCvCount)) {
						return MS::kInvalidParameter;
					}
                    
#pragma mark nurbs surface
                    
                    MFnNurbsSurface srfFn;
                    const MObject surfaceObject = dataHnd.asNurbsSurfaceTransformed();
                    if (surfaceObject.isNull()) return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(srfFn.setObject(surfaceObject));
                    
                    double startU = 0.0, endU = 0.0, startV = 0.0, endV = 0.0;
                    DOMMY_RETURN_IF_FAILURE(srfFn.getKnotDomain(startU, endU, startV, endV));
                    if (!dommy::curve::validCopyParameterDomain(startU, endU) ||
                        !dommy::curve::validCopyParameterDomain(startV, endV))
                        return MS::kInvalidParameter;
                    
                    if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "startU:" << startU << " endU:" << endU);
                    if(VERBOSE) DOMMY_MAYA_DEBUG("DCFitCopy", "startV:" << startV << " endV:" << endV);


                    
                    
#pragma mark create transform info
                    
                    MQuaternion xQuat,yQuat;
                    const MVector xAxisVec(0.0,1.0,0.0);
                    const MVector yAxisVec(1.0,0.0,0.0);
                    
                    //test
                    typedef std::shared_ptr<std::vector<std::vector<std::vector<MMatrix> > >> DDMatrixArrayPtr;
                    DDMatrixArrayPtr matrixArray = DDMatrixArrayPtr(new std::vector<std::vector<std::vector<MMatrix> > >());
                    
                    //                double offsetU = 0.0;
                    //                double offsetV = 0.0;
                    
                    
                    
                    
                    const size_t dupCount = gridInstanceCount;
                    
                    
                    //random padding
                    DScalarDataList rndPaddingUValues(dupCount, randPaddingUV[0]);
                    if(randPaddingUV[0]!=0.0) DGenerateRandomValue(seed, -randPaddingUV[0], randPaddingUV[0], dupCount, rndPaddingUValues);
                    DScalarDataList rndPaddingVValues(dupCount, randPaddingUV[1]);
                    if(randPaddingUV[1]!=0.0) DGenerateRandomValue(seedY, -randPaddingUV[1], randPaddingUV[1], dupCount, rndPaddingVValues);
                    
                    
                    
                    if(!uvSwitch) {
                        
                        size_t offset = 0;
                        
                        for (std::size_t u = 0U;
							 u < checkedCountU; ++u) {
                            
                            std::vector<std::vector<MMatrix> > matArray(countV);

                            
                            for (std::size_t v = 0U;
								 v < checkedCountV; ++v) {

                                
                                
                                
                                
                                std::vector<MMatrix> cvMatArray(copyParamsUV.size());
                                
                                for(size_t p=0;p<copyParamsUV.size();++p){
                                    double paramU = 0.0, paramV = 0.0;
                                    if (!dommy::curve::checkedCopyParameter(
                                            startU, endU, checkedCountU, u, paddingUV[0],
                                            rndPaddingUValues[offset], copyParamsUV[p][0], paramU) ||
                                        !dommy::curve::checkedCopyParameter(
                                            startV, endV, checkedCountV, v, paddingUV[1],
                                            rndPaddingVValues[offset], copyParamsUV[p][1], paramV))
                                        return MS::kInvalidParameter;

                                    
                                    
                                    MPoint posPt;
                                    MVector uVec, vVec;
                                    
                                    DOMMY_RETURN_IF_FAILURE(srfFn.getDerivativesAtParm(paramU, paramV, posPt, uVec, vVec, MSpace::kObject));
                                    if (!dommy::maya::isFinitePoint(posPt)) return MS::kInvalidParameter;
                                    MVector normal = srfFn.normal(paramU, paramV, MSpace::kObject, &stat);
                                    DOMMY_RETURN_IF_FAILURE(stat);
                                    MVector tangent = uVec;
                                    
                                    if (tangentMode==1) {//Simple vector
                                        normal= MVector(tangentVec);
                                        
                                    } else if (tangentMode==2 && useSrfRefObj) {//input Surface
                                        MPoint targetCV;
                                        double targetParamU = 0.0;
                                        double targetParamV = 0.0;
                                        refSrfRef.closestPoint(posPt, &targetParamU, &targetParamV, false,
                                            kMFnNurbsEpsilon, MSpace::kObject, &stat);
                                        DOMMY_RETURN_IF_FAILURE(stat);
                                        if (!std::isfinite(targetParamU) || !std::isfinite(targetParamV))
                                            return MS::kInvalidParameter;
                                        DOMMY_RETURN_IF_FAILURE(refSrfRef.getPointAtParam(targetParamU, targetParamV, targetCV));
                                        normal = refSrfRef.normal(targetParamU, targetParamV, MSpace::kObject, &stat);
                                        DOMMY_RETURN_IF_FAILURE(stat);
                                    }
                                    
                                    if (!dommy::maya::isFiniteVector(normal) ||
                                        !dommy::maya::isFiniteVector(tangent)) return MS::kInvalidParameter;
                                    DOMMY_RETURN_IF_FAILURE(normal.normalize());
                                    DOMMY_RETURN_IF_FAILURE(tangent.normalize());
                                    
                                    
                                    yQuat = yAxisVec.rotateTo(tangent);
                                    MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
                                    xQuat = xPostPosVec.rotateTo(normal);
                                    
                                    MTransformationMatrix transMat;
                                    
                                    DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(xQuat.x, xQuat.y, xQuat.z, xQuat.w, MSpace::kObject));
                                    DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(yQuat.x, yQuat.y, yQuat.z, yQuat.w, MSpace::kObject));
                                    DOMMY_RETURN_IF_FAILURE(transMat.addTranslation(MVector(posPt), MSpace::kWorld));
                                    if (!dommy::maya::isFiniteMatrix(transMat.asMatrix())) return MS::kInvalidParameter;
                                    cvMatArray[p] = transMat.asMatrix();
                                    
                                    
                                    
                                }//cv iter
                                
                                offset++;
                                
                                matArray[v] = cvMatArray;
                            }//v coord iter
                            
                            matrixArray->push_back(matArray);
                            
                        }//u coord iter
                        
                    } else {
                        
                        
                        size_t offset = 0;
                        
                        for (std::size_t v = 0U;
							 v < checkedCountV; ++v) {
                            
                            std::vector<std::vector<MMatrix> > matArray(countU);

                            
                            for (std::size_t u = 0U;
								 u < checkedCountU; ++u) {

                                
                                
                                
                                std::vector<MMatrix> cvMatArray(copyParamsUV.size());
                                
                                for(size_t p=0;p<copyParamsUV.size();++p){
                                    double paramU = 0.0, paramV = 0.0;
                                    if (!dommy::curve::checkedCopyParameter(
                                            startU, endU, checkedCountU, u, paddingUV[0],
                                            rndPaddingUValues[offset], copyParamsUV[p][0], paramU) ||
                                        !dommy::curve::checkedCopyParameter(
                                            startV, endV, checkedCountV, v, paddingUV[1],
                                            rndPaddingVValues[offset], copyParamsUV[p][1], paramV))
                                        return MS::kInvalidParameter;

                                    MPoint posPt;
                                    MVector uVec, vVec;
                                    
                                    DOMMY_RETURN_IF_FAILURE(srfFn.getDerivativesAtParm(paramU, paramV, posPt, uVec, vVec, MSpace::kObject));
                                    if (!dommy::maya::isFinitePoint(posPt)) return MS::kInvalidParameter;
                                    MVector normal = srfFn.normal(paramU, paramV, MSpace::kObject, &stat);
                                    DOMMY_RETURN_IF_FAILURE(stat);
                                    MVector tangent = uVec;
                                    
                                    if (tangentMode==1) {//Simple vector
                                        normal= MVector(tangentVec);
                                        
                                    } else if (tangentMode==2 && useSrfRefObj) {//input Surface
                                        MPoint targetCV;
                                        double targetParamU = 0.0;
                                        double targetParamV = 0.0;
                                        refSrfRef.closestPoint(posPt, &targetParamU, &targetParamV, false,
                                            kMFnNurbsEpsilon, MSpace::kObject, &stat);
                                        DOMMY_RETURN_IF_FAILURE(stat);
                                        if (!std::isfinite(targetParamU) || !std::isfinite(targetParamV))
                                            return MS::kInvalidParameter;
                                        DOMMY_RETURN_IF_FAILURE(refSrfRef.getPointAtParam(targetParamU, targetParamV, targetCV));
                                        normal = refSrfRef.normal(targetParamU, targetParamV, MSpace::kObject, &stat);
                                        DOMMY_RETURN_IF_FAILURE(stat);
                                    }
                                    
                                    if (!dommy::maya::isFiniteVector(normal) ||
                                        !dommy::maya::isFiniteVector(tangent)) return MS::kInvalidParameter;
                                    DOMMY_RETURN_IF_FAILURE(normal.normalize());
                                    DOMMY_RETURN_IF_FAILURE(tangent.normalize());
                                    
                                    yQuat = yAxisVec.rotateTo(tangent);
                                    MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
                                    xQuat = xPostPosVec.rotateTo(normal);
                                    
                                    MTransformationMatrix transMat;
                                    DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(xQuat.x, xQuat.y, xQuat.z, xQuat.w, MSpace::kObject));
                                    DOMMY_RETURN_IF_FAILURE(transMat.addRotationQuaternion(yQuat.x, yQuat.y, yQuat.z, yQuat.w, MSpace::kObject));
                                    DOMMY_RETURN_IF_FAILURE(transMat.addTranslation(MVector(posPt), MSpace::kWorld));
                                    if (!dommy::maya::isFiniteMatrix(transMat.asMatrix())) return MS::kInvalidParameter;
                                    cvMatArray[p] = transMat.asMatrix();
                                    
                                }//cv iter
                                matArray[u] = cvMatArray;
                                
                                
                                offset++;
                                
                            }//u coord iter
                            
                            matrixArray->push_back(matArray);
                        }//v coord iter
                        
                    }//uv switch
                    
                    if (matrixArray->size()) {
                        
                        
                        //randam scale
                        DScalarDataList rndScaleXValues(dupCount,0.0);
                        DScalarDataList rndScaleYValues(dupCount,0.0);
                        DScalarDataList rndScaleZValues(dupCount,0.0);
                        if(useRandSX && rndScale[0]!=0.0) DGenerateRandomValue(seed, israndSclAbs?0.0f:-rndScale[0], rndScale[0], dupCount, rndScaleXValues);
                        if(useRandSY && rndScale[1]!=0.0) DGenerateRandomValue(seedY, israndSclAbs?0.0f:-rndScale[1], rndScale[1], dupCount, rndScaleYValues);
                        if(useRandSZ && rndScale[2]!=0.0) DGenerateRandomValue(seedZ, israndSclAbs?0.0f:-rndScale[2], rndScale[2], dupCount, rndScaleZValues);
                        
                        
                        //random rotate
                        DScalarDataList rndRotXValues(dupCount,0.0);
                        DScalarDataList rndRotYValues(dupCount,0.0);
                        DScalarDataList rndRotZValues(dupCount,0.0);
                        
                        if(useRandRX && rndRot[0]!=0.0) DGenerateRandomValue(seed, israndRotAbs?0.0f:-rndRot[0], rndRot[0], dupCount, rndRotXValues);
                        if(useRandRY && rndRot[1]!=0.0) DGenerateRandomValue(seedY, israndRotAbs?0.0f:-rndRot[1], rndRot[1], dupCount, rndRotYValues);
                        if(useRandRZ && rndRot[2]!=0.0) DGenerateRandomValue(seedZ, israndRotAbs?0.0f:-rndRot[2], rndRot[2], dupCount, rndRotZValues);
                        
                        size_t indexOffset = 0;
                        if(connectType==0) {
                            
                            //split mode
                            
                            for(uint i=0;i<matrixArray->size();i++)
                            {
                                for(uint u=0;u<(*matrixArray)[i].size();++u) {
                                    
                                    MPointArray cvs;
                                    
                                    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(copyCvs, cvs));
                                    
                                    if ((*matrixArray)[i][u].size() != cvs.length()) return MS::kFailure;
                                    if((*matrixArray)[i][u].size()==cvs.length()){
                                        for(uint p=0;p<cvs.length();p++)
                                        {
                                            MFloatPoint pt = cvs[p];
                                            
                                            //set scale
                                            pt.x *= scale[0] * uniScale + (rndScaleXValues)[indexOffset];
                                            pt.y *= scale[1] * uniScale + (rndScaleYValues)[indexOffset];
                                            pt.z *= scale[2] * uniScale + (rndScaleZValues)[indexOffset];
                                            
                                            
                                            //set rotate
                                            MVector preRot(pt);
                                            
                                            double randRotXYZ[3] = {dommy::math::degreesToRadians((rndRotXValues)[indexOffset]),dommy::math::degreesToRadians((rndRotYValues)[indexOffset]),dommy::math::degreesToRadians((rndRotZValues)[indexOffset])};
                                            preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kXYZ);
                                            pt.setCast(preRot);
                                            
                                            
                                            cvs[p] = MPoint(pt)*(*matrixArray)[i][u][p];
                                        }
                                    }
                                    
                                    indexOffset++;
                                    
                                    if (!generatedCurves.append(DCItem(new DCurveInternalData(cvs,copyKnots,copyDegree)))) return MS::kFailure;
                                    
                                }
                                
                            }
                            
                            
                        } else {
                            
                            //connect mode
                            for(uint i=0;i<matrixArray->size();i++)
                            {
                                MPointArray totalCvs;
                                
                                for(uint u=0;u<(*matrixArray)[i].size();++u) {
                                    
                                    if ((*matrixArray)[i][u].size() != copyCvs.length()) return MS::kFailure;
                                    if((*matrixArray)[i][u].size()==copyCvs.length()){
                                        for(uint p=0;p<copyCvs.length();p++)
                                        {
                                            MFloatPoint pt = copyCvs[p];
                                            
                                            //set scale
                                            pt.x *= scale[0] * uniScale + (rndScaleXValues)[indexOffset];
                                            pt.y *= scale[1] * uniScale + (rndScaleYValues)[indexOffset];
                                            pt.z *= scale[2] * uniScale + (rndScaleZValues)[indexOffset];
                                            
                                            //set rotate
                                            MVector preRot(pt);
                                            
                                            double randRotXYZ[3] = {dommy::math::degreesToRadians((rndRotXValues)[indexOffset]),dommy::math::degreesToRadians((rndRotYValues)[indexOffset]),dommy::math::degreesToRadians((rndRotZValues)[indexOffset])};
                                            preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kXYZ);
                                            pt.setCast(preRot);
                                            //                                        rotateBy( MVector::kXaxis, dommy::math::degreesToRadians((*rndRotXValues)[indexOffset]) )
                                            
                                            
                                            MPoint finPt = MPoint(pt)*(*matrixArray)[i][u][p];
                                            
                                            DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));
                                            
                                            if (u>0 && p==0) DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));//first multi point
                                            
                                            if (u + 1U != (*matrixArray)[i].size() &&
											p == (copyCvs.length()-1)) {
                                                DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt));
                                            }
                                        }
                                    }
                                    ++indexOffset;
                                }
                                
                                MDoubleArrayPtr knots=MDoubleArrayPtr(new MDoubleArray());
                                GenerateDCurveKnots(totalCvs.length(), copyDegree, *knots);
                                
                                if (!generatedCurves.append(DCItem(new DCurveInternalData(totalCvs,*knots,copyDegree)))) return MS::kFailure;
                                
                                
                            }
                            
                        }
                        
                    }//matrix size check
                    
                }
                
            } //object plug check
            
            
            
            
#pragma mark finish output data
            if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;
            if (!outDCrvs->hasValidCurveTopology() ||
				outDCrvs->getTotalCvs() >
					dommy::generation::kMaxGeneratedPointCount) {
				return MS::kFailure;
			}
            outDCrvs->updateBBox();
            
            MDataHandle output = data.outputValue(outDC, &stat);
            if (!stat) return stat;
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishTypedPluginDataObject<DCurveData>(
                    output, pluginData.object(), DCurveData::id));
            output.setClean();
            
            return data.setClean(plug);
            
        }
		
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCFitCopy")




MStatus DCFitCopy::initialize()
{
	MStatus stat;
	
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;
	
	
	inCopyObject = gAttr.create( "inCopyObject", "ico",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inCopyObject));
	
	
	
	
	
	inObject = gAttr.create( "inObject", "ino",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));
	
	
	inReferenceObject = gAttr.create( "referenceObject", "refo",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inReferenceObject));
	
	
	
	
	aCountU= nAttr.create( "CountU", "cntu", MFnNumericData::kInt, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 4 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCountU));
	
	aCountV= nAttr.create( "CountV", "cntv", MFnNumericData::kInt, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 4 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCountV));
	

	aPadding = nAttr.create( "Padding", "pad", MFnNumericData::k2Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPadding));
	
	
	aRandomPadding = nAttr.create( "RandomPadding", "rndpad", MFnNumericData::k2Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomPadding));
	
	
	MFnEnumAttribute connectEnumFn;
	aConnectType = connectEnumFn.create( "ConnectType", "cnctty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(connectEnumFn.addField( "Split", 0 ));
	DOMMY_RETURN_IF_FAILURE(connectEnumFn.addField( "Connect", 1 ));
	DOMMY_RETURN_IF_FAILURE(connectEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConnectType));
	
	aConnectUVSwitch = nAttr.create( "ConnectUVSwitch", "cuvsw", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConnectUVSwitch));
	
	
	MFnEnumAttribute tangentEnum;
	aTangentMode = tangentEnum.create( "TangentMode", "tam", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tangentEnum.addField( "From Source Object", 0 ));
	DOMMY_RETURN_IF_FAILURE(tangentEnum.addField( "Use Tangent Vector", 1 ));
	DOMMY_RETURN_IF_FAILURE(tangentEnum.addField( "From Reference Object", 2 ));
	DOMMY_RETURN_IF_FAILURE(tangentEnum.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentMode));
	
	aTangentVector = nAttr.create( "tangentVector", "tanvec", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));
	
	
	aUniformScale= nAttr.create( "UniformScale", "uscl", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
	
	aScale= nAttr.create( "Scale", "scl", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,1.0,0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));
	
	
	aSeed= nAttr.create( "Seed", "se", MFnNumericData::kInt, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	
	aRandomScale= nAttr.create( "RandomScale", "rndscl", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,0.0,0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomScale));
	
	aRandomRotate= nAttr.create( "RandomRotate", "rndrt", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,0.0,0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 360.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomRotate));
	
	
	aIsRandScaleAbsolute = nAttr.create( "IsRandScaleAbsolute", "irsabs", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandScaleAbsolute));
	
	aIsRandRotateabsolute = nAttr.create( "IsRandRotateabsolute", "irrabs", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandRotateabsolute));
	
	
	outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( inCopyObject, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( inObject, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( inReferenceObject, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aCountU, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aCountV, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aPadding, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aRandomPadding, outDC ) );
	
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aConnectType, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aConnectUVSwitch, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aTangentMode, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aTangentVector, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aUniformScale, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aScale, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aSeed, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aRandomScale, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aRandomRotate, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aIsRandScaleAbsolute, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aIsRandRotateabsolute, outDC ) );
	
	return MS::kSuccess;
}

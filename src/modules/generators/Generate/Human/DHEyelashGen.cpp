//
//  DHEyelashGen.cpp
//  dommyTool
//
//  Created by yuzo on 2018/06/25.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DHEyelashGen.h"
#include "common/utility/DStatusOperation.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DEyelashGeneration.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DMayaValueValidation.h"

#include "general_ids.h"

#include "DDataOp.h"
#include "DNodeOperation.h"

#include "dommyMath.h"
#include "DGroupCommon.h"

#include "DDeformMath.h"


MTypeId DHEyelashGen::typeId( ID_HUMAN_EYE_SLASH );


MObject DHEyelashGen::inMesh;
MObject DHEyelashGen::inCurve;

MObject DHEyelashGen::aEdgeList;
MObject DHEyelashGen::aCenter;


MObject DHEyelashGen::aCount;

//curveParam
MObject DHEyelashGen::aLength;
MObject DHEyelashGen::aLengthRamp;
MObject DHEyelashGen::aSegment;

MObject DHEyelashGen::aSurfaceOffset;
MObject DHEyelashGen::aSurfaceOffsetRamp;



//charactor param
MObject DHEyelashGen::aRootRotate;
MObject DHEyelashGen::aRootRotateXRamp;
MObject DHEyelashGen::aRootRotateYRamp;
MObject DHEyelashGen::aRootRotateZRamp;

MObject DHEyelashGen::aGlobalBend;
MObject DHEyelashGen::aGlobalBendRamp;

MObject DHEyelashGen::aRootBend;
MObject DHEyelashGen::aTipBend;

MObject DHEyelashGen::aRootBendRamp;
MObject DHEyelashGen::aTipBendRamp;

MObject DHEyelashGen::aRootBendPerRamp;
MObject DHEyelashGen::aTipBendPerRamp;



//rand param
MObject DHEyelashGen::aSeed;
MObject DHEyelashGen::aRootNoise;
MObject DHEyelashGen::aLengthRandom;

MObject DHEyelashGen::aRandTranslateMin;
MObject DHEyelashGen::aRandTranslateMax;


MObject DHEyelashGen::aRandRotateMin;
MObject DHEyelashGen::aRandRotateMax;



MObject DHEyelashGen::outCurves;



void *DHEyelashGen::creator()
{
    return ::dommy::maya::createObject("DHEyelashGen", [&]() {
        return new DHEyelashGen();
    });
}

MStatus DHEyelashGen::compute( const MPlug& plug, MDataBlock& data )
{
	try {
	    MStatus stat;

    
    
    if(plug == outCurves)
    {
        
#pragma mark get Curve
        
        
        MObject crvObject;
        stat = getMayaNurbsCurve(thisMObject(), inCurve, data, crvObject);
        if (stat != MS::kSuccess){
            EchoWarning(name().asChar(), "Not support type in inCurve.");
            return MS::kFailure;
        }
        
        MFnNurbsCurve crvFn(crvObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
#pragma mark get attribute
        
        const MDataHandle lashCountHandle = data.inputValue(aCount, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int lashCount = lashCountHandle.asInt();
        const MDataHandle centerHandle = data.inputValue(aCenter, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MPoint center = centerHandle.asVector();
        
        
        //curve param
        const MDataHandle lashLengthHandle = data.inputValue(aLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double lashLength = lashLengthHandle.asDouble();
        MPlug lengthRampPlug( thisMObject(), aLengthRamp );
        MRampAttribute lengthRampAttr( lengthRampPlug, &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        const MDataHandle segmentHandle = data.inputValue(aSegment, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int segment = segmentHandle.asInt();
        const MDataHandle surfaceOffsetHandle = data.inputValue(aSurfaceOffset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double surfaceOffset = surfaceOffsetHandle.asDouble();
        MRampAttribute surfaceOffsetRampAttr( MPlug( thisMObject(), aSurfaceOffsetRamp ), &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        
        //charctor param
        const MDataHandle rootRotHandle = data.inputValue(aRootRotate, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double* rootRotRaw = rootRotHandle.asDouble3();
        if (!rootRotRaw) return MS::kFailure;
        const std::array<double, 3> rootRot = {{rootRotRaw[0], rootRotRaw[1], rootRotRaw[2]}};
        
        MPlug rootRotateXRampPlug( thisMObject(), aRootRotateXRamp );
        MRampAttribute rootRotateXRampAttr( rootRotateXRampPlug, &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        MPlug rootRotateYRampPlug( thisMObject(), aRootRotateYRamp );
        MRampAttribute rootRotateYRampAttr( rootRotateYRampPlug, &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        MPlug rootRotateZRampPlug( thisMObject(), aRootRotateZRamp );
        MRampAttribute rootRotateZRampAttr( rootRotateZRampPlug, &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        //bend
        const MDataHandle globalBendHandle = data.inputValue(aGlobalBend, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double globalBend = globalBendHandle.asDouble();
        MPlug globalBendRampPlug( thisMObject(), aGlobalBendRamp );
        MRampAttribute globalBendRampAttr( globalBendRampPlug, &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        const MDataHandle rootBendHandle = data.inputValue(aRootBend, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double rootBend = rootBendHandle.asDouble();
        const MDataHandle tipBendHandle = data.inputValue(aTipBend, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double tipBend = tipBendHandle.asDouble();
        
        MRampAttribute rootBendRampAttr( MPlug( thisMObject(), aRootBendRamp ), &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        MRampAttribute tipBendRampAttr( MPlug( thisMObject(), aTipBendRamp ), &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        MRampAttribute rootBendPerRampAttr( MPlug( thisMObject(), aRootBendPerRamp ), &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        MRampAttribute tipBendPerRampAttr( MPlug( thisMObject(), aTipBendPerRamp ), &stat );
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        //rand param
        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int seed = seedHandle.asInt();
        const MDataHandle randPosHandle = data.inputValue(aRootNoise, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double randPos = randPosHandle.asDouble();
        const MDataHandle lenRandHandle = data.inputValue(aLengthRandom, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double* lenRandRaw = lenRandHandle.asDouble2();
        if (!lenRandRaw) return MS::kFailure;
        const std::array<double, 2> lenRand = {{lenRandRaw[0], lenRandRaw[1]}};
        
        
        const MDataHandle randTransMinHandle = data.inputValue(aRandTranslateMin, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double* randTransMinRaw = randTransMinHandle.asDouble3();
        if (!randTransMinRaw) return MS::kFailure;
        const std::array<double, 3> randTransMin = {{randTransMinRaw[0], randTransMinRaw[1], randTransMinRaw[2]}};
        const MDataHandle randTransMaxHandle = data.inputValue(aRandTranslateMax, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double* randTransMaxRaw = randTransMaxHandle.asDouble3();
        if (!randTransMaxRaw) return MS::kFailure;
        const std::array<double, 3> randTransMax = {{randTransMaxRaw[0], randTransMaxRaw[1], randTransMaxRaw[2]}};

        const MDataHandle randRotMinHandle = data.inputValue(aRandRotateMin, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double* randRotMinRaw = randRotMinHandle.asDouble3();
        if (!randRotMinRaw) return MS::kFailure;
        const std::array<double, 3> randRotMin = {{randRotMinRaw[0], randRotMinRaw[1], randRotMinRaw[2]}};
	        const MDataHandle randRotMaxHandle = data.inputValue(aRandRotateMax, &stat);
	        DOMMY_RETURN_IF_FAILURE(stat);
	        const double* randRotMaxRaw = randRotMaxHandle.asDouble3();
	        if (!randRotMaxRaw) return MS::kFailure;
	        const std::array<double, 3> randRotMax = {{randRotMaxRaw[0], randRotMaxRaw[1], randRotMaxRaw[2]}};

			const double finiteInputs[] = {
				center.x, center.y, center.z, lashLength, surfaceOffset,
				rootRot[0], rootRot[1], rootRot[2], globalBend, rootBend,
				tipBend, randPos, lenRand[0], lenRand[1], randTransMin[0],
				randTransMin[1], randTransMin[2], randTransMax[0],
				randTransMax[1], randTransMax[2], randRotMin[0], randRotMin[1],
				randRotMin[2], randRotMax[0], randRotMax[1], randRotMax[2]};
			dommy::generation::EyelashGenerationPlan generationPlan;
			if (lashLength <= std::numeric_limits<double>::epsilon() ||
			    !dommy::generation::buildEyelashGenerationPlan(
					lashCount, segment, finiteInputs,
					sizeof(finiteInputs) / sizeof(finiteInputs[0]),
					generationPlan)) {
				MGlobal::displayError(
					"DHEyelashGen: inputs exceed the safe generation limits.");
				return MS::kFailure;
			}
        
        
        
        
        
        if (lashCount < 2) {
            EchoWarning(name().asChar(), "Count must be at least 2.");
            return MS::kFailure;
        }
        
        



























        
        
        
        
        
#pragma mark calc curve info
        
        double lasStep =  lashLength/static_cast<double>(segment+2);
        
        MDoubleArray rootBendRatioArray;
        DOMMY_RETURN_IF_FAILURE(rootBendRatioArray.setLength(segment+2));
        for (unsigned int index = 0U; index < rootBendRatioArray.length(); ++index)
            rootBendRatioArray[index] = 1.0;
        MDoubleArray tipBendRatioArray;
        DOMMY_RETURN_IF_FAILURE(tipBendRatioArray.setLength(segment+2));
        for (unsigned int index = 0U; index < tipBendRatioArray.length(); ++index)
            tipBendRatioArray[index] = 1.0;
        
        MPointArray basdeCvs;
        for (uint s=0, N=(segment+2); s<N; ++s) {
            double yVal = lasStep*static_cast<double>(s);
            
            DOMMY_RETURN_IF_FAILURE(basdeCvs.append(MPoint(0.0,yVal,0.0)));
            
            
            //local bend param
            float rootBendVal;
            float tipBendVal;
            double pos = 0.0;
            
            if(s!=0) {
                pos = static_cast<double>(s)/static_cast<double>(segment+1);
            }
            
            
            //root bend
            if (!dommy::maya::sampleFiniteRamp(
                    rootBendRampAttr, pos, rootBendVal)) {
                return MS::kFailure;
            }
            rootBendRatioArray[s] = rootBendVal;
            
            //tip bend
            if (!dommy::maya::sampleFiniteRamp(
                    tipBendRampAttr, pos, tipBendVal)) {
                return MS::kFailure;
            }
            tipBendRatioArray[s] = tipBendVal;
            
            
        }
        
        MVector xAxisVec(1.0,0.0,0.0);
        MVector yAxisVec(0.0,1.0,0.0);
        
        
        //curve length
        double start,end;
        DOMMY_RETURN_IF_FAILURE(crvFn.getKnotDomain(start, end));
        
        
        const double profCrvLength =
            crvFn.length(kMFnNurbsEpsilon, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (!std::isfinite(start) || !std::isfinite(end) ||
            end <= start ||
            !std::isfinite(profCrvLength) ||
            profCrvLength <= std::numeric_limits<double>::epsilon()) {
            return MS::kFailure;
        }
        const double profStep =
            profCrvLength /
            static_cast<double>(generationPlan.curveCount - 1U);
        
        
        //bend tip and root
        double rootBendStep = rootBend/static_cast<double>(segment+1);
        double tipBendStep = tipBend/static_cast<double>(segment+1);
        
        
        
        







        
        
        
        
#pragma mark root points
        MPointArray lashRootPtArray;
        
        MPointArrayPtr dupPointArray(new MPointArray());
        DOMMY_RETURN_IF_FAILURE(dupPointArray->setLength(lashCount));
        MVectorArrayPtr tangentArray(new MVectorArray());
        DOMMY_RETURN_IF_FAILURE(tangentArray->setLength(lashCount));
        MVectorArrayPtr normalArray(new MVectorArray());
        DOMMY_RETURN_IF_FAILURE(normalArray->setLength(lashCount));
        
        MMatrixArray transMatArray;
        MMatrixArray baseRotMatArray;
        
        MDoubleArray lengthRatioArray;
        DOMMY_RETURN_IF_FAILURE(lengthRatioArray.setLength(lashCount));
        for (unsigned int index = 0U; index < lengthRatioArray.length(); ++index)
            lengthRatioArray[index] = 1.0;
        
        MDoubleArray sufaceOffsetRatioArray;
        DOMMY_RETURN_IF_FAILURE(sufaceOffsetRatioArray.setLength(lashCount));
        for (unsigned int index = 0U; index < sufaceOffsetRatioArray.length(); ++index)
            sufaceOffsetRatioArray[index] = 0.0;
        
        //bend param
        MDoubleArray globalBendRatioArray;
        DOMMY_RETURN_IF_FAILURE(globalBendRatioArray.setLength(lashCount));
        for (unsigned int index = 0U; index < globalBendRatioArray.length(); ++index)
            globalBendRatioArray[index] = 1.0;
        MDoubleArray rootBendPerWeightArray;
        DOMMY_RETURN_IF_FAILURE(rootBendPerWeightArray.setLength(lashCount));
        for (unsigned int index = 0U; index < rootBendPerWeightArray.length(); ++index)
            rootBendPerWeightArray[index] = 1.0;
        MDoubleArray tipBendPerWeightArray;
        DOMMY_RETURN_IF_FAILURE(tipBendPerWeightArray.setLength(lashCount));
        for (unsigned int index = 0U; index < tipBendPerWeightArray.length(); ++index)
            tipBendPerWeightArray[index] = 1.0;
        
        
        //generate random for length
        MDoubleArray lenRandValueArray;
        DOMMY_RETURN_IF_FAILURE(lenRandValueArray.setLength(lashCount));
        for (unsigned int index = 0U; index < lenRandValueArray.length(); ++index)
            lenRandValueArray[index] = 0.0;
        if (lenRand[0]!=0.0 || lenRand[1]!=0.0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, lenRand[0], lenRand[1],
                    generationPlan.curveCount, lenRandValueArray)) {
                return MS::kFailure;
            }
        }
        MDoubleArray rndPosArray;
        DOMMY_RETURN_IF_FAILURE(rndPosArray.setLength(lashCount));
        for (unsigned int index = 0U; index < rndPosArray.length(); ++index)
            rndPosArray[index] = 0.0;
        if (randPos!=0.0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, -randPos, randPos,
                    generationPlan.curveCount, rndPosArray)) {
                return MS::kFailure;
            }
        }
        
        
        //rand translate
        MDoubleArray randTransX;
        DOMMY_RETURN_IF_FAILURE(randTransX.setLength(lashCount));
        for (unsigned int index = 0U; index < randTransX.length(); ++index)
            randTransX[index] = 0.0;
        MDoubleArray randTransY;
        DOMMY_RETURN_IF_FAILURE(randTransY.setLength(lashCount));
        for (unsigned int index = 0U; index < randTransY.length(); ++index)
            randTransY[index] = 0.0;
        MDoubleArray randTransZ;
        DOMMY_RETURN_IF_FAILURE(randTransZ.setLength(lashCount));
        for (unsigned int index = 0U; index < randTransZ.length(); ++index)
            randTransZ[index] = 0.0;
        if (randTransMin[0]!=0 || randTransMax[0]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randTransMin[0], randTransMax[0],
                    generationPlan.curveCount, randTransX)) {
                return MS::kFailure;
            }
        }
        if (randTransMin[1]!=0 || randTransMax[1]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randTransMin[1], randTransMax[1],
                    generationPlan.curveCount, randTransY)) {
                return MS::kFailure;
            }
        }
        if (randTransMin[2]!=0 || randTransMax[2]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randTransMin[2], randTransMax[2],
                    generationPlan.curveCount, randTransZ)) {
                return MS::kFailure;
            }
        }
        
        //rand raotate
        MDoubleArray randRotX;
        DOMMY_RETURN_IF_FAILURE(randRotX.setLength(lashCount));
        for (unsigned int index = 0U; index < randRotX.length(); ++index)
            randRotX[index] = 0.0;
        MDoubleArray randRotY;
        DOMMY_RETURN_IF_FAILURE(randRotY.setLength(lashCount));
        for (unsigned int index = 0U; index < randRotY.length(); ++index)
            randRotY[index] = 0.0;
        MDoubleArray randRotZ;
        DOMMY_RETURN_IF_FAILURE(randRotZ.setLength(lashCount));
        for (unsigned int index = 0U; index < randRotZ.length(); ++index)
            randRotZ[index] = 0.0;
        if (randRotMin[0]!=0 || randRotMax[0]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randRotMin[0], randRotMax[0],
                    generationPlan.curveCount, randRotX)) {
                return MS::kFailure;
            }
        }
        if (randRotMin[1]!=0 || randRotMax[1]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randRotMin[1], randRotMax[1],
                    generationPlan.curveCount, randRotY)) {
                return MS::kFailure;
            }
        }
        if (randRotMin[2]!=0 || randRotMax[2]!=0) {
            if (!dommy::maya::generateRandomDoubleArray(
                    seed, randRotMin[2], randRotMax[2],
                    generationPlan.curveCount, randRotZ)) {
                return MS::kFailure;
            }
        }
        
        
        for (std::size_t i = 0U; i < generationPlan.curveCount; ++i) {
            double param;
//            if (distType==0) {
                //use curve length
            double partLength = profStep*static_cast<double>(i);
            if (!std::isfinite(partLength) || partLength < 0.0) return MS::kInvalidParameter;
            const double curveParam = crvFn.findParamFromLength(partLength, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!std::isfinite(curveParam) || curveParam < start || curveParam > end)
                return MS::kInvalidParameter;
            param = curveParam + rndPosArray[i];
            if (!dommy::generation::reflectEyelashParameterOnce(param, start, end, param))
                return MS::kInvalidParameter;
//            } else {
                //use parametric
//                param = (*lengthList)[i];
//            }
            
            const double globaPos =
                static_cast<double>(i) /
                static_cast<double>(generationPlan.curveCount - 1U);
           
            
            //length ratio
            double randLenRatio = lenRandValueArray[i]/lashLength;
            float value;
            if (!dommy::maya::sampleFiniteRamp(
                    lengthRampAttr, globaPos, value)) {
                return MS::kFailure;
            }
            lengthRatioArray[i] = value+randLenRatio;
            if (!std::isfinite(randLenRatio) ||
                !std::isfinite(lengthRatioArray[i])) {
                return MS::kFailure;
            }
            
            
            
            //global bend value
            float globalBendVal;
            if (!dommy::maya::sampleFiniteRamp(
                    globalBendRampAttr, globaPos, globalBendVal)) {
                return MS::kFailure;
            }
            if (!dommy::generation::remapEyelashBendWeight(
                    globalBendVal, globalBendRatioArray[i])) return MS::kInvalidParameter;
            
            
            float rootBendVal;
            if (!dommy::maya::sampleFiniteRamp(
                    rootBendPerRampAttr, globaPos, rootBendVal)) {
                return MS::kFailure;
            }
            if (!dommy::generation::remapEyelashBendWeight(
                    rootBendVal, rootBendPerWeightArray[i])) return MS::kInvalidParameter;
            
            float tipBendVal;
            if (!dommy::maya::sampleFiniteRamp(
                    tipBendPerRampAttr, globaPos, tipBendVal)) {
                return MS::kFailure;
            }
            if (!dommy::generation::remapEyelashBendWeight(
                    tipBendVal, tipBendPerWeightArray[i])) return MS::kInvalidParameter;
            
            
            
            //root rot ramp
            float rotX, rotY, rotZ;
            if (!dommy::maya::sampleFiniteRamp(
                    rootRotateXRampAttr, globaPos, rotX)) {
                return MS::kFailure;
            }
            if (!dommy::numeric::checkedFloatCast((rotX*2.0)-1.0, rotX))
                return MS::kInvalidParameter;
            
            if (!dommy::maya::sampleFiniteRamp(
                    rootRotateYRampAttr, globaPos, rotY)) {
                return MS::kFailure;
            }
            if (!dommy::numeric::checkedFloatCast((rotY*2.0)-1.0, rotY))
                return MS::kInvalidParameter;
            
            if (!dommy::maya::sampleFiniteRamp(
                    rootRotateZRampAttr, globaPos, rotZ)) {
                return MS::kFailure;
            }
            if (!dommy::numeric::checkedFloatCast((rotZ*2.0)-1.0, rotZ))
                return MS::kInvalidParameter;
            
            
            
            //surfaceoffset ratio
            float srfoftValue;
            if (!dommy::maya::sampleFiniteRamp(
                    surfaceOffsetRampAttr, globaPos, srfoftValue)) {
                return MS::kFailure;
            }
            sufaceOffsetRatioArray[i] = srfoftValue;
            
            
            //root position
            MPoint rootPt;
            DOMMY_RETURN_IF_FAILURE(
                crvFn.getPointAtParam(param, rootPt, MSpace::kWorld));
            if (!dommy::maya::isFinitePoint(rootPt)) {
                return MS::kFailure;
            }
            
            
            
            //calc align matrix
            
//            MVector normal(baseNormal);
            MVector normal;
            if (!dommy::maya::normalizedVector(rootPt - center, normal)) {
                return MS::kFailure;
            }
            
            //get tangent
            MVector tangent = crvFn.tangent(
                param, MSpace::kObject, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            MVector normalizedTangent;
            if (!dommy::maya::normalizedVector(
                    tangent, normalizedTangent)) {
                return MS::kFailure;
            }
            tangent = normalizedTangent;
            
            MTransformationMatrix transMat;
            MTransformationMatrix alignMat;
            
            MQuaternion yQuat = yAxisVec.rotateTo(normal);
            MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
            MQuaternion xQuat = xPostPosVec.rotateTo(tangent);
//            transMat.rotateBy(quat, MSpace::kWorld);
            
            DOMMY_RETURN_IF_FAILURE(alignMat.addRotationQuaternion(xQuat.x, xQuat.y, xQuat.z, xQuat.w, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(alignMat.addRotationQuaternion(yQuat.x, yQuat.y, yQuat.z, yQuat.w, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(transMat.addTranslation(MVector(rootPt.x,rootPt.y,rootPt.z), MSpace::kWorld));
            transMat = MTransformationMatrix(transMat.asMatrix()*alignMat.asMatrix());
            
            
            //rot matrix
            MTransformationMatrix rotMat;
            double baseRot[3] = {
                dommy::math::degreesToRadians(rootRot[0]*rotX+randRotX[i]),
                dommy::math::degreesToRadians(rootRot[1]*rotY+randRotY[i]),
                dommy::math::degreesToRadians(rootRot[2]*rotZ+randRotZ[i])
            };
            if (!std::isfinite(baseRot[0]) || !std::isfinite(baseRot[1]) ||
                !std::isfinite(baseRot[2])) return MS::kInvalidParameter;
            MTransformationMatrix::RotationOrder order = MTransformationMatrix::RotationOrder::kXYZ;
            DOMMY_RETURN_IF_FAILURE(rotMat.addRotation(baseRot,order, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(baseRotMatArray.append(rotMat.asMatrix()));
            
            
            
            (*dupPointArray)[i] = rootPt;
            (*tangentArray)[i] = tangent;
            (*normalArray)[i] = normal;
            
            
            DOMMY_RETURN_IF_FAILURE(transMatArray.append(transMat.asMatrix()));
            //                            oldBiNormal = biNormal;
        }
        
        

        
        
        
        
        
#pragma mark create lash curve
        
        
        std::vector<MObject> generatedCurves;
        generatedCurves.reserve(generationPlan.curveCount);
        
        
        //curve degree
        const unsigned deg = 1;
        
        for (size_t i=0, N=dupPointArray->length(); i<N; ++i) {
            
            const MVector basePt = (*dupPointArray)[i];
            
//            MFloatArray rmpValues;
//            struggleRampAttr.sampleValueRamp(basdeCvs.length(), rmpValues);
            
            //                            MPointArray cvs;
            MPointArrayPtr cvs(new MPointArray());
            DOMMY_RETURN_IF_FAILURE(cvs->copy(basdeCvs));
            
            //base global bend
            for (size_t c=0, CN=(*cvs).length(); c<CN; ++c) {
                MVector newPt = (*cvs)[c];
                
                newPt.y *= lengthRatioArray[i];
                if (!deformBend(
                        dommy::math::degreesToRadians(globalBend * globalBendRatioArray[i]),
                        &newPt)) {
                    return MS::kFailure;
                }
                
                (*cvs)[c] = newPt;
            }
            
            
            //part bend (tip and root)
            if (!deformBendWithCvs(
                    dommy::math::degreesToRadians(rootBendStep), rootBendPerWeightArray[i],
                    rootBendRatioArray, *cvs) ||
                !deformBendWithCvs(
                    dommy::math::degreesToRadians(tipBendStep), tipBendPerWeightArray[i],
                    tipBendRatioArray, *cvs)) {
                MGlobal::displayError(
                    "DHEyelashGen: bend input produced invalid curve data.");
                return MS::kFailure;
            }
            
            
            //create lash curve
            for (size_t c=0, CN=cvs->length(); c<CN; ++c) {
                
                
                MVector newPt = (*cvs)[c];
                
                
//                newPt.y *= lengthRatioArray[i];
//                deformBend(dommy::math::degreesToRadians(globalBend*globalBendRatioArray[i]),  &newPt);

                
                //surface ofset
                if (surfaceOffset!=0.0)
                    newPt.y += surfaceOffset*sufaceOffsetRatioArray[i];
                
                
//                newPt += MPoint((*noiseXArray)[c]*struggleStrength*rmpValues[c],
//                                (*noiseYArray)[c]*struggleStrength*rmpValues[c],
//                                (*noiseZArray)[c]*struggleStrength*rmpValues[c]);
                //                                newPt = newPt.rotateBy(yQuatArray[i]).rotateBy(xQuatArray[i]);
                
             
                MPoint finPt = newPt*baseRotMatArray[i]*transMatArray[i]+ basePt;
                
                
                //add rand offset
                finPt.x+=randTransX[i];
                finPt.y+=randTransY[i];
                finPt.z+=randTransZ[i];
                if (!dommy::maya::isFinitePoint(finPt)) {
                    return MS::kFailure;
                }
                
                (*cvs)[c] = finPt;
                //                                cvs.append(finPt);
            }
            
            
            //generate knot
            MDoubleArrayPtr knots(new MDoubleArray());
            GenerateDCurveKnots(cvs->length(), deg, *knots);
            
            
            MFnNurbsCurveData dataCreator;
            MObject newCurveData = dataCreator.create( &stat );
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (newCurveData.isNull()) return MS::kFailure;
            
            MFnNurbsCurve outCurveFn;
            MObject curveObject = outCurveFn.create(
                *cvs, *knots, deg, MFnNurbsCurve::kOpen, false, false,
                newCurveData, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (curveObject.isNull()) return MS::kFailure;
            generatedCurves.push_back(newCurveData);
//            rotOffset++;
//            curveOffset++;
        }
        
        
        
        
#pragma mark -
#pragma mark Output Curve
        
        return dommy::maya::setObjectArrayOutput(
            data, outCurves, plug, generatedCurves);
        
    }
    
    
	return MS::kUnknownParameter;
	} DOMMY_COMPUTE_CATCH("DHEyelashGen")
}






MStatus DHEyelashGen::initialize()
try
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute   tAttr;
    MFnGenericAttribute gAttr;
    MFnEnumAttribute eAttr;
    
    MStatus stat;
    
    
    
    
    // data input param
    
    inMesh = gAttr.create( "inMesh", "im",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));
    
    inCurve = gAttr.create( "inCurve", "inc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));
    
    
    MFnStringData fnStringData;
    const MObject defaultEdgeList = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aEdgeList = tAttr.create("EdgeList", "elst", MFnData::kString, defaultEdgeList, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeList));
    
    
    
    //common param
    
    aCenter = nAttr.create( "Center", "ctr", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenter));
    
    
    
    aCount = nAttr.create( "Count", "cnt", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 20 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 2 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(3000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));
    
    
    
    
    
    
    //Curve param
    
    
    aLength = nAttr.create( "Length", "len", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLength));
    
    aLengthRamp = MRampAttribute::createCurveRamp("LengthRamp", "lenrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthRamp));
    
    
    
    aSegment = nAttr.create( "Segment", "sgm", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 2 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSegment));
    
    
    aSurfaceOffset = nAttr.create( "SurfaceOffset", "srfoft", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceOffset));
    
    
    aSurfaceOffsetRamp = MRampAttribute::createCurveRamp("SurfaceOffsetRamp", "srfoftrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceOffsetRamp));
    
    
    //charactor param
    
    aRootRotate = nAttr.create( "RootRotate", "rtrot", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,0.0,0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootRotate));
    
    aRootRotateXRamp = MRampAttribute::createCurveRamp("RootRotateXRamp", "rtrotrmpx", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootRotateXRamp));
    aRootRotateYRamp = MRampAttribute::createCurveRamp("RootRotateYRamp", "rtrotrmpy", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootRotateYRamp));
    aRootRotateZRamp = MRampAttribute::createCurveRamp("RootRotateZRamp", "rtrotrmpz", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootRotateZRamp));
    
    
    aGlobalBend = nAttr.create( "GlobalBend", "gbnd", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalBend));
    
    
    aGlobalBendRamp = MRampAttribute::createCurveRamp("GlobalBendRamp", "gbndrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalBendRamp));
    
    
    
    aRootBend = nAttr.create( "RootBend", "rtbnd", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootBend));
    
    aTipBend = nAttr.create( "TipBend", "tpbnd", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTipBend));
    
    
    aRootBendRamp = MRampAttribute::createCurveRamp("RootBendRamp", "rtbndrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootBendRamp));
    aTipBendRamp = MRampAttribute::createCurveRamp("TipBendRamp", "tpbndrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTipBendRamp));
    
    
    aRootBendPerRamp = MRampAttribute::createCurveRamp("RootBendPerRamp", "rtbndprmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootBendPerRamp));
    aTipBendPerRamp = MRampAttribute::createCurveRamp("TipBendPerRamp", "tpbndprmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTipBendPerRamp));
    
    
    //random param
    
    aSeed = nAttr.create( "Seed", "sde", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    
    
    aRootNoise = nAttr.create( "RootNoise", "rtns", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( -5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootNoise));
    
    aLengthRandom = nAttr.create( "LengthRandom", "lenrand", MFnNumericData::k2Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthRandom));
    
    
    aRandTranslateMin = nAttr.create( "RandTranslateMin", "rndtrmin", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandTranslateMin));
    
    aRandTranslateMax = nAttr.create( "RandTranslateMax", "rndtrmax", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandTranslateMax));

    
    
    aRandRotateMin = nAttr.create( "RandRotateMin", "rndrotmin", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandRotateMin));
    
    aRandRotateMax = nAttr.create( "RandRotateMax", "rndrotmax", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandRotateMax));
    
    
    
    
    outCurves  = tAttr.create( "outCurves", "ocs", MFnGeometryData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable( true ));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable( false ));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outCurves));
    
    
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inMesh, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inCurve, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeList, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenter, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCount, outCurves ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLength, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLengthRamp, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSegment, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSurfaceOffset, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSurfaceOffsetRamp, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootRotate, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootRotateXRamp, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootRotateYRamp, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootRotateZRamp, outCurves ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalBend, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalBendRamp, outCurves ));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootBend, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTipBend, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootBendRamp, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTipBendRamp, outCurves ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootBendPerRamp, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTipBendPerRamp, outCurves ));
    
    
    //random
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootNoise, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLengthRandom, outCurves ));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRandTranslateMin, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRandTranslateMax, outCurves ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRandRotateMin, outCurves ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRandRotateMax, outCurves ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DHEyelashGen", "initialize", std::current_exception());
}

//
//  DMWireGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMWireGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaWireTopology.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "DCurveSeExpr.h"
#include "DMeshDataHeaders.h"

#include "dommyMath.h"
#include <SeExpression.h>

#include <cmath>
#include <cstddef>


const MTypeId DMWireGen::typeId( ID_DM_WIRE_GEN );

MObject DMWireGen::inDC;

MObject DMWireGen::inTangentCurve;


MObject DMWireGen::aProfileMode;
MObject DMWireGen::aProfileCurve;

MObject DMWireGen::aCircleRadius;
MObject DMWireGen::aCircleSampling;


MObject DMWireGen::aSampling;
MObject DMWireGen::aCurveLength;
MObject DMWireGen::aCurveOffset;

MObject DMWireGen::aTangentMode;
MObject DMWireGen::aTangentVector;

MObject DMWireGen::aTwistRamp;
MObject DMWireGen::aWidthRamp;

MObject DMWireGen::aGlobalRotate;
MObject DMWireGen::aGlobalScale;

MObject DMWireGen::aExpression;


MObject DMWireGen::aUVTranslate;
MObject DMWireGen::aUVScale;

MObject DMWireGen::outDM;


void *DMWireGen::creator() {
    return ::dommy::maya::createObject("DMWireGen", [&]() {
        return new DMWireGen();
    });
}




MStatus DMWireGen::compute(const MPlug& plug, MDataBlock& data) try
{
    
    MStatus stat;
    
    if (plug == outDM) {

        
#pragma mark get input dmesh
        const DCurveData* inputDCData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
		if (!inputDCData || inputDCData->numberOfCurves() == 0U ||
			!inputDCData->hasValidCurveTopology()) {
			MGlobal::displayError("DMWireGen: input DCurve data is invalid or empty.");
			return MS::kInvalidParameter;
		}
        
        if (inputDCData && inputDCData->numberOfCurves()) {
            
            MFnPluginData pluginData;
            const MObject outputObject = pluginData.create(DMeshData::id, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
			if (outputObject.isNull()) return MS::kFailure;
            
            DMeshData* outDMeshes = NULL;
			DOMMY_RETURN_IF_FAILURE(
				dommy::maya::getPluginData(pluginData, outDMeshes));
            
            if (outDMeshes) {
                
                
                //get Attribute
                
                const MDataHandle profileModeValHandle = data.inputValue(aProfileMode, &stat);
                if (!stat) return stat;
                const int profileModeVal = profileModeValHandle.asInt();
                const MDataHandle curveLengthValHandle = data.inputValue(aCurveLength, &stat);
                if (!stat) return stat;
                const double curveLengthVal = curveLengthValHandle.asDouble();
                const MDataHandle curveOffsetValHandle = data.inputValue(aCurveOffset, &stat);
                if (!stat) return stat;
                const double curveOffsetVal = curveOffsetValHandle.asDouble();
                const MDataHandle rawSamplingHandle = data.inputValue(aSampling, &stat);
                if (!stat) return stat;
                const int rawSampling = rawSamplingHandle.asInt();
                
                const MDataHandle circleRadiusValHandle = data.inputValue(aCircleRadius, &stat);
                if (!stat) return stat;
                const double circleRadiusVal = circleRadiusValHandle.asDouble();
                const MDataHandle circleSamplingValHandle = data.inputValue(aCircleSampling, &stat);
                if (!stat) return stat;
                const int circleSamplingVal = circleSamplingValHandle.asInt();
                
                
                const MDataHandle tangentModeHandle = data.inputValue(aTangentMode, &stat);
                if (!stat) return stat;
                short tangentMode = tangentModeHandle.asShort();
                const MDataHandle tangentVecValHandle = data.inputValue(aTangentVector, &stat);
                if (!stat) return stat;
                const double3 &tangentVecVal = tangentVecValHandle.asDouble3();
                
                const MDataHandle gRotateValHandle = data.inputValue(aGlobalRotate, &stat);
                if (!stat) return stat;
                const double gRotateVal = gRotateValHandle.asDouble();
                const MDataHandle gScaleValHandle = data.inputValue(aGlobalScale, &stat);
                if (!stat) return stat;
                const double gScaleVal = gScaleValHandle.asDouble();
                
                MPlug twistPlug( thisMObject(), aTwistRamp );
                MRampAttribute twistRampAttr( twistPlug , &stat );
                if (!stat) return stat;
                
                MPlug widthPlug( thisMObject(), aWidthRamp );
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
					(tangentMode < 0 || tangentMode > 2) ||
					!dommy::generation::checkedCount(
						rawSampling, dommy::generation::kMaxGeneratedPointCount,
						samplingVal) || samplingVal < 2U ||
					!dommy::generation::checkedCount(
						circleSamplingVal,
						dommy::generation::kMaxGeneratedPointCount,
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
					MGlobal::displayError("DMWireGen: invalid or excessive input value.");
					return MS::kInvalidParameter;
				}
                
#pragma mark Get input object 入力タンジェントカーブの取得
                MDataHandle tanHnd = data.inputValue(inTangentCurve,&stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
                MPlug tangentPlug(thisMObject(), inTangentCurve);
                
                MFnNurbsCurve tanCrvFn;
                
                
                if (tangentMode==2 ) {
                    const bool tangentConnected = tangentPlug.isConnected(&stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    if (tangentConnected && tanHnd.type() == MFnData::kNurbsCurve) {
                        
						DOMMY_RETURN_IF_FAILURE(
							tanCrvFn.setObject(tanHnd.data()));
                        
                    } else {
                        tangentMode = 0;
                    }
                    
                    
                }//check tangent input curve
                
                
                // Read only scalar profile counts before allocating geometry arrays.
                MPointArray profPointArray;
                MFnNurbsCurve profCrvFn;
                std::size_t profilePointCount = checkedCircleSampling;
                MPlug profCrvPlug(thisMObject(), aProfileCurve);
                MDataHandle profCrvHnd = data.inputValue(aProfileCurve, &stat);
                if (!stat) return stat;
                bool profileConnected = false;
                if (profileModeVal == 1) {
                    profileConnected = profCrvPlug.isConnected(&stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                }
                const bool useProfile = profileModeVal == 1 && profileConnected &&
                    profCrvHnd.type() == MFnData::kNurbsCurve;
                if (useProfile) {
                    DOMMY_RETURN_IF_FAILURE(profCrvFn.setObject(profCrvHnd.data()));
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
                    !dommy::mesh::validWireGenerationBudget(topologyPlan, inputDCData->numberOfCurves()))
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

#pragma mark start create wire data
                
                DMeshData::DMeshList generatedMeshes;
                generatedMeshes.reserve(inputDCData->numberOfCurves());
                for (auto cIt=inputDCData->beginPrims(); cIt!=inputDCData->endPrims(); ++cIt) {
                    
                    //read curve
                    MFnNurbsCurve crvFn;
					MFnNurbsCurveData curveData;
					MObject crvObj = curveData.create(&stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MFnNurbsCurve::Form isOpen = (*cIt)->isCurveOpen()?MFnNurbsCurve::Form::kOpen:MFnNurbsCurve::Form::kClosed;
                    crvFn.create((*cIt)->getCvs(), (*cIt)->getKnots(), (*cIt)->getDegree(), isOpen, false, false, crvObj, &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
#pragma mark Get curve info
                    double start, end;
                    stat = crvFn.getKnotDomain (start, end);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					double length = crvFn.length(kMFnNurbsEpsilon, &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
                    length *= curveLengthVal;
					if (!std::isfinite(start) || !std::isfinite(end) ||
						start > end || !std::isfinite(length) || length < 0.0) {
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
                    
                    //                std::vector<MQuaternion> xQuatArray;
                    //                std::vector<MQuaternion> yQuatArray;
                    MQuaternionArrayPtr xQuatArray = MQuaternionArrayPtr(new std::vector<MQuaternion>(totalSampling));
                    MQuaternionArrayPtr yQuatArray = MQuaternionArrayPtr(new std::vector<MQuaternion>(totalSampling));
					MPointArrayPtr dupPointArray = MPointArrayPtr(
						new MPointArray());
                    DOMMY_RETURN_IF_FAILURE(dupPointArray->setLength(static_cast<unsigned int>(totalSampling)));
                    if (dupPointArray->length() != totalSampling) return MS::kFailure;
                    //                std::vector<MPoint> dupPointArray;
                    
                    
                    
					for (std::size_t c=0U; c<totalSampling; ++c) {
                        MPoint cv;
						double curveParam = crvFn.findParamFromLength(
							stepLength * static_cast<double>(c), &stat);
						CHECK_MSTATUS_AND_RETURN_IT(stat);
                        
                        double offsetedParam = curveParam+curveOffsetVal;
                        
                        offsetedParam = offsetedParam<start?start:offsetedParam;
                        offsetedParam = offsetedParam>end?end:offsetedParam;
                        
                        
						DOMMY_RETURN_IF_FAILURE(
							crvFn.getPointAtParam(
								offsetedParam, cv, MSpace::kWorld));
						if (!dommy::maya::isFinitePoint(cv)) return MS::kFailure;
                        
                        MVector    normal;
                        MVector    tangent = crvFn.tangent(offsetedParam , MSpace::kWorld , &stat);
						CHECK_MSTATUS_AND_RETURN_IT(stat);
                        
                        if (tangentMode==1) {//Simple vector
                            normal= MVector(tangentVecVal);
                            
                        } else if (tangentMode==2) {//input curve
                            MPoint targetCV;
                            double targetParam;
                            tanCrvFn.closestPoint(cv,&targetParam,kMFnNurbsEpsilon,MSpace::kWorld,&stat);
							CHECK_MSTATUS_AND_RETURN_IT(stat);
							DOMMY_RETURN_IF_FAILURE(
								tanCrvFn.getPointAtParam(
									targetParam, targetCV, MSpace::kWorld));
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
						if (!dommy::maya::normalizedVector(
								tangent, normalizedTangent)) {
							return MS::kFailure;
						}
						const MVector orthogonalNormal =
							normal - normalizedTangent * (normal * normalizedTangent);
						MVector normalizedNormal;
						if (!dommy::maya::normalizedVector(
								orthogonalNormal, normalizedNormal)) {
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
                        
                        (*xQuatArray)[c] = xQuat;
                        (*yQuatArray)[c] = yQuat;
                        (*dupPointArray)[c] = cv;
                        
                        oldBiNormal = biNormal;
                    }
                    
                    
                    
                    
                    
                    
#pragma mark twist and width
                    MFloatArrayPtr twistValueArray = MFloatArrayPtr(new MFloatArray());
                    MFloatArrayPtr widthValueArray = MFloatArrayPtr(new MFloatArray());
					twistRampAttr.sampleValueRamp(
						static_cast<unsigned int>(totalSampling),
						*twistValueArray,&stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					widthRampAttr.sampleValueRamp(
						static_cast<unsigned int>(totalSampling),
						*widthValueArray,&stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					if (twistValueArray->length() != totalSampling ||
						widthValueArray->length() != totalSampling) {
						return MS::kFailure;
					}
					for (std::size_t sampleIndex = 0U;
						 sampleIndex < totalSampling; ++sampleIndex) {
						if (!std::isfinite((*twistValueArray)[sampleIndex]) ||
							!std::isfinite((*widthValueArray)[sampleIndex])) {
							return MS::kFailure;
						}
					}
                    
                    
                    
#pragma mark eval expression
                    //        std::string exprStr = "$val=.5*PI*param;7*sin(val)/val";
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
							expr.setLength(
								stepLength * static_cast<double>(c));
                            
                            SeVec3d val=expr.evaluate();
                            double y=val[0];
							if (!std::isfinite(y)) {
								MGlobal::displayError(
									"DMWireGen: expression produced a non-finite value.");
								return MS::kFailure;
							}
                            
							DOMMY_RETURN_IF_FAILURE(paramExpArray.append(y));
                        }
                    } else if (exprStr.size()>0) {
                        DOMMY_MAYA_WARNING(
                            name().asChar(), "expression failed: " << expr.parseError());
                    }
                    
                    
#pragma mark Create Wire
                    MPointArrayPtr vertexArray = MPointArrayPtr(new MPointArray());
                    MIntArrayPtr polygonCount = MIntArrayPtr(new MIntArray());
                    MIntArrayPtr polygonConnect = MIntArrayPtr(new MIntArray());
                    
                    
                    
                    //create vertex
					for (std::size_t i=0U; i<totalSampling; ++i) {
						for (unsigned int p=0U;
							 p<profPointArray.length(); ++p) {
                            MVector newVec(profPointArray[p]);
                            
                            newVec*=(*widthValueArray)[i];
                            if(paramExpArray.length()==totalSampling) newVec*=paramExpArray[i];
                            newVec*=gScaleVal;
                            
                            newVec=newVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians((*twistValueArray)[i]*360.0f+gRotateVal));
                            
                            newVec = newVec.rotateBy((*yQuatArray)[i]).rotateBy((*xQuatArray)[i]);
                            MPoint finPt(newVec.x+(*dupPointArray)[i].x, newVec.y+(*dupPointArray)[i].y, newVec.z+(*dupPointArray)[i].z);
							if (!dommy::maya::isFinitePoint(finPt)) {
								return MS::kFailure;
							}
                            
							DOMMY_RETURN_IF_FAILURE(
								vertexArray->append(finPt));
                        }
                        
                    }
                    
                    
					MFloatArrayPtr uCoords = MFloatArrayPtr(new MFloatArray());
					MFloatArrayPtr vCoords = MFloatArrayPtr(new MFloatArray());
					MIntArrayPtr uvCountsArray = MIntArrayPtr(new MIntArray());
					MIntArrayPtr uvIdsArray = MIntArrayPtr(new MIntArray());
					DOMMY_RETURN_IF_FAILURE(
						dommy::maya::copyWireTopologyToMaya(
							topologyPlan, *polygonCount, *polygonConnect,
							*uCoords, *vCoords, *uvCountsArray, *uvIdsArray));
					if (static_cast<std::size_t>(vertexArray->length()) !=
						topologyPlan.vertexCount) {
						return MS::kFailure;
					}
                    
                    
                    
                    DMItem item = DMItem(new DMeshInternalData(*vertexArray, *polygonCount, *polygonConnect));
					if (!item || !item->hasValidPolygonTopology() ||
						!item->setUvData(
							*uCoords, *vCoords, *uvCountsArray, *uvIdsArray) ||
						!item->hasValidMeshData()) {
						return MS::kFailure;
					}
                    
                    generatedMeshes.push_back(item);
                }//loop curve
                if (!outDMeshes->insert(generatedMeshes)) return MS::kFailure;
                
                
                
                outDMeshes->updateBBox();
				if (!outDMeshes->hasValidMeshTopology()) return MS::kFailure;
                
                
#pragma mark Create output
				MDataHandle output = data.outputValue(outDM, &stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
				DOMMY_RETURN_IF_FAILURE(
					dommy::maya::publishTypedPluginDataObject<DMeshData>(
						output, pluginData.object(), DMeshData::id));
				output.setClean();
                
                return data.setClean(plug);
                
                
            }//check output dmesh
            
            
            
        }//check inpu dcurve
        
    }//check plug
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMWireGen")
            


MStatus DMWireGen::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    inTangentCurve = gAttr.create( "inTangentCurve", "intc",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTangentCurve));

    MFnEnumAttribute enumFn;
    aProfileMode = enumFn.create( "ProfileMode", "prfm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Simple Circle", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Profile", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aProfileMode));

    aProfileCurve = gAttr.create( "ProfileCurve", "pcrv",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aProfileCurve));

    aCircleRadius = nAttr.create( "CircleRadius", "cr", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCircleRadius));

    aCircleSampling = nAttr.create( "CircleSampling", "cs", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 3 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCircleSampling));

    aSampling = nAttr.create( "Sampling", "sa", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 2 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aCurveLength = nAttr.create( "CurveLength", "cl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveLength));

    aCurveOffset = nAttr.create( "CurveOffset", "off", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveOffset));

    aTangentMode = enumFn.create( "TangentMode", "tam", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Curve", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Simple Vector", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "input Curve", 2 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentMode));

    aTangentVector = nAttr.create( "tangentVector", "tanvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));

    aTwistRamp = MRampAttribute::createCurveRamp("TwistRamp", "twr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTwistRamp));

    aWidthRamp = MRampAttribute::createCurveRamp("WidthRamp", "wir", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthRamp));

    aGlobalRotate = nAttr.create( "GlobalRotate", "grot", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotate));

    aGlobalScale = nAttr.create( "GlobalScale", "gscal", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScale));

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

    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTangentCurve, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aProfileMode, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aProfileCurve, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCircleRadius, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCircleSampling, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSampling, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurveLength, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurveOffset, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTangentMode, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTangentVector, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTwistRamp, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWidthRamp, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalRotate, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUVTranslate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUVScale, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMWireGen", "initialize", std::current_exception());
}

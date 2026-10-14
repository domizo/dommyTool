//
//  DCurveConnectGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/22.
//
//

#include "DCurveLinkGen.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"
#include "DCurveSourceGenerationPlan.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>


const MTypeId DCurveLinkGen::typeId( ID_DC_LINK_GEN );


MObject DCurveLinkGen::aCount;
MObject DCurveLinkGen::aSegment;
MObject DCurveLinkGen::aDegree;

MObject DCurveLinkGen::aLinkList;
MObject DCurveLinkGen::aInObject;

MObject DCurveLinkGen::aMode;
MObject DCurveLinkGen::aWrapMode;

MObject DCurveLinkGen::aStepSizeU;
MObject DCurveLinkGen::aStepSizeV;
MObject DCurveLinkGen::aColumnNum;

MObject DCurveLinkGen::aRangeU;
MObject DCurveLinkGen::aRangeV;
MObject DCurveLinkGen::aOffsetU;
MObject DCurveLinkGen::aOffsetV;

MObject DCurveLinkGen::aIsRandom;
MObject DCurveLinkGen::aSeed;
MObject DCurveLinkGen::aRandom;

MObject DCurveLinkGen::outDC;


void *DCurveLinkGen::creator()
{
    return ::dommy::maya::createObject("DCurveLinkGen", [&]() {
        return new DCurveLinkGen();
    });
}



MStatus DCurveLinkGen::compute(const MPlug& plug, MDataBlock& data) try
{
	
	MStatus stat;
	

	
	if (plug == outDC) {
		//Check inobject count
		MArrayDataHandle arrayHandle = data.inputArrayValue(aLinkList, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		
		const std::size_t linkCount = arrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		
		
		if(linkCount<2) {
			EchoWarning(name().asChar(), "Input object need more than 2 curve or surface.");
			return MS::kFailure;
		}
        if (linkCount > dommy::generation::kMaxConnectedGeometryCount) {
            EchoWarning(name().asChar(), "Input object count exceeds the safety limit.");
            return MS::kInvalidParameter;
        }
		
		
		//Create Output data
            MObject outputDataObject;
            DCurveData* outDCrvs = NULL;
            stat = dommy::maya::createPluginDataObject<DCurveData>(
                DCurveData::id, outputDataObject, outDCrvs);
            if (!stat) return stat;
		
        if (outDCrvs) {
            
            
            //attribute
                MDataHandle control = data.inputValue(aCount, &stat);
                if (!stat) return stat;
                const int rawCurveCount = control.asInt();
                control = data.inputValue(aSegment, &stat);
                if (!stat) return stat;
                const int rawSegmentCount = control.asInt();
                control = data.inputValue(aDegree, &stat);
                if (!stat) return stat;
                const short degree = control.asShort();
                if (degree != 0 && degree != 1) return MS::kInvalidParameter;
                const int curveDegree = degree == 0 ? 1 : 3;

                dommy::curve::LinkGenerationPlan plan;
                if (!dommy::curve::checkedLinkGenerationPlan(linkCount, rawCurveCount, rawSegmentCount, plan)) {
                    EchoWarning(name().asChar(), "Requested curve generation exceeds the safety limit.");
                    return MS::kInvalidParameter;
                }
                const std::size_t curveCount = plan.curveCount;
                const std::size_t segmentCount = plan.segmentCount;
                const std::size_t outputPointCount = plan.outputPointCount;
	            
	            std::vector<MPointArray> pointArray(linkCount, MPointArray());
            
            //generate source points for objects.
            for(size_t i = 0; i < linkCount; i++) {
                
                DOMMY_RETURN_IF_FAILURE(arrayHandle.jumpToArrayElement(static_cast<unsigned int>(i)));
                MDataHandle attrHnd = arrayHandle.inputValue(&stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                
                const short mode = attrHnd.child(aMode).asShort();
                const short wrapMode = attrHnd.child(aWrapMode).asShort();
                const float stepSizeU = attrHnd.child(aStepSizeU).asFloat();
                const float stepSizeV = attrHnd.child(aStepSizeV).asFloat();
                const float rangeU = attrHnd.child(aRangeU).asFloat();
                const float rangeV = attrHnd.child(aRangeV).asFloat();
	                const float offsetU = attrHnd.child(aOffsetU).asFloat();
	                const float offsetV = attrHnd.child(aOffsetV).asFloat();
	                MDataHandle objHnd = attrHnd.child(aInObject);

                    if ((mode != 0 && mode != 1) ||
                        wrapMode < 0 || wrapMode > 2 ||
                        !std::isfinite(stepSizeU) ||
                        !std::isfinite(stepSizeV) ||
                        !std::isfinite(rangeU) ||
                        !std::isfinite(rangeV) ||
                        !std::isfinite(offsetU) ||
                        !std::isfinite(offsetV)) {
                        EchoWarning(name().asChar(), "Curve sampling values must be finite and valid.");
                        return MS::kInvalidParameter;
                    }
                
                
                
                
                
                
                
                
                
                
                if(objHnd.type() == MFnData::kNurbsCurve) {//Nurbs Curve Object
                    MFnNurbsCurve crvFn;
	                    stat = crvFn.setObject( objHnd.data() );
	                    if (!stat) return stat;
	                    
	                    
	                    double start, end;
	                    stat = crvFn.getKnotDomain(start, end);
                        if (!stat) return stat;
                    
                    //                double minParam = fmin(minU,fmin(start, end));
                    //                double maxParam = fmax(maxU,fmax(start, end));
                    //                double totalLength = fabs(maxParam-minParam);
	                    const double totalLength =
                            crvFn.length(kMFnNurbsEpsilon, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
	                    const double finLength = totalLength * rangeU;

                        if (!std::isfinite(start) || !std::isfinite(end) ||
                            !std::isfinite(totalLength) ||
                            !std::isfinite(finLength)) {
                            EchoWarning(name().asChar(), "Curve sampling produced a non-finite value.");
                            return MS::kInvalidParameter;
                        }
                    
	                    const double step =
                            finLength /
                            static_cast<double>(curveCount + 1U);
	                    const double paramRange =
                            std::fabs(end - start);
                        if (wrapMode == 2 && paramRange <=
                                std::numeric_limits<double>::epsilon()) {
                            return MS::kInvalidParameter;
                        }
                    
                    
                    
                    
                    
                    
                    std::vector<double> stepArray;
                    
                    if(mode==0) {//divide
                        for (size_t c=0; c<curveCount; c++) {
                            stepArray.push_back(step*(double)(c+1));
                        }
                    } else {//step
                        
                        for (size_t c=0; c<curveCount; c++) {
                            stepArray.push_back(stepSizeU*(double)c);
                        }
                    }
                    
                    //create random
                    const bool isBool = attrHnd.child(aIsRandom).asBool();
                    std::vector<float> randArray(stepArray.size(), 0.0);
                    
                    if (isBool) {//random
                        const uint seed = attrHnd.child(aSeed).asInt();
	                        const float randomValue =
                                attrHnd.child(aRandom).asFloat();
                            if (!std::isfinite(randomValue)) {
                                EchoWarning(name().asChar(), "Random range must be finite.");
                                return MS::kInvalidParameter;
                            }
                            const float randVal =
                                std::fabs(randomValue);
                        
                        if(randVal!=0.0) {
                            
                            
                            
                            
                            float halfval = randVal*.5;
                            std::mt19937 eng(seed);
                            
                            std::uniform_real_distribution<float> distribU(
                                -halfval, halfval);
                            
                            for (size_t c=0,N=stepArray.size(); c<N; c++) {
                                float val = distribU(eng);
                                randArray[c] = val;
                            }
                        }
                        
                    }
                    
                    
                    
                    
                    
                    for (size_t c=0; c<curveCount; c++) {
	                        double param;
	                        if(mode==0) {//divide
	                            param = crvFn.findParamFromLength(
                                    stepArray[c], &stat);
	                        } else {//step
	                            param = crvFn.findParamFromLength(
                                    stepSizeU *
                                        static_cast<double>(c),
                                    &stat);
	                        }
                            if (!stat || !std::isfinite(param)) {
                                return stat ? MStatus(MS::kInvalidParameter)
                                            : stat;
                            }
                        
                        
                        if (wrapMode == 2) {
                            if (!dommy::numeric::wrapPeriodicProductSum(
                                    std::array<double, 3>{{param, offsetU, randArray[c]}},
                                    std::array<double, 3>{{1.0, 1.0, 1.0}},
                                    start, end, param)) return MS::kInvalidParameter;
                        } else {
                            param += offsetU + randArray[c];
                        }
                        
                        
                        if (wrapMode==1) {//clamp
                            if (param < start) {
                                param = start;
                            } else if (param > end) {
                                param = end;
                            }
	                        }
                        
                        
                        
	                        MPoint pt;
	                        stat = crvFn.getPointAtParam(
                                param, pt, MSpace::kWorld);
                            if (!stat) return stat;
                            if (!dommy::maya::isFinitePoint(pt)) {
                                return MS::kInvalidParameter;
                            }
	                        DOMMY_RETURN_IF_FAILURE(pointArray[i].append(pt));
                    }
                    
                    
                } else if(objHnd.type() == MFnData::kNurbsSurface) {//Nurbs Surface Object
                    
#pragma mark nurbs surface
                    
	                    int columnUNum = attrHnd.child(aColumnNum).asInt();
                        std::size_t checkedColumnCount = 0U;
                        if (!dommy::generation::checkedCount(
                                columnUNum, curveCount, checkedColumnCount) ||
                            checkedColumnCount == 0U) {
                            EchoWarning(name().asChar(), "Column count must be positive and within the curve count.");
                            return MS::kInvalidParameter;
                        }
                    
	                    MFnNurbsSurface srfFn;
	                    stat = srfFn.setObject( objHnd.data() );
	                    if (!stat) return stat;
	                    
	                    double startU,endU,startV,endV;
	                    stat = srfFn.getKnotDomain(
                            startU, endU, startV, endV);
                        if (!stat) return stat;
                    
                    
                    
                    
                    
                    
	                    const double uTotalLength =
                            std::fabs(endU-startU);
	                    const double vTotalLength =
                            std::fabs(endV-startV);
	                    
	                    const double finLengthU =
                            uTotalLength * rangeU;
	                    const double finLengthV =
                            vTotalLength * rangeV;
                        if (!std::isfinite(startU) ||
                            !std::isfinite(endU) ||
                            !std::isfinite(startV) ||
                            !std::isfinite(endV) ||
                            !std::isfinite(finLengthU) ||
                            !std::isfinite(finLengthV) ||
                            (wrapMode == 2 &&
                             (uTotalLength <=
                                  std::numeric_limits<double>::epsilon() ||
                              vTotalLength <=
                                  std::numeric_limits<double>::epsilon()))) {
                            return MS::kInvalidParameter;
                        }
                    
	                    const std::size_t columnCount =
                            std::min(checkedColumnCount, curveCount);
	                    
	                    size_t rowVNum = curveCount / columnCount;
	                    size_t amari = curveCount % columnCount;
                    
                    
                    
                    if (amari!=0) {
                        rowVNum++;
                    }
                    
	                    
                    
                    
                    
	                    double stepU = finLengthU/(double)(columnCount+1U);
                    double stepV = finLengthV/(double)(rowVNum+1);
                    
                    
                    
                    
                    std::vector<double> stepUArray;
                    std::vector<double> stepVArray;
                    
                    
                    if(mode==0) {//divide
	                        for (size_t c=0; c<columnCount; c++) {
                            stepUArray.push_back(stepU*(double)(c+1));
                        }
                        for (size_t c=0; c<rowVNum; c++) {
                            stepVArray.push_back(stepV*(double)(c+1));
                        }
                    } else {//step
                        
	                        for (size_t c=0; c<columnCount; c++) {
                            stepUArray.push_back(stepSizeU*(double)c);
                        }
                        for (size_t c=0; c<rowVNum; c++) {
                            stepVArray.push_back(stepSizeV*(double)c);
                        }
                    }
                    
                    
                    
                    
                    
                    
                    
                    
                    //create random
                    const bool isBool = attrHnd.child(aIsRandom).asBool();
                    std::vector<float> randUArray(curveCount, 0.0);
                    std::vector<float> randVArray(curveCount, 0.0);
                    
                    if (isBool) {//random
                        const uint seed = attrHnd.child(aSeed).asInt();
	                        const float randomValue =
                                attrHnd.child(aRandom).asFloat();
                            if (!std::isfinite(randomValue)) {
                                EchoWarning(name().asChar(), "Random range must be finite.");
                                return MS::kInvalidParameter;
                            }
                            const float randVal =
                                std::fabs(randomValue);
                        
                        if(randVal!=0.0) {
                            
                            
                            
                            
                            float halfval = randVal*.5;
                            std::mt19937 eng(seed);
                            
                            std::uniform_real_distribution<float> distribU(
                                -halfval, halfval);
                            std::uniform_real_distribution<float> distribV(
                                -halfval, halfval);
                            
                            for (size_t c=0; c<curveCount; c++) {
                                float valU = distribU(eng);
                                randUArray[c] = valU;
                                float valV = distribV(eng);
                                randVArray[c] = valV;
                            }
                            
                        }
                        
                    }
                    
                    
                    
                    
                    
                    
                    
                    
                    double uParam=startU;
                    double vParam=startV;
                    
                    size_t curveIndex = 0;
                    for (size_t r=0; r<rowVNum; r++) {
	                        for (size_t c=0; c<columnCount; c++) {
                            
                            if (curveIndex==curveCount) {
                                break;
                            }
                            
                            double paramU;
                            double paramV;
                            if(mode==0) {//divide
                                paramU = uParam+stepUArray[c];
                                paramV = vParam+stepVArray[r];
                            } else {//step
                                paramU = stepSizeU*c;
                                paramV = stepSizeV*r;
                            }
                            
                            if (wrapMode == 2) {
                                if (!dommy::numeric::wrapPeriodicProductSum(
                                        std::array<double, 3>{{paramU, offsetU, randUArray[curveIndex]}},
                                        std::array<double, 3>{{1.0, 1.0, 1.0}},
                                        startU, endU, paramU) ||
                                    !dommy::numeric::wrapPeriodicProductSum(
                                        std::array<double, 3>{{paramV, offsetV, randVArray[curveIndex]}},
                                        std::array<double, 3>{{1.0, 1.0, 1.0}},
                                        startV, endV, paramV)) return MS::kInvalidParameter;
                            } else {
                                paramU += offsetU + randUArray[curveIndex];
                                paramV += offsetV + randVArray[curveIndex];
                            }
                            
                            
                            //check wrap
                            if (wrapMode==1) {//clamp
                                if (paramU < startU) {
                                    paramU = startU;
                                } else if (paramU > endU) {
                                    paramU = endU;
                                }
                                if (paramV < startV) {
                                    paramV = startV;
                                } else if (paramV > endV) {
                                    paramV = endV;
                                }
                                
	                            }
                            
                            
                            
                            
                            
	                            MPoint pt;
	                            stat = srfFn.getPointAtParam(
                                    paramU, paramV, pt,
                                    MSpace::kWorld);
                                if (!stat) return stat;
                                if (!dommy::maya::isFinitePoint(pt)) {
                                    return MS::kInvalidParameter;
                                }
                            
                            DOMMY_RETURN_IF_FAILURE(pointArray[i].append(pt));
                            
                            
                            curveIndex++;
                        }
                    }
                    
                    
                } else {
                    EchoWarning(name().asChar(), "Not support input object.");
                    
                    MDataHandle output = data.outputValue(outDC, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
	                    DOMMY_RETURN_IF_FAILURE(
                            dommy::maya::publishPluginDataObject(
                                output, outputDataObject, *outDCrvs));
                    return data.setClean(plug);
                }
            }
            
            
            
            //generate curves
            //        size_t kontCount = segmentNum * ((pointArray.size()-1) +  pointArray.size());
            //        float stepKnots = 1.0/(double)(kontCount);
            //        if (VERBOSE)cout << "kontCount:" << kontCount << endl;
            //        if (VERBOSE)cout << "stepKnots:" << stepKnots << endl;
            
            
            DCurveData::DCurveList generatedCurves;
            generatedCurves.reserve(curveCount);
            for (size_t c=0; c<curveCount; ++c) {
                
                //generate cvs
                MPointArray cvs;
                for (size_t i=0,N=pointArray.size(); i<N; ++i) {
                    
                    MPoint pt = pointArray[i][c];
                    DOMMY_RETURN_IF_FAILURE(cvs.append(pt));
                    
                    //ass segment
	                    if (i<(pointArray.size()-1) && segmentCount) {
	                        MPoint nextpt = pointArray[i+1][c];
	                        MVector dirVec = nextpt-pt;
	                        double vecStep = 1.0/(double)(segmentCount+1U);
	                        for (size_t s=1U; s<=segmentCount; ++s) {
                            MPoint segPt = pt+(dirVec*(vecStep*s));
                            DOMMY_RETURN_IF_FAILURE(cvs.append(segPt));
                        }
                    }
                    
                }
                //            if (VERBOSE)cout << "cvs:" << cvs.length() << endl;
                
                // The constructor normalizes short curves and builds matching knots.
                generatedCurves.push_back(DCItem(new DCurveInternalData(cvs, curveDegree, true)));
            }
            
            
            
            
            std::size_t actualPointCount = 0U;
            if (!outDCrvs->insert(generatedCurves) ||
                outDCrvs->numberOfCurves() != curveCount ||
                !outDCrvs->getCheckedTotalCvs(actualPointCount) ||
                actualPointCount != outputPointCount) return MS::kFailure;
            generatedCurves.clear();
            outDCrvs->updateBBox();
            
            MDataHandle output = data.outputValue(outDC, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
	            DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishPluginDataObject(output, outputDataObject, *outDCrvs));
            output.setClean();
            return data.setClean(plug);
        }
		
		
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCurveLinkGen")



MStatus DCurveLinkGen::initialize() try
{
	MStatus stat;
	
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	
	
	
	
	
	aCount = nAttr.create( "Count", "cnt", MFnNumericData::kInt , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 10 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));
	
	
	aSegment = nAttr.create( "Segment", "seg", MFnNumericData::kInt , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 2 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSegment));
	
    
    MFnEnumAttribute degEnumFn;
    aDegree = degEnumFn.create( "Degree", "deg", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField( "Linear", 0 ));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField( "Cubic", 1 ));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));
    
    
    aInObject = gAttr.create( "InObject", "inobj" , &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));
	
	
	MFnEnumAttribute mdEnumFn;
	aMode = mdEnumFn.create( "Mode", "md", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(mdEnumFn.addField( "Divide", 0 ));
	DOMMY_RETURN_IF_FAILURE(mdEnumFn.addField( "Step", 1 ));
	DOMMY_RETURN_IF_FAILURE(mdEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));
	
	
	MFnEnumAttribute wrapEnumFn;
	aWrapMode = wrapEnumFn.create( "WrapMode", "wrmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField( "None", 0 ));
	DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField( "Clamp", 1 ));
	DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField( "Loop", 2 ));
	DOMMY_RETURN_IF_FAILURE(wrapEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aWrapMode));
	
    aColumnNum = nAttr.create( "ColumnNum", "clmnum", MFnNumericData::kInt , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 50 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColumnNum));
	
	aStepSizeU = nAttr.create( "StepSizeU", "stszu", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 50.0 ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStepSizeU));
    
    aStepSizeV = nAttr.create( "StepSizeV", "stszv", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 50.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStepSizeV));
	
    aRangeU = nAttr.create( "RangeU", "rngu", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRangeU));
    
    aRangeV = nAttr.create( "RangeV", "rngv", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRangeV));
    
    aOffsetU = nAttr.create( "OffsetU", "ofu", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( -1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetU));
    
    aOffsetV = nAttr.create( "OffsetV", "ofv", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( -1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetV));
	
	
	aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	aIsRandom = nAttr.create( "IsRandom", "isrnd", MFnNumericData::kBoolean , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandom));
	
	aRandom = nAttr.create( "Random", "rnd", MFnNumericData::kFloat , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 2.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 5.0 ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRandom));
	
	MFnCompoundAttribute cmpAttr;
	aLinkList = cmpAttr.create("LinkList", "llst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aInObject));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aMode));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aWrapMode));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aColumnNum));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aStepSizeU));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aStepSizeV));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aRangeU));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aRangeV));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aOffsetU));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aOffsetV));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aSeed));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aIsRandom));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aRandom));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray( true ));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLinkList));
	
	
	outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
	
	
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aCount, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSegment, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aDegree, outDC ) );
    
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aInObject, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aMode, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aWrapMode, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aColumnNum, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aStepSizeU, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aStepSizeV, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRangeU, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aRangeV, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aOffsetU, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aOffsetV, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aSeed, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aIsRandom, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aRandom, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aLinkList, outDC ) );
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DCurveLinkGen", "initialize", std::current_exception());
}

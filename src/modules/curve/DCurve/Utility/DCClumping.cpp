//
//  DCClumping.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/24.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCClumping.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DDataUtility.h"

#include <cmath>

#include "DDataOp.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DAttributeBlendMath.h"


const MTypeId DCClumping::typeId( ID_DC_CLUMPING );


MObject DCClumping::inDC;

MObject DCClumping::targetDC;
MObject DCClumping::aIndexAttribute;
MObject DCClumping::aWeight;
MObject DCClumping::aClumpRamp;
MObject DCClumping::outDC;


void *DCClumping::creator()
{
    return ::dommy::maya::createObject("DCClumping", [&]() {
        return new DCClumping();
    });
}


void DCClumping::postConstructor() {
    dommy::maya::initializeNodeRamps("DCClumping", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aClumpRamp,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}


MStatus DCClumping::compute(const MPlug& plug, MDataBlock& data) try
{
    
    MStatus stat;
    
    if (plug == outDC) {

        
#pragma mark get target dcurve
        const DCurveData* target_dcurve = nullptr;
        stat = getValidInputDC(targetDC, data, target_dcurve);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        if(!target_dcurve) {
            EchoWarning(name().asChar(), "Failed to get a target DCurve.");
            return MS::kFailure;
        }
        
        const MDataHandle indexHandle = data.inputValue(aIndexAttribute, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        std::string indexAttr = indexHandle.asString().asChar();
        
        if (indexAttr.empty()) {
            indexAttr = "idx";
        }
        
        
        
        
#pragma mark get input dcurve
        const DCurveData* in_dcurve = nullptr;
        stat = getValidInputDC(inDC, data, in_dcurve);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
        if (in_dcurve) {
            
            if (!in_dcurve->isExistAttr(indexAttr)) {
                EchoWarning(name().asChar(), "target dcurve is not have index attribute!");
                return MS::kFailure;
            } else if (in_dcurve->getAttr(indexAttr)->getAttrType() != DP_ATTR_INT_ARRAY) {
                EchoWarning(name().asChar(), "Index attribute isn't int array type!");
                return MS::kFailure;
            }
            
            
            //出力DCurveの作成
            MFnPluginData pluginData;
            pluginData.create(DCurveData::id, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            DCurveData* const outDCrvs(dommy::maya::pluginDataPointer<DCurveData>(pluginData, &stat));
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            
            if (outDCrvs) {
                
                
                DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*in_dcurve));
                
                
                const size_t curveCount = outDCrvs->numberOfCurves();
                
                if (curveCount && target_dcurve->numberOfCurves()) {
                    
                    
                    //get attribute
                    std::vector<DIntArray> idxList = outDCrvs->attrListValue<DIntArray>(indexAttr);
                    
                    
                    MPlug clumpRampPlug( thisMObject(), aClumpRamp );
                    MRampAttribute clmpRampAttr( clumpRampPlug );
                    
                    const MDataHandle weightHandle = data.inputValue(aWeight, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const float weight = weightHandle.asFloat();
                    if (!std::isfinite(weight) || idxList.size() < curveCount) {
						MGlobal::displayError("DCClumping: invalid weight or incomplete index attribute.");
						return MS::kFailure;
					}
					const size_t targetCurveCount = target_dcurve->numberOfCurves();
					for (size_t i=0; i<curveCount; ++i) {
						if (!idxList[i].empty() &&
							(idxList[i][0] < 0 || static_cast<size_t>(idxList[i][0]) >= targetCurveCount)) {
							MGlobal::displayError("DCClumping: target curve index is out of range.");
							return MS::kFailure;
						}
					}

                    for (size_t i=0; i<curveCount; ++i) {
                            
                            DCItem cIt=outDCrvs->getCurvePtr(i);
                            
                            if (idxList[i].size()==0) {
                                continue;
                            }
                            
                            
							const size_t targetIndex = static_cast<size_t>(idxList[i][0]);
                            ConstDCItem item = target_dcurve->getCurvePtr(targetIndex);
							if (!cIt || !item) return MS::kFailure;
                            const DPointList *tPts = item->getPositionPtrConst();
                            size_t tpSize = tPts->size();
                            //現在は同じポイント数のみ対応
                            if (tpSize == 0 || tpSize != cIt->numberOfPoints() ) {
                                continue;
                            }
                            
                            size_t pIndex=0;
                            DPointList points = cIt->getPositionConst();
                            for (auto &pIt : points) {
                                if (pIndex==0) {
                                    pIndex++;
                                    continue;
                                }
                                
                                
                                float cval = 0.0f;
								const float pos = tpSize > 1
									? static_cast<float>(pIndex) / static_cast<float>(tpSize-1)
									: 0.0f;
                                if (!dommy::maya::sampleFiniteRamp(
                                        clmpRampAttr, pos, cval)) {
                                    return MS::kFailure;
                                }
                                
                                DPoint blended = pIt;
                                for (unsigned int axis = 0U; axis < 3U; ++axis) {
                                    if (!dommy::attribute::checkedMaskedLinearBlend(
                                            pIt[axis], (*tPts)[pIndex][axis], weight, cval,
                                            blended[axis])) return MS::kInvalidParameter;
                                }
                                pIt = blended;
                                
                                pIndex++;
                            }//loop point
                            if (!cIt->replacePositions(points)) {
                                return MS::kFailure;
                            }
                            
                            
                    }
                    
                    outDCrvs->updateBBox();
                    
                }//main process
                
                
#pragma mark Create output
                MStatus outputStatus;
                MDataHandle output = data.outputValue(outDC, &outputStatus);
                CHECK_MSTATUS_AND_RETURN_IT(outputStatus);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DCurveData>(
                        output, pluginData.object(), DCurveData::id));
                
                return data.setClean(plug);
                
                
            }//check output dcurve
            
            
        }//check input dcurve
        
    }//check plug
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCClumping")
            
            
            
            
            


MStatus DCClumping::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    targetDC = tAttr.create("targetDC", "tdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetDC));

    const MObject aIndexAttributeDefault = fnStringData.create("idx", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aIndexAttribute =
        tAttr.create("IndexAttribute", "idxattr", MFnData::kString, aIndexAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIndexAttribute));

    aClumpRamp = MRampAttribute::createCurveRamp("ClumpRamp", "clprmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClumpRamp));

    aWeight = nAttr.create("Weight", "wei", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(targetDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIndexAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aClumpRamp, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeight, outDC));
    return MS::kSuccess;
}

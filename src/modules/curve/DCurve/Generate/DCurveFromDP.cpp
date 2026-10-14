//
//  DCurveFromDP.cpp
//  dommyTool
//
//  Created by yuzo on 1/24/17.
//
//

#include "DCurveFromDP.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DCurveGenerationStorage.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaNameList.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"





#include "dommyMath.h"

#include "DPointCommon.h"

#include <random>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <cstring>

namespace {

MStatus selectPointCurveInput(const DPData& input, const MString& groupNames,
                              dommy::generation::CurveGenerationStorage& storage,
                              DPIdList& selection, bool& groupUse, DPointList& points)
{
    const std::size_t count = input.numberOfPoints();
    if (count > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
    // Source-cardinality selection masks; upstream points/attributes are borrowed.
    storage.add(count, 8U);
    const std::size_t characters = groupNames.length();
    storage.add(characters, 64U);
    const std::size_t bytes = std::strlen(groupNames.asChar());
    if (bytes > characters) storage.add(bytes - characters, 64U);
    dommy::maya::NameListBudget namesBudget;
    MStringArray names;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::splitCheckedNameList(groupNames, count, namesBudget, names));

    DPIdList selected;
    selected.resize(count);
    std::fill(selected.begin(), selected.end(), false);
    bool foundGroup = false;
    for (unsigned int index = 0U; index < names.length(); ++index) {
        std::string name = names[index].asChar();
        const bool inverted = !name.empty() && name[0] == '!';
        if (inverted) name.erase(0U, 1U);
        if (!input.isExistGrp(name)) continue;
        const DPGroupData::ConstPtr group = input.getGrp(name);
        if (!group || group->getGroupIds().size() != count) return MS::kInvalidParameter;
        const DPIdList& ids = group->getGroupIds();
        for (std::size_t i = 0U; i < count; ++i)
            selected[i] = selected[i] || (inverted ? !ids[i] : ids[i]);
        foundGroup = true;
    }
    // Legacy behavior: empty lists and lists containing only missing names use
    // all points; existing empty groups instead select no points.
    if (!foundGroup) std::fill(selected.begin(), selected.end(), true);
    const DPointList* source = input.getPositionPtrConst();
    if (!source || source->size() != count) return MS::kInvalidParameter;
    const std::size_t selectedCount = static_cast<std::size_t>(std::count(selected.begin(), selected.end(), true));
    // Selected points, random/search points, and per-root vector capacity.
    storage.add(selectedCount, 256U);
    DPointList candidate;
    candidate.reserve(selectedCount);
    for (std::size_t i = 0U; i < count; ++i)
        if (selected[i]) candidate.push_back((*source)[i]);
    points.swap(candidate);
    selection.swap(selected);
    groupUse = foundGroup;
    return MS::kSuccess;
}

void admitPointCurveOutput(dommy::generation::CurveGenerationStorage& storage,
                           std::size_t curves, std::size_t points, std::size_t largestCurve)
{
    std::size_t bytes = 0U;
    if (curves > dommy::generation::kMaxGeneratedCurveCount ||
        points > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::generation::curveGenerationStorageBytes(curves, points, largestCurve, 0U, bytes))
        throw std::length_error("DCurveFromDP output exceeds its generation budget");
    storage.add(1U, bytes);
}

void preflightPointCurveAttributes(const DPData& input,
                                   dommy::generation::CurveGenerationStorage& storage)
{
    dommy::point::PointPayloadBudget metadata;
    dommy::point::preflightAttributePayload(input.beginAttr(), input.endAttr(),
                                           input.numberOfPoints(), metadata);
    storage.add(1U, metadata.estimatedBytes());
    for (auto attr = input.beginAttr(); attr != input.endAttr(); ++attr)
        storage.add(1U, 512U); // attribute objects/maps and clone bookkeeping
}

MStatus transferPointCurveAttributes(const DPData& input, const DPIdList& selection,
                                     bool groupUse, DCurveInternalData& output)
{
    for (auto attribute = input.beginAttr(); attribute != input.endAttr(); ++attribute) {
        if (!attribute->second) return MS::kInvalidParameter;
        if (groupUse) {
            DPAttributeData::Ptr selected = attribute->second->copy();
            if (!selected || !selected->cropArrayFromIds(selection)) return MS::kFailure;
            output.addAttr(attribute->first, *selected);
        } else {
            output.addAttr(attribute->first, *attribute->second);
        }
    }
    return MS::kSuccess;
}

} // namespace




const MTypeId DCurveFromDP::typeId( ID_DC_FROM_DP_GEN );

MObject DCurveFromDP::inDP;
MObject DCurveFromDP::aGroup;

MObject DCurveFromDP::aTargetDP;

MObject DCurveFromDP::aCreateType;
MObject DCurveFromDP::aDegreeType;

MObject DCurveFromDP::aIsClose;

MObject DCurveFromDP::aStepCount;
MObject DCurveFromDP::aSearchDistance;
MObject DCurveFromDP::aSearchMaxCount;
MObject DCurveFromDP::aDistanceMethod;
MObject DCurveFromDP::aSeed;
MObject DCurveFromDP::aRandomMethod;



MObject DCurveFromDP::outDC;





void *DCurveFromDP::creator() {
    return ::dommy::maya::createObject("DCurveFromDP", [&]() {
        return new DCurveFromDP();
    });
}


MStatus DCurveFromDP::compute( const MPlug& plug, MDataBlock& data ) try
{
	MStatus stat;
	

	
	if( plug == outDC )
	{
		
        MDataHandle handle = data.inputValue(aGroup, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MString groupName = handle.asString();
        handle = data.inputValue(aCreateType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const DC_DP_GEN_TYPE createTypeVal = handle.asShort();
        handle = data.inputValue(aDegreeType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const DC_DEGREE_TYPE degreeTypeVal = handle.asShort();
        handle = data.inputValue(aDistanceMethod, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const DC_DISTANCE_METHOD_TYPE distanceMethodVal = handle.asShort();
        handle = data.inputValue(aRandomMethod, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const DC_RANDOM_METHOD_TYPE randomMethodVal = handle.asShort();
        handle = data.inputValue(aIsClose, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const bool isCloseVal = handle.asBool();
        handle = data.inputValue(aSeed, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const int rawSeed = handle.asInt();
        handle = data.inputValue(aStepCount, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const int rawStepCount = handle.asInt();
        handle = data.inputValue(aSearchDistance, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double searchDistanceVal = handle.asDouble();
        handle = data.inputValue(aSearchMaxCount, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const int rawSearchMaxCount = handle.asInt();

        // Validate signed inputs before conversion, including inactive mode
        // controls. Large positive counts retain their legacy grouping/limit
        // semantics; actual output/work budgets are enforced below.
        std::size_t stepCountVal = 0U;
        std::size_t searchMaxCountVal = 0U;
        const std::size_t maximumInputCount =
            static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (createTypeVal < DC_DP_GEN_ALL ||
            createTypeVal >= DC_DP_GEN_TYPE_COUNT ||
            (degreeTypeVal != DC_kLinear && degreeTypeVal != DC_kCubic) ||
            distanceMethodVal < DC_DISTANCE_ALL ||
            distanceMethodVal >= DC_DISTANCE_METHOD_TYPE_COUNT ||
            randomMethodVal < DC_RANDOM_ALL ||
            randomMethodVal >= DC_RANDOM_METHOD_TYPE_COUNT ||
            rawSeed < 0 ||
            !dommy::generation::checkedCount(
                rawStepCount, maximumInputCount, stepCountVal) ||
            stepCountVal < 2U ||
            !dommy::generation::checkedCount(
                rawSearchMaxCount, maximumInputCount, searchMaxCountVal) ||
            !std::isfinite(searchDistanceVal) || searchDistanceVal < 0.0) {
            MGlobal::displayError(
                "DCurveFromDP: invalid mode, degree, seed, step count, or search limit.");
            return MS::kInvalidParameter;
        }
        // Zero remains a supported deterministic seed for existing scenes.
        const unsigned int seedVal = static_cast<unsigned int>(rawSeed);
		
#pragma mark inDPの取得
		
		const DPData* inputDPData = nullptr;
		DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
		
		{
			
			
			
			
#pragma mark Create Output data
			MFnPluginData pluginData;
			pluginData.create(DCurveData::id, &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			
            DCurveData* outDCrvs = nullptr;
            stat = dommy::maya::getPluginData(pluginData, outDCrvs);
            if (!stat) return stat;
#pragma mark Select input points without cloning unused metadata
            dommy::generation::CurveGenerationStorage storage(0U);
            DPIdList grpIds;
            bool groupUse = false;
            DPointList selectedPoints;
            DOMMY_RETURN_IF_FAILURE(selectPointCurveInput(
                *inputDPData, groupName, storage, grpIds, groupUse, selectedPoints));
            const DPointList* pts = &selectedPoints;
			
            size_t pointCount = pts->size();
            if (pointCount > dommy::generation::kMaxGeneratedPointCount) {
                return MS::kInvalidParameter;
            }
            if (createTypeVal == DC_DP_GEN_ALL && pointCount > 1U) {
                preflightPointCurveAttributes(*inputDPData, storage);
            }
            DCurveData::DCurveList generatedCurves;
            std::size_t generatedPointCount = 0U;
            const auto stageCurve = [&](const DCItem& curve) {
                std::size_t nextPointCount = 0U;
                if (!curve || !curve->hasValidCurveTopology() ||
                    generatedCurves.size() >= dommy::generation::kMaxGeneratedCurveCount ||
                    !dommy::generation::checkedSum(generatedPointCount, curve->numberOfPoints(),
                        dommy::generation::kMaxGeneratedPointCount, nextPointCount)) {
                    throw std::invalid_argument("DCurveFromDP: curve batch exceeds its safety limits.");
                }
                generatedCurves.push_back(curve);
                generatedPointCount = nextPointCount;
            };
            
            
            int deg = degreeTypeVal==DC_kCubic?3:1;
            
            bool isOpen = true;
            if (isCloseVal) {
                isOpen = false;
            }
            
            
            
            
            
//            if(VERBOSE)cout << "Curve isOp:" << isOp << endl;
            
//            DPIdList dpIndexList;
			
			switch (createTypeVal) {
				case DC_DP_GEN_ALL:
				{
#pragma mark all
                    admitPointCurveOutput(storage, pointCount > 1U ? 1U : 0U,
                        pointCount > 1U ? pointCount : 0U, pointCount > 1U ? pointCount : 0U);
					MPointArray cvs;
					for (size_t pointIndex=0; pointIndex<pointCount; ++pointIndex) {
						DOMMY_RETURN_IF_FAILURE(cvs.append(DPToMP((*pts)[pointIndex])));
					}
						
					if(cvs.length()>1) {
                            //generate knot
                            MDoubleArray knots;
                            GenerateDCurveKnots(cvs.length(), deg, knots);
                            
                            DCItem crvItem = DCItem(new DCurveInternalData(cvs, knots, deg , isOpen));
                            
                            DOMMY_RETURN_IF_FAILURE(transferPointCurveAttributes(
                                *inputDPData, grpIds, groupUse, *crvItem));
                            
						stageCurve(crvItem);
					}
				}
					break;
				case DC_DP_GEN_STEP:
				{
                    
#pragma mark step
                    const std::size_t remainder = pointCount % stepCountVal;
                    const std::size_t count = pointCount / stepCountVal + (remainder > 1U ? 1U : 0U);
                    admitPointCurveOutput(storage, count, pointCount - (remainder == 1U ? 1U : 0U),
                                          count == 0U ? 0U : std::min(pointCount, stepCountVal));

					MPointArray cvs;
                    for(size_t i=0; i<pointCount; i++) {
                        
                        DOMMY_RETURN_IF_FAILURE(cvs.append(DPToMP((*pts)[i])));
                        
                        
                        if (cvs.length()==stepCountVal) {
                            //generate knot
                            MDoubleArray knots;
							GenerateDCurveKnots(
								cvs.length(), cvs.length()<3?1:deg, knots);
                            
                            stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                            
                            DOMMY_RETURN_IF_FAILURE(cvs.clear());
                        } else if (i==(pointCount-1) && cvs.length()>1) {
                            //last curve
                            
                            MDoubleArray knots;
							GenerateDCurveKnots(
								cvs.length(), cvs.length()<3?1:deg, knots);
                            
                            stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                            DOMMY_RETURN_IF_FAILURE(cvs.clear());
                            
                        }
                        
                        
                        
					}
                    
                    
				}
					break;
				case DC_DP_GEN_DISTANCE:
				{
#pragma mark distance
                    
                    if (searchDistanceVal > 0) {
                        // Bound the quadratic search and retained candidate pairs before batching.
                        std::size_t pairWork = 0U;
                        if (!dommy::generation::checkedProduct(pointCount, pointCount == 0U ? 0U : pointCount - 1U,
                                dommy::generation::kMaxWorkItemCount * 2U, pairWork)) return MS::kInvalidParameter;
                        std::size_t retainedPairCount = 0U;
                        
                        std::vector<MPoint> searchPtArray;
                        
						for (size_t p=0; p<pointCount; p++) {
                            searchPtArray.push_back(DPToMP((*pts)[p]));
                        }
                        
                        
                        std::vector< std::vector<MPoint> > curveCVPairArray;
						curveCVPairArray.resize(pointCount);
                        uint total=0;
                        
                        for (auto i = searchPtArray.begin(); i != searchPtArray.end(); ++i) {
                            
							std::vector<MPoint> pairs;
                            
                            for (auto j = i + 1; j != searchPtArray.end(); ++j) {
                                
                                double dx = std::abs(i->x - j->x), dy = std::abs(i->y - j->y), dz = std::abs(i->z - j->z);
                                if (dx > searchDistanceVal || dy > searchDistanceVal || dz > searchDistanceVal) {
                                    continue;
                                }
                                double distance = i->distanceTo(*j);
                                if (distance > searchDistanceVal) {
                                    continue;
                                }
                                
                                storage.add(1U, 128U); // retained MPoint pair and vector growth/copy capacity
                                if (!dommy::generation::checkedSum(retainedPairCount, 1U,
                                        dommy::generation::kMaxGeneratedPointCount, retainedPairCount)) {
                                    return MS::kInvalidParameter;
                                }
								pairs.push_back(*j);
                                // Only this ordered prefix can be emitted by either distance mode.
                                if (searchMaxCountVal != 0U && pairs.size() == searchMaxCountVal) break;
                            }
                            
                            
                            curveCVPairArray[total].swap(pairs);
                            //                        cout << "Pair size:" << total << " count:" << curveCVPairArray[total].size() << endl;
                            
                            total++;
                            
                            
                        }
                        
                        //                cout << "curveCVPairArray size:" << curveCVPairArray.size() << endl;
                        
                        
                        
                        std::size_t plannedCurves = 0U, plannedPoints = 0U, largestCurve = 0U;
                        for (const auto& pairs : curveCVPairArray) {
                            if (pairs.empty()) continue;
                            const std::size_t curves = distanceMethodVal == DC_DISTANCE_ALL ? 1U : pairs.size();
                            const std::size_t points = distanceMethodVal == DC_DISTANCE_ALL ? pairs.size() + 1U : pairs.size() * 2U;
                            if (!dommy::generation::checkedSum(plannedCurves, curves,
                                    dommy::generation::kMaxGeneratedCurveCount, plannedCurves) ||
                                !dommy::generation::checkedSum(plannedPoints, points,
                                    dommy::generation::kMaxGeneratedPointCount, plannedPoints)) return MS::kInvalidParameter;
                            largestCurve = std::max(largestCurve, distanceMethodVal == DC_DISTANCE_ALL ? points : 2U);
                        }
                        admitPointCurveOutput(storage, plannedCurves, plannedPoints, largestCurve);
                        if (distanceMethodVal == DC_DISTANCE_ALL) {//全てのポイントをつないで作成
                            
							for (size_t c=0; c<curveCVPairArray.size(); c++) {
                                const std::vector<MPoint>& pairs = curveCVPairArray[c];
                                //                    cout << "Create pairs size:" << c << " count:" << pairs.size() << endl;
                                
                                if (pairs.size() ==0) {
                                    continue;
                                }
                                
                                
                                MPointArray cvs;
                                MPoint basePt= DPToMP((*pts)[c]);
                                DOMMY_RETURN_IF_FAILURE(cvs.append(basePt));
                                
                                //接続するポイントの最大数の決定
								size_t appendCountMax;
                                if (!searchMaxCountVal) {
                                    appendCountMax = pairs.size();
                                } else {
                                    appendCountMax = searchMaxCountVal<pairs.size()?searchMaxCountVal:pairs.size();
                                }
                                
                                
								for (size_t p=0; p<appendCountMax; p++) {
                                    DOMMY_RETURN_IF_FAILURE(cvs.append(pairs[p]));
                                }
                                
                                
                                
                                //generate knot
                                MDoubleArray knots;
								GenerateDCurveKnots(
									cvs.length(), cvs.length()<3?1:deg, knots);
                                
                                stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                                
                            }
                            
                            
                        } else if (distanceMethodVal == DC_DISTANCE_ONE) {//見つかったポイント一つ一つをつないで作成
                            
							for (size_t c=0; c<curveCVPairArray.size(); c++) {
                                const std::vector<MPoint>& pairs = curveCVPairArray[c];
                                //                    cout << "Create pairs size:" << c << " count:" << pairs.size() << endl;
                                
                                if (pairs.size()==0) {
                                    continue;
                                }
                                
                                MPoint basePt= DPToMP((*pts)[c]);
                                
                                
                                //接続するポイントの最大数の決定
								size_t appendCountMax;
                                if (!searchMaxCountVal) {
                                    appendCountMax = pairs.size();
                                } else {
                                    appendCountMax = searchMaxCountVal<pairs.size()?searchMaxCountVal:pairs.size();
                                }
                                
                                
								for (size_t p=0; p<appendCountMax; p++) {
                                    
                                    MPointArray cvs;
                                    
                                    DOMMY_RETURN_IF_FAILURE(cvs.append(basePt));
                                    DOMMY_RETURN_IF_FAILURE(cvs.append(pairs[p]));
                                    
                                    
                                    //generate knot
                                    MDoubleArray knots;
									GenerateDCurveKnots(2U, 1, knots);
                                    
                                    stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                                    
								}
                                
                                
                            }
                            
                        }
                        
                    }
                    
				}
					break;
				case DC_DP_GEN_RANDOM:
				{
					const size_t curveCount = pointCount/2U;
                    
#pragma mark random
                    const bool singleCurve = randomMethodVal == DC_RANDOM_ALL;
                    admitPointCurveOutput(storage, singleCurve ? (pointCount > 1U ? 1U : 0U) : curveCount,
                        singleCurve ? (pointCount > 1U ? pointCount : 0U) : curveCount * 2U,
                        singleCurve ? (pointCount > 1U ? pointCount : 0U) : (curveCount == 0U ? 0U : 2U));
                    std::vector<MPoint> ptArray;
					for (size_t p=0; p<pointCount; p++) {
                        ptArray.push_back(DPToMP((*pts)[p]));
                    }
                    
                    //            srand(seedVal);
                    std::mt19937 engine(seedVal);
                    std::shuffle(ptArray.begin(), ptArray.end(),engine);
                    
                    
					if (randomMethodVal == DC_RANDOM_ALL && ptArray.size() > 1U) {
                        
                        
                        MPointArray cvs;
						for (size_t i=0; i<ptArray.size(); i++) {
                            DOMMY_RETURN_IF_FAILURE(cvs.append(ptArray[i]));
                        }
                        
                        
                        //generate knot
                        MDoubleArray knots;
						GenerateDCurveKnots(
							cvs.length(), cvs.length()<3?1:deg, knots);
                        
                        stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                        
                        
                    } else if (randomMethodVal == DC_RANDOM_PAIR) {
                        
						size_t totalIndex=0;
						for (size_t i=0; i<curveCount; i++) {
                            
                            
                            //カーブデータ作成
                            MPointArray cvs;
                            
                            DOMMY_RETURN_IF_FAILURE(cvs.append(ptArray[totalIndex]));
                            ++totalIndex;
                            DOMMY_RETURN_IF_FAILURE(cvs.append(ptArray[totalIndex]));
                            ++totalIndex;
                            
                            //generate knot
                            MDoubleArray knots;
							GenerateDCurveKnots(2U, 1, knots);
                            
                            stageCurve(DCItem(new DCurveInternalData(cvs, knots, cvs.length()<3?1:deg , isOpen)));
                            
                        }
                    }
                    
                    
				}
					break;
				case DC_DP_GEN_TARGET:
				{
#pragma mark target
                    
                    const DPData* targetDPData = getInputDP(aTargetDP, data);
                    
                    if (!targetDPData) {
                        EchoWarning(name().asChar(), "TargetDp data is empty.");
                        return MS::kFailure;
                    }
                    
                    const DPointList *targetPts = targetDPData->getPositionPtrConst();
                    
                    
					size_t lessTotalCount =
						pointCount<targetPts->size()?pointCount:targetPts->size();
                    admitPointCurveOutput(storage, lessTotalCount, lessTotalCount * 2U,
                                          lessTotalCount == 0U ? 0U : 2U);
                    
                    for (size_t c=0; c<lessTotalCount; c++) {
                        
                        MPointArray cvs(2);
                        if (cvs.length() != 2U) return MS::kFailure;
                        
                        cvs[0] = DPToMP((*pts)[c]);
                        cvs[1] = DPToMP((*targetPts)[c]);
                        
                        //generate knot
                        MDoubleArrayPtr knots = MDoubleArrayPtr(new MDoubleArray());
                        GenerateDCurveKnots(cvs.length(), 1, *knots);
                        
                        stageCurve(DCItem(new DCurveInternalData(cvs, *knots, 1)));
                    }
				}
					break;
				default:
                    return MS::kInvalidParameter;
			}
			
			
			
			
			
			
			
            std::size_t actualPointCount = 0U;
            if (!outDCrvs->insert(generatedCurves) ||
                outDCrvs->numberOfCurves() != generatedCurves.size() ||
                !outDCrvs->getCheckedTotalCvs(actualPointCount) ||
                actualPointCount != generatedPointCount) return MS::kFailure;
            generatedCurves.clear();
			outDCrvs->updateBBox();
			
			MDataHandle output = data.outputValue(outDC, &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			stat = dommy::maya::publishTypedPluginDataObject<DCurveData>(
				output, pluginData.object(), DCurveData::id);
			if (!stat) return stat;
			output.setClean();
			return data.setClean(plug);
			
		}
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCurveFromDP")






MStatus DCurveFromDP::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnGenericAttribute gAttr;
	
	
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    MFnStringData fnStringData;
    const MObject groupDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroup = tAttr.create("aGroup", "grp", MFnData::kString, groupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroup));
	
	aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));
	
	
	
	
	
	MFnEnumAttribute creEnumFn;
	aCreateType = creEnumFn.create( "CreateType", "crt", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(creEnumFn.addField( "All", DC_DP_GEN_ALL ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.addField( "Step", DC_DP_GEN_STEP ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.addField( "Ditance", DC_DP_GEN_DISTANCE ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.addField( "Random", DC_DP_GEN_RANDOM ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.addField( "Target Object",DC_DP_GEN_TARGET ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(creEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCreateType));
	
	aIsClose = nAttr.create( "IsClose", "icl", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsClose));
	
	MFnEnumAttribute degEnumFn;
	aDegreeType = degEnumFn.create( "DegreeType", "det", DC_kLinear, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(degEnumFn.addField( "Linear", DC_kLinear ));
	DOMMY_RETURN_IF_FAILURE(degEnumFn.addField( "Cubic", DC_kCubic));
	DOMMY_RETURN_IF_FAILURE(degEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(degEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDegreeType));
	
	
	
	
	aStepCount = nAttr.create( "StepCount", "stc", MFnNumericData::kInt, 2, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStepCount));
	
	aSearchDistance = nAttr.create( "SearchDistance", "sdi", MFnNumericData::kDouble, 5.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchDistance));
	
	aSearchMaxCount = nAttr.create( "SearchMaxCount", "sma", MFnNumericData::kInt, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchMaxCount));
	
	MFnEnumAttribute distEnumFn;
	aDistanceMethod = distEnumFn.create( "DistanceMethod", "dim", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(distEnumFn.addField( "All", DC_DISTANCE_ALL ));
	DOMMY_RETURN_IF_FAILURE(distEnumFn.addField( "One", DC_DISTANCE_ONE));
	DOMMY_RETURN_IF_FAILURE(distEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(distEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDistanceMethod));
	
	aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt, 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	MFnEnumAttribute rndEnumFn;
	aRandomMethod = rndEnumFn.create( "RandomMethod", "ram", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(rndEnumFn.addField( "All", DC_RANDOM_ALL ));
	DOMMY_RETURN_IF_FAILURE(rndEnumFn.addField( "Pair", DC_RANDOM_PAIR));
	DOMMY_RETURN_IF_FAILURE(rndEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(rndEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomMethod));
	
	
	outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
	
	
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroup, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aCreateType, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDegreeType, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsClose, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStepCount, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSearchDistance, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSearchMaxCount, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistanceMethod, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRandomMethod, outDC ));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveFromDP", "initialize", std::current_exception());
}

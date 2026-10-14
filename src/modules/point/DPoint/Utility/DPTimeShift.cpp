//
//  DPTimeShift.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/03.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPTimeShift.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "dpoint_ids.h"
#include "common/utility/DTimeSamplingBudget.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeArrayConversion.h"

#include "DNodeOperation.h"
#include "math/DIndexOrder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <random>

#include <tbb/blocked_range.h>






const MTypeId DPTimeShift::typeId( ID_DP_TIME_SHIFT );

MObject DPTimeShift::inDP;
MObject DPTimeShift::inObject;

MObject DPTimeShift::aPointOrderType;

MObject DPTimeShift::aCurrentTime;
MObject DPTimeShift::aTimeOffset;
MObject DPTimeShift::aTimeLength;
MObject DPTimeShift::aTimeSampling;

MObject DPTimeShift::aTimeLengthType;

MObject DPTimeShift::aSeed;
MObject DPTimeShift::aTimeLengthRandom;

MObject DPTimeShift::aTimeOffsetAttrName;
MObject DPTimeShift::aTimeLengthAttrName;

MObject DPTimeShift::inTimeOffsetData;
MObject DPTimeShift::inTimeLengthData;

MObject DPTimeShift::aTimeRamp;

MObject DPTimeShift::aIsAddTimeAttr;
MObject DPTimeShift::aTimeAttrValueType;
MObject DPTimeShift::aTimeAttrName;

MObject DPTimeShift::aIsAddParentIdAttr;
MObject DPTimeShift::aParentIdAttrName;

MObject DPTimeShift::outDP;

void *DPTimeShift::creator() {
    return ::dommy::maya::createObject("DPTimeShift", [&]() {
        return new DPTimeShift();
    });
}





// Keep the historical float distribution and its sequence for supported input.
DScalarDataList timeRandomValues(uint count, float length, float offset, long seed, float random)
{
    dommy::sampling::requireTimeSamplingInputs(count, length, offset, seed, random);
    DScalarDataList values;
    dommy::random::generateFloatValues(static_cast<std::uint32_t>(seed), -random, random, count, values);
    return values;
}

void getTimeValueArray(DScalarDataList& timeArray, const uint& timeSample,
                       const float& timeLength, const float& timeOffset,
                       const long& seedVal, const float& timeRandom)
{
    const auto random = timeRandomValues(timeSample, timeLength, timeOffset, seedVal, timeRandom);
    DScalarDataList candidate(timeSample);
    if (timeSample == 1U) candidate[0] = timeOffset + random[0];
    else if (timeSample == 2U) {
        candidate[0] = timeOffset + random[0];
        candidate[1] = timeLength + timeOffset + random[1];
    } else {
        const float step = timeLength / static_cast<float>(timeSample - 1U);
        for (uint i = 0U; i < timeSample; ++i)
            candidate[i] = step * static_cast<float>(i) + timeOffset + random[i];
    }
    dommy::sampling::requireFiniteTimes(candidate);
    timeArray.swap(candidate);
}

void getFrameValueArray(DScalarDataList& timeArray, const uint& timeSample,
                        const float& timeOffset, const long& seedVal, const float& timeRandom)
{
    const auto random = timeRandomValues(timeSample, 0.0f, timeOffset, seedVal, timeRandom);
    DScalarDataList candidate(timeSample);
    const double offset = std::trunc(static_cast<double>(timeOffset));
    for (uint i = 0U; i < timeSample; ++i) {
        // Preserve the one/two-sample placement and two-sample first jitter.
        // Truncate toward zero without an unsigned conversion or integer wrap.
        if (timeSample == 2U && i == 0U) candidate[i] = static_cast<float>(offset) + random[i];
        else {
            const double index = timeSample <= 2U ? timeSample : i;
            const double value = index + offset + std::trunc(static_cast<double>(random[i]));
            if (!dommy::numeric::checkedNumericCast(value, candidate[i]))
                throw std::invalid_argument("Frame offset cannot be represented");
        }
    }
    dommy::sampling::requireFiniteTimes(candidate);
    timeArray.swap(candidate);
}

MStatus getTimeRampValueArray(DScalarDataList& timeArray, MRampAttribute timeRampAttr,
                             const uint& timeSample, const float& timeLength,
                             const float& timeOffset, const long& seedVal, const float& timeRandom)
{
    const auto random = timeRandomValues(timeSample, timeLength, timeOffset, seedVal, timeRandom);
    DScalarDataList candidate(timeSample);
    const float step = timeSample > 1U ? 1.0f / static_cast<float>(timeSample - 1U) : 0.0f;
    for (uint i = 0U; i < timeSample; ++i) {
        float value = 0.0f;
        MStatus status;
        timeRampAttr.getValueAtPosition(timeSample == 1U ? 1.0f : step * i, value, &status);
        if (!status) return status;
        if (!std::isfinite(value)) return MS::kInvalidParameter;
        candidate[i] = timeLength * value + timeOffset + random[i];
    }
    dommy::sampling::requireFiniteTimes(candidate);
    timeArray.swap(candidate);
    return MS::kSuccess;
}

MStatus getTimeValueArrayFromData(DScalarDataList &timeOffsetArray, MDataHandle dataArrayHnd) {
	
	MStatus stat;
	MFnData::Type inDataType = dataArrayHnd.type();
	if (inDataType == MFnData::kFloatArray) {
		
		MFnFloatArrayData pData(dataArrayHnd.data(), &stat);
		if (!stat) return stat;
        const unsigned int count = pData.length(&stat);
        if (!stat) return stat;
        if (!dommy::sampling::validTimeStorage(count, 0U)) return MS::kInvalidParameter;
		
		MFloatArray dataArray = pData.array( &stat );
		if (!stat) return stat;
        if (dataArray.length() != count) return MS::kFailure;
		
        if (!dommy::numeric::checkedScalarArrayCopy(
                dataArray, dataArray.length(), count, timeOffsetArray))
            return MS::kInvalidParameter;
        
	} else if (inDataType == MFnData::kDoubleArray) {
		
		MFnDoubleArrayData pData(dataArrayHnd.data(), &stat);
		if (!stat) return stat;
        const unsigned int count = pData.length(&stat);
        if (!stat) return stat;
        if (!dommy::sampling::validTimeStorage(count, 0U)) return MS::kInvalidParameter;
		
		MDoubleArray dataArray = pData.array( &stat );
		if (!stat) return stat;
        if (dataArray.length() != count) return MS::kFailure;
		
        if (!dommy::numeric::checkedScalarArrayCopy(
                dataArray, dataArray.length(), count, timeOffsetArray))
            return MS::kInvalidParameter;
        
	} else if (inDataType == MFnData::kIntArray) {
		
		MFnIntArrayData pData(dataArrayHnd.data(), &stat);
		if (!stat) return stat;
        const unsigned int count = pData.length(&stat);
        if (!stat) return stat;
        if (!dommy::sampling::validTimeStorage(count, 0U)) return MS::kInvalidParameter;
		
		MIntArray dataArray = pData.array( &stat );
		if (!stat) return stat;
        if (dataArray.length() != count) return MS::kFailure;
		
        if (!dommy::numeric::checkedScalarArrayCopy(
                dataArray, dataArray.length(), count, timeOffsetArray))
            return MS::kInvalidParameter;
        
	} else {
		MGlobal::displayWarning("DPTimeShift:Not Supported type is inObject.");
		return MS::kInvalidParameter;
	}
	for (DScalarDataList::const_iterator value = timeOffsetArray.begin();
		 value != timeOffsetArray.end(); ++value) {
		if (!std::isfinite(*value)) return MS::kInvalidParameter;
	}
	return MS::kSuccess;
}

MStatus getTimeValueArrayFromData(DScalarDataList& values, const MPlug& plug) {
    dommy::maya::ScopedPlugDataHandle owner;
    DOMMY_RETURN_IF_FAILURE(owner.read(plug));
    return getTimeValueArrayFromData(values, owner.handle());
}

MStatus getTimeValueArrayFromData(DScalarDataList& values, MDataBlock& data,
                                const MObject& attribute) {
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    return getTimeValueArrayFromData(values, handle);
}

MStatus getInObjectPointFomTime(const MTime& time,
                             MDataHandle &hnd,
                             const MPlug &objPlug,
                             const DScalarDataList &timeArray,
                             DScalarDataList *timeAttrArray,
                             DAUIntList *parentIdArray,
                             std::vector<MPointArray> &objPointArray
                             )
{
    
    
    if(hnd.type() == MFnData::kNurbsCurve) {//Nurbs Curve Object
        
		return DGetPlugTimeCurve(time, objPointArray, objPlug, timeArray, timeAttrArray, parentIdArray);
        
    } else if(hnd.type() == MFnData::kNurbsSurface) {//Nurbs Surface Object
        
		return DGetPlugTimeNurbs(time, objPointArray, objPlug, timeArray, timeAttrArray, parentIdArray);
        
    } else if(hnd.type() == MFnData::kMesh) {//Nurbs Mesh Object
        
		return DGetPlugTimeMesh(time, objPointArray, objPlug, timeArray, timeAttrArray, parentIdArray);
        
    } else {
        MGlobal::displayWarning("DPTimeShift:Not Supported type is inObject.");
		return MS::kInvalidParameter;
	}
}

void normalizedTimeAttrValue(const DPTimeAttributeType &type,
                             const float &timeLength,
                             const float &timeOffset,
                             const float &timeSample,
                             const bool &isFramed,
                             DScalarDataList *timeAttrArray) {
	if (!timeAttrArray) {
		return;
	}
	
	float normlizedSize = isFramed?(timeSample-1):timeLength;
	
    
    if (isFramed && timeSample==2) {
        normlizedSize=2.0;
    }
    
    if (normlizedSize!=0.0 ) {
        if (type==DP_TIME_ATTR_NORMALIZE) {
            //Normalize
            for (uint i=0,N=timeAttrArray->size(); i<N; i++) {
                if (timeAttrArray->at(i)) {
                    timeAttrArray->at(i)= (timeAttrArray->at(i)-timeOffset)*(1.0f/normlizedSize);
                }
            }
        } else if(type==DP_TIME_ATTR_IGNORE_OFFSET) {
            //Ignore Time offset
            for (uint i=0,N=timeAttrArray->size(); i<N; i++) {
                if (timeAttrArray->at(i)) {
                    timeAttrArray->at(i)= (timeAttrArray->at(i))*(1.0f/normlizedSize);
                }
            }
        }
    }
        
    
    dommy::sampling::requireFiniteTimes(*timeAttrArray);
}

class TimeShiftPointFromGeo {
public:
	std::vector<MPointArray> *geoPoints;
	DScalarDataList *timeAttrArray;
	DAUIntList *pIdAttrArray;
    DPTimeShiftParams params;
	const MPlug objPlug;
	MDataHandle hnd;
	DScalarDataList timeOffsetList;
	DScalarDataList timeLengthList;
	const MTime currentTime;
	mutable MStatus status;
	
	
	TimeShiftPointFromGeo(const MDataHandle& hnd,
				const MPlug &objPlug,
				const DScalarDataList& timeOffsetList,
                const DScalarDataList& timeLengthList,
                          const DPTimeShiftParams& params,
				std::vector<MPointArray> &geoPoints,
				DScalarDataList *timeAttrArray,
				DAUIntList *pIdAttrArray) :
	geoPoints(&geoPoints),
	timeAttrArray(timeAttrArray),
	pIdAttrArray(pIdAttrArray),
	params(params),
	objPlug(objPlug),
	hnd(hnd),
	timeOffsetList(timeOffsetList),
	timeLengthList(timeLengthList),
	currentTime(params.m_time),
	status(MS::kSuccess)
	{ }
	
	void operator() ( const tbb::blocked_range<uint>& r ) const {
        dommy::sampling::TimeSamplingBudget budget;
		if(hnd.type() == MFnData::kNurbsCurve) {//Nurbs Curve Object
				
				for (uint i = r.begin(); i != r.end(); i++ ) {
					DScalarDataList timeValueArray;
					
					getTimeValueArray(timeValueArray,
                                      params.m_time_sample,
                                      timeLengthList[i],
                                      timeOffsetList[i],
                                      params.m_seed,
                                      params.m_time_rand);
					MPointArray pts;
					DScalarDataList newtimeAttrArray;
					status = DGetPlugTimeCurvePP(
						currentTime, pts, objPlug, timeValueArray, i, &newtimeAttrArray, &budget);
					if (!status) return;
                    
                    geoPoints->emplace_back();
                    status = geoPoints->back().copy(pts);
                    if (!status) return;
                    if (geoPoints->back().length() != pts.length()) { status = MS::kFailure; return; }
					
					if (timeAttrArray && newtimeAttrArray.size()) {
						
						normalizedTimeAttrValue(params.m_time_attr_type,
                                                timeLengthList[i],
                                                timeOffsetList[i],
                                                params.m_time_sample,
                                                false,
                                                &newtimeAttrArray);
						
						timeAttrArray->insert(timeAttrArray->end(), newtimeAttrArray.begin(), newtimeAttrArray.end());
					}
					
					if (pIdAttrArray && pts.length()) {
						DAUIntList newIds(pts.length(),i);
						pIdAttrArray->insert(pIdAttrArray->end(), newIds.begin(), newIds.end());
					}
				}

			
			
		}else if(hnd.type() == MFnData::kNurbsSurface) {//Nurbs Surface Object
			
			for (uint i = r.begin(); i != r.end(); i++ ) {
				
				DScalarDataList timeValueArray;
				getTimeValueArray(timeValueArray,
                                  params.m_time_sample,
                                  timeLengthList[i],
                                  timeOffsetList[i],
                                  params.m_seed,
                                  params.m_time_rand);
				
				MPointArray pts;
				DScalarDataList newtimeAttrArray;
				status = DGetPlugTimeNurbsPP(
					currentTime, pts, objPlug, timeValueArray, i, &newtimeAttrArray, &budget);
				if (!status) return;
				geoPoints->emplace_back();
				status = geoPoints->back().copy(pts);
				if (!status) return;
				if (geoPoints->back().length() != pts.length()) { status = MS::kFailure; return; }
				
				if (timeAttrArray && newtimeAttrArray.size()) {
					
					normalizedTimeAttrValue(params.m_time_attr_type,
                                            timeLengthList[i],
                                            timeOffsetList[i],
                                            params.m_time_sample,
                                            false,
                                            &newtimeAttrArray);
					
					timeAttrArray->insert(timeAttrArray->end(), newtimeAttrArray.begin(), newtimeAttrArray.end());
				}
				
				if (pIdAttrArray && pts.length()) {
					DAUIntList newIds(pts.length(),i);
					pIdAttrArray->insert(pIdAttrArray->end(), newIds.begin(), newIds.end());
				}
			}
			
			
			
		} else if(hnd.type() == MFnData::kMesh) {//Mesh Object
			
			for (uint i = r.begin(); i != r.end(); i++ ) {
				
				DScalarDataList timeValueArray;
				getTimeValueArray(timeValueArray,
                                  params.m_time_sample,
                                  timeLengthList[i],
                                  timeOffsetList[i],
                                  params.m_seed,
                                  params.m_time_rand);
				
				MPointArray pts;
				DScalarDataList newtimeAttrArray;
				status = DGetPlugTimeMeshPP(
					currentTime, pts, objPlug, timeValueArray, i, &newtimeAttrArray, &budget);
				if (!status) return;
				geoPoints->emplace_back();
				status = geoPoints->back().copy(pts);
				if (!status) return;
				if (geoPoints->back().length() != pts.length()) { status = MS::kFailure; return; }
				
				if (timeAttrArray && newtimeAttrArray.size()) {
					
					normalizedTimeAttrValue(params.m_time_attr_type,
                                            timeLengthList[i],
                                            timeOffsetList[i],
                                            params.m_time_sample,
                                            false,
                                            &newtimeAttrArray);
					
					timeAttrArray->insert(timeAttrArray->end(), newtimeAttrArray.begin(), newtimeAttrArray.end());
				}
				
				if (pIdAttrArray && pts.length()) {
					DAUIntList newIds(pts.length(),i);
					pIdAttrArray->insert(pIdAttrArray->end(), newIds.begin(), newIds.end());
				}
			}
			
		}
	}
};


class TimeShiftPointFromDP {
public:
    std::vector<MPointArray> *geoPoints;
    DScalarDataList *timeAttrArray;
	DAUIntList *pIdAttrArray;
    const DPTimeShiftParams params;
    const MPlug dpPlug;
    const DScalarDataList timeOffsetList;
	const DScalarDataList timeLengthList;
	const MTime currentTime;
	mutable MStatus status;
    
    TimeShiftPointFromDP(const MPlug &dpPlug,
                         const DScalarDataList &timeOffsetList,
                         const DScalarDataList &timeLengthList,
                         const DPTimeShiftParams &params,
                         std::vector<MPointArray> *geoPoints,
                         DScalarDataList *timeAttrArray,
						 DAUIntList *pIdAttrArray) :
    geoPoints(geoPoints),
    timeAttrArray(timeAttrArray),
	pIdAttrArray(pIdAttrArray),
	params(params),
	dpPlug(dpPlug),
	timeOffsetList(timeOffsetList),
	timeLengthList(timeLengthList),
	currentTime(params.m_time),
	status(MS::kSuccess)
    { }
    
    void operator() ( const tbb::blocked_range<uint>& r ) const {
        dommy::sampling::TimeSamplingBudget budget;
        
        
        for (uint i = r.begin(); i != r.end(); i++ ) {
            DScalarDataList timeValueArray;
            getTimeValueArray(timeValueArray,
                              params.m_time_sample,
                              timeLengthList[i],
                              timeOffsetList[i],
                              params.m_seed,
                              params.m_time_rand);
            MPointArray pts;
            DScalarDataList newtimeAttrArray;
			status = DGetPlugTimeDPPP(
				currentTime, pts, dpPlug, timeValueArray, i, &newtimeAttrArray, &budget);
			if (!status) return;
            geoPoints->emplace_back();
            status = geoPoints->back().copy(pts);
            if (!status) return;
            if (geoPoints->back().length() != pts.length()) { status = MS::kFailure; return; }
            
            if (timeAttrArray && newtimeAttrArray.size()) {
                if (params.m_time_attr_type!=DP_TIME_ATTR_RAW) {
                    normalizedTimeAttrValue(params.m_time_attr_type,
                                            timeLengthList[i],
                                            timeOffsetList[i],
                                            params.m_time_sample,
                                            false,
                                            &newtimeAttrArray);
                }
                
                timeAttrArray->insert(timeAttrArray->end(), newtimeAttrArray.begin(), newtimeAttrArray.end());
            }
			if (pIdAttrArray && pts.length()) {
				DAUIntList newIds(pts.length(),i);
				pIdAttrArray->insert(pIdAttrArray->end(), newIds.begin(), newIds.end());
			}
        }
        
    }
};



MStatus getInObjectPointFomTimePP(MDataHandle hnd,
                               const MPlug& objPlug,
                               DScalarDataList timeOffsetList,
                               DScalarDataList timeLengthList,
                               const DPTimeShiftParams& params,
                               DScalarDataList *timeAttrArray,
                               DAUIntList *pIdAttrArray,
                               std::vector<MPointArray> &objPointArray) {
    
	
    
    if(hnd.type() == MFnData::kNurbsCurve) {//Nurbs Curve Object
        
		MStatus status;
        MObject obj;
		status = objPlug.getValue(obj);
		if (!status) return status;
        
		MFnNurbsCurve crvFn(obj, &status);
		if (!status) return status;
		const int rawCount = crvFn.numCVs(&status);
		if (!status) return status;
		if (rawCount < 0) return MS::kFailure;
		const uint count = static_cast<uint>(rawCount);
        if (!dommy::sampling::validTimePointProduct(count, params.m_time_sample))
            return MS::kInvalidParameter;
        
        if (timeOffsetList.size()!=count) {
            timeOffsetList = DScalarDataList(count,params.m_time_offset);
        }
        
        if (timeLengthList.size()!=count) {
            timeLengthList = DScalarDataList(count,params.m_time_length);
        }
		
		if (count == timeOffsetList.size() && count == timeLengthList.size()) {
			tbb::blocked_range<uint> range = tbb::blocked_range<uint>(0, count, 10000);
            TimeShiftPointFromGeo body = TimeShiftPointFromGeo(hnd,
                                                               objPlug,
                                                               timeOffsetList,
                                                               timeLengthList,
                                                               params,
                                                               objPointArray,
                                                               timeAttrArray,
                                                               pIdAttrArray);
			// MPlug access and shared output containers are not thread-safe.
			body(range);
			if (!body.status) return body.status;
		}
		
        
    } else if(hnd.type() == MFnData::kNurbsSurface) {//Nurbs Surface Object
        
		MStatus status;
        MObject obj;
		status = objPlug.getValue(obj);
		if (!status) return status;
        
		MFnNurbsSurface surfFn(obj, &status);
		if (!status) return status;
        const int rawU = surfFn.numCVsInU(&status);
        if (!status) return status;
        const int rawV = surfFn.numCVsInV(&status);
        if (!status) return status;
        std::size_t countValue = 0U;
        if (rawU < 0 || rawV < 0 || !dommy::generation::checkedProduct(
                static_cast<std::size_t>(rawU), static_cast<std::size_t>(rawV),
                dommy::generation::kMaxGeneratedPointCount, countValue) ||
            !dommy::sampling::validTimePointProduct(countValue, params.m_time_sample))
            return MS::kInvalidParameter;
        const uint count = static_cast<uint>(countValue);
        
        if (timeOffsetList.size()!=count) {
            timeOffsetList = DScalarDataList(count,params.m_time_offset);
        }
        
        if (timeLengthList.size()!=count) {
            timeLengthList = DScalarDataList(count,params.m_time_length);
        }
        
		
		if (count == timeOffsetList.size() && count == timeLengthList.size()) {
			tbb::blocked_range<uint> range = tbb::blocked_range<uint>(0, count, 10000);
            TimeShiftPointFromGeo body = TimeShiftPointFromGeo(hnd,
                                                               objPlug,
                                                               timeOffsetList,
                                                               timeLengthList,
                                                               params,
                                                               objPointArray,
                                                               timeAttrArray,
															   pIdAttrArray);
			body(range);
			if (!body.status) return body.status;
		}
		
		
    } else if(hnd.type() == MFnData::kMesh) {//Mesh Object
		MStatus status;
        MObject obj;
		status = objPlug.getValue(obj);
		if (!status) return status;
        
		MFnMesh meshFn(obj, &status);
		if (!status) return status;
		const int rawCount = meshFn.numVertices(&status);
		if (!status) return status;
		if (rawCount < 0) return MS::kFailure;
		const uint count = static_cast<uint>(rawCount);
        if (!dommy::sampling::validTimePointProduct(count, params.m_time_sample))
            return MS::kInvalidParameter;
        
        if (timeOffsetList.size()!=count) {
            timeOffsetList = DScalarDataList(count,params.m_time_offset);
        }
        
        if (timeLengthList.size()!=count) {
            timeLengthList = DScalarDataList(count,params.m_time_length);
        }
        
		if (count == timeOffsetList.size() && count == timeLengthList.size()) {
			tbb::blocked_range<uint> range = tbb::blocked_range<uint>(0, count, 10000);
            TimeShiftPointFromGeo body = TimeShiftPointFromGeo(hnd,
                                                               objPlug,
                                                               timeOffsetList,
                                                               timeLengthList,
                                                               params,
                                                               objPointArray,
                                                               timeAttrArray,
															   pIdAttrArray);
			body(range);
			if (!body.status) return body.status;
		}
		
		
    } else {
        MGlobal::displayWarning("DPTimeShift:Not Supported type is inObject.");
		return MS::kInvalidParameter;
    }
	return MS::kSuccess;
}




void generateFinalPointList(const DPTimeShiftParams& params, const std::vector<MPointArray> &srclist, DPointList &ptlist)
{
    ptlist.clear();
    size_t totalPointCount = 0;
    size_t longestGroup = 0;
    for (size_t group = 0; group < srclist.size(); ++group) {
        const size_t length = srclist[group].length();
        if (length > std::numeric_limits<size_t>::max() - totalPointCount) {
            throw std::length_error("Time point count overflow");
        }
        totalPointCount += length;
        longestGroup = std::max(longestGroup, length);
    }
    std::size_t work = 0U;
    if (!dommy::sampling::validTimeStorage(totalPointCount, srclist.size()) ||
        (params.m_birth_type == DP_TIME_BIRTH_SAME_INDEX &&
         !dommy::generation::checkedProduct(longestGroup, srclist.size(),
             dommy::generation::kMaxWorkItemCount, work)))
        throw std::length_error("Time output/work budget exceeded");
    ptlist.reserve(totalPointCount);

    if (params.m_birth_type==DP_TIME_BIRTH_SAME_INDEX) {
        //same point index base 同じポイントから発生したポイント順に並び替え
        std::vector<std::size_t> groupLengths;
        groupLengths.reserve(srclist.size());
        for (const auto& group : srclist) groupLengths.push_back(group.length());
        if (!dommy::math::visitInterleavedIndices(groupLengths,
                [&srclist, &ptlist](std::size_t group, std::size_t pointIndex, std::size_t) {
                    const MPoint& point = srclist[group][pointIndex];
                    if (!dommy::maya::isFloatRepresentablePoint(point))
                        throw std::invalid_argument("Time point cannot be represented");
                    ptlist.push_back(DPoint(point[0], point[1], point[2]));
                }))
            throw std::length_error("Time point ordering overflow");
    } else {
        //same time base 同じ時間に発生したポイント順に並び替え
        for (size_t group = 0; group < srclist.size(); ++group) {
            for (size_t pointIndex = 0; pointIndex < srclist[group].length(); ++pointIndex) {
                const MPoint& point = srclist[group][pointIndex];
                if (!dommy::maya::isFloatRepresentablePoint(point))
                    throw std::invalid_argument("Time point cannot be represented");
                ptlist.push_back(DPoint(point[0], point[1], point[2]));
            }
        }
    }
}




namespace {
MStatus validateTimeOutputNames(const DPTimeShiftParams& params)
{
    const bool writeTime = params.m_is_time_attr && !params.m_time_attr_name.empty();
    const bool writeIds = params.m_is_parent_id_attr && !params.m_parent_id_attr_name.empty();
    if ((writeTime && (!dommy::attribute::validAttributeName(params.m_time_attr_name) ||
        params.m_time_attr_name.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
        (writeIds && (!dommy::attribute::validAttributeName(params.m_parent_id_attr_name) ||
        params.m_parent_id_attr_name.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
        (writeTime && writeIds && params.m_time_attr_name == params.m_parent_id_attr_name))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}
} // namespace

void generateOutputAttributeList(const DPTimeShiftParams& params ,
                                 DPData &dpdata,
                                 const std::vector<MPointArray> &srclist,
                                 DScalarDataList *timeAttrArray,
                                 DAUIntList *pIdAttrArray)
{
    if (!validateTimeOutputNames(params))
        throw std::invalid_argument("Time output attribute names are invalid or collide");
    if ((params.m_is_time_attr && (!timeAttrArray || dpdata.numberOfPoints() != timeAttrArray->size())) ||
        (params.m_is_parent_id_attr && (!pIdAttrArray || dpdata.numberOfPoints() != pIdAttrArray->size())))
        throw std::invalid_argument("Time output attribute count mismatch");
    std::vector<size_t> groupLengths;
    if (!dommy::sampling::validTimeStorage(dpdata.numberOfPoints(), srclist.size()))
        throw std::length_error("Time attribute storage budget exceeded");
    groupLengths.reserve(srclist.size());
    std::size_t total = 0U, longest = 0U;
    for (size_t group = 0; group < srclist.size(); ++group) {
        const std::size_t length = srclist[group].length();
        if (!dommy::generation::checkedSum(total, length, dpdata.numberOfPoints(), total))
            throw std::invalid_argument("Time output group count mismatch");
        longest = std::max(longest, length);
        groupLengths.push_back(length);
    }
    if (total != dpdata.numberOfPoints())
        throw std::invalid_argument("Time output group count mismatch");
    std::vector<size_t> order;
    if (params.m_birth_type == DP_TIME_BIRTH_SAME_INDEX) {
        std::size_t work = 0U;
        if (!dommy::generation::checkedProduct(longest, srclist.size(),
                dommy::generation::kMaxWorkItemCount, work) ||
            !dommy::math::buildInterleavedOrder(groupLengths, order))
            throw std::length_error("Time attribute ordering budget exceeded");
    }

    if (params.m_is_time_attr && timeAttrArray) {
        
        if (!params.m_time_attr_name.empty() && dpdata.numberOfPoints() == timeAttrArray->size()) {
            
            if (params.m_birth_type==DP_TIME_BIRTH_SAME_INDEX) {
                DScalarDataList reorderlist;
                reorderlist.reserve(order.size());
                for (size_t index = 0; index < order.size(); ++index) {
                    reorderlist.push_back((*timeAttrArray)[order[index]]);
                }
                const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(reorderlist);
                if (!dommy::attribute::commitStagedAttribute(
                        dpdata, params.m_time_attr_name, candidate, dpdata.numberOfPoints()))
                    throw std::runtime_error("Time attribute publication failed");
            } else {
                const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(*timeAttrArray);
                if (!dommy::attribute::commitStagedAttribute(
                        dpdata, params.m_time_attr_name, candidate, dpdata.numberOfPoints()))
                    throw std::runtime_error("Time attribute publication failed");
            }
        }
        
    }
    if (params.m_is_parent_id_attr && pIdAttrArray) {
        
        if (!params.m_parent_id_attr_name.empty() && dpdata.numberOfPoints() == pIdAttrArray->size()) {
            
            if (params.m_birth_type==DP_TIME_BIRTH_SAME_INDEX) {
                DAUIntList reorderlist;
                reorderlist.reserve(order.size());
                for (size_t index = 0; index < order.size(); ++index) {
                    reorderlist.push_back((*pIdAttrArray)[order[index]]);
                }
                const DPAttributeData::Ptr candidate = std::make_shared<DPUInt32Attr>(reorderlist, 0);
                if (!dommy::attribute::commitStagedAttribute(
                        dpdata, params.m_parent_id_attr_name, candidate, dpdata.numberOfPoints()))
                    throw std::runtime_error("Parent ID attribute publication failed");
            } else {
                const DPAttributeData::Ptr candidate = std::make_shared<DPUInt32Attr>(*pIdAttrArray, 0);
                if (!dommy::attribute::commitStagedAttribute(
                        dpdata, params.m_parent_id_attr_name, candidate, dpdata.numberOfPoints()))
                    throw std::runtime_error("Parent ID attribute publication failed");
            }
            
            
            
        }
    }
    
}



MStatus finishTimeShiftOutput(MDataBlock& data, const MPlug& plug, const MObject& output,
                              const DPTimeShiftParams& params,
                              const std::vector<MPointArray>& groups,
                              DScalarDataList* times, DAUIntList* ids)
{
    // No sampled groups historically produced an attribute-free empty output.
    if (!groups.empty()) DOMMY_RETURN_IF_FAILURE(validateTimeOutputNames(params));
    DPointList points;
    generateFinalPointList(params, groups, points);
    MObject object;
    DPData* staged = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DPData>(DPData::id, object, staged));
    if (!staged) return MS::kFailure;
    staged->setPosition(points);
    if (staged->numberOfPoints() != points.size()) return MS::kFailure;
    // Historically, no sampled groups produced an empty output without attributes.
    if (!groups.empty()) generateOutputAttributeList(params, *staged, groups, times, ids);
    staged->updateBBox();
    MStatus status;
    MDataHandle handle = data.outputValue(output, &status);
    if (!status) return status;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(handle, object, *staged));
    handle.setClean();
    return data.setClean(plug);
}

MStatus DPTimeShift::evalParams(MDataBlock& data, DPTimeShiftParams& params)
{
	MStatus stat;
	MDataHandle valueHandle = data.inputValue(aPointOrderType, &stat);
	if (!stat) return stat;
	params.m_birth_type = valueHandle.asShort();
	valueHandle = data.inputValue(aTimeLengthType, &stat);
	if (!stat) return stat;
	params.m_length_type = valueHandle.asShort();
	valueHandle = data.inputValue(aCurrentTime, &stat);
	if (!stat) return stat;
	params.m_time = valueHandle.asTime();
	valueHandle = data.inputValue(aTimeOffset, &stat);
	if (!stat) return stat;
	params.m_time_offset = valueHandle.asFloat();
	valueHandle = data.inputValue(aTimeLength, &stat);
	if (!stat) return stat;
	params.m_time_length = valueHandle.asFloat();
	valueHandle = data.inputValue(aTimeSampling, &stat);
	if (!stat) return stat;
	params.m_time_sample = valueHandle.asInt();
	valueHandle = data.inputValue(aTimeLengthRandom, &stat);
	if (!stat) return stat;
	params.m_time_rand = valueHandle.asFloat();
	valueHandle = data.inputValue(aSeed, &stat);
	if (!stat) return stat;
	params.m_seed = valueHandle.asInt();
	if (params.m_birth_type < 0 || params.m_birth_type >= DPTimeBirthType_COUNT ||
		params.m_length_type < 0 || params.m_length_type >= DPTimeLengthType_COUNT ||
		!std::isfinite(params.m_time.value()) ||
		!std::isfinite(params.m_time_offset) ||
		!std::isfinite(params.m_time_length) || params.m_time_length < 0.0f ||
		params.m_time_sample < 1 || !std::isfinite(params.m_time_rand) ||
		params.m_time_rand < 0.0f) {
		return MS::kInvalidParameter;
	}
    dommy::sampling::requireTimeSamplingInputs(params.m_time_sample, params.m_time_length,
        params.m_time_offset, params.m_seed, params.m_time_rand);
    MPlug timeRampPlug( thisMObject(), aTimeRamp );
	MRampAttribute timeRampAttr(timeRampPlug, &stat);
	if (!stat) return stat;
    params.m_time_ramp = timeRampAttr;
    
	valueHandle = data.inputValue(aIsAddTimeAttr, &stat);
	if (!stat) return stat;
	params.m_is_time_attr = valueHandle.asBool();
	valueHandle = data.inputValue(aTimeAttrValueType, &stat);
	if (!stat) return stat;
	params.m_time_attr_type = valueHandle.asShort();
	if (params.m_time_attr_type < 0 ||
		params.m_time_attr_type >= DPTimeAttributeType_COUNT) {
		return MS::kInvalidParameter;
	}
	valueHandle = data.inputValue(aIsAddParentIdAttr, &stat);
	if (!stat) return stat;
	params.m_is_parent_id_attr = valueHandle.asBool();
    
    //attribute name
	valueHandle = data.inputValue(aTimeOffsetAttrName, &stat);
	if (!stat) return stat;
	params.m_time_offset_name = valueHandle.asString().asChar();
	valueHandle = data.inputValue(aTimeLengthAttrName, &stat);
	if (!stat) return stat;
	params.m_time_length_name = valueHandle.asString().asChar();
	valueHandle = data.inputValue(aTimeAttrName, &stat);
	if (!stat) return stat;
	params.m_time_attr_name = valueHandle.asString().asChar();
	valueHandle = data.inputValue(aParentIdAttrName, &stat);
	if (!stat) return stat;
	params.m_parent_id_attr_name = valueHandle.asString().asChar();
    
    return MS::kSuccess;
    
    
}


MStatus DPTimeShift::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;
	
    if( plug == outDP ) {
		

		
		//データプラグの取得
        MPlug dpPlug(thisMObject(),inDP);
        MPlug objPlug(thisMObject(),inObject);
        
        const bool dpConnected = dpPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool objectConnected = objPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!dpConnected && !objectConnected) {
            MGlobal::displayWarning("DPTimeShift:Not connected inDp or inObject.");
            return MS::kFailure;
        }
		
		//アトリビュートの取得
        DPTimeShiftParams params;
        
        stat = evalParams(data, params);
        if (!stat) {
            EchoWarning(name().asChar(), "Failed read parameter.");
            return stat;
        }
        
        
		MPlug inTimeOffsetDataPlug(thisMObject(), inTimeOffsetData);
		MPlug inTimeLengthDataPlug(thisMObject(), inTimeLengthData);
        bool offsetConnected = false, lengthConnected = false;
        if (params.m_length_type == DP_TIME_LEN_DATA) {
            offsetConnected = inTimeOffsetDataPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            lengthConnected = inTimeLengthDataPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
        }

		
#pragma mark -------------------------------
#pragma mark inObject Base
        if (objectConnected) {
            
			std::vector<MPointArray> objPointArray;
            
            DScalarDataList timeAttrValues;
            DAUIntList pIdAttrValues;
            DScalarDataList *timeAttrArray = params.m_is_time_attr ? &timeAttrValues : nullptr;
            DAUIntList *pIdAttrArray = params.m_is_parent_id_attr ? &pIdAttrValues : nullptr;
            
			//lengthType==1 is ignored.
			if (params.m_length_type==DP_TIME_LEN_DATA) {
				//Input Dataから作成
				
				if (offsetConnected || lengthConnected) {
					DScalarDataList timeOffsetArray,timeLengthArray;
                    if (offsetConnected) {
						stat = getTimeValueArrayFromData(
							timeOffsetArray, data, inTimeOffsetData);
						if (!stat) return stat;
                    }
                    
                    if (lengthConnected) {
						stat = getTimeValueArrayFromData(
							timeLengthArray, data, inTimeLengthData);
						if (!stat) return stat;
                    }
					
					
                    
                    
                    
                    
                    MDataHandle objHnd = data.inputValue( inObject, &stat );
                    DOMMY_RETURN_IF_FAILURE(stat);
                    
					stat = getInObjectPointFomTimePP(objHnd,
                                              objPlug,
                                              timeOffsetArray,
                                              timeLengthArray,
                                              params,
                                              timeAttrArray,
                                              pIdAttrArray,
                                              objPointArray);
					if (!stat) return stat;
                    
					
				} else {
					MGlobal::displayWarning("DPTimeShift:Not connected inData plug.");
					return MS::kFailure;
				}
				
			} else if (params.m_length_type==DP_TIME_LEN_RAMP) {
				//Ramp Map Biasから作成
				
				DScalarDataList timeValueArray;
				stat = getTimeRampValueArray(timeValueArray,
                                      params.m_time_ramp,
                                      params.m_time_sample,
                                      params.m_time_length,
                                      params.m_time_offset,
                                      params.m_seed,
                                      params.m_time_rand);
				if (!stat) return stat;
				
				MDataHandle objHnd = data.inputValue( inObject, &stat );
				DOMMY_RETURN_IF_FAILURE(stat);
				stat = getInObjectPointFomTime(
					params.m_time, objHnd, objPlug, timeValueArray,
					timeAttrArray, pIdAttrArray, objPointArray);
				if (!stat) return stat;
				
				//time attributeをノーマライズ
				if (params.m_is_time_attr && timeAttrArray->size()) {
					normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            false,
                                            timeAttrArray);
				}
				
			} else if (params.m_length_type==DP_TIME_LEN_FRAME) {
				//Frame Base
				
				DScalarDataList timeValueArray;
				getFrameValueArray(timeValueArray,
                                   params.m_time_sample,
                                   params.m_time_offset,
                                   params.m_seed,
                                   params.m_time_rand);
				
				MDataHandle objHnd = data.inputValue( inObject, &stat );
				DOMMY_RETURN_IF_FAILURE(stat);
				stat = getInObjectPointFomTime(
					params.m_time, objHnd, objPlug, timeValueArray,
					timeAttrArray, pIdAttrArray, objPointArray);
				if (!stat) return stat;
				
				//time attributeをノーマライズ
				if (params.m_is_time_attr && timeAttrArray->size()) {
					normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            true,
                                            timeAttrArray);
					
				}
			} else {
				//Time Length
				
				//基本のTimeデータから作成
                DScalarDataList timeValueArray;
				getTimeValueArray(timeValueArray,
                                  params.m_time_sample,
                                  params.m_time_length,
                                  params.m_time_offset,
                                  params.m_seed,
                                  params.m_time_rand);
				

                //インプットオブジェクトの読み込み
                MDataHandle objHnd = data.inputValue( inObject, &stat );
                DOMMY_RETURN_IF_FAILURE(stat);
				stat = getInObjectPointFomTime(
					params.m_time, objHnd, objPlug, timeValueArray,
					timeAttrArray, pIdAttrArray, objPointArray);
				if (!stat) return stat;
				
				
				//time attributeをノーマライズ
				if (params.m_is_time_attr && timeAttrArray->size()) {
					
					normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            false,
                                            timeAttrArray);

				}
				
			}

			
			//ポイントデータの構築
			return finishTimeShiftOutput(data, plug, outDP, params,
                objPointArray, timeAttrArray, pIdAttrArray);
			
			
			
        } else  {
			
#pragma mark -------------------------------
#pragma mark inDP Base
			
			
            const DPData* inputDPData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
			
            
			std::vector<MPointArray> objPointArray;
            DScalarDataList timeAttrValues;
            DAUIntList pIdAttrValues;
            DScalarDataList *timeAttrArray = params.m_is_time_attr ? &timeAttrValues : NULL;
            DAUIntList *pIdAttrArray = params.m_is_parent_id_attr ? &pIdAttrValues : NULL;
			
			
			if (params.m_length_type==DP_TIME_LEN_ATTRIBUTE && inputDPData) {
				//From Attribute
               
                
                size_t count = inputDPData->numberOfPoints();
                if (!dommy::sampling::validTimePointProduct(count, params.m_time_sample))
                    return MS::kInvalidParameter;
				DScalarDataList timeOffsetArray(count,params.m_time_offset);
				DScalarDataList timeLengthArray(count,params.m_time_length);
				
                if (count != 0U && inputDPData->isExistAttr(params.m_time_offset_name)) {

                    if (!dommy::attribute::convertScalarAttribute(
                            inputDPData->getAttr(params.m_time_offset_name), count, timeOffsetArray))
                        return MS::kInvalidParameter;
                }
                if (count != 0U && inputDPData->isExistAttr(params.m_time_length_name)) {

                    if (!dommy::attribute::convertScalarAttribute(
                            inputDPData->getAttr(params.m_time_length_name), count, timeLengthArray))
                        return MS::kInvalidParameter;
                }
                
				
                if (count == timeOffsetArray.size() && count == timeLengthArray.size()) {
                    tbb::blocked_range<uint> range = tbb::blocked_range<uint>(0, count, 10000);
                    TimeShiftPointFromDP body = TimeShiftPointFromDP(dpPlug,
                                                                     timeOffsetArray,
                                                                     timeLengthArray,
                                                                     params,
                                                                     &objPointArray,
                                                                     timeAttrArray,
																	 pIdAttrArray);
                    // The worker reads Maya plugs and appends to shared containers.
                    // Run serially until the outputs are indexed per source point.
                    body(range);
					if (!body.status) return body.status;
                }
				
				
			} else if (params.m_length_type==DP_TIME_LEN_DATA) {
				//Input Data
				
				if (offsetConnected || lengthConnected) {
					
                    DScalarDataList timeOffsetArray,timeLengthArray;
                    if (offsetConnected) {
						stat = getTimeValueArrayFromData(
							timeOffsetArray, data, inTimeOffsetData);
						if (!stat) return stat;
                    }
                    
                    if (lengthConnected) {
						stat = getTimeValueArrayFromData(
							timeLengthArray, data, inTimeLengthData);
						if (!stat) return stat;
                    }
                    
                    const std::size_t count = inputDPData->numberOfPoints();
                    if (!dommy::sampling::validTimePointProduct(count, params.m_time_sample))
                        return MS::kInvalidParameter;
					
                    if (timeOffsetArray.size()!=count) {
                        timeOffsetArray = DScalarDataList(count,params.m_time_offset);
                    }
                    
                    if (timeLengthArray.size()!=count) {
                        timeLengthArray = DScalarDataList(count,params.m_time_length);
                    }
                    
                    if (count == timeOffsetArray.size() && count == timeLengthArray.size()) {
                        tbb::blocked_range<uint> range = tbb::blocked_range<uint>(0, count, 10000);
                        TimeShiftPointFromDP body = TimeShiftPointFromDP(dpPlug,
                                                                         timeOffsetArray,
                                                                         timeLengthArray,
                                                                         params,
                                                                         &objPointArray,
                                                                         timeAttrArray,
																		 pIdAttrArray);
                        body(range);
						if (!body.status) return body.status;
                    }
                    
				} else {
					MGlobal::displayWarning("DPTimeShift:Not connected inData plug.");
					return MS::kFailure;
				}
				
            } else if (params.m_length_type==DP_TIME_LEN_RAMP) {
                //Ramp Map Biasから作成
                DScalarDataList timeValueArray;
				stat = getTimeRampValueArray(timeValueArray,
                                      params.m_time_ramp,
                                      params.m_time_sample,
                                      params.m_time_length,
                                      params.m_time_offset,
                                      params.m_seed,
                                      params.m_time_rand);
				if (!stat) return stat;
                
				stat = DGetPlugTimeDP(
					params.m_time, objPointArray, dpPlug, timeValueArray,
					timeAttrArray, pIdAttrArray);
				if (!stat) return stat;
                
                //time attributeをノーマライズ
                if (params.m_is_time_attr && timeAttrArray->size()) {
                    
                    normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            false,
                                            timeAttrArray);
                    
                }
                
			} else if (params.m_length_type==DP_TIME_LEN_FRAME) {
				//Frame Base
				
				DScalarDataList timeValueArray;
				getFrameValueArray(timeValueArray,
                                   params.m_time_sample,
                                   params.m_time_offset,
                                   params.m_seed,
                                   params.m_time_rand);
				
				stat = DGetPlugTimeDP(
					params.m_time, objPointArray, dpPlug, timeValueArray,
					timeAttrArray, pIdAttrArray);
				if (!stat) return stat;
				
                //time attributeをノーマライズ
                if (params.m_is_time_attr && timeAttrArray->size()) {
                    normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            true,
                                            timeAttrArray);
                }
                
			} else {
				//Time Length
                DScalarDataList timeValueArray;
				getTimeValueArray(timeValueArray,
                                  params.m_time_sample,
                                  params.m_time_length,
                                  params.m_time_offset,
                                  params.m_seed,
                                  params.m_time_rand);
				
                
                
                
				stat = DGetPlugTimeDP(
					params.m_time, objPointArray, dpPlug, timeValueArray,
					timeAttrArray, pIdAttrArray);
				if (!stat) return stat;
				
                
                //time attributeをノーマライズ
                if (params.m_is_time_attr && timeAttrArray->size()) {
                    
                    normalizedTimeAttrValue(params.m_time_attr_type,
                                            params.m_time_length,
                                            params.m_time_offset,
                                            params.m_time_sample,
                                            false,
                                            timeAttrArray);
                    
                }
			}
			
			
			
			
			
			return finishTimeShiftOutput(data, plug, outDP, params,
                objPointArray, timeAttrArray, pIdAttrArray);
        }
		
    }
    else
    {
        return MS::kUnknownParameter;
    }
	
	
	
}
DOMMY_COMPUTE_CATCH("DPTimeShift")



MStatus DPTimeShift::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    inObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));
    
    
    
    MFnEnumAttribute ptOrderEnumFn;
    aPointOrderType = ptOrderEnumFn.create( "PointOrderType", "ptordty", DP_TIME_BIRTH_SAME_INDEX, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(ptOrderEnumFn.addField( "Same Point Index", DP_TIME_BIRTH_SAME_INDEX ));
    DOMMY_RETURN_IF_FAILURE(ptOrderEnumFn.addField( "Same Time", DP_TIME_BIRTH_SAME_TIME ));
    DOMMY_RETURN_IF_FAILURE(ptOrderEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(ptOrderEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointOrderType));
    
    
    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create( "CurrentTime", "ctm" , MFnUnitAttribute::kTime,  0.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));
    
    
    aTimeOffset = nAttr.create( "TimeOffset", "to", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeOffset));
    
    aTimeLength = nAttr.create( "TimeLength", "tl", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeLength));
    
    aTimeSampling = nAttr.create( "TimeSampling", "ts", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeSampling));
    
    
    
    MFnEnumAttribute enumFn;
    aTimeLengthType = enumFn.create( "TimeLengthType", "tlt", DP_TIME_LEN, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Time Length", DP_TIME_LEN ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Attribute", DP_TIME_LEN_ATTRIBUTE ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Data", DP_TIME_LEN_DATA ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Ramp Map Bias", DP_TIME_LEN_RAMP ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Frame Sample", DP_TIME_LEN_FRAME ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeLengthType));
    
    
    aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    
    aTimeLengthRandom = nAttr.create( "TimeLengthRandom", "tlr", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeLengthRandom));
    
    
    MFnStringData fnStringData;
    
    MObject attrOffName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTimeOffsetAttrName = tAttr.create("TimeOffsetAttrName", "toattr", MFnData::kString, attrOffName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeOffsetAttrName));
    
    
    
    MObject attrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTimeLengthAttrName = tAttr.create("TimeLengthAttrName", "tlattr", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeLengthAttrName));
    
    
    
    inTimeOffsetData = gAttr.create( "inTimeOffsetData", "itod",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTimeOffsetData));
	
    
    inTimeLengthData = gAttr.create( "inTimeLengthData", "itld",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTimeLengthData));
	
	
	aTimeRamp = MRampAttribute::createCurveRamp("TimeRamp", "tramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeRamp));
	
	
	aIsAddTimeAttr = nAttr.create( "IsAddTimeAttr", "isat", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAddTimeAttr));
	
	
	MFnEnumAttribute enumTValFn;
	aTimeAttrValueType = enumTValFn.create( "TimeAttrValueType", "tavt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumTValFn.addField( "Normalize", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumTValFn.addField( "Ignore Offset", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumTValFn.addField( "Raw", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumTValFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(enumTValFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeAttrValueType));
    
    MObject taName = fnStringData.create("time", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTimeAttrName = tAttr.create("TimeAttrName", "tattr", MFnData::kString, taName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeAttrName));
    
    

    aIsAddParentIdAttr = nAttr.create( "IsAddParentIdAttr", "isap", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAddParentIdAttr));
    
    
    MObject pIdName = fnStringData.create("pid", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aParentIdAttrName = tAttr.create("ParentIdAttrName", "pattr", MFnData::kString, pIdName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aParentIdAttrName));
	
	
	
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPointOrderType, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurrentTime, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeOffset, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeLength, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeSampling, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeLengthType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeLengthRandom, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeOffsetAttrName, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeLengthAttrName, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTimeOffsetData, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTimeLengthData, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeRamp, outDP ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsAddTimeAttr, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeAttrValueType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTimeAttrName, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsAddParentIdAttr, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aParentIdAttrName, outDP ));
	
	
    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DPTimeShift::initialize")

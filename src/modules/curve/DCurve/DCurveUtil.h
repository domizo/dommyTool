//
//  DMultiCurveUtil.hpp
//  dommyTool
//
//  Created by yuzo on 5/1/16.
//
//

#pragma once

#include <cstddef>

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include "DCurveData.h"
#include "DPAttributeType.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaAttributeArray.h"
#include <stdexcept>
#include "dcurve_ids.h"




enum {
    DC_kClose = 0,
    DC_kOpen,
    DC_STATE_TYPE_COUNT
};typedef short DC_STATE_TYPE;

enum {
    DC_kLinear = 0,
    DC_kCubic,
    DC_DEGREE_TYPE_COUNT
};typedef short DC_DEGREE_TYPE;

enum {
    DC_LEN_PARAM = 0,
    DC_LEN_DIST,
    DC_LENGTH_TYPE_COUNT
};typedef short DC_LENGTH_TYPE;


enum {
    DC_VEC_kTangent = 0,
    DC_VEC_kNormal,
    DC_VEC_kManual,
    DC_VEC_kAttribute,
    DC_VECTOR_TYPE_COUNT
};typedef short DC_VECTOR_TYPE;


/// @brief  returns a pointer to the input multi curve data object or NULL if this fails.
const DCurveData* getInputDC(const MObject& crv, MDataBlock& data);
MStatus getValidInputDC(const MObject& curve,
                        MDataBlock& data,
                        const DCurveData*& inputData);
const DCurveData* getInputDCfromPlug(const MPlug &plg, MObject &obj);
const DCurveData* getInputDCfromHandle(MDataHandle hnd);
//const DCurveData* getInputDC(MObject& data);

//DCurveData* createOutputDCData();

// Create owned geometry data only; leave newCurve unchanged on failure.
MStatus generateMCurveFromDcurve(const DCItem &crv, MObject &newCurve);
MStatus generateMCurveFromDcurve(ConstDCItem &crv, MObject &newCurve);
// Owned data with the primitive matrix baked once; use kObject for all queries.
// Lengths and directions on this curve are in the resulting world frame.
MStatus generateWorldMCurveFromDcurve(ConstDCItem curve, MObject& newCurve);

//knot generate
void GenerateDCurveKnots(const size_t &cvCount, const int &degree, MDoubleArray &knots, bool isLoop=false);

// Collect bounded, validated curves without repeatedly copying/scanning the
// growing output. Publication uses DCurveData's existing atomic batch insert.
class DCurveOutputBatch final {
public:
    bool append(const DCItem& curve) try {
        dommy::curve::CurveBatchBudget candidate = budget_;
        if (!curve || !curve->appendOperationCost(candidate)) return false;
        curves_.push_back(curve);
        budget_ = candidate;
        return true;
    } catch (...) {
        return false;
    }

    bool publishTo(DCurveData& output) const {
        return curves_.empty() || output.insert(curves_);
    }

private:
    DCurveData::DCurveList curves_;
    dommy::curve::CurveBatchBudget budget_;
};

//attribute method
void checkDCurvePointAttribute(const std::string attrName,  DCurveData &outputDCData, bool delAttr);
void checkDCurveAttribute(const std::string attrName,  DCurveData &outputDCData, bool delAttr);


//attribute add
MStatus DCAddAttrScalar(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result);
MStatus DCAddAttrVec2(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result);
MStatus DCAddAttrVec3(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result);

MStatus DCPAddAttrScalar(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result);
MStatus DCPAddAttrVec2(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result);
MStatus DCPAddAttrVec3(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result);


#pragma mark DCurve prim attribute method

template <typename T>
inline void getDCStringList(const DCurveData &inputData, const std::string &name, std::vector<std::string> &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
	for (std::size_t i = 0; i < srcList.size(); ++i) {
		newValues[i] = srcList[i];
	}
}

template <typename T>
inline void getDCMatrixList(const DCurveData &inputData, const std::string &name, std::vector<MMatrix> &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
	for (std::size_t i = 0; i < srcList.size(); ++i) {
		newValues[i] = srcList[i];
	}
}
template <typename T>
inline void getDCVectorList(const DCurveData &inputData, const std::string &name, DVec3DataList &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
	for (std::size_t i = 0; i < srcList.size(); ++i) {
		T val= srcList[i];
		newValues[i] = DVec3Data(val[0],val[1],val[2]);
	}
}

template <typename T>
inline void getDC2DVectorList(const DCurveData &inputData, const std::string &name, DVec2DataList &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
	for (std::size_t i = 0; i < srcList.size(); ++i) {
		T val= srcList[i];
		newValues[i] = DVec2Data(val[0],val[1]);
	}
}

#pragma mark DCurve prim attribute vec1
template <typename T>
inline void getDCSingleValueToFloat(const DCurveData &inputData, const std::string &name, MDoubleArray &finValues)
{
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, double>(
        inputData.getAttr(name), inputData.numberOfCurves(), finValues));
}

template <typename T>
inline void getDCSingleValueToFloat(const DCurveData &inputData, const std::string &name, DScalarDataList &finValues)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfCurves(), finValues);
}

#pragma mark DCurve prim attribute vec2
template <typename T>
inline void getDCTwoArray(const DCurveData &inputData, const std::string &name, MDoubleArray &firstArray, MDoubleArray &secondArray)
{
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaUvArrays<T>(
        inputData.getAttr(name), inputData.numberOfCurves(), firstArray, secondArray));
}

template <typename T>
inline void getDCVec2ToVectorArray(const DCurveData &inputData, const std::string &name, DVec2DataList &vecArray)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfCurves(), vecArray);
}

template <typename T>
inline void getDCVec2ToVectorArray(const DCurveData &inputData, const std::string &name, MVectorArray &vecArray)
{
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
        inputData.getAttr(name), inputData.numberOfCurves(),
        [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); }, vecArray));
}

#pragma mark DCurve prim attribute vec3
template <typename T>
inline void getDCVec3ToVectorArray(const DCurveData& inputData, const std::string& name, DVec3DataList& vecArray)
{
    if (!dommy::attribute::convertTypedArray<T>(inputData.getAttr(name),
            inputData.numberOfCurves(), vecArray))
        throw std::invalid_argument("Invalid primitive Vec3 attribute");
}

template <typename T>
inline void getDCVec3ToVectorArray(const DCurveData& inputData, const std::string& name, MVectorArray& vecArray)
{
    std::vector<Imath::V3d> values;
    if (!dommy::attribute::convertTypedArray<T>(inputData.getAttr(name),
            inputData.numberOfCurves(), values))
        throw std::invalid_argument("Invalid primitive Vec3 attribute");
    const MStatus status = dommy::maya::buildCheckedArray(values.size(),
        [&](unsigned int i, MVector& value) -> MStatus {
            value = MVector(values[i][0], values[i][1], values[i][2]);
            return MS::kSuccess;
        }, vecArray);
    if (!status) throw std::runtime_error("Unable to construct primitive vector array");
}


#pragma mark DCurve attribute pointer
template <typename T>
inline const std::vector<T>* getDCAttributeListPtr(
    const DCurveData &inputData, const std::string &name)
{
    typename TypedDPAttributeData<T>::ConstPtr m =
        inputData.getValidTypedAttributedata<T>(name);
    return &(m->value());
}




#pragma mark ----------------------------------------
#pragma mark DCurve Point attribute method

template <typename T>
inline void getDCPStringList(const DCItem &crv, const std::string &name, std::vector<std::string> &newValues)
{
    std::vector<T> srcList = (*crv).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i = 0; i < srcList.size(); ++i) {
        newValues[i] = srcList[i];
    }
}
template <typename T>
inline void getDCPVectorList(const DCItem &crv, const std::string &name, DVec3DataList &newValues)
{
    std::vector<T> srcList = (*crv).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i = 0; i < srcList.size(); ++i) {
        T val= srcList[i];
        newValues[i] = DVec3Data(val[0],val[1],val[2]);
    }
}

template <typename T>
inline void getDCP2DVectorList(const DCItem &crv, const std::string &name, DVec2DataList &newValues)
{
    std::vector<T> srcList = (*crv).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i = 0; i < srcList.size(); ++i) {
        T val= srcList[i];
        newValues[i] = DVec2Data(val[0],val[1]);
    }
}

#pragma mark DCurve Point attribute vec1
template <typename T>
inline void getDCPSingleValueToInt(const DCItem &crv, const std::string &name, MIntArray &finValues)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, int>(
        crv->getAttr(name), crv->numberOfPoints(), finValues));
}

template <typename T>
inline void getDCPSingleValueToFloat(const DCItem &crv, const std::string &name, MDoubleArray &finValues)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, double>(
        crv->getAttr(name), crv->numberOfPoints(), finValues));
}

template <typename T>
inline void getDCPSingleValueToFloat(const DCItem &crv, const std::string &name, DScalarDataList &finValues)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireTypedArray<T>(crv->getAttr(name), crv->numberOfPoints(), finValues);
}

#pragma mark DCurve Point attribute vec2
template <typename T>
inline void getDCPTwoArray(const DCItem &crv, const std::string &name, MDoubleArray &firstArray, MDoubleArray &secondArray)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaUvArrays<T>(
        crv->getAttr(name), crv->numberOfPoints(), firstArray, secondArray));
}

template <typename T>
inline void getDCPVec2ToVectorArray(const DCItem &crv, const std::string &name, DVec2DataList &vecArray)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireTypedArray<T>(crv->getAttr(name), crv->numberOfPoints(), vecArray);
}

template <typename T>
inline void getDCPVec2ToVectorArray(const DCItem &crv, const std::string &name, MVectorArray &vecArray)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
        crv->getAttr(name), crv->numberOfPoints(),
        [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); }, vecArray));
}



#pragma mark DCurve Point attribute vec3
template <typename T>
inline void getDCPVec3ToVectorArray(const DCItem &crv, const std::string &name, DVec3DataList &vecArray)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireTypedArray<T>(crv->getAttr(name), crv->numberOfPoints(), vecArray);
}

template <typename T>
inline void getDCPVec3ToVectorArray(const DCItem &crv, const std::string &name, MVectorArray &vecArray)
{
    if (!crv) throw std::invalid_argument("Missing curve");
    dommy::attribute::requireArrayStatus(dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
        crv->getAttr(name), crv->numberOfPoints(),
        [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); }, vecArray));
}

#pragma mark attribute useful mesthod



#pragma mark Dcurve prim attribute

inline void getDCAttributeMDoubleArrayFromScalar(const DCurveData &dcdata, std::string name, MDoubleArray &newArray) {
    
    if (dcdata.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = dcdata.getAttr(name)->getAttrType();
        
        if (isSingleRealAttrType(type)) {
            
            if (type == DP_ATTR_FLOAT) {
                getDCSingleValueToFloat<float>(dcdata, name, newArray);
            } else if (type == DP_ATTR_DOUBLE) {
                getDCSingleValueToFloat<double>(dcdata, name, newArray);
            } else if (type == DP_ATTR_INT) {
                getDCSingleValueToFloat<int32_t>(dcdata, name, newArray);
            } else if (type == DP_ATTR_UINT32) {
                getDCSingleValueToFloat<uint32_t>(dcdata, name, newArray);
            } else if (type == DP_ATTR_INT64) {
                getDCSingleValueToFloat<int64_t>(dcdata, name, newArray);
            }
        }
    }
}

inline void getDCAttributeMDoubleArrayFromScalar(const DCurveData &dcdata, std::string name, DScalarDataList &newArray) {
    
    if (dcdata.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = dcdata.getAttr(name)->getAttrType();
        
        if (isSingleRealAttrType(type)) {
            
            if (type == DP_ATTR_FLOAT) {
                getDCSingleValueToFloat<float>(dcdata, name, newArray);
            } else if (type == DP_ATTR_DOUBLE) {
                getDCSingleValueToFloat<double>(dcdata, name, newArray);
            } else if (type == DP_ATTR_INT) {
                getDCSingleValueToFloat<int32_t>(dcdata, name, newArray);
            } else if (type == DP_ATTR_UINT32) {
                getDCSingleValueToFloat<uint32_t>(dcdata, name, newArray);
            } else if (type == DP_ATTR_INT64) {
                getDCSingleValueToFloat<int64_t>(dcdata, name, newArray);
            }
        }
    }
}



inline void getDCAttributeMDoubleArrayFromVec2(const DCurveData &dcdata, std::string &name,
												MDoubleArray &valuesA, MDoubleArray &valuesB) {
	
	if (dcdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dcdata.getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
                getDCTwoArray<Imath::V2i>(dcdata, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2F) {
                getDCTwoArray<Imath::V2f>(dcdata, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2D) {
                getDCTwoArray<Imath::V2d>(dcdata, name, valuesA,valuesB);
			}
		}
	}
}

inline void getDCAttributeMVectorArrayFromVec2(const DCurveData &dcdata, std::string name, MVectorArray &newArray) {
    
    if (dcdata.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = dcdata.getAttr(name)->getAttrType();
        
        if (isVector2DAttrType(type)) {
            
            if (type == DP_ATTR_VEC2I) {
                getDCVec2ToVectorArray<Imath::V2i>(dcdata, name, newArray);
            } else if (type == DP_ATTR_VEC2F) {
                getDCVec2ToVectorArray<Imath::V2f>(dcdata, name, newArray);
            } else if (type == DP_ATTR_VEC2D) {
                getDCVec2ToVectorArray<Imath::V2d>(dcdata, name, newArray);
            }
        }
    }
}

inline void getDCAttributeMVectorArrayFromVec2(const DCurveData &dcdata, std::string name, DVec2DataList &newArray) {
	
	if (dcdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dcdata.getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
                getDCVec2ToVectorArray<Imath::V2i>(dcdata, name, newArray);
			} else if (type == DP_ATTR_VEC2F) {
                getDCVec2ToVectorArray<Imath::V2f>(dcdata, name, newArray);
			} else if (type == DP_ATTR_VEC2D) {
                getDCVec2ToVectorArray<Imath::V2d>(dcdata, name, newArray);
			}
		}
	}
}

inline void getDCAttributeMVectorArrayFromVec3(const DCurveData& dcdata, std::string name, MVectorArray& newArray)
{
    const auto attribute = dcdata.getAttr(name);
    if (!attribute) throw std::invalid_argument("Missing primitive Vec3 attribute");
    switch (attribute->getAttrType()) {
    case DP_ATTR_VEC3I: return getDCVec3ToVectorArray<Imath::V3i>(dcdata, name, newArray);
    case DP_ATTR_VEC3F: return getDCVec3ToVectorArray<Imath::V3f>(dcdata, name, newArray);
    case DP_ATTR_VEC3D: return getDCVec3ToVectorArray<Imath::V3d>(dcdata, name, newArray);
    default: throw std::invalid_argument("Primitive attribute must be Vec3");
    }
}

inline void getDCAttributeMVectorArrayFromVec3(const DCurveData& dcdata, std::string name, DVec3DataList& newArray)
{
    const auto attribute = dcdata.getAttr(name);
    if (!attribute) throw std::invalid_argument("Missing primitive Vec3 attribute");
    switch (attribute->getAttrType()) {
    case DP_ATTR_VEC3I: return getDCVec3ToVectorArray<Imath::V3i>(dcdata, name, newArray);
    case DP_ATTR_VEC3F: return getDCVec3ToVectorArray<Imath::V3f>(dcdata, name, newArray);
    case DP_ATTR_VEC3D: return getDCVec3ToVectorArray<Imath::V3d>(dcdata, name, newArray);
    default: throw std::invalid_argument("Primitive attribute must be Vec3");
    }
}




#pragma mark Dcurve point attribute


inline void getDCPAttributeMDoubleArray(const DCItem &crv, const std::string &name, MDoubleArray &values) {
    std::vector<double> weights;
    if (!crv || !dommy::attribute::convertWeightAttribute(crv->getAttr(name),
            crv->numberOfPoints(), weights))
        throw std::invalid_argument("Invalid curve weight attribute");
    const MStatus status = dommy::maya::buildCheckedArray(weights.size(),
        [&](unsigned int i, double& value) -> MStatus {
            value = weights[i];
            return MS::kSuccess;
        }, values);
    if (status != MS::kSuccess) throw std::runtime_error("Unable to construct curve weight array");
}



inline void getDCPAttributeMDoubleArrayFromScalar(const DCItem &crv, std::string name, DScalarDataList &newArray) {
//
    if (crv->isExistAttr(name)) {
    
        DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
        
        if (isSingleRealAttrType(type)) {
//            MDoubleArray doubleArray;
            
            if (type == DP_ATTR_FLOAT) {
                getDCPSingleValueToFloat<float>(crv, name, newArray);
            } else if (type == DP_ATTR_DOUBLE) {
                getDCPSingleValueToFloat<double>(crv, name, newArray);
            } else if (type == DP_ATTR_INT) {
                getDCPSingleValueToFloat<int32_t>(crv, name, newArray);
            } else if (type == DP_ATTR_UINT32) {
                getDCPSingleValueToFloat<uint32_t>(crv, name, newArray);
            } else if (type == DP_ATTR_INT64) {
                getDCPSingleValueToFloat<int64_t>(crv, name, newArray);
            }
            
            
//            if (doubleArray.length() <= newArray.length()) {
//                for (uint i=0; i<newArray.length(); i++) {
//                    newArray[i]=doubleArray[i];
//                }
//            }
            
        }
    }
}

inline void getDCPAttributeMIntArrayFromScalar(const DCItem &crv, std::string name, MIntArray &newArray) {
	
	if (crv->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_INT) {
				getDCPSingleValueToInt<int32_t>(crv, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
				getDCPSingleValueToInt<uint32_t>(crv, name, newArray);
			} else if (type == DP_ATTR_INT64) {
				getDCPSingleValueToInt<int64_t>(crv, name, newArray);
			}
			
		}
	}
}


inline void getDCPAttributeFromScalar(const DCItem &crv, std::string name, DScalarDataList &newArray) {
	
	if (crv->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_FLOAT) {
				getDCPSingleValueToFloat<float>(crv, name, newArray);
			} else if (type == DP_ATTR_DOUBLE) {
				getDCPSingleValueToFloat<double>(crv, name, newArray);
			} else if (type == DP_ATTR_INT) {
				getDCPSingleValueToFloat<int32_t>(crv, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
				getDCPSingleValueToFloat<uint32_t>(crv, name, newArray);
			} else if (type == DP_ATTR_INT64) {
				getDCPSingleValueToFloat<int64_t>(crv, name, newArray);
			}
			
		}
	}
}


inline void getDCPAttributeMDoubleArrayFromVec2(const DCItem &crv, std::string &name,
												MDoubleArray &valuesA, MDoubleArray &valuesB) {
	
	if (crv->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
				getDCPTwoArray<Imath::V2i>(crv, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2F) {
				getDCPTwoArray<Imath::V2f>(crv, name, valuesA,valuesB);
				} else if (type == DP_ATTR_VEC2D) {
					getDCPTwoArray<Imath::V2d>(crv, name, valuesA,valuesB);
				}
			
		}
	}
}

inline void getDCPAttributeMVectorArrayFromVec2(const DCItem &crv, std::string name, DVec2DataList &newArray) {
	
	if (crv->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
				getDCPVec2ToVectorArray<Imath::V2i>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC2F) {
				getDCPVec2ToVectorArray<Imath::V2f>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC2D) {
				getDCPVec2ToVectorArray<Imath::V2d>(crv, name, newArray);
			}
		}
	}
}

inline void getDCPAttributeMVectorArrayFromVec2(const DCItem &crv, std::string name, MVectorArray &newArray) {
    
    if (crv->isExistAttr(name)) {
        DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
        
        if (isVector2DAttrType(type)) {
            
            if (type == DP_ATTR_VEC2F) {
                getDCPVec2ToVectorArray<Imath::V2f>(crv, name, newArray);
            } else if (type == DP_ATTR_VEC2D) {
                getDCPVec2ToVectorArray<Imath::V2d>(crv, name, newArray);
            } else if (type == DP_ATTR_VEC2I) {
                getDCPVec2ToVectorArray<Imath::V2i>(crv, name, newArray);
            }
            
        }
    }
}





inline void getDCPAttributeMVectorArrayFromVec3(const DCItem &crv, std::string name, MVectorArray &newArray) {
	
	if (crv->isExistAttr(name)) {
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			if (type == DP_ATTR_VEC3F) {
				getDCPVec3ToVectorArray<Imath::V3f>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDCPVec3ToVectorArray<Imath::V3d>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDCPVec3ToVectorArray<Imath::V3i>(crv, name, newArray);
			}
		}
	}
}

inline void getDCPAttributeMVectorArrayFromVec3(const DCItem &crv, std::string name, DVec3DataList &newArray) {
	
	if (crv->isExistAttr(name)) {
		DP_ATTR_DATA_TYPE type = crv->getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			
			if (type == DP_ATTR_VEC3F) {
				getDCPVec3ToVectorArray<Imath::V3f>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDCPVec3ToVectorArray<Imath::V3d>(crv, name, newArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDCPVec3ToVectorArray<Imath::V3i>(crv, name, newArray);
			}
		}
	}
}



#pragma mark Get DCurve attribute function

inline void DCGetMDoubleArrayFromCurveAttribute(const DCurveData &inputData, std::string name, MDoubleArray &newArray) {
	
    if (inputData.isExistAttr(name)) {
		
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		
		if (isSingleRealAttrType(type)) {
			
			MDoubleArray doubleArray;
			
			if (type == DP_ATTR_FLOAT) {
				getDCSingleValueToFloat<float>(inputData, name, doubleArray);
			} else if (type == DP_ATTR_DOUBLE) {
				getDCSingleValueToFloat<double>(inputData, name, doubleArray);
			} else if (type == DP_ATTR_INT) {
				getDCSingleValueToFloat<int32_t>(inputData, name, doubleArray);
			} else if (type == DP_ATTR_UINT32) {
				getDCSingleValueToFloat<uint32_t>(inputData, name, doubleArray);
			} else if (type == DP_ATTR_INT64) {
				getDCSingleValueToFloat<int64_t>(inputData, name, doubleArray);
			}
			
			uint minCount =  fmin(doubleArray.length(),newArray.length());
			
			for (uint i=0; i<minCount; i++) {
				newArray[i]=doubleArray[i];
			}
			
			
		}
		
	}
}




#pragma mark dcurve group method

void getDCurveMatchedGroups(const DCurveData &dcurve, const std::string& patternStr, std::vector<std::string> &matchedGrpNames);

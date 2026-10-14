//
//  DPAttributeCommon.cpp
//  dommyTool
//
//  Created by yuzo on 7/3/16.
//
//

#include "DPAttributeCommon.h"
#include "common/string/DStreamFormat.h"

#include <stdexcept>
#include <string>

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"

#include "DNodeOperation.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DAttributeLookup.h"

#pragma mark common method

void CopyAttributeToAttribute(DPAttrList &baseAttrlist, const DPAttrList &targetAttrlist) {
    DPAttrList copiedAttributes;
    for (ConstDPAttrIter attribute = targetAttrlist.begin();
         attribute != targetAttrlist.end(); ++attribute) {
        if (!attribute->second) {
            throw std::invalid_argument(
                "Cannot copy a null point attribute");
        }
        DPAttributeData::Ptr copiedAttribute = attribute->second->copy();
        if (!copiedAttribute) {
            throw std::runtime_error(
                "Point attribute copy returned a null value");
        }
        copiedAttributes.emplace(attribute->first, copiedAttribute);
    }

    baseAttrlist.swap(copiedAttributes);
}



bool IsExistInAttributeList(const DPAttrList &attrlist, const std::string& name) {
    return static_cast<bool>(dommy::attribute::findAttribute(attrlist, name));
}

void AddAttributeList(DPAttrList &attrlist, const std::string &name, const DPAttributeData& attr ) {
    
    if (!dommy::attribute::validAttributeName(name)) {
        return;
    }
    
    //already exist check
    
    auto iter = attrlist.find(name);
    
    if (iter != attrlist.end() && iter->second &&
        iter->second->typeName() != attr.typeName()) return;
    DPAttributeData::Ptr copied = attr.copy();
    if (!copied) throw std::runtime_error("Attribute copy returned a null value");
    attrlist[name] = copied;
}

void RemoveFromAttributeList(DPAttrList &attrlist, const std::string &name) {
    auto iter = attrlist.find(name);
    if (iter != attrlist.end()) {
        attrlist.erase(iter);
    }
}


DPAttributeData::Ptr GetAttrPtr(DPAttrList &attrlist, const std::string& name) {
    return dommy::attribute::findAttribute(attrlist, name);
}

DPAttributeData::ConstPtr ExtractAttributeFromAttributeList(const DPAttrList &attrlist, const std::string& name) {
    return dommy::attribute::findAttribute(attrlist, name);
}

DString GetAttributeTypeName(const DPAttrList &attrlist, const std::string& name) {
    const auto attribute = ExtractAttributeFromAttributeList(attrlist, name);
    return attribute ? attribute->typeName() : "";
}

DStringList GetAllNameAttributeList(const DPAttrList &attrlist) {
    DStringList attributeNames;
    
    for(auto itr:attrlist) {
        attributeNames.push_back(itr.first);
    }
    return attributeNames;
}

DStringList GetAllAttributeListInfo(const DPAttrList &attrlist) {
    DStringList attributeInfos;
    for(const auto& itr:attrlist) {
        std::stringstream infoStr;
        infoStr << "@" << itr.first << " : " << itr.second->typeName();
        attributeInfos.push_back(dommy::text::checkedStreamText(infoStr, "Attribute description formatting failed"));
    }
    
    return attributeInfos;
}



void InsertAttributeDefaultValue(DPAttrList &attrlist, const size_t &count, const uint &index) {
    for (auto &aIt:attrlist) {
        aIt.second->insertDefaultValue(count, index);
    }
}

void ExpandAttributeSize(DPAttrList &attrlist, const size_t &count) {
    for (auto &aIt: attrlist) {
        aIt.second->incrementDefaultValue(count);
    }
}


bool DDeleteAttributeFromIds(
    DPAttrList &attrlist, const DPIdList &ids) {
    try {
        DPAttrList candidate;
        CopyAttributeToAttribute(candidate, attrlist);
        for (auto &attribute: candidate) {
            if (!attribute.second ||
                !attribute.second->deleteArrayFromIds(ids)) {
                return false;
            }
        }
        attrlist.swap(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool DCropAttributeFromIds(
    DPAttrList &attrlist, const DPIdList &ids) {
    try {
        DPAttrList candidate;
        CopyAttributeToAttribute(candidate, attrlist);
        for (auto &attribute: candidate) {
            if (!attribute.second ||
                !attribute.second->cropArrayFromIds(ids)) {
                return false;
            }
        }
        attrlist.swap(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool DSortAttributeFromIds(
    DPAttrList &attrlist, const DPIndexList &ids) {
    try {
        DPAttrList candidate;
        CopyAttributeToAttribute(candidate, attrlist);
        for (auto &attribute: candidate) {
            if (!attribute.second ||
                !attribute.second->sortArrayFromIds(ids)) {
                return false;
            }
        }
        attrlist.swap(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool DRemapAtttributeFromIds(
    DPAttrList &attrlist, const DPIndexList &ids) {
    try {
        DPAttrList candidate;
        CopyAttributeToAttribute(candidate, attrlist);
        for (auto &attribute: candidate) {
            if (!attribute.second ||
                !attribute.second->remapFromIds(ids)) {
                return false;
            }
        }
        attrlist.swap(candidate);
        return true;
    } catch (...) {
            return false;
    }
}


template <typename T>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string &name, const std::vector<T> &values) {
    (void)attrlist;
    (void)name;
    (void)values;
    static_assert(
        !std::is_same<T, T>::value,
        "DAddAttributeWithValues does not support this type");
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<int32_t> &values) {
    DPAttributeData::Ptr tmp = DPIntAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<bool> &values) {
    DPAttributeData::Ptr tmp = DPBoolAttr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<uint32_t> &values) {
    DPAttributeData::Ptr tmp = DPUInt32Attr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<int64_t> &values) {
    DPAttributeData::Ptr tmp = DPInt64Attr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<float> &values) {
    DPAttributeData::Ptr tmp = DPFloatAttr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<double> &values) {
    DPAttributeData::Ptr tmp = DPDoubleAttr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V2i> &values) {
    DPAttributeData::Ptr tmp = DPVec2iAttr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V2f> &values) {
    DPAttributeData::Ptr tmp = DPVec2fAttr(values).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V2d> &values) {
    DPAttributeData::Ptr tmp = DPVec2dAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V3i> &values) {
    DPAttributeData::Ptr tmp = DPVec3iAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V3f> &values) {
    DPAttributeData::Ptr tmp = DPVec3fAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<Imath::V3d> &values) {
    DPAttributeData::Ptr tmp = DPVec3dAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<std::string> &values) {
    DPAttributeData::Ptr tmp = DPStringAttr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<MMatrix> &values) {
    DPAttributeData::Ptr tmp = DPMat4Attr(values).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<DIntArray> &values) {
    DPAttributeData::Ptr tmp = DPIntArrayAttr(values).copy();
    attrlist[name] = tmp;
}







template <typename T>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<T> &values, const T &defaultValue) {
    (void)attrlist;
    (void)name;
    (void)values;
    (void)defaultValue;
    static_assert(
        !std::is_same<T, T>::value,
        "DAddAttributeWithValues does not support this type");
}


template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<int32_t> &values, const int32_t &defaultValue) {
    DPAttributeData::Ptr tmp = DPIntAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<bool> &values, const bool &defaultValue) {
    DPAttributeData::Ptr tmp = DPBoolAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<uint32_t> &values, const uint32_t &defaultValue) {
    DPAttributeData::Ptr tmp = DPUInt32Attr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<int64_t> &values, const int64_t &defaultValue) {
    DPAttributeData::Ptr tmp = DPInt64Attr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<float> &values, const float &defaultValue) {
    DPAttributeData::Ptr tmp = DPFloatAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<double> &values, const double &defaultValue) {
    DPAttributeData::Ptr tmp = DPDoubleAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V2i> &values, const Imath::V2i &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec2iAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V2f> &values, const Imath::V2f &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec2fAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}

template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V2d> &values, const Imath::V2d &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec2dAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V3i> &values, const Imath::V3i &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec3iAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V3f> &values, const Imath::V3f &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec3fAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<Imath::V3d> &values, const Imath::V3d &defaultValue) {
    DPAttributeData::Ptr tmp = DPVec3dAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<std::string> &values, const std::string &defaultValue) {
    DPAttributeData::Ptr tmp = DPStringAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<MMatrix> &values, const MMatrix &defaultValue) {
    DPAttributeData::Ptr tmp = DPMat4Attr(values,defaultValue).copy();
    attrlist[name] = tmp;
}
template <>
void DAddAttributeWithValues(DPAttrList &attrlist,const std::string& name, const std::vector<DIntArray> &values, const DIntArray &defaultValue) {
    DPAttributeData::Ptr tmp = DPIntArrayAttr(values,defaultValue).copy();
    attrlist[name] = tmp;
}





#pragma mark getter


void getMVectorArrayFromAttribute(const DPData &inputData, const std::string &name, MVectorArray &newArray) {
    
    
    if (inputData.isExistAttr(name)) {
		
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
        if (isVector3DAttrType(type)) {
			
            switch (type) {
                case DP_ATTR_VEC3F:
                    getDPVec3ToVectorArray<Imath::V3f>(inputData, name, newArray);
                    break;
                case DP_ATTR_VEC3D:
                    getDPVec3ToVectorArray<Imath::V3d>(inputData, name, newArray);
                    break;
                case DP_ATTR_VEC3I:
                    getDPVec3ToVectorArray<Imath::V3i>(inputData, name, newArray);
                    break;
                default:
                    break;
            }
        }
    }
}


void getMVectorArrayFromAttribute(const DPData &inputData, const std::string &name, DVec3DataList &newArray) {
	
	
    if (inputData.isExistAttr(name)) {
		
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			
			if (type == DP_ATTR_VEC3F) {
				getDPVec3ToVectorArray<Imath::V3f>(inputData, name, newArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDPVec3ToVectorArray<Imath::V3d>(inputData, name, newArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDPVec3ToVectorArray<Imath::V3i>(inputData, name, newArray);
			}
		}
	}
}



void getMFloatPointArrayFromAttribute(const DPData &inputData, const std::string &name, MFloatPointArray &newArray, float ww) {
	
	if (inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			DVec3DataList vectorArray;
			
			if (type == DP_ATTR_VEC3F) {
				getDPVec3ToVectorArray<Imath::V3f>(inputData, name, vectorArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDPVec3ToVectorArray<Imath::V3d>(inputData, name, vectorArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDPVec3ToVectorArray<Imath::V3i>(inputData, name, vectorArray);
			}
			
			if (vectorArray.size() <= newArray.length()) {
                size_t pIndex=0;
                for (const auto &vec: vectorArray) {
                    for (int j=0; j<3; ++j) {
                        newArray[pIndex][j]=vec[j];
                    }
                    newArray[pIndex][3]=ww;
                    pIndex++;
//                    newArray[i]=MFloatPoint(vectorArray[i][0],vectorArray[i][1],vectorArray[i][2], ww);
				}
			}
			
		}
	}
}



void getMVector2DArrayFromAttribute(const DPData &inputData, const std::string &name, MVectorArray &newArray) {
    
    if (inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
        if (isVector2DAttrType(type)) {
			
            if (type == DP_ATTR_VEC2F) {
                getDPVec2ToVectorArray<Imath::V2f>(inputData, name, newArray);
            } else if (type == DP_ATTR_VEC2D) {
                getDPVec2ToVectorArray<Imath::V2d>(inputData, name, newArray);
            } else if (type == DP_ATTR_VEC2I) {
                getDPVec2ToVectorArray<Imath::V2i>(inputData, name, newArray);
            }
            
        }
    }
}


void getMVector2DArrayFromAttribute(const DPData &inputData, const std::string &name, DVec2DataList &newArray) {
	
	if (inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2F) {
				getDPVec2ToVectorArray<Imath::V2f>(inputData, name, newArray);
			} else if (type == DP_ATTR_VEC2D) {
				getDPVec2ToVectorArray<Imath::V2d>(inputData, name, newArray);
			} else if (type == DP_ATTR_VEC2I) {
				getDPVec2ToVectorArray<Imath::V2i>(inputData, name, newArray);
			}
			
		}
	}
}

void getMIntArrayFromAttribute(const DPData &inputData, const std::string &name, MIntArray &newArray) {
    //whitespaceの削除
    
    if ((inputData.isExistAttr(name))) {
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
        
        
        if (type == DP_ATTR_BOOL|| type == DP_ATTR_INT || type == DP_ATTR_UINT32 || type == DP_ATTR_INT64) {
            std::vector<int> sourceValues;
            getMIntArrayFromAttribute(inputData, name, sourceValues);
            dommy::attribute::requireArrayStatus(dommy::attribute::copyMayaArrayPrefix(
                sourceValues, [](int value) { return value; }, newArray));
        }
    }
}
void getMIntArrayFromAttribute(const DPData &inputData, const std::string &name, std::vector<int> &newArray) {
	
	if ((inputData.isExistAttr(name))) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		if (isIntSingleValueAttrType(type)) {
			
			if (type == DP_ATTR_BOOL) {
				getDPSingleValueToInt<bool>(inputData, name, newArray);
			} else if (type == DP_ATTR_INT) {
				getDPSingleValueToInt<int32_t>(inputData, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
				getDPSingleValueToInt<uint32_t>(inputData, name, newArray);
			} else if (type == DP_ATTR_INT64) {
				getDPSingleValueToInt<int64_t>(inputData, name, newArray);
            }
		}
	}
}

void getMDoubleArrayFromAttribute(const DPData &inputData, const std::string &name, MDoubleArray &newArray) {
    
    if (inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		
        if (isSingleRealAttrType(type)) {
            DDoubleArray sourceValues;
            getDDoubleArrayFromAttribute(inputData, name, sourceValues);
            dommy::attribute::requireArrayStatus(dommy::attribute::copyMayaArrayPrefix(
                sourceValues, [](double value) { return value; }, newArray));
        }
		
    }
}

void getDDoubleArrayFromAttribute(const DPData &inputData, const std::string &name, DDoubleArray &newArray) {
    
    if (inputData.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
        
        
        if (isSingleRealAttrType(type)) {
            
            if (type == DP_ATTR_FLOAT) {
                getDPSingleValueToDouble<float>(inputData, name, newArray);
            } else if (type == DP_ATTR_DOUBLE) {
                getDPSingleValueToDouble<double>(inputData, name, newArray);
            } else if (type == DP_ATTR_INT) {
                getDPSingleValueToDouble<int32_t>(inputData, name, newArray);
            } else if (type == DP_ATTR_UINT32) {
                getDPSingleValueToDouble<uint32_t>(inputData, name, newArray);
            } else if (type == DP_ATTR_INT64) {
                getDPSingleValueToDouble<int64_t>(inputData, name, newArray);
            }
            
        }
        
    }
}

void getMFloatArrayFromAttribute(const DPData &inputData, const std::string &name, MFloatArray &newArray) {
    
    if (inputData.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
        
        
        if (isSingleRealAttrType(type)) {
            
            if (type == DP_ATTR_FLOAT) {
                getDPSingleValueToFloat<float>(inputData, name, newArray);
            } else if (type == DP_ATTR_DOUBLE) {
                getDPSingleValueToFloat<double>(inputData, name, newArray);
            } else if (type == DP_ATTR_INT) {
                getDPSingleValueToFloat<int32_t>(inputData, name, newArray);
            } else if (type == DP_ATTR_UINT32) {
                getDPSingleValueToFloat<uint32_t>(inputData, name, newArray);
            } else if (type == DP_ATTR_INT64) {
                getDPSingleValueToFloat<int64_t>(inputData, name, newArray);
            }
            
        }
        
    }
}

void getDScalarArrayFromAttribute(const DPData &inputData, const std::string &name, DScalarDataList &newArray) {
    
    if (inputData.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
        
        
        if (isSingleRealAttrType(type)) {
            
            switch (type) {
                case DP_ATTR_FLOAT:
                    getDPSingleValueToScalar<float>(inputData, name, newArray);
                    break;
                case DP_ATTR_DOUBLE:
                    getDPSingleValueToScalar<double>(inputData, name, newArray);
                    break;
                case DP_ATTR_INT:
                    getDPSingleValueToScalar<int32_t>(inputData, name, newArray);
                    break;
                case DP_ATTR_UINT32:
                    getDPSingleValueToScalar<uint32_t>(inputData, name, newArray);
                    break;
                case DP_ATTR_INT64:
                    getDPSingleValueToScalar<int64_t>(inputData, name, newArray);
                    break;
                default:
                    break;
            }
        }
    }
}


void getMMatrixArrayFromAttribute(const DPData &inputData, const std::string &name, MMatrixArray &newArray) {
	
	if ( inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		
		if (type == DP_ATTR_MATRIX) {
			
			getDPMatrixList<MMatrix>(inputData, name, newArray);
			
		}
	}
}

void getMStringArrayFromAttribute(const DPData &inputData, const std::string &name, MStringArray &newArray) {
	
	if (inputData.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = inputData.getAttr(name)->getAttrType();
		
		if (type == DP_ATTR_STRING) {
			
            dommy::attribute::requireArrayStatus(dommy::attribute::copyStringMayaArray(
                inputData.getAttr(name), inputData.numberOfPoints(), newArray, true));
		}
	}
}



#pragma mark copy and append method



bool appendOtherDPAttribute(
	const DPAttributeData::Ptr& destination,
	const DPAttributeData::ConstPtr& source)
{
	if (!destination || !source) return false;
	return destination->appendValues(*source);
}




#pragma mark check functions


bool isSingleValueAttrType(const std::string &attrTypeStr) {
    if (attrTypeStr == "bool" || attrTypeStr == "int" || attrTypeStr == "uint32" || attrTypeStr == "int64" ||
        attrTypeStr == "float" || attrTypeStr == "double") {
        return true;
    } else {
        return false;
    }
}

bool isSingleValueAttrType(const DP_ATTR_DATA_TYPE &type) {
	if (type == DP_ATTR_BOOL || type == DP_ATTR_INT || type == DP_ATTR_UINT32 || type == DP_ATTR_INT64 ||
		type == DP_ATTR_FLOAT ||  type == DP_ATTR_DOUBLE) {
		return true;
	} else {
		return false;
	}
}


bool isIntSingleValueAttrType(const DP_ATTR_DATA_TYPE &type) {
    if (type == DP_ATTR_BOOL || type == DP_ATTR_INT || type == DP_ATTR_UINT32 || type == DP_ATTR_INT64) {
        return true;
    } else {
        return false;
    }
}

bool isSingleRealAttrType(const std::string &attrTypeStr) {
    if (attrTypeStr == "int" || attrTypeStr == "uint32" || attrTypeStr == "int64" || attrTypeStr == "float" || attrTypeStr == "double") {
        return true;
    } else {
        return false;
    }

}

bool isSingleRealAttrType(const DP_ATTR_DATA_TYPE &type) {
	if (type == DP_ATTR_INT || type == DP_ATTR_UINT32 || type == DP_ATTR_INT64 ||
		type == DP_ATTR_FLOAT ||  type == DP_ATTR_DOUBLE) {
		return true;
	} else {
		return false;
	}
	
}


bool isVector2DAttrType(const std::string &attrTypeStr) {
    if(attrTypeStr == "vec2i" || attrTypeStr == "vec2f" || attrTypeStr == "vec2d") {
        return true;
    } else {
        return false;
    }
    
}

bool isVector2DAttrType(const DP_ATTR_DATA_TYPE &type) {
	if(type == DP_ATTR_VEC2I || type == DP_ATTR_VEC2F || type == DP_ATTR_VEC2D) {
		return true;
	} else {
		return false;
	}
}

bool isVector3DAttrType(const std::string &attrTypeStr) {
    if(attrTypeStr == "vec3i" || attrTypeStr == "vec3f" || attrTypeStr == "vec3d") {
        return true;
    } else {
        return false;
    }
}

bool isVector3DAttrType(const DP_ATTR_DATA_TYPE &type) {
    if(type == DP_ATTR_VEC3I || type == DP_ATTR_VEC3F || type == DP_ATTR_VEC3D) {
        return true;
    } else {
        return false;
    }
}



bool isMatrixAttrType(const std::string &attrTypeStr) {
    if(attrTypeStr == "matrix") {
        return true;
    } else {
        return false;
    }
}

bool isMatrixAttrType(const DP_ATTR_DATA_TYPE &type) {
	if(type == DP_ATTR_MATRIX) {
		return true;
	} else {
		return false;
	}
}

bool isStringAttrType(const std::string &attrTypeStr) {
    if(attrTypeStr == "string") {
        return true;
    } else {
        return false;
    }
}

bool isStringAttrType(const DP_ATTR_DATA_TYPE &type) {
	if(type == DP_ATTR_STRING) {
		return true;
	} else {
		return false;
	}
}

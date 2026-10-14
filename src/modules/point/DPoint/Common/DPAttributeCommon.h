//
//  DPAttributeCommon.hpp
//  dommyTool
//
//  Created by yuzo on 7/3/16.
//
//

#pragma once

#include <stdio.h>
#include <vector>

#include "DPData.h"
#include "DPAttributeData.h"
#include "DPAttributeAccess.h"
#include "DPAttributeType.h"
#include "common/utility/DAttributeValueConversion.h"

#include "DCurveData.h"

#include "DMayaHeaders.h"



template <typename T, typename S>
inline void DGetDPAttrSingleValueList(const DPData *inputData, const std::string &name, std::vector<S> &newValues)
{
	std::vector<T> srcList = inputData->attrListValue<T>(name);
	for (long i=0; i<srcList.size(); i++) {
		T val= srcList.at(i);
		newValues.push_back((S)val);
	}
}

struct DABBoxClipOp
{
    DABBoxClipOp()
    {}
    
    template<typename T>
    void operator()(const typename TypedDPAttributeData<T>::Ptr& /*attribute*/)
    {
//        outputGrid = openvdb::tools::clip(grid, bbox, inside);
    }
    
};
//Common method
template<typename T, typename OpType, typename AttrPtrType>
inline void doProcessTypedDAttr(DPAttributeData::Ptr attr, OpType& op)
{
    typename TypedDPAttributeData<T>::Ptr m = std::static_pointer_cast<AttrPtrType, DPAttributeData>(attr);
    op.template operator()<T>(m);
//    DAttrProcessor<AttrType, OpType,
//    boost::is_const<typename GridPtrType::element_type>::value>::call(op, grid);
}
template</*typename DAttrPtrType*/typename OpType>
inline bool processTypedAttr(DPAttributeData::Ptr attr, OpType& op)
{
    if (!attr) {
        return false;
    }

    DP_ATTR_DATA_TYPE type = attr->getAttrType();
    if (type == DP_ATTR_BOOL) {
        doProcessTypedDAttr<bool, OpType, DPBoolAttr>(attr, op);
        return true;
    }
    if (type == DP_ATTR_INT) {
        doProcessTypedDAttr<int32_t, OpType, DPIntAttr>(attr, op);
        return true;
    }
    return false;
}

//list edit
void CopyAttributeToAttribute(DPAttrList &baseAttrlist, const DPAttrList &targetAttrlist);

bool IsExistInAttributeList(const DPAttrList &attrlist, const std::string& name);
void AddAttributeList(DPAttrList &attrlist, const std::string &name, const DPAttributeData& attr );
void RemoveFromAttributeList(DPAttrList &attrlist, const std::string &name);
DPAttributeData::Ptr GetAttrPtr(DPAttrList &attrlist, const std::string& name);
DPAttributeData::ConstPtr ExtractAttributeFromAttributeList(const DPAttrList &attrlist, const std::string& name);
DString GetAttributeTypeName(const DPAttrList &attrlist, const std::string& name);

DStringList GetAllNameAttributeList(const DPAttrList &attrlist);
DStringList GetAllAttributeListInfo(const DPAttrList &attrlist);


//edit

void InsertAttributeDefaultValue(DPAttrList &attrlist, const size_t &count, const uint &index);
void ExpandAttributeSize(DPAttrList &attrlist, const size_t &count);

bool DDeleteAttributeFromIds(DPAttrList &attrlist, const DPIdList &ids);
bool DCropAttributeFromIds(DPAttrList &attrlist, const DPIdList &ids);
bool DSortAttributeFromIds(DPAttrList &attrlist, const DPIndexList &ids);
bool DRemapAtttributeFromIds(DPAttrList &attrlist, const DPIndexList &ids);

template <typename T>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string &name, const std::vector<T> &values);
template <typename T>
void DAddAttributeWithValues(DPAttrList &attrlist, const std::string& name, const std::vector<T> &values, const T &defaultValue);


//Get DPoint Attribute

void getMVectorArrayFromAttribute(const DPData &inputData, const std::string &name, MVectorArray &newArray);
void getMVectorArrayFromAttribute(const DPData &inputData, const std::string &name, DVec3DataList &newArray);
void getMFloatPointArrayFromAttribute(const DPData &inputData, const std::string &name, MFloatPointArray &newArray, float ww);
void getMVector2DArrayFromAttribute(const DPData &inputData, const std::string &name, MVectorArray &newArray);
void getMVector2DArrayFromAttribute(const DPData &inputData, const std::string &name, DVec2DataList &newArray);

void getMIntArrayFromAttribute(const DPData &inputData, const std::string &name, MIntArray &newArray);
void getMIntArrayFromAttribute(const DPData &inputData, const std::string &name, DIntArray &newArray);
void getMDoubleArrayFromAttribute(const DPData &inputData, const std::string &name, MDoubleArray &newArray);
void getMFloatArrayFromAttribute(const DPData &inputData, const std::string &name, MFloatArray &newArray);
void getDDoubleArrayFromAttribute(const DPData &inputData, const std::string &name, DDoubleArray &newArray);
void getMMatrixArrayFromAttribute(const DPData &inputData, const std::string &name, MMatrixArray &newArray);
void getMStringArrayFromAttribute(const DPData &inputData, const std::string &name, MStringArray &newArray);







//copy appen method
bool appendOtherDPAttribute(
	const DPAttributeData::Ptr& destination,
	const DPAttributeData::ConstPtr& source);


//check value Type

bool isSingleValueAttrType(const std::string &attrTypeStr);
bool isSingleValueAttrType(const DP_ATTR_DATA_TYPE &type);

bool isIntSingleValueAttrType(const DP_ATTR_DATA_TYPE &type);

bool isSingleRealAttrType(const std::string &attrTypeStr);
bool isSingleRealAttrType(const DP_ATTR_DATA_TYPE &type);

bool isVector2DAttrType(const std::string &attrTypeStr);
bool isVector2DAttrType(const DP_ATTR_DATA_TYPE &type);


bool isVector3DAttrType(const std::string &attrTypeStr);
bool isVector3DAttrType(const DP_ATTR_DATA_TYPE &type);

bool isMatrixAttrType(const std::string &attrTypeStr);
bool isMatrixAttrType(const DP_ATTR_DATA_TYPE &type);

bool isStringAttrType(const std::string &attrTypeStr);
bool isStringAttrType(const DP_ATTR_DATA_TYPE &type);

//
//  DGroupCommon.hpp
//  dommyTool
//
//  Created by yuzo on 4/29/17.
//
//





#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <maya/MDataHandle.h>
#include <maya/MString.h>
#include <maya/MVector.h>

#include "DPGroupData.h"

#include "DCurveUtil.h"
#include "common/utility/DAttributeMathInput.h"
#include "common/utility/DAttributeArrayConversion.h"

class DPData;


/////////////////////////////Group Data/////////////////////////////

//enum {
//    DGRP_TYPE_PRIM = 0,
//    DGRP_TYPE_POINT,
//    DGRP_TYPE_FACE,
//    DGRP_TYPE_EDGE,
//    DGRP_TYPE_COUNT
//};typedef short DGroupType;


enum {
	DGRP_NUM_PATTERN = 0,
	DGRP_NUM_RANGE,
	DGRP_NUM_EXPRESSION,
	DGRP_NUM_COUNT
};typedef short DGroupNumericType;


enum {
	DGRP_BOUND_BOX = 0,
	DGRP_BOUND_SPHERE,
	DGRP_BOUND_OBJECT,
	DGRP_BOUND_COUNT
};typedef short DGroupBoundType;

enum {
	DGRP_EXT_REPLACE = 0,
	DGRP_EXT_UNION,
	DGRP_EXT_SUBSTRUCT,
	DGRP_EXT_INTERSECT,
	DGRP_EXT_COUNT
};typedef short DGroupExistActionType;


struct DGroupParams
{
	DGroupParams()
	: m_grp_type(D_COMP_PRIM)
	, m_grp_name()
	, m_exist_act(DGRP_EXT_REPLACE)
	, m_invert(false)
	, isNumeric(false)
	, m_pattern_str()
	, m_numeric_type(DGRP_NUM_PATTERN)
	, m_start(0)
	, m_end(0)
	, m_select(0)
	, m_step(0)
	, m_expression_str()
	, isBound(false)
	, boundingType(DGRP_BOUND_BOX)
	, m_bbox_center(MVector::zero)
	, m_bbox_size(1.0, 1.0, 1.0)
	, m_objHnd()
	, isNormal(false)
	, m_normal_attr()
	, m_normal(MVector::yAxis)
	, m_spread(90.0)
	, m_normalType(DC_VEC_kTangent)
	, isFaceArea(false)
	, faceAreaMin(0.0)
	, faceAreaMax(0.0)
	, isFaceVtxCount(false)
	, faceVtxMin(0)
	, faceVtxMax(0)
	, isConvex(false)
	, isPlanar(false)
	, isFaceBoundry(false)
	, isEdgeAngle(false)
	, edgeAngleMin(0.0)
	, edgeAngleMax(0.0)
	, isEdgeLength(false)
	, edgeLengthMin(0.0)
	, edgeLengthMax(0.0)
	, isEdgeBoundry(false)
	, isCurveLength(false)
	, curveLength(0.0)
	, isCurveArea(false)
	, curveArea(0.0)
	{ }
	
	
	DComponentType m_grp_type;
//    DGroupType m_grp_type;
	std::string m_grp_name;
	
	DGroupExistActionType m_exist_act;
	
	bool m_invert;
	
	//numeric
	bool isNumeric;
	MString m_pattern_str;
	DGroupNumericType m_numeric_type;
	
	int m_start, m_end;
	int m_select, m_step;
	
	std::string m_expression_str;
	
	
	//boundingbox
	bool isBound;
	DGroupBoundType boundingType;
	MVector m_bbox_center;
	MVector m_bbox_size;
	
	MDataHandle m_objHnd;
	
	//normal
	bool isNormal;
	std::string m_normal_attr;
	MVector m_normal;
	double m_spread;
    DC_VECTOR_TYPE m_normalType;//for curve
    
	
	//face
	bool isFaceArea;
	double faceAreaMin,faceAreaMax;
	bool isFaceVtxCount;
	int faceVtxMin, faceVtxMax;
	bool isConvex;
    bool isPlanar;//curve and face
	bool isFaceBoundry;
	
	
	//edge
	bool isEdgeAngle;
	double edgeAngleMin,edgeAngleMax;
	bool isEdgeLength;
	double edgeLengthMin,edgeLengthMax;
	bool isEdgeBoundry;
    
    
    //curve
    bool isCurveLength;
    double curveLength;
    bool isCurveArea;
    double curveArea;
    
	
};




/////////////////////////////Group Function/////////////////////////////

void CopyGroupToGroup(DPGrpList &baseGroup, const DPGrpList &targetGroup);


bool IsExistInGroupList(const DPGrpList &grplist, const std::string& name);
void addGroupList(DPGrpList &grplist, const std::string &name, const DPGroupData& grp );
void removeFromGroup(DPGrpList &grplist, const std::string &name);
void removeFromGroups(DPGrpList &grplist, const MString nameList);
DPGroupData::Ptr GetGrpPtr(DPGrpList &grplist, const std::string& name);
DPGroupData::ConstPtr ExtractGroupFromGroupList(const DPGrpList &grplist, const std::string& name);

void DeleteFromGroupUseIds(DPGrpList &grplist, const DPIdList &ids);
void SortFromGroupUseIds(DPGrpList &grplist, const DPIndexList &ids);
void RemapFromGroupUseIds(DPGrpList &grplist, const DPIndexList &ids);
DStringList GetAllNameInGroupList(const DPGrpList &grplist);
void GetAllGroupListInfo(const DPGrpList &grplist, DStringList &groupInfos);

//edit
void ExpandGroupListSize(DPGrpList &grplist, const size_t &count);
void InsertGroupList(DPGrpList &grplist, const size_t &count, const uint &index, const bool &state);

//merge
bool MergeGroupToGroup(DPGrpList &baseGroup, const DPGrpList &targetGroup);



/////////////////////////////Generate Group Ids/////////////////////////////
//numeric
MStatus validateGroupParams(const DGroupParams& params, bool expressionAllowed);
MStatus generateNumericGroup(const DGroupParams &params, const size_t &pCount, DPIdList &idsArray);
bool getPatternStrIndex(const MString &pattern, const size_t &pCount, bool &reverse, DPIndexList &idsArray);
void getRangeStrIndex(int start, int end, int step, int select, const size_t &pCount, DPIdList &idsArray);


//bbox
MStatus generateBoundGroup(const DGroupParams &params, const DPointList &pList, DPIdList &idsArray);


//norm
MStatus generateNormalGroup(const DGroupParams &params, const std::vector<Imath::V3d>& vectors, DPIdList &idsArray);

template <typename Owner>
MStatus generateAttributeNormalGroup(const DGroupParams& params, const Owner& owner,
                                     std::size_t count, DPIdList& selection)
{
    std::vector<Imath::V3d> vectors;
    if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)) ||
        !dommy::attribute::readNumericAttributeValues(owner, params.m_normal_attr, count, vectors)) {
        return MS::kInvalidParameter;
    }
    return generateNormalGroup(params, vectors, selection);
}


//check active


///StringでDPointData内の指定されたGroupのアクティブなポイントIDを抽出する
void checkDPGroupActiveIdsFromString(const DPData &dpdata, MString groupName, DPIdList &idsArray, bool &groupUse);

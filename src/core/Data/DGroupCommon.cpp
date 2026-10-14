//
//  DGroupCommon.cpp
//  dommyTool
//
//  Created by yuzo on 4/29/17.
//
//

#include "DGroupCommon.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DGroupEvaluation.h"

#include <stdexcept>

#include "DMeshUtil.h"

#include "dommyMath.h"

#include "DPointSeExpr.h"
#include "DNodeOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DGroupSelection.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MGlobal.h>

#pragma mark group common method

void CopyGroupToGroup(DPGrpList &baseGroup, const DPGrpList &targetGroup) {
    DPGrpList copiedGroups;
    for (ConstDPGrpIter group = targetGroup.begin();
         group != targetGroup.end(); ++group) {
        if (!group->second) {
            throw std::invalid_argument("Cannot copy a null component group");
        }
        DPGroupData::Ptr copiedGroup = group->second->copy();
        if (!copiedGroup) {
            throw std::runtime_error(
                "Component group copy returned a null value");
        }
        copiedGroups.emplace(group->first, copiedGroup);
    }

    baseGroup.swap(copiedGroups);
}

bool IsExistInGroupList(const DPGrpList &grplist, const std::string& name) {
    if(name.size() == 0) {
        return false;
    }
    
    auto iter = grplist.find(name);
    if(iter == grplist.end()) {
        return false;
    } else {
        return true;
    }
}

void addGroupList(DPGrpList &grplist, const std::string &name, const DPGroupData& grp) {
    if(name.size() == 0) {
        return;
    }
    
    //already exist check
    auto iter = grplist.find(name);
    
    if(iter == grplist.end()) {
        DPGroupData::Ptr tmp = grp.copy();
        grplist[name] = tmp;
    }
}

void removeFromGroup(DPGrpList &grplist, const std::string &name) {
    
    auto iter = grplist.find(name);
    if (iter != grplist.end()) {
        grplist.erase(iter);
    }
    
}

void removeFromGroups(DPGrpList &grplist, const MString nameList) {
    MStringArray groupNameArray;
    nameList.split(*MString(" ").asChar(), groupNameArray);
    
    for (size_t i=0; i<groupNameArray.length(); i++) {
        
        if (IsExistInGroupList(grplist, groupNameArray[i].asChar())) {
            //delete group
            removeFromGroup(grplist, groupNameArray[i].asChar());
        }
    }
}

DPGroupData::Ptr GetGrpPtr(DPGrpList &grplist, const std::string& name) {
    auto iter = grplist.find(name);
    return (iter == grplist.end() ? DPGroupData::Ptr() : iter->second);
}

DPGroupData::ConstPtr ExtractGroupFromGroupList(const DPGrpList &grplist, const std::string& name) {
    
    auto iter = grplist.find(name);
    return (iter == grplist.end() ? DPGroupData::ConstPtr() : iter->second);
}

void DeleteFromGroupUseIds(DPGrpList &grplist, const DPIdList &ids) {
    
    for (auto &iter : grplist) {
        iter.second->deleteArrayFromIds(ids);
    }
}

void SortFromGroupUseIds(DPGrpList &grplist, const DPIndexList &ids) {
    for (auto &iter : grplist) {
        iter.second->sortArrayFromIds(ids);
    }
}

void RemapFromGroupUseIds(DPGrpList &grplist, const DPIndexList &ids) {
    
    for (auto &iter : grplist) {
        iter.second->remapFromIds(ids);
    }
}

DStringList GetAllNameInGroupList(const DPGrpList &grplist) {
    
    DStringList groupNames;
    
    for(auto iter : grplist) {
        groupNames.push_back(iter.first);
    }
    
    return groupNames;
}

void GetAllGroupListInfo(const DPGrpList &grplist, DStringList &groupInfos) {
    if (grplist.empty()) return;
    DStringList candidate(groupInfos);
    for(const auto& iter : grplist) {
        std::stringstream infoStr;
        infoStr << "*" << iter.first;
        candidate.push_back(dommy::text::checkedStreamText(infoStr, "Group description formatting failed"));
    }
    groupInfos.swap(candidate);
}


void ExpandGroupListSize(DPGrpList &grplist, const size_t &count) {
    for (auto it :grplist) {
        it.second->expandOtherGrp(count);
    }
}

void InsertGroupList(DPGrpList &grplist, const size_t &count, const uint &index, const bool &state) {
    for (auto it :grplist) {
        it.second->insertOtherGrp(count, index, state);
    }
}



bool MergeGroupToGroup(DPGrpList &baseGroup, const DPGrpList &targetGroup) {
    //ターゲット側とソース側に存在するグループをコピー
    for (auto iter: targetGroup) {
        if (IsExistInGroupList(baseGroup, iter.first))  {
            baseGroup[iter.first]->appendOtherGrp(iter.second);
//            GetGrpPtr(baseGroup, iter.first)->appendOtherGrp(iter.second);
        }
        
    }
    
    //ソース側に存在してターゲット側に存在しないプリミティブグループは削除
    DStringList needDeleteGrpList;
    for (auto iter :baseGroup) {
        if (!IsExistInGroupList(targetGroup, iter.first))  {
            needDeleteGrpList.push_back(iter.first);
        }
        
        
    }
    
    if (!needDeleteGrpList.empty()) {
        for (auto it: needDeleteGrpList) {
            removeFromGroup(baseGroup, it);
        }
    }

    return !needDeleteGrpList.empty();
}







#pragma mark numeric method

MStatus validateGroupParams(const DGroupParams& params, bool expressionAllowed) {
    static_assert(DGRP_EXT_REPLACE == 0 && DGRP_EXT_UNION == 1 && DGRP_EXT_SUBSTRUCT == 2 &&
                      DGRP_EXT_INTERSECT == 3,
                  "Group action values are serialized");
    if (!dommy::attribute::validAttributeName(params.m_grp_name) ||
        params.m_grp_name.size() > dommy::generation::kMaxGeneratedStringByteCount ||
        params.m_exist_act < DGRP_EXT_REPLACE || params.m_exist_act >= DGRP_EXT_COUNT)
        return MS::kInvalidParameter;
    if (params.isNumeric) {
        if (params.m_numeric_type < DGRP_NUM_PATTERN || params.m_numeric_type >= DGRP_NUM_COUNT ||
            (params.m_numeric_type == DGRP_NUM_EXPRESSION &&
             (!expressionAllowed || params.m_expression_str.empty() ||
              params.m_expression_str.size() > dommy::generation::kMaxGeneratedStringByteCount)))
            return MS::kInvalidParameter;
        if (params.m_numeric_type == DGRP_NUM_PATTERN) {
            DPIdList empty;
            if (!dommy::group::selectPattern(params.m_pattern_str.asChar(), 0U, empty))
                return MS::kInvalidParameter;
        }
        if (params.m_numeric_type == DGRP_NUM_RANGE && (params.m_step < 0 || params.m_select <= 0))
            return MS::kInvalidParameter;
    }
    if (params.isBound) {
        if (params.boundingType < DGRP_BOUND_BOX || params.boundingType >= DGRP_BOUND_COUNT)
            return MS::kInvalidParameter;
        if (params.boundingType == DGRP_BOUND_OBJECT) {
            if (params.m_objHnd.type() != MFnData::kMesh || params.m_objHnd.asMesh().isNull())
                return MS::kInvalidParameter;
        } else {
            if (!dommy::group::finiteTriple(params.m_bbox_center) ||
                !dommy::group::finiteTriple(params.m_bbox_size))
                return MS::kInvalidParameter;
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                const double size = std::fabs(params.m_bbox_size[axis]);
                if (!std::isfinite(params.m_bbox_center[axis] - size) ||
                    !std::isfinite(params.m_bbox_center[axis] + size) ||
                    (params.boundingType == DGRP_BOUND_SPHERE &&
                     size <= std::sqrt(std::numeric_limits<double>::min())))
                    return MS::kInvalidParameter;
            }
        }
    }
    if (params.isNormal) {
        bool selected = false;
        if (!std::isfinite(params.m_spread) || params.m_spread < 0.0 || params.m_spread > 180.0 ||
            !dommy::group::normalSelected(params.m_normal, params.m_normal,
                                          dommy::math::degreesToRadians(params.m_spread), selected))
            return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

MStatus generateNumericGroup(const DGroupParams& params, const size_t& pCount, DPIdList& idsArray) {
    if (idsArray.size() != pCount || pCount > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    if (params.m_numeric_type == DGRP_NUM_PATTERN)
        return dommy::group::selectPattern(params.m_pattern_str.asChar(), pCount, idsArray)
                   ? MS::kSuccess
                   : MS::kInvalidParameter;
    if (params.m_numeric_type == DGRP_NUM_RANGE) {
        if (params.m_step < 0 || params.m_select <= 0)
            return MS::kInvalidParameter;
        getRangeStrIndex(params.m_start, params.m_end, params.m_step, params.m_select, pCount,
                         idsArray);
        return MS::kSuccess;
    }
    // DPGroup handles expressions explicitly. Other callers must not silently
    // turn an unsupported expression mode into an empty successful selection.
    return MS::kInvalidParameter;
}

bool getPatternStrIndex(const MString& pattern, const size_t& pCount, bool& reverse,
                        DPIndexList& idsArray) {
    return dommy::component::parseIndexPattern(
        pattern.asChar(), pCount, dommy::generation::kMaxGeneratedPointCount, reverse, idsArray);
}

void getRangeStrIndex(int start, int end, int step, int select, const size_t& pCount,
                      DPIdList& idsArray) {
    dommy::group::selectPeriodicRange(start, end, step, select, pCount, idsArray);
}

#pragma mark Bound method

MStatus generateBoundGroup(const DGroupParams& params, const DPointList& points,
                           DPIdList& selection) {
    if (selection.size() != points.size() ||
        points.size() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    if (params.boundingType == DGRP_BOUND_BOX || params.boundingType == DGRP_BOUND_SPHERE) {
        const bool success = dommy::group::evaluateSelection(
            points.size(), selection, [&](std::size_t i, bool& selected) {
                const auto& point = points[i];
                if (!dommy::group::finiteTriple(point))
                    return false;
                if (params.boundingType == DGRP_BOUND_BOX)
                    selected = dommy::group::isInsideOpenBox(
                        point[0], point[1], point[2], params.m_bbox_center[0],
                        params.m_bbox_center[1], params.m_bbox_center[2], params.m_bbox_size[0],
                        params.m_bbox_size[1], params.m_bbox_size[2]);
                else
                    selected = dommy::group::isInsideEllipsoid(
                        point[0], point[1], point[2], params.m_bbox_center[0],
                        params.m_bbox_center[1], params.m_bbox_center[2], params.m_bbox_size[0],
                        params.m_bbox_size[1], params.m_bbox_size[2]);
                return true;
            });
        return success ? MS::kSuccess : MS::kInvalidParameter;
    }
    if (params.boundingType != DGRP_BOUND_OBJECT || params.m_objHnd.type() != MFnData::kMesh)
        return MS::kInvalidParameter;
    MStatus status;
    MFnMesh mesh(params.m_objHnd.asMesh(), &status);
    if (!status)
        return status;
    MMeshIsectAccelParams acceleration = mesh.autoUniformGridParams();
    DPIdList candidate(selection);
    bool success = false;
    try {
        success = dommy::group::evaluateSelection(
            points.size(), candidate, [&](std::size_t i, bool& selected) {
                const auto& point = points[i];
                float x, y, z;
                if (!dommy::numeric::checkedFloatCast(point[0], x) ||
                    !dommy::numeric::checkedFloatCast(point[1], y) ||
                    !dommy::numeric::checkedFloatCast(point[2], z))
                    return false;
                MFloatPointArray hits;
                mesh.allIntersections(MFloatPoint(x, y, z), MFloatVector(0.0f, 1.0f, 0.0f), nullptr,
                                      nullptr, true, MSpace::kWorld, 9999.0f, false, &acceleration,
                                      false, hits, nullptr, nullptr, nullptr, nullptr, nullptr,
                                      1.0e-6f, &status);
                if (!status)
                    return false;
                selected = hits.length() % 2U != 0U;
                return true;
            });
    } catch (...) {
        mesh.freeCachedIntersectionAccelerator();
        throw;
    }
    const MStatus cleanup = mesh.freeCachedIntersectionAccelerator();
    if (!cleanup)
        return cleanup;
    if (!success)
        return status ? MStatus(MS::kInvalidParameter) : status;
    selection.swap(candidate);
    return MS::kSuccess;
}

MStatus generateNormalGroup(const DGroupParams& params, const std::vector<Imath::V3d>& vectors,
                            DPIdList& selection) {
    const double spread = dommy::math::degreesToRadians(params.m_spread);
    return dommy::group::evaluateSelection(vectors.size(), selection,
                                           [&](std::size_t i, bool& selected) {
                                               return dommy::group::normalSelected(
                                                   vectors[i], params.m_normal, spread, selected);
                                           })
               ? MS::kSuccess
               : MS::kInvalidParameter;
}

#pragma mark check active group ids

void checkDPGroupActiveIdsFromString(const DPData &dpdata, MString groupName, DPIdList &idsArray, bool &groupUse) {
    

    
    //split group name
    MStringArray grpNameArray;
    groupName.split(*MString(" ").asChar(), grpNameArray);
    
    
    idsArray.resize(dpdata.numberOfPoints());
    
    groupUse = false;
    
    
    
    if (grpNameArray.length()) {
        
        //一度全てディアクティブに
        std::fill(idsArray.begin(), idsArray.end(), false);
        
        for (uint i=0; i<grpNameArray.length(); i++) {
            
            
            MString gName = grpNameArray[i];
            
            
            //check invert
            int isRev = gName.index('!');
            bool isInvert = false;
            if (isRev!=-1 && isRev==0) {//reverse
                
                gName = gName.substring(1, gName.length());
                isInvert = true;
            }
            
            
            
            //check group exist
            if(dpdata.isExistGrp(gName.asChar())) {
                
                DPGroupData::ConstPtr grpdata = dpdata.getGrp(gName.asChar());
                DPIdList gids(grpdata->getGroupIds());
                if (isInvert) {
                    gids.flip();
                }
                mergeUnionIdListOp(&gids, idsArray);
                groupUse = true;
            }
            
        }
    } else {
        
        //指定されたグループがないので全てアクティブに
        std::fill(idsArray.begin(), idsArray.end(), true);
    }
}

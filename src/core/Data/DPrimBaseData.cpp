//
//  DPrimBaseData.cpp
//  dommyTool
//
//  Created by yuzo on 1/14/17.
//
//

#include "DPrimBaseData.h"

#include "DCurveUtil.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "DGroupCommon.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DPointMean.h"
#include "common/utility/DPointArrayConversion.h"

#include <cmath>
#include <stdexcept>

namespace {

dommy::math::Bounds3d worldBounds(const DPointList& points, const MMatrix& matrix) {
    dommy::math::Bounds3d bounds;
    if (!dommy::attribute::isFiniteMatrix(matrix) ||
        !dommy::numeric::appendTransformedBounds(points, matrix, bounds))
        throw std::invalid_argument("DPrimBaseData: invalid transformed point bounds");
    return bounds;
}

bool hasValidBasePointState(
    const DPointList& points,
    const DPAttrList& attributes,
    const DPGrpList& groups)
{
    if (points.size() >
        dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    for (ConstDPointIter point = points.begin();
         point != points.end(); ++point) {
        if (!std::isfinite((*point)[0]) ||
            !std::isfinite((*point)[1]) ||
            !std::isfinite((*point)[2])) {
            return false;
        }
    }
	dommy::attribute::ValidationBudget budget(points.size());
	if (!dommy::attribute::validateAttributeList(
			attributes, points.size(), budget)) {
		return false;
    }
    for (ConstDPGrpIter group = groups.begin();
         group != groups.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != points.size() ||
			!dommy::generation::checkedSum(
				budget.workItems,
				points.size(),
				dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }
    return true;
}

} // namespace



DPrimBaseData::DPrimBaseData() {
    
    pointArray.clear();
    mAttributeList.clear();
    mGrooupList.clear();
    
    m_mat = MMatrix();
}


DPrimBaseData::DPrimBaseData(DPrimBaseData const &src) {
    
	setPosition(src.pointArray);
	
    //attribute
    CopyAttributeToAttribute(mAttributeList, src.mAttributeList);
    
    //group
    CopyGroupToGroup(mGrooupList, src.mGrooupList);
	
    m_mat = src.m_mat;
}

DPrimBaseData::~DPrimBaseData() = default;







// point method


// Return points transformed to world space.
DPointList DPrimBaseData::getPointsInWorld() {
    return static_cast<const DPrimBaseData&>(*this).getPointsInWorld();
}

DPointList DPrimBaseData::getPointsInWorld() const {
    DPointList result;
    if (!dommy::attribute::isFiniteMatrix(m_mat) ||
        !dommy::numeric::checkedTransformPoints(pointArray, m_mat, result))
        throw std::invalid_argument("DPrimBaseData: transformed points exceed float storage");
    return result;
}



MPointArray DPrimBaseData::getMPosition() const {
    MPointArray points;
    if (dommy::geometry::copyPointsToMayaArray(pointArray, points) != MS::kSuccess)
        throw std::runtime_error("DPrimBaseData: unable to export point array");
    return points;
}

MFloatPointArray DPrimBaseData::getMFPosition() const {
    MFloatPointArray points;
    if (dommy::geometry::copyPointsToMayaArray(pointArray, points) != MS::kSuccess)
        throw std::runtime_error("DPrimBaseData: unable to export float point array");
    return points;
}


void DPrimBaseData::setPosition(const MPointArray &pts) {
    dommy::geometry::replacePointArray(pts, pts.length(), pointArray);
}

void DPrimBaseData::setPosition(const DPointList &pts) {
    dommy::geometry::replacePointArray(pts, pts.size(), pointArray);
}

bool DPrimBaseData::appendPoint(const DPoint &pt) {
    if (!hasValidBasePointState(
            pointArray, mAttributeList, mGrooupList) ||
        pointArray.size() >=
            dommy::generation::kMaxGeneratedPointCount ||
        !std::isfinite(pt[0]) ||
        !std::isfinite(pt[1]) ||
        !std::isfinite(pt[2])) {
        return false;
    }

    try {
        DPointList updatedPoints(pointArray);
        DPAttrList updatedAttributes;
        DPGrpList updatedGroups;
        CopyAttributeToAttribute(updatedAttributes, mAttributeList);
        CopyGroupToGroup(updatedGroups, mGrooupList);

        updatedPoints.push_back(pt);
        ExpandAttributeSize(updatedAttributes, 1U);
        ExpandGroupListSize(updatedGroups, 1U);
        if (!hasValidBasePointState(
                updatedPoints, updatedAttributes, updatedGroups)) {
            return false;
        }

        pointArray.swap(updatedPoints);
        mAttributeList.swap(updatedAttributes);
        mGrooupList.swap(updatedGroups);
        return true;
    } catch (...) {
        return false;
    }
}

bool DPrimBaseData::insertPoint(
    const DPoint &pt, const size_t &index)
{
    if (index > pointArray.size() ||
        !hasValidBasePointState(
            pointArray, mAttributeList, mGrooupList) ||
        pointArray.size() >=
            dommy::generation::kMaxGeneratedPointCount ||
        !std::isfinite(pt[0]) ||
        !std::isfinite(pt[1]) ||
        !std::isfinite(pt[2])) {
        return false;
    }

    try {
        DPointList updatedPoints(pointArray);
        DPAttrList updatedAttributes;
        DPGrpList updatedGroups;
        CopyAttributeToAttribute(updatedAttributes, mAttributeList);
        CopyGroupToGroup(updatedGroups, mGrooupList);

        updatedPoints.insert(updatedPoints.begin() + index, pt);
        InsertAttributeDefaultValue(
            updatedAttributes, 1U, static_cast<uint>(index));
        InsertGroupList(
            updatedGroups, 1U, static_cast<uint>(index), false);
        if (!hasValidBasePointState(
                updatedPoints, updatedAttributes, updatedGroups)) {
            return false;
        }

        pointArray.swap(updatedPoints);
        mAttributeList.swap(updatedAttributes);
        mGrooupList.swap(updatedGroups);
        return true;
    } catch (...) {
        return false;
    }
}

bool DPrimBaseData::deletePointFromIds(const DPIdList &ids) {
    if (ids.size() != numberOfPoints() ||
        !hasValidBasePointState(
            pointArray, mAttributeList, mGrooupList)) {
        return false;
    }

    try {
        DPointList updatedPoints;
        updatedPoints.reserve(pointArray.size());
        for (std::size_t index = 0U;
             index < pointArray.size(); ++index) {
            if (!ids[index]) {
                updatedPoints.push_back(pointArray[index]);
            }
        }

        DPAttrList updatedAttributes;
        DPGrpList updatedGroups;
        CopyAttributeToAttribute(updatedAttributes, mAttributeList);
        CopyGroupToGroup(updatedGroups, mGrooupList);
        if (!DDeleteAttributeFromIds(updatedAttributes, ids)) {
            return false;
        }
        DeleteFromGroupUseIds(updatedGroups, ids);
        if (!hasValidBasePointState(
                updatedPoints, updatedAttributes, updatedGroups)) {
            return false;
        }

        pointArray.swap(updatedPoints);
        mAttributeList.swap(updatedAttributes);
        mGrooupList.swap(updatedGroups);
        return true;
    } catch (...) {
        return false;
    }
}

bool DPrimBaseData::sortPointFromIds(const DPIndexList &ids) {
    if (!hasValidBasePointState(
            pointArray, mAttributeList, mGrooupList) ||
        !dommy::container::isValidPermutation(
            ids, pointArray.size())) {
        return false;
    }
    try {
        DPointList updatedPoints(pointArray);
        DPAttrList updatedAttributes;
        DPGrpList updatedGroups;
        CopyAttributeToAttribute(updatedAttributes, mAttributeList);
        CopyGroupToGroup(updatedGroups, mGrooupList);
        if (!dommy::container::reorderInPlace(
                updatedPoints, ids)) {
            return false;
        }
        if (!DSortAttributeFromIds(updatedAttributes, ids)) {
            return false;
        }
        SortFromGroupUseIds(updatedGroups, ids);
        if (!hasValidBasePointState(
                updatedPoints, updatedAttributes, updatedGroups)) {
            return false;
        }
        pointArray.swap(updatedPoints);
        mAttributeList.swap(updatedAttributes);
        mGrooupList.swap(updatedGroups);
        return true;
    } catch (...) {
        return false;
    }
}

bool DPrimBaseData::remapPointFromIds(const DPIndexList &ids) {
    if (!hasValidBasePointState(
            pointArray, mAttributeList, mGrooupList) ||
        ids.size() >
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    try {
        DPointList updatedPoints(pointArray);
        DPAttrList updatedAttributes;
        DPGrpList updatedGroups;
        CopyAttributeToAttribute(updatedAttributes, mAttributeList);
        CopyGroupToGroup(updatedGroups, mGrooupList);
        if (!dommy::container::remapInPlace(
                updatedPoints, ids)) {
            return false;
        }
        if (!DRemapAtttributeFromIds(updatedAttributes, ids)) {
            return false;
        }
        RemapFromGroupUseIds(updatedGroups, ids);
        if (!hasValidBasePointState(
                updatedPoints, updatedAttributes, updatedGroups)) {
            return false;
        }
        pointArray.swap(updatedPoints);
        mAttributeList.swap(updatedAttributes);
        mGrooupList.swap(updatedGroups);
        return true;
    } catch (...) {
        return false;
    }
}



// attribute method
bool DPrimBaseData::isExistAttr(const std::string& name) const {
    
    return IsExistInAttributeList(mAttributeList, name);
    
}


void DPrimBaseData::addAttr(const std::string& name, const DPAttributeData& attr)
{
    AddAttributeList(mAttributeList, name, attr);
}

void DPrimBaseData::removeAttr(const std::string& name) {
    RemoveFromAttributeList(mAttributeList, name);
}

void DPrimBaseData::removeAttrAll() {
    mAttributeList.clear();
}


DPAttributeData::ConstPtr DPrimBaseData::getAttr(const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
    
    
}

DPAttributeData::Ptr DPrimBaseData::getAttrPtr(
        const std::string& name) {
    return GetAttrPtr(mAttributeList, name);
}

DPAttributeData::ConstPtr DPrimBaseData::getAttrPtr(
        const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
}

DString DPrimBaseData::getAttrTypeName(const std::string& name) const {
    return GetAttributeTypeName(mAttributeList, name);
    
    
}


// add attribute with values
template <typename T>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<T> &values) {
    (void)name;
    (void)values;
	static_assert(
		!std::is_same<T, T>::value,
		"DPrimBaseData::addAttrWithValues has no specialization for this type");
}



template<>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<bool> &values) {
    addAttr(name, DPBoolAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<int32_t> &values) {
    addAttr(name, DPIntAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<uint32_t> &values) {
    addAttr(name, DPUInt32Attr(values));

}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<int64_t> &values) {
    addAttr(name, DPInt64Attr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<float> &values) {
    addAttr(name, DPFloatAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<double> &values) {
    addAttr(name, DPDoubleAttr(values));

}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2i> &values) {
    addAttr(name, DPVec2iAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2f> &values) {
    addAttr(name, DPVec2fAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2d> &values) {
    addAttr(name, DPVec2dAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3i> &values) {
    addAttr(name, DPVec3iAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3f> &values) {
    addAttr(name, DPVec3fAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3d> &values) {
    addAttr(name, DPVec3dAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<std::string> &values) {
    addAttr(name, DPStringAttr(values));
}

template <>
void DPrimBaseData::addAttrWithValues(const std::string& name, const std::vector<MMatrix> &values) {
    addAttr(name, DPMat4Attr(values));
}

template <typename S, typename T>
void DPrimBaseData::setAttrWithValuesTypeSwap(const std::string& name, const std::vector<T> &values) {
    if (!replaceConvertedAttributeValues<S>(*this, name, values)) {
        throw std::invalid_argument(
            "DPrimBaseData::setAttrWithValuesTypeSwap requires a matching typed attribute");
    }
}

void DPrimBaseData::setDPAttributeValues(const std::string& name, const DScalarDataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isSingleRealAttrType(type)) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues requires a matching scalar attribute");
    }

    switch (type) {
        case DP_ATTR_INT:
            setAttrWithValuesTypeSwap<int32_t>(name, values);
            break;
        case DP_ATTR_UINT32:
            setAttrWithValuesTypeSwap<uint32_t>(name, values);
            break;
        case DP_ATTR_INT64:
            setAttrWithValuesTypeSwap<int64_t>(name, values);
            break;
        case DP_ATTR_FLOAT:
            setAttrWithValuesTypeSwap<float>(name, values);
            break;
        case DP_ATTR_DOUBLE:
            setAttrWithValuesTypeSwap<double>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPrimBaseData::setDPAttributeValues received an unsupported scalar attribute");
    }
}


void DPrimBaseData::setDPAttributeValues(const std::string& name, const DVec2DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isVector2DAttrType(type)) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues requires a matching vec2 attribute");
    }

    switch (type) {
        case DP_ATTR_VEC2I:
            setAttrWithValuesTypeSwap<Imath::V2i>(name, values);
            break;
        case DP_ATTR_VEC2F:
            setAttrWithValuesTypeSwap<Imath::V2f>(name, values);
            break;
        case DP_ATTR_VEC2D:
            setAttrWithValuesTypeSwap<Imath::V2d>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPrimBaseData::setDPAttributeValues received an unsupported vec2 attribute");
    }
}


void DPrimBaseData::setDPAttributeValues(const std::string& name, const DVec3DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isVector3DAttrType(type)) {
        throw std::invalid_argument(
            "DPrimBaseData::setDPAttributeValues requires a matching vec3 attribute");
    }

    switch (type) {
        case DP_ATTR_VEC3I:
            setAttrWithValuesTypeSwap<Imath::V3i>(name, values);
            break;
        case DP_ATTR_VEC3F:
            setAttrWithValuesTypeSwap<Imath::V3f>(name, values);
            break;
        case DP_ATTR_VEC3D:
            setAttrWithValuesTypeSwap<Imath::V3d>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPrimBaseData::setDPAttributeValues received an unsupported vec3 attribute");
    }
}





size_t DPrimBaseData::numberOfAttributes() const {
	
	return mAttributeList.size();
}


DStringList DPrimBaseData::getAllAttributeName() const {
    
    return GetAllNameAttributeList(mAttributeList);
}

DStringList DPrimBaseData::getAllAttributeInfo() const {
    return GetAllAttributeListInfo(mAttributeList);
}

// group method

bool DPrimBaseData::isExistGrp(const std::string& name) const {
    
    return IsExistInGroupList(mGrooupList, name);
    
}

void DPrimBaseData::addGrp(const std::string& name, const DPGroupData& attr) {
    
    addGroupList(mGrooupList, name, attr);
   
}

bool DPrimBaseData::replaceGrp(const std::string& name, const DPIdList& values) {
    return dommy::group::replaceGroupValues(mGrooupList, name, values, numberOfPoints());
}

void DPrimBaseData::removeGrp(const std::string& name) {
    
    removeFromGroup(mGrooupList, name);
	
}

void DPrimBaseData::removeGrpAll() {
    mGrooupList.clear();
}

DPGroupData::ConstPtr DPrimBaseData::getGrp(const std::string& name) const {
    return ExtractGroupFromGroupList(mGrooupList, name);
    
}

size_t DPrimBaseData::numberOfGroups() const {
	return mGrooupList.size();
}


void DPrimBaseData::deleteGroupFromIds(const DPIdList &ids) {
	
    DeleteFromGroupUseIds(mGrooupList, ids);
	
}

void DPrimBaseData::sortGroupFromIds(const DPIndexList &ids) {
	
    SortFromGroupUseIds(mGrooupList, ids);
    
}

void DPrimBaseData::remapGroupFromIds(const DPIndexList &ids) {
	
    RemapFromGroupUseIds(mGrooupList, ids);
	
}


// -----------------------------------------------------------------------------
// Primitive transforms

bool DPrimBaseData::isIdentity() {
    
    if (m_mat==MMatrix::identity) {
        return true;
    } else {
        return false;
    }
    
}

bool DPrimBaseData::isIdentity() const {
    if (m_mat==MMatrix::identity) {
        return true;
    } else {
        return false;
    }
}

void DPrimBaseData::setIdentity() {
    
    m_mat = MMatrix::identity;
    
}

MMatrix DPrimBaseData::getTransformMatrix() const {
    return m_mat;
}

void DPrimBaseData::setTransformMatrix(const MMatrix &mat) {
    m_mat = mat;
}




// Bounding box
MBoundingBox DPrimBaseData::getBBox() const {
    const dommy::math::Bounds3d bounds = worldBounds(pointArray, m_mat);
    if (bounds.empty()) return MBoundingBox();
    return MBoundingBox(MPoint(bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)),
                        MPoint(bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)));
}

MVector DPrimBaseData::getCenterDouble() const {
    // The legacy center is the vertex mean, not the box midpoint. Validate all
    // transformed points, then exploit affine linearity to avoid huge world sums.
    if (worldBounds(pointArray, m_mat).empty()) return MVector(0.0, 0.0, 0.0);
    std::array<double, 3> local, transformed;
    if (!dommy::numeric::checkedPointMean(pointArray, local) ||
        !dommy::numeric::checkedTransformPoint(local, m_mat, transformed))
        throw std::invalid_argument("DPrimBaseData: invalid transformed point mean");
    return MVector(transformed[0], transformed[1], transformed[2]);
}

DPoint DPrimBaseData::getCenter() const {
    const MVector center = getCenterDouble();
    float values[3];
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!dommy::numeric::checkedFloatCast(center[axis], values[axis]))
            throw std::invalid_argument("DPrimBaseData: point mean exceeds float storage");
    return DPoint(values[0], values[1], values[2]);
}

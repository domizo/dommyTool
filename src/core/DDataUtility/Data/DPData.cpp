//
//  DPData.cpp
//  dommyTool
//
//  Created by yuzo on 2015/04/10.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPData.h"

#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <maya/MArgList.h>

#include "DPAttributeCommon.h"
#include "DGroupCommon.h"

#include "DPointCommon.h"

#include "dpoint_ids.h"
#include "DBBoxOp.h"
#include "DStringOp.h"

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DStatusOperation.h"

#include <iomanip>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

// Ascii file IO defines
//
#define kDblQteChar             "\""
#define kSpaceChar              "   "
#define kWrapString             "\n\t\t"
#define kDPointKeyword          "p"




// DPData

const MTypeId DPData::id( ID_DP_DATA );
const MString DPData::typeName( "DPointData" );

DPData::DPData()
{

	mBBox = MBoundingBox();
	mBBoxDirty = false;
}


DPData::~DPData() = default;


/* override */
MStatus DPData::readASCII( const MArgList& argList, unsigned& index )
{
    try {
        return readPointArrayASCII(argList, index);
    } catch (...) {
        return MS::kFailure;
    }
}
/* override */
MStatus DPData::readBinary( istream& /*in*/, unsigned /*length*/ )
{
    // DPData has no historical binary representation. Reporting success here
    // made Maya accept an empty payload and silently discard the data.
    return MS::kFailure;
}
/* override */
MStatus DPData::writeASCII( ostream& out )
{
    try {
        // The legacy ASCII contract stores positions only. Never claim that a
        // partial representation preserved attributes or groups.
        if (!mAttributeList.empty() || !mGrooupList.empty()) {
            return MS::kFailure;
        }
        return writePointArrayASCII(out);
    } catch (...) {
        return MS::kFailure;
    }
}
/* override */
MStatus DPData::writeBinary( ostream& /*out*/ )
{
    return MS::kFailure;
}
/* override */
void DPData::copy ( const MPxData& other )
{
	if (other.typeId() != DPData::id || this == &other) return;
    try {
        setDPData(static_cast<const DPData&>(other));
    } catch (const std::exception& error) {
        MGlobal::displayError(
            MString("DPData::copy failed: ") + error.what());
    } catch (...) {
        MGlobal::displayError("DPData::copy failed.");
    }
}
/* override */
MTypeId DPData::typeId() const
{
    return DPData::id;
}
/* override */
MString DPData::name() const
{
    return DPData::typeName;
}
void * DPData::creator()
{
    return ::dommy::maya::createObject("DPData", [&]() {
        return new DPData;
    });
}


MStatus DPData::readPointArrayASCII( const MArgList& argList, unsigned& index )
{
    MStatus result = MS::kFailure;
    MString geomStr;
    int vertexCount = 0;
    unsigned decodedIndex = index;
    result = argList.get(decodedIndex, geomStr);
    if (!result || geomStr != kDPointKeyword ||
        decodedIndex == std::numeric_limits<unsigned>::max()) {
        return MS::kFailure;
    }

    result = argList.get(++decodedIndex, vertexCount);
    std::size_t checkedCount = 0U;
    if (!result || !dommy::generation::checkedCount(
            vertexCount, dommy::generation::kMaxGeneratedPointCount,
            checkedCount)) {
        return MS::kFailure;
    }

    DPointList decodedPoints;
    decodedPoints.reserve(checkedCount);
    for (std::size_t pointIndex = 0; pointIndex < checkedCount; ++pointIndex) {
        if (decodedIndex == std::numeric_limits<unsigned>::max()) {
            return MS::kFailure;
        }
        MPoint vertex;
        result = argList.get(++decodedIndex, vertex);
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (!result ||
            !dommy::numeric::checkedFloatCast(vertex.x, x) ||
            !dommy::numeric::checkedFloatCast(vertex.y, y) ||
            !dommy::numeric::checkedFloatCast(vertex.z, z)) {
            return MS::kFailure;
        }
        decodedPoints.push_back(DPoint(x, y, z));
    }

    pointArray.swap(decodedPoints);
    mAttributeList.clear();
    mGrooupList.clear();
    updateBBox();
    index = decodedIndex;
    return MS::kSuccess;
}

MStatus DPData::writePointArrayASCII( ostream& out )
{
    if (pointArray.size() > dommy::generation::kMaxGeneratedPointCount ||
        pointArray.size() > static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return MS::kFailure;
    }
    for (DPointList::const_iterator it = pointArray.begin();
         it != pointArray.end(); ++it) {
        if (!dommy::numeric::canRepresentAsFloat((*it)[0]) ||
            !dommy::numeric::canRepresentAsFloat((*it)[1]) ||
            !dommy::numeric::canRepresentAsFloat((*it)[2])) {
            return MS::kFailure;
        }
    }

    // Build the complete payload before touching Maya's stream. This keeps a
    // formatting or allocation failure from leaving a partially written value.
    std::ostringstream encoded;
    encoded.imbue(std::locale::classic());
    encoded << std::setprecision(std::numeric_limits<float>::max_digits10);
    encoded << "\n";
    encoded << kWrapString;
    encoded << kDblQteChar << kDPointKeyword << kDblQteChar
            << kSpaceChar << static_cast<int>(pointArray.size());
    for (DPointList::const_iterator vertex = pointArray.begin();
         vertex != pointArray.end(); ++vertex) {
        encoded << kWrapString;
        encoded << (*vertex)[0] << kSpaceChar;
        encoded << (*vertex)[1] << kSpaceChar;
        encoded << (*vertex)[2];
    }
    if (!encoded.good()) {
        return MS::kFailure;
    }
    out << encoded.str();
    return out.good() ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}



void DPData::setDPData(const DPData& data) {
    if (this == &data) return;
    if (!data.hasValidPointTopology()) {
        throw std::invalid_argument(
            "DPData::setDPData rejected invalid point data");
    }
    dommy::point::PointMergePlan copyPlan;
    if (!dommy::point::buildPointCopyPlan(data, 1U, copyPlan)) {
        throw std::length_error(
            "DPData::setDPData rejected oversized point data");
    }

    DPData candidate;
    candidate.pointArray = data.pointArray;
    CopyAttributeToAttribute(
        candidate.mAttributeList, data.mAttributeList);
    CopyGroupToGroup(candidate.mGrooupList, data.mGrooupList);
    if (!candidate.hasValidPointTopology()) {
        throw std::runtime_error(
            "DPData::setDPData could not build a valid copy");
    }
    dommy::point::PointMergePlan finalPlan;
    if (!dommy::point::buildPointCopyPlan(candidate, 1U, finalPlan)) {
        throw std::length_error(
            "DPData::setDPData rejected oversized cloned point data");
    }
    candidate.updateBBox();

    pointArray.swap(candidate.pointArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGrooupList.swap(candidate.mGrooupList);
    mBBox = candidate.mBBox;
	mBBoxDirty = false;
}



bool DPData::appendMetadataFrom(const DPData& source, std::size_t destinationPointCount) {
    const std::size_t sourcePointCount = source.numberOfPoints();
    for (const auto& field : mAttributeList) {
        const auto found = source.mAttributeList.find(field.first);
        if (found == source.mAttributeList.end()) field.second->incrementDefaultValue(sourcePointCount);
        else if (!appendOtherDPAttribute(field.second, found->second)) return false;
    }
    for (const auto& field : source.mAttributeList) {
        if (mAttributeList.count(field.first) != 0U) continue;
        DPAttributeData::Ptr copied = field.second->copy();
        dommy::attribute::ValidationBudget cost;
        if (!copied || copied.get() == field.second.get() ||
            copied->typeName() != field.second->typeName() ||
            !dommy::attribute::validateAttribute(copied, sourcePointCount, cost)) return false;
        copied->insertDefaultValue(destinationPointCount, 0U);
        mAttributeList.emplace(field.first, copied);
    }
    for (const auto& group : mGrooupList) {
        const auto found = source.mGrooupList.find(group.first);
        if (found == source.mGrooupList.end()) group.second->expandOtherGrp(sourcePointCount);
        else group.second->appendOtherGrp(found->second);
    }
    for (const auto& group : source.mGrooupList) {
        if (mGrooupList.count(group.first) != 0U) continue;
        DPGroupData::Ptr copied = group.second->copy();
        if (!copied || copied.get() == group.second.get() || copied->getGroupIds().size() != sourcePointCount)
            return false;
        copied->insertOtherGrp(destinationPointCount, 0U, false);
        mGrooupList.emplace(group.first, copied);
    }
    return true;
}

MStatus DPData::mergeFrom(const std::vector<const DPData*>& sources) try {
    dommy::point::PointMergePlan plan;
    if (!dommy::point::buildPointMergePlan(sources, plan)) return MS::kInvalidParameter;
    for (const DPData* source : sources)
        if (!source->hasValidPointTopology()) return MS::kInvalidParameter;
    DPData candidate;
    candidate.pointArray.reserve(plan.points);
    for (std::size_t index = 0U; index < sources.size(); ++index) {
        const DPData& source = *sources[index];
        // Preserve the first empty schema; later empty sources are validated
        // above but, as in mergeDPData, do not add fields or groups.
        if (index != 0U && source.pointArray.empty()) continue;
        const std::size_t offset = candidate.pointArray.size();
        candidate.pointArray.insert(candidate.pointArray.end(), source.pointArray.begin(), source.pointArray.end());
        if (!candidate.appendMetadataFrom(source, offset)) return MS::kFailure;
    }
    if (!candidate.hasValidPointTopology()) return MS::kFailure;
    dommy::point::PointMergePlan finalPlan;
    if (!dommy::point::buildPointCopyPlan(candidate, 1U, finalPlan)) return MS::kInvalidParameter;
    candidate.updateBBox();
    pointArray.swap(candidate.pointArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGrooupList.swap(candidate.mGrooupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) { return MS::kFailure; }

MStatus DPData::copyRepeatedFrom(const DPData& source, std::size_t copies,
                               const PointCopyOperation& operation) try {
    dommy::point::PointMergePlan plan;
    if (!dommy::point::buildPointCopyPlan(source, copies, plan) ||
        !source.hasValidPointTopology()) return MS::kInvalidParameter;
    DPData candidate;
    candidate.pointArray.reserve(plan.points);
    // Empty repeated copies retain one schema without copy-count-sized loops.
    if (source.pointArray.empty()) {
        if (!candidate.appendMetadataFrom(source, 0U)) return MS::kFailure;
    } else {
        for (std::size_t copyIndex = 0U; copyIndex < copies; ++copyIndex) {
            const std::size_t offset = candidate.pointArray.size();
            for (std::size_t pointIndex = 0U; pointIndex < source.pointArray.size(); ++pointIndex) {
                DPoint point = source.pointArray[pointIndex];
                if (operation) DOMMY_RETURN_IF_FAILURE(operation(point, copyIndex, pointIndex));
                if (!dommy::attribute::isValidValue(point)) return MS::kInvalidParameter;
                candidate.pointArray.push_back(point);
            }
            if (!candidate.appendMetadataFrom(source, offset)) return MS::kFailure;
        }
    }
    if (!candidate.hasValidPointTopology()) return MS::kFailure;
    dommy::point::PointMergePlan finalPlan;
    if (!dommy::point::buildPointCopyPlan(candidate, 1U, finalPlan)) return MS::kInvalidParameter;
    candidate.updateBBox();
    pointArray.swap(candidate.pointArray);
    mAttributeList.swap(candidate.mAttributeList);
    mGrooupList.swap(candidate.mGrooupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) { return MS::kFailure; }

bool DPData::mergeDPData(const DPData& data) {
    if (!hasValidPointTopology() || !data.hasValidPointTopology()) {
        MGlobal::displayError(
            "DPData::mergeDPData rejected invalid point data.");
        return false;
    }
    if (data.pointArray.empty()) return true;

    std::size_t mergedPointCount = 0U;
    if (!dommy::generation::checkedSum(
            pointArray.size(), data.pointArray.size(),
            dommy::generation::kMaxGeneratedPointCount,
            mergedPointCount)) {
        MGlobal::displayError(
            "DPData::mergeDPData exceeds the point generation budget.");
        return false;
    }

    try {
        const std::size_t destinationPointCount = pointArray.size();

        DPData candidate;
        candidate.setDPData(*this);
        candidate.pointArray.reserve(mergedPointCount);
        candidate.pointArray.insert(
            candidate.pointArray.end(),
            data.pointArray.begin(), data.pointArray.end());

        if (!candidate.appendMetadataFrom(data, destinationPointCount)) {
            MGlobal::displayError("DPData::mergeDPData could not append compatible metadata.");
            return false;
        }

        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) {
            MGlobal::displayError(
                "DPData::mergeDPData produced invalid point data.");
            return false;
        }

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (const std::exception& error) {
        MGlobal::displayError(
            MString("DPData::mergeDPData failed without changing the "
                    "destination: ") + error.what());
    } catch (...) {
        MGlobal::displayError(
            "DPData::mergeDPData failed without changing the destination.");
    }
    return false;
}


/*全てのデータのクリア*/
void DPData::cleanDataAll() {
    pointArray.clear();
    mAttributeList.clear();
    mGrooupList.clear();
    
    mBBox = MBoundingBox();
	mBBoxDirty = false;
}






// position method
DPointList DPData::getPosition() {
    return pointArray;
}



DPointList DPData::getPositionConst() const {
	return pointArray;
}

// Point conversion
MPointArray DPData::getMPosition() const {
    MPointArray points;
    if (dommy::geometry::copyPointsToMayaArray(pointArray, points) != MS::kSuccess)
        throw std::runtime_error("DPData: unable to export point array");
    return points;
}



bool DPData::addPoints(const DPointList& points) {
	if (!hasValidPointTopology() || points.empty()) {
		return points.empty();
	}

	std::size_t targetPointCount = 0U;
	if (!dommy::generation::checkedSum(
			pointArray.size(),
			points.size(),
			dommy::generation::kMaxGeneratedPointCount,
			targetPointCount)) {
		return false;
	}

	try {
		DPData candidate;
		candidate.setDPData(*this);
		candidate.pointArray.reserve(targetPointCount);
		candidate.pointArray.insert(
			candidate.pointArray.end(),
			points.begin(),
			points.end());
		ExpandAttributeSize(
			candidate.mAttributeList, points.size());
		ExpandGroupListSize(
			candidate.mGrooupList, points.size());
		candidate.updateBBox();
		if (!candidate.hasValidPointTopology()) {
			return false;
		}

		pointArray.swap(candidate.pointArray);
		mAttributeList.swap(candidate.mAttributeList);
		mGrooupList.swap(candidate.mGrooupList);
		mBBox = candidate.mBBox;
		mBBoxDirty = false;
		return true;
	} catch (...) {
		return false;
	}
}




void DPData::setPosition(const MPointArray &pts) {
    dommy::geometry::replacePointArray(pts, pts.length(), pointArray);
    markBBoxDirty();
}

void DPData::setPosition(const MFloatPointArray &pts) {
    dommy::geometry::replacePointArray(pts, pts.length(), pointArray);
    markBBoxDirty();
}

void DPData::setPosition(const DPointList &pts) {
    dommy::geometry::replacePointArray(pts, pts.size(), pointArray);
    markBBoxDirty();
}

void DPData::setPosition(const DPoint &pt, size_t index) {
    if(index<this->pointArray.size()) {
        if (!dommy::attribute::isValidValue(pt))
            throw std::invalid_argument("DPData: invalid point coordinate");
        this->pointArray[index] = pt;
        markBBoxDirty();
	}
}

bool DPData::adoptPositions(DPointList&& points) {
    try {
        DPData candidate;
        candidate.pointArray.swap(points);
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) {
            return false;
        }

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
        mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool DPData::replacePositions(const DPointList& points) {
    if (!hasValidPointTopology() ||
        points.size() != pointArray.size()) {
        return false;
    }
    try {
        DPData candidate;
        candidate.pointArray = points;
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) {
            return false;
        }

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
        mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool DPData::hasValidPointTopology() const {
    const std::size_t pointCount = pointArray.size();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }

    dommy::attribute::ValidationBudget budget(pointCount);
    for (std::size_t index = 0U; index < pointCount; ++index) {
        if (!dommy::numeric::canRepresentAsFloat(pointArray[index][0]) ||
            !dommy::numeric::canRepresentAsFloat(pointArray[index][1]) ||
            !dommy::numeric::canRepresentAsFloat(pointArray[index][2])) {
            return false;
        }
    }

    if (!dommy::attribute::validateAttributeList(
            mAttributeList, pointCount, budget)) {
        return false;
    }

    for (ConstDPGrpIter iter = mGrooupList.begin();
         iter != mGrooupList.end(); ++iter) {
        if (!iter->second ||
            iter->second->getGroupIds().size() != pointCount ||
            !dommy::generation::checkedSum(
                budget.workItems, pointCount,
                dommy::generation::kMaxWorkItemCount,
                budget.workItems)) {
            return false;
        }
    }

    return true;
}
bool DPData::deletePointFromIds(const DPIdList& ids) {
    if (ids.size() != numberOfPoints() || !hasValidPointTopology()) {
        return false;
    }

    try {
        DPData candidate;
        candidate.pointArray.reserve(pointArray.size());
        for (std::size_t index = 0U; index < pointArray.size(); ++index) {
            if (!ids[index]) {
                candidate.pointArray.push_back(pointArray[index]);
            }
        }
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        if (!DDeleteAttributeFromIds(
                candidate.mAttributeList, ids)) {
            return false;
        }
        DeleteFromGroupUseIds(candidate.mGrooupList, ids);
        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) return false;

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool DPData::sortPointFromIds(const DPIndexList& ids) {
    if (!hasValidPointTopology() ||
        !dommy::container::isValidPermutation(ids, pointArray.size())) {
        return false;
    }

    try {
        DPData candidate;
        candidate.pointArray = pointArray;
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        if (!dommy::container::reorderInPlace(
                candidate.pointArray, ids)) {
            return false;
        }
        if (!DSortAttributeFromIds(
                candidate.mAttributeList, ids)) {
            return false;
        }
        SortFromGroupUseIds(candidate.mGrooupList, ids);
        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) return false;

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool DPData::remapPointFromIds(const DPIndexList& ids) {
    if (!hasValidPointTopology() ||
        !dommy::container::isValidIndexSelection(
            ids, pointArray.size())) {
        return false;
    }

    try {
        DPData candidate;
        candidate.pointArray = pointArray;
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
        if (!dommy::container::remapInPlace(
                candidate.pointArray, ids)) {
            return false;
        }
        if (!DRemapAtttributeFromIds(
                candidate.mAttributeList, ids)) {
            return false;
        }
        RemapFromGroupUseIds(candidate.mGrooupList, ids);
        candidate.updateBBox();
        if (!candidate.hasValidPointTopology()) return false;

        pointArray.swap(candidate.pointArray);
        mAttributeList.swap(candidate.mAttributeList);
        mGrooupList.swap(candidate.mGrooupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}



// attribute method
bool DPData::isExistAttr(const std::string& name) const {
    
    return IsExistInAttributeList(mAttributeList, name);
    
}


void DPData::addAttr(const std::string& name, const DPAttributeData& attr)
{
    AddAttributeList(mAttributeList, name, attr);
	
}




// add attribute with values
template <typename T>
void DPData::addAttrWithValues(
	const std::string& name,
	const std::vector<T>& values)
{
	(void)name;
	(void)values;
	static_assert(
		!std::is_same<T, T>::value,
		"DPData::addAttrWithValues has no specialization for this type");
}


template<>
void DPData::addAttrWithValues(const std::string& name, const std::vector<bool> &values) {
    addAttr(name, DPBoolAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<int32_t> &values) {
    addAttr(name, DPIntAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<uint32_t> &values) {
    addAttr(name, DPUInt32Attr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<int64_t> &values) {
    addAttr(name, DPInt64Attr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<float> &values) {
    addAttr(name, DPFloatAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<double> &values) {
    addAttr(name, DPDoubleAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2i> &values) {
    addAttr(name, DPVec2iAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2f> &values) {
    addAttr(name, DPVec2fAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2d> &values) {
    addAttr(name, DPVec2dAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3i> &values) {
    addAttr(name, DPVec3iAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3f> &values) {
    addAttr(name, DPVec3fAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3d> &values) {
    addAttr(name, DPVec3dAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<std::string> &values) {
    addAttr(name, DPStringAttr(values));
}

template <>
void DPData::addAttrWithValues(const std::string& name, const std::vector<MMatrix> &values) {
    addAttr(name, DPMat4Attr(values));
}






void DPData::removeAttr(const std::string& name) {
    RemoveFromAttributeList(mAttributeList, name);
	
}


DPAttributeData::ConstPtr DPData::getAttr(const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
    
}

DPAttributeData::Ptr DPData::getAttrPtr(const std::string& name) {
    return GetAttrPtr(mAttributeList, name);
}

DPAttributeData::ConstPtr DPData::getAttrPtr(
        const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
}

template <typename S, typename T>
void DPData::setAttrWithValues(const std::string& name, const T &values) {
    if (!replaceConvertedAttributeValues<S>(*this, name, values)) {
		throw std::invalid_argument(
			"DPData::setAttrWithValues requires a matching typed attribute");
    }
}




void DPData::setDPAttributeValues(const std::string& name, const DScalarDataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isSingleRealAttrType(type)) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues requires a matching scalar attribute");
    }

    switch (type) {
        case DP_ATTR_INT:
            setAttrWithValues<int32_t>(name, values);
            break;
        case DP_ATTR_UINT32:
            setAttrWithValues<uint32_t>(name, values);
            break;
        case DP_ATTR_INT64:
            setAttrWithValues<int64_t>(name, values);
            break;
        case DP_ATTR_FLOAT:
            setAttrWithValues<float>(name, values);
            break;
        case DP_ATTR_DOUBLE:
            setAttrWithValues<double>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPData::setDPAttributeValues received an unsupported scalar attribute");
    }
}

void DPData::setDPAttributeValues(const std::string& name, const DVec2DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isVector2DAttrType(type)) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues requires a matching vec2 attribute");
    }

    switch (type) {
        case DP_ATTR_VEC2I:
            setAttrWithValues<Imath::V2i>(name, values);
            break;
        case DP_ATTR_VEC2F:
            setAttrWithValues<Imath::V2f>(name, values);
            break;
        case DP_ATTR_VEC2D:
            setAttrWithValues<Imath::V2d>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPData::setDPAttributeValues received an unsupported vec2 attribute");
    }
}

void DPData::setDPAttributeValues(const std::string& name, const DVec3DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues cannot find the attribute");
    }

    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
    if (!isVector3DAttrType(type)) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues requires a matching vec3 attribute");
    }

    switch (type) {
        case DP_ATTR_VEC3I:
            setAttrWithValues<Imath::V3i>(name, values);
            break;
        case DP_ATTR_VEC3F:
            setAttrWithValues<Imath::V3f>(name, values);
            break;
        case DP_ATTR_VEC3D:
            setAttrWithValues<Imath::V3d>(name, values);
            break;
        default:
            throw std::invalid_argument(
                "DPData::setDPAttributeValues received an unsupported vec3 attribute");
    }
}



void DPData::setDPAttributeValues(const std::string& name, const MFloatPointArray &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues cannot find the attribute");
    }
    if (!isVector3DAttrType(attribute->getAttrType())) {
        throw std::invalid_argument(
            "DPData::setDPAttributeValues requires a matching vec3 attribute");
    }

    const size_t count = values.length();
    DVec3DataList dataarray(count);
    for (size_t i = 0; i < count; ++i) {
        dataarray[i][0] = values[i][0];
        dataarray[i][1] = values[i][1];
        dataarray[i][2] = values[i][2];
    }

    setDPAttributeValues(name, dataarray);
}


size_t DPData::numberOfAttributes() const {
	
	return mAttributeList.size();
}


void DPData::extendAttributeFromCount(const size_t &count) {
	//attribute add default value
    ExpandAttributeSize(mAttributeList, count);
	
}


// attribute name method
DStringList DPData::getAttributeNameList() const {
    DStringList attributeNames;
    for(auto itr:mAttributeList) {
        attributeNames.push_back(itr.first);
    }
    return attributeNames;
}



DStringList DPData::getAllAttributeList() const {
    
    DStringList allCompAttrList;
    
    DStringList pattrList = getAttributeNameList();
    IntersectDStringList(allCompAttrList, pattrList);
    
    return allCompAttrList;
}

DComponentType DPData::getAttributeComponentType(const std::string& name) const {
    
    if (numberOfPoints()==0)
        return D_COMP_COUNT;
    
    //primitive
    if(isContainDStringList(getAttributeNameList(), name)) {
        return D_COMP_POINT;
    }
    
    return D_COMP_COUNT;
}


// group method

bool DPData::isExistGrp(const std::string& name) const {
    
    return IsExistInGroupList(mGrooupList, name);
    
}

void DPData::addGrp(const std::string& name, const DPGroupData& attr) {
	
    addGroupList(mGrooupList, name, attr);
	
}

bool DPData::replaceGrp(const std::string& name, const DPIdList& values) {
    return dommy::group::replaceGroupValues(mGrooupList, name, values, numberOfPoints());
}

void DPData::removeGrp(const std::string& name) {
    
    removeFromGroup(mGrooupList, name);
	
}

void DPData::removeGrps(const MString nameList) {
    
    removeFromGroups(mGrooupList, nameList);
    
}



DPGroupData::Ptr DPData::getGrp(const std::string& name) {
    return GetGrpPtr(mGrooupList, name);
}

DPGroupData::ConstPtr DPData::getGrp(const std::string& name) const {
    return ExtractGroupFromGroupList(mGrooupList, name);
}

size_t DPData::numberOfGroups() const {
	return mGrooupList.size();
}


void DPData::deleteGroupFromIds(const DPIdList &ids) {
    DeleteFromGroupUseIds(mGrooupList, ids);
	
}

void DPData::sortGroupFromIds(const DPIndexList &ids) {
    SortFromGroupUseIds(mGrooupList, ids);
}

void DPData::extendGroupFromCount(const size_t &count) {
    
    ExpandGroupListSize(mGrooupList, count);
}


// group name method

DStringList DPData::getGroupNameList() const {
    DStringList groupNames;
    for(auto iter : mGrooupList) {
        groupNames.push_back(iter.first);
    }
    return groupNames;
}

DStringList DPData::getAllGroupList() const {
    DStringList allCompGrpList;
    
    DStringList pgrpList = getGroupNameList();
    
    IntersectDStringList(allCompGrpList, pgrpList);
    
    return allCompGrpList;
}

DComponentType DPData::getGroupComponentType(const std::string& name) const {
    
    if (numberOfPoints()==0)
        return D_COMP_COUNT;
    
    //point only
    if(isContainDStringList(getGroupNameList(), name)) {
        return D_COMP_POINT;
    }
    
    return D_COMP_COUNT;
}






// bbox func
MBoundingBox DPData::calculateBoundingBox() const {
    MBoundingBox bounds;
    if (numberOfPoints()) {
        CalBBoxFromPointArray(pointArray, bounds);
    }
    return bounds;
}

void DPData::updateBBox() {
    mBBox = calculateBoundingBox();
	mBBoxDirty = false;
}

void DPData::getBBox(MBoundingBox &bbox) const {
    bbox = mBBoxDirty ? calculateBoundingBox() : mBBox;
}

MBoundingBox DPData::getBoundingBox() const {
	return mBBoxDirty ? calculateBoundingBox() : mBBox;
}

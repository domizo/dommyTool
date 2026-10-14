//
//  DMultiCurveData.cpp
//  dommyTool
//
//  Created by yuzo on 2016/05/01.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCurveData.h"

#include "DMayaHeaders.h"

#include "DPAttributeCommon.h"
#include "DCurveUtil.h"

#include "dcurve_ids.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "DGroupCommon.h"
#include "DStringOp.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DCurveOperationBudget.h"
#include "common/utility/DStatusOperation.h"

// DCurveInternalData



namespace {

int normalizedCurveDegree(std::size_t pointCount, int requestedDegree)
{
	return requestedDegree > 0 &&
		pointCount > static_cast<std::size_t>(requestedDegree)
		? requestedDegree : 1;
}

bool hasValidSourceIndices(
	const DPIndexList& indices,
	std::size_t sourcePointCount,
	std::size_t replacementPointCount)
{
	if (indices.size() != replacementPointCount) {
		return false;
	}
	for (const std::size_t index : indices) {
		if (index >= sourcePointCount) {
			return false;
		}
	}
	return true;
}

} // namespace

bool DCurveInternalData::appendOperationCost(dommy::curve::CurveBatchBudget& budget) const try {
    if (!hasValidCurveTopology()) return false;
    dommy::attribute::ValidationBudget components;
    if (!dommy::geometry::addCopyMetadataCost(*this, numberOfPoints(), components)) return false;
    return budget.append(numberOfPoints(), numberOfKnots(), components.workItems, components.stringBytes);
} catch (...) {
    return false;
}

DCurveInternalData::DCurveInternalData(): DPrimBaseData() {
    degree = 1;
    mIsOpen = true;
}

MStatus DCurveInternalData::createFromMayaObject(
    const MObject& object,
    Ptr& result)
try {
    MStatus status;
    MFnNurbsCurve curveFunction(object, &status);
    if (!status) return status;

    MPointArray points;
    // Preserve local CVs; primitive transforms are managed separately.
    status = curveFunction.getCVs(points, MSpace::kObject);
    if (!status) return status;

    MDoubleArray curveKnots;
    status = curveFunction.getKnots(curveKnots);
    if (!status) return status;

    const int curveDegree = curveFunction.degree(&status);
    if (!status) return status;
    const MFnNurbsCurve::Form form = curveFunction.form(&status);
    if (!status) return status;

    Ptr candidate(new DCurveInternalData(
        points, curveKnots, curveDegree, form == MFnNurbsCurve::kOpen));
    if (!candidate->hasValidCurveTopology()) return MS::kInvalidParameter;

    result.swap(candidate);
    return MS::kSuccess;
} catch (const std::invalid_argument&) {
    return MS::kInvalidParameter;
} catch (...) {
    return MS::kFailure;
}

DCurveInternalData::DCurveInternalData(DCurveInternalData const &src): DPrimBaseData(src) {
	
	
	//set other param
    if (dommy::maya::copyCheckedArray(src.arrays_->knots, arrays_->knots) != MS::kSuccess) {
        throw std::runtime_error("Unable to copy curve knots.");
    }
	degree = src.degree;
	
    mIsOpen = src.mIsOpen;
    m_mat = src.getTransformMatrix();
    
}

DCurveInternalData::DCurveInternalData(const DPointList &c, int d, bool isOpen): DPrimBaseData() {
    setPosition(c);
	d = normalizedCurveDegree(c.size(), d);
    
    GenerateDCurveKnots(c.size(),d, arrays_->knots);
    
    degree = d;
    
    mIsOpen = isOpen;
    m_mat = MMatrix();
}

DCurveInternalData::DCurveInternalData(const MPointArray &c, int d, bool isOpen): DPrimBaseData() {
    setPosition(c);
	d = normalizedCurveDegree(static_cast<std::size_t>(c.length()), d);
    
    GenerateDCurveKnots(c.length(),d, arrays_->knots);
    
    degree = d;
    
    mIsOpen = isOpen;
    m_mat = MMatrix();
}

DCurveInternalData::DCurveInternalData(const DPointList &c, const MDoubleArray &k, int d, bool isOpen): DPrimBaseData() {
    setPosition(c);
	const int requestedDegree = d;
	d = normalizedCurveDegree(c.size(), d);
	if (d == requestedDegree) {
        if (dommy::maya::copyCheckedArray(k, arrays_->knots) != MS::kSuccess)
            throw std::runtime_error("Unable to copy curve knots.");
	} else {
		GenerateDCurveKnots(c.size(), d, arrays_->knots);
	}
    degree = d;
    
    mIsOpen = isOpen;
    m_mat = MMatrix();
    
}


DCurveInternalData::DCurveInternalData(const MPointArray &c, const MDoubleArray &k, int d, bool isOpen): DPrimBaseData() {
	

	setPosition(c);
	const int requestedDegree = d;
	d = normalizedCurveDegree(static_cast<std::size_t>(c.length()), d);
	if (d == requestedDegree) {
        if (dommy::maya::copyCheckedArray(k, arrays_->knots) != MS::kSuccess)
            throw std::runtime_error("Unable to copy curve knots.");
	} else {
		GenerateDCurveKnots(c.length(), d, arrays_->knots);
	}
	degree = d;
	
    mIsOpen = isOpen;
	m_mat = MMatrix();
    
	
	
}


DCurveInternalData::~DCurveInternalData() = default;

bool DCurveInternalData::hasValidCurveTopology() const {
    const std::size_t pointCount = numberOfPoints();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount ||
        degree <= 0 || pointCount <= static_cast<std::size_t>(degree) ||
		!dommy::attribute::isFiniteMatrix(m_mat)) {
        return false;
    }

    std::size_t expectedKnotCount = 0;
    if (!dommy::memory::checkedAdd(
			pointCount, static_cast<std::size_t>(degree - 1), expectedKnotCount) ||
		expectedKnotCount != static_cast<std::size_t>(arrays_->knots.length())) {
		return false;
	}

	for (ConstDPointIter point = beginPt(); point != endPt(); ++point) {
		if (!std::isfinite((*point)[0]) || !std::isfinite((*point)[1]) ||
			!std::isfinite((*point)[2])) {
			return false;
		}
	}

	double previousKnot = 0.0;
	for (unsigned int index = 0U; index < arrays_->knots.length(); ++index) {
		const double knot = arrays_->knots[index];
		if (!std::isfinite(knot) || (index > 0U && knot < previousKnot)) {
			return false;
		}
		previousKnot = knot;
	}

    dommy::attribute::ValidationBudget budget(pointCount);
	if (!dommy::attribute::validateAttributeList(
			mAttributeList, pointCount, budget)) {
		return false;
    }
    for (ConstDPGrpIter group = mGrooupList.begin();
         group != mGrooupList.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != pointCount ||
            !dommy::generation::checkedSum(
                budget.workItems, pointCount,
                dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }
	return true;
}

bool DCurveInternalData::replacePositions(
	const DPointList& points)
{
	if (!hasValidCurveTopology() ||
		points.size() != numberOfPoints()) {
		return false;
	}
	try {
		DCurveInternalData candidate(*this);
		candidate.pointArray = points;
		(void)candidate.getBBox();
		if (!candidate.hasValidCurveTopology()) {
			return false;
		}
		swapState(candidate);
		return true;
	} catch (...) {
		return false;
	}
}

bool DCurveInternalData::replacePositions(
	const MPointArray& points)
{
	DPointList convertedPoints;
	try {
		dommy::geometry::replacePointArray(points, points.length(), convertedPoints);
	} catch (...) {
		return false;
	}
	return replacePositions(convertedPoints);
}

bool DCurveInternalData::replaceKnots(
	const MDoubleArray& replacementKnots)
{
	if (!hasValidCurveTopology()) {
		return false;
	}
	try {
		DCurveInternalData candidate(*this);
		if (dommy::maya::copyCheckedArray(
				replacementKnots, candidate.arrays_->knots) != MS::kSuccess) {
			return false;
		}
		if (!candidate.hasValidCurveTopology() ||
			candidate.arrays_->knots.length() != arrays_->knots.length()) {
			return false;
		}
		// Only knot values change. Equal-length indexed writes cannot allocate
		// and leave the existing point, metadata, and transform storage untouched.
		for (unsigned int index = 0U; index < arrays_->knots.length(); ++index) {
			arrays_->knots[index] = candidate.arrays_->knots[index];
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool DCurveInternalData::setReplaceCurveData(
	const DPointList& points,
	const MDoubleArray& replacementKnots,
	int replacementDegree,
	bool isOpen,
	DCurveReplacementPolicy policy)
{
	if (points.size() >
			dommy::generation::kMaxGeneratedPointCount ||
		replacementDegree <= 0 ||
		points.size() <=
			static_cast<std::size_t>(replacementDegree)) {
		return false;
	}

	try {
		DCurveInternalData candidate(
			points, replacementKnots, replacementDegree, isOpen);
		candidate.m_mat = m_mat;

		if (policy ==
			DCurveReplacementPolicy::RequireCompatibleMetadata) {
			CopyAttributeToAttribute(
				candidate.mAttributeList, mAttributeList);
			CopyGroupToGroup(
				candidate.mGrooupList, mGrooupList);
		}

		(void)candidate.getBBox();
		if (!candidate.hasValidCurveTopology()) {
			return false;
		}
		swapState(candidate);
		return true;
	} catch (...) {
		return false;
	}
}

bool DCurveInternalData::setReplaceCurveData(
	const MPointArray& points,
	const MDoubleArray& replacementKnots,
	int replacementDegree,
	bool isOpen,
	DCurveReplacementPolicy policy)
{
	DPointList convertedPoints;
	try {
		dommy::geometry::replacePointArray(points, points.length(), convertedPoints);
	} catch (...) {
		return false;
	}
	return setReplaceCurveData(
		convertedPoints,
		replacementKnots,
		replacementDegree,
		isOpen,
		policy);
}

bool DCurveInternalData::setRemappedCurveData(
	const DPointList& points,
	const MDoubleArray& replacementKnots,
	int replacementDegree,
	bool isOpen,
	const DPIndexList& sourcePointIndices)
{
	if (!hasValidCurveTopology() ||
		points.size() >
			dommy::generation::kMaxGeneratedPointCount ||
		replacementDegree <= 0 ||
		points.size() <=
			static_cast<std::size_t>(replacementDegree) ||
		!hasValidSourceIndices(
			sourcePointIndices, numberOfPoints(), points.size())) {
		return false;
	}

	try {
		DCurveInternalData candidate(
			points, replacementKnots, replacementDegree, isOpen);
		candidate.m_mat = m_mat;
		CopyAttributeToAttribute(
			candidate.mAttributeList, mAttributeList);
		CopyGroupToGroup(candidate.mGrooupList, mGrooupList);
		DRemapAtttributeFromIds(
			candidate.mAttributeList, sourcePointIndices);
		RemapFromGroupUseIds(
			candidate.mGrooupList, sourcePointIndices);

		(void)candidate.getBBox();
		if (!candidate.hasValidCurveTopology()) {
			return false;
		}
		swapState(candidate);
		return true;
	} catch (...) {
		return false;
	}
}

bool DCurveInternalData::setRemappedCurveData(
	const MPointArray& points,
	const MDoubleArray& replacementKnots,
	int replacementDegree,
	bool isOpen,
	const DPIndexList& sourcePointIndices)
{
	DPointList convertedPoints;
	try {
		dommy::geometry::replacePointArray(points, points.length(), convertedPoints);
	} catch (...) {
		return false;
	}
	return setRemappedCurveData(
		convertedPoints,
		replacementKnots,
		replacementDegree,
		isOpen,
		sourcePointIndices);
}

DCurveInternalData::Ptr DCurveInternalData::copy() {
	return Ptr(new DCurveInternalData(*this));
}

DCurveInternalData::CPtr DCurveInternalData::copy() const {
	return CPtr(new DCurveInternalData(*this));
}

DCurveInternalData::Ptr DCurveInternalData::copyNewCurve() const {
	return Ptr(new DCurveInternalData(*this));
}



void DCurveInternalData::copy(const DCurveInternalData &other)
{
    if (this == &other) return;
    if (!other.hasValidCurveTopology()) {
        throw std::invalid_argument(
            "DCurveInternalData::copy rejected invalid curve data");
    }

    DCurveInternalData candidate(other);
    if (!candidate.hasValidCurveTopology()) {
        throw std::runtime_error(
            "DCurveInternalData::copy could not build a valid copy");
    }

    swapState(candidate);
}

DCurveInternalData& DCurveInternalData::operator=(const DCurveInternalData& other)
{
    copy(other);
    return *this;
}

void DCurveInternalData::swapState(DCurveInternalData& other) noexcept
{
    pointArray.swap(other.pointArray);
    mAttributeList.swap(other.mAttributeList);
    mGrooupList.swap(other.mGrooupList);
    arrays_.swap(other.arrays_);
    std::swap(degree, other.degree);
    std::swap(mIsOpen, other.mIsOpen);
    std::swap(m_mat, other.m_mat);
}


bool DCurveInternalData::appendPoint(const DPoint &pt) {
    if (!hasValidCurveTopology()) return false;
    try {
        DCurveInternalData candidate(*this);
        if (!candidate.DPrimBaseData::appendPoint(pt)) {
            return false;
        }
        MDoubleArray updatedKnots;
        GenerateDCurveKnots(
            candidate.pointArray.size(),
            candidate.degree, updatedKnots);
        if (dommy::maya::copyCheckedArray(updatedKnots, candidate.arrays_->knots) != MS::kSuccess)
            return false;
        (void)candidate.getBBox();
        if (!candidate.hasValidCurveTopology()) return false;
        swapState(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool DCurveInternalData::insertPoint(
    const DPoint &pt, const uint &index)
{
    if (!hasValidCurveTopology() ||
        static_cast<std::size_t>(index) > pointArray.size()) {
        return false;
    }
    try {
        DCurveInternalData candidate(*this);
        if (!candidate.DPrimBaseData::insertPoint(pt, index)) {
            return false;
        }
        MDoubleArray updatedKnots;
        GenerateDCurveKnots(
            candidate.pointArray.size(),
            candidate.degree, updatedKnots);
        if (dommy::maya::copyCheckedArray(updatedKnots, candidate.arrays_->knots) != MS::kSuccess)
            return false;
        (void)candidate.getBBox();
        if (!candidate.hasValidCurveTopology()) return false;
        swapState(candidate);
        return true;
    } catch (...) {
        return false;
    }
}


// -----------------------------------------------------------------------------
// Component attributes
const DPAttrList *DCurveInternalData::AttributeListConst(const DComponentType &cType) const{
    switch (cType) {
        case D_COMP_POINT:
            return &mAttributeList;
        case D_COMP_PER_VTX:
        case D_COMP_FACE:
        case D_COMP_PRIM:
        case D_COMP_EDGE:
        default:
            return nullptr;
            
    }
}

DPAttrList *DCurveInternalData::AttributeList(const DComponentType &cType) {
    switch (cType) {
        case D_COMP_POINT:
            return &mAttributeList;
        case D_COMP_PER_VTX:
        case D_COMP_FACE:
        case D_COMP_EDGE:
        default:
            return nullptr;
    }
}

DStringList DCurveInternalData::getAllCompAttrName(const DComponentType &cType) const {
    const DPAttrList* attributes = AttributeListConst(cType);
    return attributes ? GetAllNameAttributeList(*attributes) : DStringList();
}

// Component groups
const DPGrpList *DCurveInternalData::GroupListConst(const DComponentType &cType) const {
    switch (cType) {
        case D_COMP_POINT:
            return &mGrooupList;
        default:
            return nullptr;
    }
}
DPGrpList *DCurveInternalData::GroupList(const DComponentType &cType) {
    switch (cType) {
        case D_COMP_POINT:
            return &mGrooupList;
        default:
            return nullptr;
    }
}


bool DCurveInternalData::isExistCompGrp(const DComponentType &cType, const std::string& name) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups && IsExistInGroupList(*groups, name);
}

bool DCurveInternalData::addCompGrp(const DComponentType &cType, const std::string& name, const DPGroupData& attr) {
    DPGrpList* groups = GroupList(cType);
    if (!groups || name.empty()) return false;
    try {
        addGroupList(*groups, name, attr);
        return IsExistInGroupList(*groups, name);
    } catch (...) {
        return false;
    }
}

bool DCurveInternalData::removeCompGrp(const DComponentType &cType, const std::string& name) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    removeFromGroup(*groups, name);
    return true;
}

bool DCurveInternalData::removeCompGrpAll(const DComponentType &cType) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    groups->clear();
    return true;
}

DPGroupData::ConstPtr DCurveInternalData::getCompGrp(const DComponentType &cType, const std::string& name) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups ? ExtractGroupFromGroupList(*groups, name)
                  : DPGroupData::ConstPtr();
}

size_t DCurveInternalData::numberOfCompGroups(const DComponentType &cType) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups ? groups->size() : 0U;
}

bool DCurveInternalData::deleteCompGroupFromIds(const DComponentType &cType, const DPIdList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    DeleteFromGroupUseIds(*groups, ids);
    return true;
}

bool DCurveInternalData::sortCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    SortFromGroupUseIds(*groups, ids);
    return true;
}

bool DCurveInternalData::remapCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids) {
    DPGrpList* groups = GroupList(cType);
    if (!groups) return false;
    RemapFromGroupUseIds(*groups, ids);
    return true;
}


DStringList DCurveInternalData::getAllCompGroupName(const DComponentType &cType) const {
    const DPGrpList* groups = GroupListConst(cType);
    return groups ? GetAllNameInGroupList(*groups) : DStringList();
}

DStringList DCurveInternalData::getAllCompGroupInfo(const DComponentType &cType) const {
    DStringList groupInfos;
    const DPGrpList* groups = GroupListConst(cType);
    if (groups) GetAllGroupListInfo(*groups, groupInfos);
    return groupInfos;
}



// DCurveData

const MTypeId DCurveData::id(ID_DC_DATA);
const MString DCurveData::typeName("DCurveData");

void* DCurveData::creator()
{
    return ::dommy::maya::createObject("DCurveData", [&]() {
        return new DCurveData();
    });
}


DCurveData::DCurveData() : MPxData()
{
	curvearray = DCurveList();
    mBBox = MBoundingBox();
	mBBoxDirty = false;
}

DCurveData::~DCurveData() = default;


void DCurveData::copy(const MPxData& other)
{
    const DCurveData* source = dynamic_cast<const DCurveData*>(&other);
    if (!source || other.typeId() != DCurveData::id ||
        !copyFrom(*source)) {
        MGlobal::displayError(
            "DCurveData::copy failed; destination was left unchanged.");
    }
}

MStatus DCurveData::copyFrom(const DCurveData& rhs) try
{
    if (!rhs.hasValidCurveTopology()) return MS::kInvalidParameter;
    if (this == &rhs) return MS::kSuccess;

    dommy::mesh::MeshMergePlan plan;
    dommy::attribute::ValidationBudget budget;
    if (!dommy::curve::addCurveOperationCost(rhs.beginCurves(), rhs.endCurves(), plan, budget) ||
        !dommy::curve::validateCurveOperationBudget(plan, rhs.mAttributeList, rhs.mGroupList,
            rhs.curvearray.size(), 0U, nullptr, budget)) return MS::kInvalidParameter;

    DCurveData candidate;
    candidate.curvearray.reserve(rhs.curvearray.size());
    for (const auto& source : rhs.curvearray) {
        const DCurveInternalData* curve = source.get();
        DCItem copied = curve->copyNewCurve();
        if (!copied) return MS::kFailure;
        candidate.curvearray.push_back(copied);
    }
    CopyAttributeToAttribute(candidate.mAttributeList, rhs.mAttributeList);
    CopyGroupToGroup(candidate.mGroupList, rhs.mGroupList);
    if (!candidate.hasValidCurveTopology()) return MS::kFailure;
    dommy::mesh::MeshMergePlan candidatePlan;
    dommy::attribute::ValidationBudget candidateBudget;
    if (!dommy::curve::addCurveOperationCost(candidate.curvearray.cbegin(), candidate.curvearray.cend(), candidatePlan, candidateBudget) ||
        !dommy::curve::validateCurveOperationBudget(candidatePlan, candidate.mAttributeList, candidate.mGroupList,
            candidate.curvearray.size(), 0U, nullptr, candidateBudget)) return MS::kFailure;
    candidate.updateBBox();

    curvearray.swap(candidate.curvearray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

MTypeId DCurveData::typeId() const
{
    return DCurveData::id;
}


MString DCurveData::name() const
{
    return DCurveData::typeName;
}




MStatus DCurveData::readASCII(const MArgList&, unsigned&)
{
	return MS::kFailure;
}


MStatus DCurveData::writeASCII(ostream&)
{
	return MS::kFailure;
}

MStatus DCurveData::readBinary(istream&, unsigned)
{
	return MS::kFailure;
}


MStatus DCurveData::writeBinary(ostream&)
{
	return MS::kFailure;
}


bool DCurveData::getCheckedTotalCvs(size_t& total) const {
	total = 0;
    for (const auto& curve : curvearray) {
		std::size_t updatedTotal = 0;
		if (!curve || !curve->hasValidCurveTopology() ||
			!dommy::memory::checkedAdd(
				total, curve->numberOfPoints(), updatedTotal) ||
            updatedTotal > dommy::generation::kMaxGeneratedPointCount) {
			total = 0;
			return false;
		}
		total = updatedTotal;
	}
	return true;
}

bool DCurveData::hasValidCurveTopology() const {
    if (curvearray.size() > dommy::generation::kMaxGeneratedCurveCount) {
        return false;
    }

    std::size_t totalCvs = 0U;
    if (!getCheckedTotalCvs(totalCvs)) return false;

    dommy::attribute::ValidationBudget budget(curvearray.size());
	if (!dommy::attribute::validateAttributeList(
			mAttributeList, curvearray.size(), budget)) {
		return false;
    }
    for (ConstDPrimGrpListIter group = mGroupList.begin();
         group != mGroupList.end(); ++group) {
        if (!group->second ||
            group->second->getGroupIds().size() != curvearray.size() ||
            !dommy::generation::checkedSum(
                budget.workItems, curvearray.size(),
                dommy::generation::kMaxWorkItemCount,
				budget.workItems)) {
            return false;
        }
    }
	return true;
}

size_t DCurveData::getTotalCvs() const {
	size_t total = 0;
	getCheckedTotalCvs(total);
	return total;
}


const DCurveInternalData& DCurveData::getCurve(const size_t &index) const
{
	ConstDCItem curve = curvearray.at(index);
	if (!curve) {
		throw std::runtime_error("DCurveData contains a null curve");
	}
    return *curve;
}

DCurveInternalData& DCurveData::getCurve(const size_t &index) {
	markBBoxDirty();
	DCItem curve = curvearray.at(index);
	if (!curve) {
		throw std::runtime_error("DCurveData contains a null curve");
	}
    return *curve;
}


ConstDCItem DCurveData::getCurvePtr(const size_t &index) const {
	return curvearray.at(index);
}

DCItem DCurveData::getCurvePtr(const size_t &index) {
	markBBoxDirty();
    return curvearray.at(index);
}


MStatus DCurveData::copyRepeatedFrom(const DCurveData& source, std::size_t copies,
    const CurveCopyOperation& operation) try {
    if (!source.hasValidCurveTopology()) return MS::kInvalidParameter;
    dommy::mesh::MeshMergePlan plan;
    if (!dommy::curve::buildCurveCopyPlan(source, copies, plan)) return MS::kInvalidParameter;

    DCurveData candidate;
    candidate.curvearray.reserve(plan.meshes);
    if (!source.curvearray.empty()) {
        for (std::size_t copyIndex = 0U; copyIndex < copies; ++copyIndex) {
            std::size_t curveIndex = 0U;
            for (ConstDCItem curve : source.curvearray) {
                DCItem copied = curve->copyNewCurve();
                if (!copied) return MS::kFailure;
                if (copyIndex != 0U && operation)
                    DOMMY_RETURN_IF_FAILURE(operation(*copied, copyIndex, curveIndex));
                candidate.curvearray.push_back(copied);
                ++curveIndex;
            }
        }
    }
    CopyAttributeToAttribute(candidate.mAttributeList, source.mAttributeList);
    CopyGroupToGroup(candidate.mGroupList, source.mGroupList);
    if (!source.curvearray.empty()) {
        for (std::size_t copyIndex = 1U; copyIndex < copies; ++copyIndex) {
            for (const auto& field : candidate.mAttributeList)
                if (!appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first))) return MS::kFailure;
            for (const auto& group : candidate.mGroupList)
                group.second->appendOtherGrp(source.mGroupList.at(group.first));
        }
    }
    if (!candidate.hasValidCurveTopology()) return MS::kFailure;
    // Callbacks and virtual metadata copies cannot bypass retained admission.
    dommy::mesh::MeshMergePlan finalPlan;
    if (!dommy::curve::buildCurveCopyPlan(candidate, 1U, finalPlan)) return MS::kInvalidParameter;
    candidate.updateBBox();
    curvearray.swap(candidate.curvearray);
    mAttributeList.swap(candidate.mAttributeList);
    mGroupList.swap(candidate.mGroupList);
    mBBox = candidate.mBBox;
    mBBoxDirty = false;
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

bool DCurveData::insert(const DCurveInternalData &data) {
    try {
        const CurveSourceList source(1U, &data);
        return appendCurvesTransactional(source);
    } catch (...) {
        return false;
    }
}

bool DCurveData::insert(const DCItem& curve) {
    try {
        const CurveSourceList source(1U, curve.get());
        return appendCurvesTransactional(source);
    } catch (...) {
        return false;
    }
}

bool DCurveData::insert(const ConstDCItem& curve) {
    try {
        const CurveSourceList source(1U, curve.get());
        return appendCurvesTransactional(source);
    } catch (...) {
        return false;
    }
}


bool DCurveData::insert(const DCurveList& rhs) {
    try {
        CurveSourceList source;
        source.reserve(rhs.size());
        for (DCurveList::const_iterator curve = rhs.begin();
             curve != rhs.end(); ++curve) {
            source.push_back(curve->get());
        }
        return appendCurvesTransactional(source);
    } catch (...) {
        return false;
    }
}

bool DCurveData::appendCurvesTransactional(
    const CurveSourceList& curves, const DPIndexList* sourceIndices) {
    if (sourceIndices && sourceIndices->size() != curves.size()) return false;
    if (!hasValidCurveTopology()) return false;
    if (curves.empty()) return true;

    try {
        for (const DCurveInternalData* curve : curves)
            if (!curve || !curve->hasValidCurveTopology()) return false;
        dommy::mesh::MeshMergePlan plan;
        dommy::attribute::ValidationBudget budget;
        // Mutable public iterators invalidate bounds. Admission is read-only:
        // use const vector iterators so a rejected transaction changes no state.
        if (!dommy::curve::addCurveOperationCost(curvearray.cbegin(), curvearray.cend(), plan, budget) ||
            !dommy::curve::addCurveOperationCost(curves.begin(), curves.end(), plan, budget) ||
            !dommy::curve::validateCurveOperationBudget(plan, mAttributeList, mGroupList,
                curvearray.size(), curves.size(), sourceIndices, budget)) return false;

        DCurveData candidate;
        candidate.curvearray = curvearray;
        candidate.curvearray.reserve(plan.meshes);
        for (const DCurveInternalData* curve : curves) {
            DCItem copied = curve->copyNewCurve();
            if (!copied) return false;
            candidate.curvearray.push_back(copied);
        }
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGroupList, mGroupList);
        if (sourceIndices) {
            for (auto& attribute : candidate.mAttributeList)
                for (std::size_t index : *sourceIndices)
                    attribute.second->incrementIndexElement(index);
            for (auto& group : candidate.mGroupList)
                for (std::size_t index : *sourceIndices)
                    group.second->expandOtherGrpFromIndex(index);
        } else {
            ExpandAttributeSize(candidate.mAttributeList, curves.size());
            ExpandGroupListSize(candidate.mGroupList, curves.size());
        }
        if (!candidate.hasValidCurveTopology()) return false;
        dommy::mesh::MeshMergePlan candidatePlan;
        dommy::attribute::ValidationBudget candidateBudget;
        if (!dommy::curve::addCurveOperationCost(candidate.curvearray.cbegin(), candidate.curvearray.cend(), candidatePlan, candidateBudget) ||
            !dommy::curve::validateCurveOperationBudget(candidatePlan, candidate.mAttributeList, candidate.mGroupList,
                candidate.curvearray.size(), 0U, nullptr, candidateBudget)) return false;
        candidate.updateBBox();

        curvearray.swap(candidate.curvearray);
        mAttributeList.swap(candidate.mAttributeList);
        mGroupList.swap(candidate.mGroupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool DCurveData::expand(const DCItem& data, size_t itemIndex) {
    try {
        if (itemIndex >= curvearray.size()) return false;
        const CurveSourceList curves(1U, data.get());
        const DPIndexList indices(1U, itemIndex);
        return appendCurvesTransactional(curves, &indices);
    } catch (...) {
        return false;
    }
}


bool DCurveData::deleteFromIds(const DPIdList &ids) {
    if (ids.size() != numberOfCurves() ||
        !hasValidCurveTopology()) {
        return false;
    }

    try {
        DCurveData candidate;
        candidate.curvearray.reserve(curvearray.size());
        for (std::size_t index = 0U; index < curvearray.size(); ++index) {
            if (!ids[index]) {
                candidate.curvearray.push_back(curvearray[index]);
            }
        }
        CopyAttributeToAttribute(
            candidate.mAttributeList, mAttributeList);
        CopyGroupToGroup(candidate.mGroupList, mGroupList);
        if (!DDeleteAttributeFromIds(
                candidate.mAttributeList, ids)) {
            return false;
        }
        DeleteFromGroupUseIds(candidate.mGroupList, ids);
        candidate.updateBBox();
        if (!candidate.hasValidCurveTopology()) return false;

        curvearray.swap(candidate.curvearray);
        mAttributeList.swap(candidate.mAttributeList);
        mGroupList.swap(candidate.mGroupList);
        mBBox = candidate.mBBox;
		mBBoxDirty = false;
        return true;
    } catch (...) {
        return false;
    }
}





void DCurveData::clear() {
    curvearray.clear();
    curvearray.shrink_to_fit();
	mAttributeList.clear();
	mGroupList.clear();
	mBBox = MBoundingBox();
	mBBoxDirty = false;
}



// center method

void DCurveData::getPrimitiveCenterList(DPointList &centerList) const
{
    DPointList centers;
    centers.reserve(numberOfCurves());
    for (ConstDCItem curve : curvearray) {
        if (!curve || !curve->hasValidCurveTopology()) {
            centerList.clear();
            return;
        }
        centers.push_back(curve->getCenter());
    }
    centerList.swap(centers);
}

void DCurveData::getPrimitiveCenterList(MVectorArray &centerList) const
{
    if (!hasValidCurveTopology())
        throw std::invalid_argument("Invalid curve data for primitive centers");
    const MStatus status = dommy::maya::buildCheckedArray(curvearray.size(),
        [&](unsigned int index, MVector& value) -> MStatus {
            value = curvearray[index]->getCenterDouble();
            return MS::kSuccess;
        }, centerList);
    if (!status) throw std::runtime_error("Unable to construct curve center array");
}

// start end cv method
void DCurveData::getPrimitiveStartPointList(DPointList &ptList) const {
	ptList.clear();
	ptList.reserve(numberOfCurves());
	for (ConstDCItem curve : curvearray) {
		if (curve && curve->numberOfPoints() > 0U) {
			ptList.push_back(curve->getPositionPtrConst()->front());
		}
	}
}

void DCurveData::getPrimitiveEndPointList(DPointList &ptList) const {
	ptList.clear();
	ptList.reserve(numberOfCurves());
	for (ConstDCItem curve : curvearray) {
		if (curve && curve->numberOfPoints() > 0U) {
			ptList.push_back(curve->getPositionPtrConst()->back());
		}
	}
}


// BBox method
MBoundingBox DCurveData::calculateBoundingBox() const
{
    MBoundingBox bounds;

	for (ConstDCurveIter curve = curvearray.cbegin();
		 curve != curvearray.cend(); ++curve) {
		if (*curve && (*curve)->hasValidCurveTopology()) {
			bounds.expand((*curve)->getBBox());
		}
	}
    return bounds;
}

void DCurveData::updateBBox()
{
    mBBox = calculateBoundingBox();
	mBBoxDirty = false;
}


MBoundingBox DCurveData::getBoundingBox() const
{
	return mBBoxDirty ? calculateBoundingBox() : mBBox;
}

// attribute method

bool DCurveData::isExistAttr(const std::string& name) const {
    
    return IsExistInAttributeList(mAttributeList, name);
    
}


void DCurveData::addAttr(const std::string& name, const DPAttributeData& attr)
{
    AddAttributeList(mAttributeList, name, attr);
}


void DCurveData::removeAttr(const std::string& name) {
    RemoveFromAttributeList(mAttributeList, name);
    
}


DPAttributeData::ConstPtr DCurveData::getAttr(const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
    
}

DPAttributeData::Ptr DCurveData::getAttrPtr(
        const std::string& name) {
    return GetAttrPtr(mAttributeList, name);
}

DPAttributeData::ConstPtr DCurveData::getAttrPtr(
        const std::string& name) const {
    return ExtractAttributeFromAttributeList(mAttributeList, name);
}
std::string DCurveData::getAttributeType(const std::string& name) const {
    return GetAttributeTypeName(mAttributeList, name);
	
}

//set
void DCurveData::setDPAttributeValues(const std::string& name, const DScalarDataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            replaced = replaceConvertedAttributeValues<int32_t>(
                *this, name, values);
            break;
        case DP_ATTR_UINT32:
            replaced = replaceConvertedAttributeValues<uint32_t>(
                *this, name, values);
            break;
        case DP_ATTR_INT64:
            replaced = replaceConvertedAttributeValues<int64_t>(
                *this, name, values);
            break;
        case DP_ATTR_FLOAT:
            replaced = replaceConvertedAttributeValues<float>(
                *this, name, values);
            break;
        case DP_ATTR_DOUBLE:
            replaced = replaceConvertedAttributeValues<double>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues requires a matching scalar attribute");
    }
}


void DCurveData::setDPAttributeValues(const std::string& name, const DVec2DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC2I:
            replaced = replaceConvertedAttributeValues<Imath::V2i>(
                *this, name, values);
            break;
        case DP_ATTR_VEC2F:
            replaced = replaceConvertedAttributeValues<Imath::V2f>(
                *this, name, values);
            break;
        case DP_ATTR_VEC2D:
            replaced = replaceConvertedAttributeValues<Imath::V2d>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues requires a matching vec2 attribute");
    }
}


template <typename T>
void DCurveData::addAttrWithValues(
        const std::string& name, const std::vector<T>& values) {
    (void)name;
    (void)values;
    static_assert(
        !std::is_same<T, T>::value,
        "DCurveData::addAttrWithValues does not support this type");
}


template<>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<bool> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPBoolAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<int32_t> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPIntAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<uint32_t> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPUInt32Attr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<int64_t> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPInt64Attr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<float> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPFloatAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<double> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPDoubleAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2i> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec2iAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2f> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec2fAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V2d> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec2dAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3i> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec3iAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3f> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec3fAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<Imath::V3d> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPVec3dAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<std::string> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPStringAttr(values));
    }
}

template <>
void DCurveData::addAttrWithValues(const std::string& name, const std::vector<MMatrix> &values) {
    if (values.size() == numberOfCurves()) {
        addAttr(name, DPMat4Attr(values));
    }
}

void DCurveData::setDPAttributeValues(const std::string& name, const DVec3DataList &values) {
    const DPAttributeData::ConstPtr attribute = getAttr(name);
    if (!attribute) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues cannot find the attribute");
    }

    bool replaced = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC3I:
            replaced = replaceConvertedAttributeValues<Imath::V3i>(
                *this, name, values);
            break;
        case DP_ATTR_VEC3F:
            replaced = replaceConvertedAttributeValues<Imath::V3f>(
                *this, name, values);
            break;
        case DP_ATTR_VEC3D:
            replaced = replaceConvertedAttributeValues<Imath::V3d>(
                *this, name, values);
            break;
        default:
            break;
    }
    if (!replaced) {
        throw std::invalid_argument(
            "DCurveData::setDPAttributeValues requires a matching vec3 attribute");
    }
}



size_t DCurveData::numberOfAttributes() const {
    
    return mAttributeList.size();
}


void DCurveData::extendAttributeFromCount(const size_t &count) {
    //attribute add default value
    
    for (auto aIt=beginAttr(); aIt!=endAttr(); ++aIt) {
        aIt->second->incrementDefaultValue( count );
    }
}

void DCurveData::extendAttributeFromIndex(const size_t &index) {
    for (auto aIt=beginAttr(); aIt!=endAttr(); ++aIt) {
        aIt->second->incrementIndexElement(index);
    }
}


// point attribute method
size_t DCurveData::numberOfPointAttribute(bool detail) const {
    DStringList attributeList;

    for (const auto& curve : curvearray) {
        if (!curve) {
            continue;
        }
        IntersectDStringList(attributeList, curve->getAllAttributeName());
        if (!detail) {
            break;
        }
    }

    return attributeList.size();
}

DStringList DCurveData::getPointAttributeInfo(bool detail) const {
    DStringList attributeInfo;

    for (const auto& curve : curvearray) {
        if (!curve) {
            continue;
        }
        IntersectDStringList(attributeInfo, curve->getAllAttributeInfo());
        if (!detail) {
            break;
        }
    }

    return attributeInfo;
}

bool DCurveData::isExistPointAttr(const std::string& name) const {
	
	for (const auto& curve : curvearray) {
		if (curve && curve->isExistAttr(name)) {
			return true;
        }
	}
	
	return false;
}

DString DCurveData::getPointAttributeTypeStr(const std::string& name) const {
    for (ConstDCItem curve : curvearray) {
        if (!curve) {
            continue;
        }
        const DPAttributeData::ConstPtr attribute = curve->getAttr(name);
        if (attribute) {
            return attribute->typeName();
        }
	}
	
	return "";
}

DP_ATTR_DATA_TYPE DCurveData::getPointAttributeType(const std::string& name) const {
    for (ConstDCItem curve : curvearray) {
        if (!curve) {
            continue;
        }
        const DPAttributeData::ConstPtr attribute = curve->getAttr(name);
        if (attribute) {
            return attribute->getAttrType();
        }
    }
    
    return DP_ATTR_INVALID;
}

// attribute name
DStringList DCurveData::getPrimAttributeList() const {
    DStringList attributeNames;
    for(auto itr:mAttributeList) {
        attributeNames.push_back(itr.first);
    }
    return attributeNames;
}

DStringList DCurveData::getAllCompAttrName(const DComponentType &cType) const {
    DStringList allCompAttrList;
    
    for (auto mIt:curvearray) {
        if (!mIt) continue;
        DStringList attrList = mIt->getAllCompAttrName(cType);
        IntersectDStringList(allCompAttrList, attrList);
    }
    
    return allCompAttrList;
}

DStringList DCurveData::getAllAttributeList() const {
    
    DStringList allCompAttrList;
    
    DStringList pattrList = getPrimAttributeList();
    
    IntersectDStringList(allCompAttrList, pattrList);
    
    for (auto mIt:curvearray) {
        if (!mIt) continue;
        DStringList attrList = mIt->getAllCompAttrName(D_COMP_POINT);
        IntersectDStringList(allCompAttrList, attrList);
    }
    
    return allCompAttrList;
}

DComponentType DCurveData::getAttributeComponentType(const std::string& name) const {
    
    if (numberOfCurves()==0)
        return D_COMP_COUNT;
    
    //primitive
    if(isContainDStringList(getPrimAttributeList(), name)) {
        return D_COMP_PRIM;
    }
    
    //Component
    const ConstDCItem curve = curvearray.front();
    if (!curve) {
        return D_COMP_COUNT;
    }

    if(isContainDStringList(curve->getAllCompAttrName(D_COMP_POINT), name)) {
        return D_COMP_POINT;
    }
    
    return D_COMP_COUNT;
}


// group method
bool DCurveData::isExistGrp(const std::string& name) const {
    
    return IsExistInGroupList(mGroupList, name);
    
}

void DCurveData::addGrp(const std::string& name, const DPGroupData& attr) {
	
    addGroupList(mGroupList, name, attr);
    
}

void DCurveData::removeGrp(const std::string& name) {
    removeFromGroup(mGroupList, name);
}



DPGroupData::ConstPtr DCurveData::getGrp(const std::string& name) const {
    return ExtractGroupFromGroupList(mGroupList, name);
}

DPGroupData::Ptr DCurveData::getGrpPtr(const std::string& name) {
    auto iter = mGroupList.find(name);
    return (iter == mGroupList.end() ? DPGroupData::Ptr() : iter->second);
}

DPGroupData::ConstPtr DCurveData::getGrpPtr(
        const std::string& name) const {
    auto iter = mGroupList.find(name);
    return (iter == mGroupList.end()
        ? DPGroupData::ConstPtr() : iter->second);
}


size_t DCurveData::numberOfGroups() const {
    return mGroupList.size();
}


void DCurveData::deleteGroupFromIds(const DPIdList &ids) {
    
    DeleteFromGroupUseIds(mGroupList, ids);
    
}

void DCurveData::sortGroupFromIds(const DPIndexList &ids) {
    
    SortFromGroupUseIds(mGroupList, ids);
    
}


void DCurveData::extendGroupFromCount(const size_t &count) {
    
    ExpandGroupListSize(mGroupList, count);
    
}

void DCurveData::extendGroupFromIndex(const size_t &index) {
    for (auto iter = mGroupList.begin(); iter!=mGroupList.end(); iter++) {
        iter->second->expandOtherGrpFromIndex(index);
    }
}



// component group method
bool DCurveData::isExistCompGrp(const DComponentType &cType, const std::string& name) const {
	if (curvearray.empty() || !curvearray.front()) {
		return false;
	}
    return curvearray.front()->isExistCompGrp(cType, name);
}



size_t DCurveData::numberOfCompGroup(const DComponentType &cType) const {
    DStringList groupList;
	if (curvearray.empty() || !curvearray.front()) {
		return 0U;
	}

    IntersectDStringList(
        groupList, curvearray.front()->getAllCompGroupName(cType));
    
    return groupList.size();
}





DStringList DCurveData::getCompGroupInfo(const DComponentType &cType) const {
    
    DStringList groupInfo;
	if (curvearray.empty() || !curvearray.front()) {
		return groupInfo;
	}
    
    //最初のみチェック
    auto cIt=beginCurves();
    
    DStringList infos = (*cIt)->getAllCompGroupInfo(cType);
    IntersectDStringList(groupInfo, infos);

    return groupInfo;
    
    
}

DStringList DCurveData::getAllCompGroupName(const DComponentType &cType) const {
    
    DStringList allCompGrpList;
    
    for (auto cIt:curvearray) {
        if (!cIt) continue;
        //check point attribute
        
        DStringList grpList = cIt->getAllCompGroupName(cType);
        IntersectDStringList(allCompGrpList, grpList);
        
    }
    
    return allCompGrpList;
}

DStringList DCurveData::getPrimGroupList() const {
    DStringList groupNames;
    for(auto iter : mGroupList) {
        groupNames.push_back(iter.first);
    }
    return groupNames;
}

DStringList DCurveData::getAllGroupList() const {
    DStringList allCompGrpList;
    
    DStringList pgrpList = getPrimGroupList();
    
    IntersectDStringList(allCompGrpList, pgrpList);
    
    for (auto mIt:curvearray) {
        if (!mIt) continue;
        
        DStringList grpList = mIt->getAllCompGroupName(D_COMP_POINT);
        IntersectDStringList(allCompGrpList, grpList);
    }
    return allCompGrpList;
}

DComponentType DCurveData::getGroupComponentType(const std::string& name) const {
    
    if (numberOfCurves()==0)
        return D_COMP_COUNT;
    
    //primitive
    if(isContainDStringList(getPrimGroupList(), name)) {
        return D_COMP_PRIM;
    }
    
    //Component
    DCItem mitem = curvearray[0];
    
    if(isContainDStringList(mitem->getAllCompGroupName(D_COMP_POINT), name)) {
        return D_COMP_POINT;
    }
    
    return D_COMP_COUNT;
}

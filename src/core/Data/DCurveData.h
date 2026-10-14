//
//  DMultiCurveData.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/01.
//
//

#pragma once
#include "common/utility/DAttributeLookup.h"
#include "common/utility/DCurveBatchBudget.h"

#include <maya/MPxData.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDoubleArray.h>

#include <maya/MBoundingBox.h>

#include <vector>
#include <memory>
#include <functional>


#include "DPAttributeData.h"
#include "DPGroupData.h"
#include "DPrimBaseData.h"
#include "common/utility/DMayaPluginData.h"

enum class DCurveReplacementPolicy
{
	ResetMetadata,
	RequireCompatibleMetadata
};

class DCurveInternalData : public DPrimBaseData
{
public:
	
	typedef std::shared_ptr<DCurveInternalData> Ptr;
	typedef std::shared_ptr<const DCurveInternalData> CPtr;
		
	    DCurveInternalData();
		DCurveInternalData(DCurveInternalData const &src);
	    DCurveInternalData(const DPointList &c, int d, bool isOpen=true);
	    DCurveInternalData(const MPointArray &c, int d, bool isOpen=true);
		DCurveInternalData(const DPointList &c, const MDoubleArray &k, int d, bool isOpen=true);
	    DCurveInternalData(const MPointArray &c, const MDoubleArray &k, int d, bool isOpen=true);
	    virtual ~DCurveInternalData();

		static MStatus createFromMayaObject(const MObject& object, Ptr& result);
	
	
	DCurveInternalData::Ptr copy();
	DCurveInternalData::CPtr copy() const;
	DCurveInternalData::Ptr copyNewCurve() const;
	void copy(const DCurveInternalData& other);
	DCurveInternalData& operator=(const DCurveInternalData& other);
	
	//get curve info
	inline MPointArray getCvs() const { return getMPosition();}
	inline MDoubleArray getKnots() const { return arrays_->knots;}
    inline std::size_t numberOfKnots() const { return arrays_->knots.length(); }
	inline int getDegree() const { return degree;}
	bool hasValidCurveTopology() const;
    // Validate and admit geometry plus point metadata without cloning arrays.
    // Rejection leaves the accumulated batch budget unchanged.
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const;
    inline bool isCurveOpen() const {return mIsOpen;}
	bool replacePositions(const DPointList& points);
	bool replacePositions(const MPointArray& points);
	bool replaceKnots(const MDoubleArray& replacementKnots);
	bool setReplaceCurveData(
		const DPointList& points,
		const MDoubleArray& replacementKnots,
		int replacementDegree,
		bool isOpen,
		DCurveReplacementPolicy policy =
			DCurveReplacementPolicy::ResetMetadata);
	bool setReplaceCurveData(
		const MPointArray& points,
		const MDoubleArray& replacementKnots,
		int replacementDegree,
		bool isOpen,
		DCurveReplacementPolicy policy =
			DCurveReplacementPolicy::ResetMetadata);
	bool setRemappedCurveData(
		const DPointList& points,
		const MDoubleArray& replacementKnots,
		int replacementDegree,
		bool isOpen,
		const DPIndexList& sourcePointIndices);
	bool setRemappedCurveData(
		const MPointArray& points,
		const MDoubleArray& replacementKnots,
		int replacementDegree,
		bool isOpen,
		const DPIndexList& sourcePointIndices);
	
    
    //inherit
    bool appendPoint(const DPoint &pt);
    bool insertPoint(const DPoint &pt, const uint &index);
	
    
    //Component Attribute method
    DStringList getAllCompAttrName(const DComponentType &cType) const;
    
    
    //for display data
    
    
    //Component Group method
    DStringList getAllCompGroupName(const DComponentType &cType) const;
    DStringList getAllCompGroupInfo(const DComponentType &cType) const;
    
    bool isExistCompGrp(const DComponentType &cType, const std::string& name) const;
    bool addCompGrp(const DComponentType &cType, const std::string& name, const DPGroupData& attr);
    bool removeCompGrp(const DComponentType &cType, const std::string& name);
    bool removeCompGrpAll(const DComponentType &cType);
    DPGroupData::ConstPtr getCompGrp(const DComponentType &cType, const std::string& name) const;
    size_t numberOfCompGroups(const DComponentType &cType) const;
    bool deleteCompGroupFromIds(const DComponentType &cType, const DPIdList &ids);
    bool sortCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids);
    
    bool remapCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids);
    
    
    
protected:
    
    //attribute method
    const DPAttrList *AttributeListConst(const DComponentType &cType) const;
    DPAttrList *AttributeList(const DComponentType &cType);
    
    const DPGrpList *GroupListConst(const DComponentType &cType) const;
    DPGrpList *GroupList(const DComponentType &cType);
    
    
    // Own Maya arrays behind a swappable pointer. Their assignment operators
    // cannot report failure and must never be used to commit staged geometry.
    struct ArrayStorage {
        ArrayStorage() = default;
        ArrayStorage(const ArrayStorage&) = delete;
        ArrayStorage& operator=(const ArrayStorage&) = delete;
        MDoubleArray knots;
    };
    std::unique_ptr<ArrayStorage> arrays_{new ArrayStorage()};
    void swapState(DCurveInternalData& other) noexcept;
    
//    int numSpans;
    int degree;
    bool mIsOpen;
    
    
    
};






typedef DCurveInternalData::Ptr DCItem;
typedef DCurveInternalData::CPtr ConstDCItem;


class DCurveData final : public MPxData
{
public:
    
	//curves
	typedef std::vector<DCItem> DCurveList;
	typedef std::vector<ConstDCItem> ConstDCurveList;
	typedef DCurveList::iterator DCurveIter;
	typedef DCurveList::const_iterator ConstDCurveIter;
	
    //attribute
    typedef std::map<std::string, DPAttributeData::Ptr> DPrimAttrList;
    typedef DPrimAttrList::iterator DPrimAttrListIter;
    typedef DPrimAttrList::const_iterator ConstDPrimAttrListIter;
    
    //group
    typedef std::unordered_map<std::string, DPGroupData::Ptr > DPrimGrpList;
    typedef DPrimGrpList::iterator DPrimGrpListIter;
    typedef DPrimGrpList::const_iterator ConstDPrimGrpListIter;
    
    
    
    
    /////////////////////Maya
    DCurveData();
    ~DCurveData() override;
    MStatus         readASCII( const MArgList&, unsigned&  ) override;
    MStatus         readBinary( istream&, unsigned length ) override;
    MStatus         writeASCII( ostream&  ) override;
    MStatus         writeBinary( ostream&  ) override;
    void            copy ( const MPxData& ) override;
    // Internal copies must check this status. Failure preserves the destination.
    MStatus         copyFrom(const DCurveData& source);
    using CurveCopyOperation = std::function<MStatus(DCurveInternalData&, std::size_t, std::size_t)>;
    // Own each copied primitive once. The callback applies only to later
    // batches; failure preserves the destination, including cached bounds.
    MStatus copyRepeatedFrom(const DCurveData& source, std::size_t copies,
        const CurveCopyOperation& operation = CurveCopyOperation());
    MTypeId         typeId() const override;
    MString         name() const override;

	
	static void * creator();
    
    static const MString typeName;
    static const MTypeId id;
	
	
	
	
	
	/////////////////////
    // curve iterator
    DCurveIter beginCurves() {
		markBBoxDirty();
		return curvearray.begin();
	}
    DCurveIter endCurves() {
		markBBoxDirty();
		return curvearray.end();
	}
    ConstDCurveIter beginCurves() const { return curvearray.begin(); }
    ConstDCurveIter endCurves() const { return curvearray.end(); }
    DCurveIter beginPrims() {
		markBBoxDirty();
		return curvearray.begin();
	}
    DCurveIter endPrims() {
		markBBoxDirty();
		return curvearray.end();
	}
    ConstDCurveIter beginPrims() const { return curvearray.begin(); }
    ConstDCurveIter endPrims() const { return curvearray.end(); }
	
	
	size_t numberOfCurves() const {return curvearray.size();}
	bool hasValidCurveTopology() const;
	bool getCheckedTotalCvs(size_t& total) const;
	size_t getTotalCvs() const;
	
    const DCurveList* getCurveListConst() const { return &curvearray; }
	const DCurveInternalData& getCurve(const size_t &index) const;
    DCurveInternalData& getCurve(const size_t &index);
    ConstDCItem getCurvePtr(const size_t &index) const;
    DCItem getCurvePtr(const size_t &index);
    
    bool insert(const DCurveInternalData &data);
    bool insert(const DCItem&);
	bool insert(const ConstDCItem&);
    bool insert(const DCurveList&);
    bool expand(const DCItem&, size_t itemIndex);
	bool deleteFromIds(const DPIdList &ids);
	
	
	
    void clear();
	
    /////////////////////
    //Center
    void getPrimitiveCenterList(DPointList &centerList) const;
    // Staged double centers; invalid data or Maya allocation/copy failure throws.
    void getPrimitiveCenterList(MVectorArray &centerList) const;
	
    //Start Cv
    void getPrimitiveStartPointList(DPointList &ptList) const;
    //End Cv
    void getPrimitiveEndPointList(DPointList &ptList) const;
    
	
	/////////////////////
	//Bounding box
    void updateBBox();
	MBoundingBox getBoundingBox() const;
	
    
    /////////////////////
    //curve attribute
    DPAttributeData::Ptr operator[](const std::string&);
    DPAttributeData::ConstPtr operator[](const std::string&) const;
    
    template<typename T> typename T::Ptr getAttribute(const std::string&);
    template<typename T> typename T::ConstPtr getAttribute(const std::string&) const;
    
    template<typename T> const std::vector<T>& attrListValue(const std::string&) const;
    
    // Functions for iterating over the attribute
    DPrimAttrListIter beginAttr() { return mAttributeList.begin(); }
    DPrimAttrListIter endAttr() { return mAttributeList.end(); }
    ConstDPrimAttrListIter beginAttr() const { return mAttributeList.begin(); }
    ConstDPrimAttrListIter endAttr() const { return mAttributeList.end(); }
	
	
    
	/////////////////////
	//primitive attribute
    bool isExistAttr(const std::string& name) const;
    void addAttr(const std::string& name, const DPAttributeData& attr);
    void removeAttr(const std::string& name);
    DPAttributeData::ConstPtr getAttr(const std::string& name) const;
    DPAttributeData::Ptr getAttrPtr(const std::string& name);
    DPAttributeData::ConstPtr getAttrPtr(
        const std::string& name) const;
    std::string getAttributeType(const std::string& name) const;
    size_t getPrimAttributeCount() const { return mAttributeList.size();}
    
    //set
    void setDPAttributeValues(const std::string& name, const DScalarDataList &values);
    void setDPAttributeValues(const std::string& name, const DVec2DataList &values);
    void setDPAttributeValues(const std::string& name, const DVec3DataList &values);
    //add attribute
    template <typename T>
    void addAttrWithValues(const std::string& name,  const std::vector<T> &values);
    
	
    size_t numberOfAttributes() const;
    void extendAttributeFromCount(const size_t &count);
    void extendAttributeFromIndex(const size_t &index);
    
    
	/////////////////////
	//point attribute
    size_t numberOfPointAttribute(bool detail=false) const;
    DStringList getPointAttributeInfo(bool detail=false) const;
	bool isExistPointAttr(const std::string& name) const;
    DString getPointAttributeTypeStr(const std::string& name) const;
	DP_ATTR_DATA_TYPE getPointAttributeType(const std::string& name) const;
    //attribute name methd
    DStringList getPrimAttributeList() const;
    DStringList getAllCompAttrName(const DComponentType &cType) const;
    DStringList getAllAttributeList() const;
    DComponentType getAttributeComponentType(const std::string& name) const;
    
//	
	
	
	
    
    /////////////////////
    //group
    // Functions for iterating over the group
    DPrimGrpListIter beginGrp() { return mGroupList.begin(); }
    DPrimGrpListIter endGrp() { return mGroupList.end(); }
    ConstDPrimGrpListIter beginGrp() const { return mGroupList.begin(); }
    ConstDPrimGrpListIter endGrp() const { return mGroupList.end(); }
    
    bool isExistGrp(const std::string& name) const;
    void addGrp(const std::string& name, const DPGroupData& attr);
    void removeGrp(const std::string& name);
    DPGroupData::ConstPtr getGrp(const std::string& name) const;
    DPGroupData::Ptr getGrpPtr(const std::string& name);
    DPGroupData::ConstPtr getGrpPtr(
        const std::string& name) const;
    size_t numberOfGroups() const;
    void deleteGroupFromIds(const DPIdList &ids);
    void sortGroupFromIds(const DPIndexList &ids);
    void extendGroupFromCount(const size_t &count);
    void extendGroupFromIndex(const size_t &index);
    
    
    //component group info
    bool isExistCompGrp(const DComponentType &cType, const std::string& name) const;
    size_t numberOfCompGroup(const DComponentType &cType) const;
    DStringList getCompGroupInfo(const DComponentType &cType) const;
    DStringList getAllCompGroupName(const DComponentType &cType) const;
    //group name methd
    DStringList getPrimGroupList() const;
    DStringList getAllGroupList() const;
    DComponentType getGroupComponentType(const std::string& name) const;
    
    
    
    template<typename T>
    typename TypedDPAttributeData<T>::Ptr getValidTypedAttributedata(
        const std::string&);
    template<typename T>
    typename TypedDPAttributeData<T>::ConstPtr getValidTypedAttributedata(
        const std::string&) const;
	
protected:
    // Synchronous borrowed views; never stored or adopted as owned primitives.
    using CurveSourceList = std::vector<const DCurveInternalData*>;
    bool appendCurvesTransactional(const CurveSourceList& curves,
                                   const DPIndexList* sourceIndices = nullptr);
    
    DCurveList curvearray;
    MBoundingBox mBBox;
	bool mBBoxDirty;
    
    DPrimAttrList mAttributeList;
    DPrimGrpList mGroupList;
private:
	void markBBoxDirty() { mBBoxDirty = true; }
	MBoundingBox calculateBoundingBox() const;
};



inline DPAttributeData::Ptr
DCurveData::operator[](const std::string& name)
{
    DPrimAttrListIter iter = mAttributeList.find(name);
    return (iter == endAttr() ? DPAttributeData::Ptr() : iter->second);
}

inline DPAttributeData::ConstPtr
DCurveData::operator[](const std::string &name) const
{
    ConstDPrimAttrListIter iter = mAttributeList.find(name);
    return (iter == endAttr() ? DPAttributeData::Ptr() : iter->second);
}



template <typename T>
inline typename T::Ptr
DCurveData::getAttribute(const std::string &name)
{
    const auto attribute = dommy::attribute::findAttribute(mAttributeList, name);
    if (!attribute) {
        return typename T::Ptr();
    }
    
    
    if (attribute->typeName() == T::staticTypeName()) {
        return std::static_pointer_cast<T, DPAttributeData>(attribute);
    }
    return typename T::Ptr();
}

template <typename T>
inline typename T::ConstPtr
DCurveData::getAttribute(const std::string &name) const
{
    const auto attribute = dommy::attribute::findAttribute(mAttributeList, name);
    if (!attribute) {
        return typename T::ConstPtr();
    }
    
    if (attribute->typeName() == T::staticTypeName()) {
        return std::static_pointer_cast<const T, const DPAttributeData>(attribute);
    }
    return typename T::ConstPtr();
}



template <typename T>
inline const std::vector<T>& DCurveData::attrListValue(const std::string &name) const
{
    typename TypedDPAttributeData<T>::ConstPtr m =
        getValidTypedAttributedata<T>(name);
    return m->value();
}



template <typename T>
inline typename TypedDPAttributeData<T>::Ptr
DCurveData::getValidTypedAttributedata(const std::string &name)
{
    const auto attribute = dommy::attribute::findAttribute(mAttributeList, name);
    if (!attribute ||
        attribute->typeName() !=
            TypedDPAttributeData<T>::staticTypeName()) {
        return typename TypedDPAttributeData<T>::Ptr();
    }
    return std::static_pointer_cast<
        TypedDPAttributeData<T>, DPAttributeData>(attribute);
}

template <typename T>
inline typename TypedDPAttributeData<T>::ConstPtr
DCurveData::getValidTypedAttributedata(const std::string &name) const
{
    const auto attribute = dommy::attribute::findAttribute(mAttributeList, name);
    if (!attribute ||
        attribute->typeName() !=
            TypedDPAttributeData<T>::staticTypeName()) {
        return typename TypedDPAttributeData<T>::ConstPtr();
    }
    return std::static_pointer_cast<
        const TypedDPAttributeData<T>, const DPAttributeData>(
            attribute);
}

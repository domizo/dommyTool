//
//  DPrimBaseData.hpp
//  dommyTool
//
//  Created by yuzo on 1/14/17.
//
//




#pragma once
#include "common/utility/DAttributeLookup.h"
#include "DType.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <stdexcept>

#include <ImathVec.h>

#include <maya/MBoundingBox.h>
#include <maya/MFloatPointArray.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>

#include "DPAttributeData.h"
#include "DPGroupData.h"


class DPrimBaseData
{
public:
		
		
	DPrimBaseData();
	DPrimBaseData(DPrimBaseData const &src);
	virtual ~DPrimBaseData();

	// Exact-key lookup for the historical public array interface.
	DPAttributeData::Ptr operator[](const std::string&);
	DPAttributeData::ConstPtr operator[](const std::string&) const;
	
	
	template<typename T> typename T::Ptr getAttribute(const std::string&);
	template<typename T> typename T::ConstPtr getAttribute(const std::string&) const;
	
	
	template<typename T> const std::vector<T>& attrListValue(const std::string&) const;
	
	
	// Functions for iterating over the point
	DPointIter beginPt() { return pointArray.begin(); }
	DPointIter endPt() { return pointArray.end(); }
	ConstDPointIter beginPt() const { return pointArray.cbegin(); }
	ConstDPointIter endPt() const { return pointArray.cend(); }
	
	// Functions for iterating over the attribute
	DPAttrIter beginAttr() { return mAttributeList.begin(); }
	DPAttrIter endAttr() { return mAttributeList.end(); }
	ConstDPAttrIter beginAttr() const { return mAttributeList.cbegin(); }
	ConstDPAttrIter endAttr() const { return mAttributeList.cend(); }
	
	// Functions for iterating over the group
	DPGrpIter beginGrp() { return mGrooupList.begin(); }
	DPGrpIter endGrp() { return mGrooupList.end(); }
	ConstDPGrpIter beginGrp() const { return mGrooupList.cbegin(); }
	ConstDPGrpIter endGrp() const { return mGrooupList.cend(); }
	//point method
    
    // Legacy float storage API; throws on invalid/out-of-range transformation.
	DPointList getPointsInWorld();
    DPointList getPointsInWorld() const;
	DPointList getPosition() { return pointArray; }
    const DPointList* getPositionPtrConst() const { return &pointArray; }
    // Array export throws on failed Maya allocation/copy or invalid coordinates.
	MPointArray getMPosition() const;
    MFloatPointArray getMFPosition() const;
	DPointList getPositionConst() const {return pointArray;}
    // Stage XYZ conversion before replacement; invalid input leaves positions
    // unchanged. Metadata resizing remains the responsibility of the caller.
	void setPosition(const MPointArray &pts);
	void setPosition(const DPointList &pts);
	size_t numberOfPoints() const { return pointArray.size(); }
	bool deletePointFromIds(const DPIdList &ids);
	bool sortPointFromIds(const DPIndexList &ids);
	
	bool remapPointFromIds(const DPIndexList &ids);
	
	bool appendPoint(const DPoint &pt);
	bool insertPoint(const DPoint &pt, const size_t &index);
	
	//attribute method
	bool isExistAttr(const std::string& name) const;
	void addAttr(const std::string& name, const DPAttributeData& attr);
	void removeAttr(const std::string& name);
	void removeAttrAll();
	DPAttributeData::ConstPtr getAttr(const std::string& name) const;
	DPAttributeData::Ptr getAttrPtr(const std::string& name);
	DPAttributeData::ConstPtr getAttrPtr(
        const std::string& name) const;
	
	DString getAttrTypeName(const std::string& name) const;
	size_t getAttributeCount() const { return mAttributeList.size();}
	
	//add attribute
    template <typename T>
    void addAttrWithValues(const std::string& name,  const std::vector<T> &values);
	
    template <typename S, typename T>
    void setAttrWithValuesTypeSwap(const std::string& name, const std::vector<T> &values);
    
    
	//set
	void setDPAttributeValues(const std::string& name, const DScalarDataList &values);
    void setDPAttributeValues(const std::string& name, const DVec2DataList &values);
	void setDPAttributeValues(const std::string& name, const DVec3DataList &values);
	
	
	
	size_t numberOfAttributes() const;
	
	
    std::vector<std::string> getAllAttributeName() const;
	std::vector<std::string> getAllAttributeInfo() const;
	
	
	
	
	
	//group method
	bool isExistGrp(const std::string& name) const;
	void addGrp(const std::string& name, const DPGroupData& attr);
	// Checked point-group replacement; ordinary attributes are unaffected.
	bool replaceGrp(const std::string& name, const DPIdList& values);
	void removeGrp(const std::string& name);
    void removeGrpAll();
	DPGroupData::ConstPtr getGrp(const std::string& name) const;
	size_t numberOfGroups() const;
	void deleteGroupFromIds(const DPIdList &ids);
	void sortGroupFromIds(const DPIndexList &ids);
	void remapGroupFromIds(const DPIndexList &ids);
	
    
    
    
    
    
    
    //Transform
    bool isIdentity();
    bool isIdentity() const;
    void setIdentity();
    MMatrix getTransformMatrix() const;
    void setTransformMatrix(const MMatrix &mat);
	
	// Double world bounds and arithmetic vertex mean. Invalid transforms throw.
	MBoundingBox getBBox() const;
	// Legacy float mean; also throws when the result exceeds float storage.
	DPoint getCenter() const;
    MVector getCenterDouble() const;
	
    
	
	template<typename T>
	typename TypedDPAttributeData<T>::Ptr getValidTypedAttributedata(
		const std::string&);
	template<typename T>
	typename TypedDPAttributeData<T>::ConstPtr getValidTypedAttributedata(
		const std::string&) const;
	
	
protected:
	
	DPointList pointArray;
	DPAttrList mAttributeList;
	DPGrpList mGrooupList;
	
    //transform
    MMatrix m_mat;
	
};








inline DPAttributeData::Ptr DPrimBaseData::operator[](const std::string& name)
{
    const auto entry = mAttributeList.find(name);
    if (entry == mAttributeList.end()) return {};
    return entry->second;
}

inline DPAttributeData::ConstPtr DPrimBaseData::operator[](const std::string& name) const
{
    const auto entry = mAttributeList.find(name);
    if (entry == mAttributeList.end()) return {};
    return entry->second;
}

template <typename T>
inline typename T::Ptr DPrimBaseData::getAttribute(const std::string& name)
{
    return dommy::attribute::findTypedAttribute<T>(mAttributeList, name);
}

template <typename T>
inline typename T::ConstPtr DPrimBaseData::getAttribute(const std::string& name) const
{
    return dommy::attribute::findTypedAttribute<const T>(mAttributeList, name);
}

template <typename T>
inline const std::vector<T>& DPrimBaseData::attrListValue(const std::string& name) const
{
    const auto found = getValidTypedAttributedata<T>(name);
    if (!found) throw std::invalid_argument("Missing primitive attribute or incompatible attribute type: " + name);
    return found->value();
}

template <typename T>
inline typename TypedDPAttributeData<T>::Ptr
DPrimBaseData::getValidTypedAttributedata(const std::string& name)
{
    const auto found = getAttribute<TypedDPAttributeData<T>>(name);
    return found;
}

template <typename T>
inline typename TypedDPAttributeData<T>::ConstPtr
DPrimBaseData::getValidTypedAttributedata(const std::string& name) const
{
    const auto found = getAttribute<TypedDPAttributeData<T>>(name);
    return found;
}

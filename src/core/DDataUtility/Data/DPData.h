//
//  DPData.h
//  dommyTool
//
//  Created by yuzo on 2015/04/10.
//
//

#pragma once
#include "common/utility/DAttributeLookup.h"

#include <stdexcept>
#include <functional>



#include <maya/MPxData.h>
#include <maya/MPointArray.h>
#include <maya/MPoint.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MBoundingBox.h>

#include "DType.h"
#include "DPAttributeData.h"
#include "DPGroupData.h"
#include "common/utility/DMayaPluginData.h"


#include "DMayaDebug.h"



class DPData final : public MPxData
{
public:
	
    DPData();
    ~DPData() override;
    MStatus         readASCII( const MArgList& argList, unsigned& idx ) override;
    MStatus         readBinary( istream& in, unsigned length ) override;
    MStatus         writeASCII( ostream& out ) override;
    MStatus         writeBinary( ostream& out ) override;
    void            copy ( const MPxData& ) override;
	MTypeId typeId() const override;
	MString name() const override;
	static void * creator();
    
    //
    // Helper methods
    //
    MStatus readPointArrayASCII( const MArgList&, unsigned& );
    MStatus writePointArrayASCII( ostream& out );
    
    
    
    // Exact-key lookup for the historical public array interface.
    DPAttributeData::Ptr operator[](const std::string&);
    DPAttributeData::ConstPtr operator[](const std::string&) const;
    
    
    template<typename T> typename T::Ptr getAttribute(const std::string&);
    template<typename T> typename T::ConstPtr getAttribute(const std::string&) const;
	
	
    template<typename T> const std::vector<T>& attrListValue(const std::string&) const;
	
	template<typename T> T attrListValueWithIndex(const std::string&, size_t);
	template<typename T> T attrListValueWithIndex(const std::string& , size_t) const;
	
	template<typename T> void setAttrListValueWithIndex(const std::string&, size_t, T);
	
	
	template<typename T> T attrDefaultValue(const std::string&);
	template<typename T> T attrDefaultValue(const std::string&) const;
	
	
    // Functions for iterating over the point
    DPointIter beginPt() {
		markBBoxDirty();
		return pointArray.begin();
	}
    DPointIter endPt() {
		markBBoxDirty();
		return pointArray.end();
	}
    ConstDPointIter beginPt() const { return pointArray.begin(); }
    ConstDPointIter endPt() const { return pointArray.end(); }
    
    // Functions for iterating over the attribute
    DPAttrIter beginAttr() { return mAttributeList.begin(); }
    DPAttrIter endAttr() { return mAttributeList.end(); }
    ConstDPAttrIter beginAttr() const { return mAttributeList.begin(); }
    ConstDPAttrIter endAttr() const { return mAttributeList.end(); }
	
	// Functions for iterating over the group
	DPGrpIter beginGrp() { return mGrooupList.begin(); }
	DPGrpIter endGrp() { return mGrooupList.end(); }
	ConstDPGrpIter beginGrp() const { return mGrooupList.begin(); }
	ConstDPGrpIter endGrp() const { return mGrooupList.end(); }
	
	
	
    // Transactional internal copy; validation/allocation failure throws without
    // changing the destination. Compute callers must use their exception guard.
    void setDPData(const DPData& data);
    bool mergeDPData(const DPData& data);
    // Owning batch transactions. Sources may alias this destination; failure
    // leaves its payload and cached bounds unchanged. Placement visits every
    // copied point, including batch zero, in source order.
    using PointCopyOperation = std::function<MStatus(DPoint&, std::size_t, std::size_t)>;
    MStatus mergeFrom(const std::vector<const DPData*>& sources);
    MStatus copyRepeatedFrom(const DPData& source, std::size_t copies,
                            const PointCopyOperation& operation = PointCopyOperation());
    void cleanDataAll();
	
	//point method
    DPointList getPosition();
	DPointList getPositionConst() const;
    const DPointList* getPositionPtrConst() const { return &pointArray; }
	const DPoint& positionAt(std::size_t index) const {
		return pointArray.at(index);
	}
    MPointArray getMPosition() const;
	
	bool addPoints(const DPointList& points);
	
	
    
    // Stage XYZ conversion before replacement; invalid input leaves positions
    // and the bounding-box dirty state unchanged. Metadata is not resized here.
	void setPosition(const MPointArray &pts);
	void setPosition(const MFloatPointArray &pts);
    void setPosition(const DPointList &pts);
	void setPosition(const DPoint &pt, size_t index);
	bool adoptPositions(DPointList&& points);
	bool replacePositions(const DPointList& points);
//    size_t numberOfPoints() const;
	size_t numberOfPoints() const { return pointArray.size(); }
	bool hasValidPointTopology() const;
	bool deletePointFromIds(const DPIdList& ids);
    bool sortPointFromIds(const DPIndexList& ids);
    bool remapPointFromIds(const DPIndexList& ids);
    
	//attribute method
	bool isExistAttr(const std::string& name) const;
	void addAttr(const std::string& name, const DPAttributeData& attr);
	void removeAttr(const std::string& name);
    DPAttributeData::ConstPtr getAttr(const std::string& name) const;
	DPAttributeData::Ptr getAttrPtr(const std::string& name);
	DPAttributeData::ConstPtr getAttrPtr(
        const std::string& name) const;
	
	
	//add attribute
    template <typename T>
    void addAttrWithValues(const std::string& name,  const std::vector<T> &values);
	
    template <typename S, typename T>
    void setAttrWithValues(const std::string& name, const T &values);
    
    void setDPAttributeValues(const std::string& name, const DScalarDataList &values);
    void setDPAttributeValues(const std::string& name, const DVec2DataList &values);
    void setDPAttributeValues(const std::string& name, const DVec3DataList &values);
	void setDPAttributeValues(const std::string& name, const MFloatPointArray &values);
	
	size_t numberOfAttributes() const;
	void extendAttributeFromCount(const size_t &count);
    //attribute name methd
    DStringList getAttributeNameList() const;
    DStringList getAllAttributeList() const;
    DComponentType getAttributeComponentType(const std::string& name) const;
	
	
	
	//group method
    bool isExistGrp(const std::string& name) const;
    void addGrp(const std::string& name, const DPGroupData& attr);
    // Checked point-group replacement; ordinary attributes are unaffected.
    bool replaceGrp(const std::string& name, const DPIdList& values);
    void removeGrp(const std::string& name);
    void removeGrps(const MString nameList);
	DPGroupData::Ptr getGrp(const std::string& name);
	DPGroupData::ConstPtr getGrp(const std::string& name) const;
	size_t numberOfGroups() const;
	void deleteGroupFromIds(const DPIdList &ids);
	void sortGroupFromIds(const DPIndexList &ids);
	void extendGroupFromCount(const size_t &count);
    //group name methd
    DStringList getGroupNameList() const;
    DStringList getAllGroupList() const;
    DComponentType getGroupComponentType(const std::string& name) const;
    
    //BBox
    void getBBox(MBoundingBox &bbox) const;
	MBoundingBox getBoundingBox() const;
    void updateBBox();
    
    
	template<typename T>
	typename TypedDPAttributeData<T>::Ptr getValidTypedAttributedata(const std::string&);
	template<typename T>
	typename TypedDPAttributeData<T>::ConstPtr getValidTypedAttributedata(const std::string&) const;
	
	
public:
	static const MString typeName;
	static const MTypeId id;
	
private:
	bool appendMetadataFrom(const DPData& source, std::size_t destinationPointCount);
	void markBBoxDirty() { mBBoxDirty = true; }
	MBoundingBox calculateBoundingBox() const;
    
	DPointList pointArray;
	DPAttrList mAttributeList;
	DPGrpList mGrooupList;
    MBoundingBox mBBox;
	bool mBBoxDirty;
};




inline DPAttributeData::Ptr DPData::operator[](const std::string& name)
{
    const auto entry = mAttributeList.find(name);
    if (entry == mAttributeList.end()) return {};
    return entry->second;
}

inline DPAttributeData::ConstPtr DPData::operator[](const std::string& name) const
{
    const auto entry = mAttributeList.find(name);
    if (entry == mAttributeList.end()) return {};
    return entry->second;
}

template <typename T>
inline typename T::Ptr DPData::getAttribute(const std::string& name)
{
    return dommy::attribute::findTypedAttribute<T>(mAttributeList, name);
}

template <typename T>
inline typename T::ConstPtr DPData::getAttribute(const std::string& name) const
{
    return dommy::attribute::findTypedAttribute<const T>(mAttributeList, name);
}

template <typename T>
inline const std::vector<T>& DPData::attrListValue(const std::string& name) const
{
    const auto found = getValidTypedAttributedata<T>(name);
    return found->value();
}

template <typename T>
inline T DPData::attrListValueWithIndex(const std::string& name, size_t index)
{
    return getValidTypedAttributedata<T>(name)->getValue(index);
}

template <typename T>
inline T DPData::attrListValueWithIndex(const std::string& name, size_t index) const
{
    return getValidTypedAttributedata<T>(name)->getValue(index);
}

template <typename T>
inline void DPData::setAttrListValueWithIndex(const std::string& name, size_t index, T value)
{
    const auto found = getValidTypedAttributedata<T>(name);
    if (!found->setValueAt(index, value))
        throw std::out_of_range("DPData::setAttrListValueWithIndex index is out of range");
}

template <typename T>
inline T DPData::attrDefaultValue(const std::string& name)
{
    return getValidTypedAttributedata<T>(name)->getDefaultValue();
}

template <typename T>
inline T DPData::attrDefaultValue(const std::string& name) const
{
    return getValidTypedAttributedata<T>(name)->getDefaultValue();
}

template <typename T>
inline typename TypedDPAttributeData<T>::Ptr
DPData::getValidTypedAttributedata(const std::string& name)
{
    const auto found = getAttribute<TypedDPAttributeData<T>>(name);
    if (!found) throw std::invalid_argument("Missing DPData attribute or incompatible attribute type: " + name);
    return found;
}

template <typename T>
inline typename TypedDPAttributeData<T>::ConstPtr
DPData::getValidTypedAttributedata(const std::string& name) const
{
    const auto found = getAttribute<TypedDPAttributeData<T>>(name);
    if (!found) throw std::invalid_argument("Missing DPData attribute or incompatible attribute type: " + name);
    return found;
}

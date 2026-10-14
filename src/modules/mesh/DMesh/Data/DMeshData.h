//
//  DMeshData.hpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//



#pragma once
#include "common/utility/DAttributeLookup.h"

#include <maya/MPxData.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDoubleArray.h>

#include <maya/MBoundingBox.h>

#include <vector>
#include <memory>
#include <functional>
#include <cstddef>



#include "DPAttributeData.h"
#include "DPGroupData.h"
#include "DPrimBaseData.h"
#include "common/utility/DMayaPluginData.h"




enum class DMeshReplacementPolicy {
	ResetMetadata,
	RequireCompatibleMetadata
};

// Generated geometry keeps automatic normals unless the caller is importing
// shading from an existing Maya mesh. This preserves legacy generator behavior.
enum class DMeshNormalImportPolicy {
	Recompute,
	PreserveFaceVertex
};

class DMeshInternalData : public DPrimBaseData
{
public:
	
	typedef std::shared_ptr<DMeshInternalData> Ptr;
	typedef std::shared_ptr<const DMeshInternalData> CPtr;
    
    
    
    
		
		DMeshInternalData();
		DMeshInternalData(DMeshInternalData const &src);
		DMeshInternalData(const MPointArray &pts, const MIntArray &faceCountList, const MIntArray &connectList);
	    DMeshInternalData(const DPointList &pts, const MIntArray &faceCountList, const MIntArray &connectList);
	    DMeshInternalData(const DPointList &pts, const MIntArray &faceCountList, const MIntArray &connectList,
	                      const MFloatArray &ucoords, const MFloatArray &vcoords, const MIntArray &uvcounts, const MIntArray &uvconnects);
		virtual ~DMeshInternalData();

		static MStatus createFromMayaObject(
			const MObject& object,
			Ptr& result,
			MSpace::Space space = MSpace::kObject,
			DMeshNormalImportPolicy normals = DMeshNormalImportPolicy::Recompute);
	
	
	DMeshInternalData::Ptr copy();
	DMeshInternalData::CPtr copy() const;
	DMeshInternalData::Ptr copyNewMesh() const;
	void copy(const DMeshInternalData& other);
    DMeshInternalData& operator=(const DMeshInternalData& rhs);
    
    
	//get mesh info
    
	MIntArray getFaceList() { return arrays_->mFaceList;}
    MIntArray getFaceList() const { return arrays_->mFaceList;}
	MIntArray getConnectList() { return arrays_->mConnectList;}
    MIntArray getConnectList() const { return arrays_->mConnectList;}
	int numFace() const { return arrays_->mFaceList.length();}
    std::size_t numberOfConnections() const { return arrays_->mConnectList.length(); }
    std::size_t numberOfUVs() const { return arrays_->m_u_vals.length(); }
    // Checked copies into distinct local stages; discard both outputs on failure.
    MStatus copyTopology(MIntArray& faces, MIntArray& connections) const;
	bool hasValidPolygonTopology() const;
	bool hasValidUVTopology() const;
	bool hasValidMeshData() const;
	bool replacePositions(const DPointList& points);
    // Apply the complete local-to-output affine matrix, including any existing
    // primitive transform supplied by the caller. Preserve metadata, transform
    // explicit normals by inverse transpose and reset the matrix to identity.
    // Failure leaves every part of the mesh unchanged.
    MStatus bakeTransform(const MMatrix& matrix);
	// Preserve point/face metadata; remap corners, UV assignments and edge groups.
	// Any failure leaves the original mesh unchanged.
	bool reverseWinding();
	
	    bool setReplaceMeshData(
			const DPointList &pts,
			const MIntArray &faceList,
			const MIntArray &connectList,
			DMeshReplacementPolicy policy =
				DMeshReplacementPolicy::ResetMetadata);
		bool setReplaceMeshData(
			const MPointArray &pts,
			const MIntArray &faceList,
			const MIntArray &connectList,
			DMeshReplacementPolicy policy =
				DMeshReplacementPolicy::ResetMetadata);
	    bool setReplaceMeshData(
			const MObject &meshObj,
			DMeshReplacementPolicy policy =
				DMeshReplacementPolicy::ResetMetadata);
    
    bool deleteFaceFromIds(const DPIdList &ids, bool &isAllDelete);
    void deleteEdgeFromIds(const DPIdList &ids);
    
	//Uv method
    bool hasUV() const;
	void hasUVData(bool &isUvs);
    void hasUVData(bool &isUvs) const;
	MStatus getUvData(MFloatArray &ucoords, MFloatArray &vcoords, MIntArray &uvcounts, MIntArray &uvconnects );
    MStatus getUvData(MFloatArray &ucoords, MFloatArray &vcoords, MIntArray &uvcounts, MIntArray &uvconnects ) const;
	// Invalid input or allocation/copy failure leaves all mesh data unchanged.
	bool setUvData(const MFloatArray &ucoords, const MFloatArray &vcoords, const MIntArray &uvcounts, const MIntArray &uvconnects );
	
	void clearUVs();
	
	
    // Build once in transformed coordinates; the result carries identity.
    // Explicit corner normals survive with inverse-transpose transforms;
    // other component metadata is reset. UVs survive only if all nonempty inputs
    // have complete UVs. Failure leaves result unchanged.
    static MStatus createMerged(
        const std::vector<const DMeshInternalData*>& sources, Ptr& result);
    
    
	
    
	//for display data
	
    
    
    
    
    //Component Attribute method
    DPAttrIter beginFaceAttr() { return mFaceAttributeList.begin(); }
    DPAttrIter endFaceAttr() { return mFaceAttributeList.end(); }
    ConstDPAttrIter beginFaceAttr() const { return mFaceAttributeList.cbegin(); }
    ConstDPAttrIter endFaceAttr() const { return mFaceAttributeList.cend(); }
    DPAttrIter beginPVtxAttr() { return mPVtxAttributeList.begin(); }
    DPAttrIter endPVtxAttr() { return mPVtxAttributeList.end(); }
    ConstDPAttrIter beginPVtxAttr() const { return mPVtxAttributeList.cbegin(); }
    ConstDPAttrIter endPVtxAttr() const { return mPVtxAttributeList.cend(); }
	
    DStringList getAllCompAttrName(const DComponentType &cType) const;
    DStringList getAllCompAttrInfo(const DComponentType &cType) const;
    
    bool isExistCompAttr(const DComponentType &cType, const std::string& name) const;
    bool addCompAttr(const DComponentType &cType, const std::string& name, const DPAttributeData& attr);
    bool removeCompAttr(const DComponentType &cType, const std::string& name);
    bool removeCompAttrAll(const DComponentType &cType);
    DPAttributeData::ConstPtr getCompAttr(const DComponentType &cType, const std::string& name) const;
    DPAttributeData::Ptr getCompAttr(const DComponentType &cType, const std::string& name);
    DString getCompAttrTypeName(const DComponentType &cType,const std::string& name) const;
    size_t numberOfCompAttr(const DComponentType &cType) const;
    
    
    
    
    
    //Component Group method
    DPGrpIter beginFaceGrp() { return mFaceGroupList.begin(); }
    DPGrpIter endFaceGrp() { return mFaceGroupList.end(); }
    ConstDPGrpIter beginFaceGrp() const { return mFaceGroupList.cbegin(); }
    ConstDPGrpIter endFaceGrp() const { return mFaceGroupList.cend(); }
    
    DPGrpIter beginEdgeGrp() { return mEdgeGroupList.begin(); }
    DPGrpIter endEdgeGrp() { return mEdgeGroupList.end(); }
    ConstDPGrpIter beginEdgeGrp() const { return mEdgeGroupList.cbegin(); }
    ConstDPGrpIter endEdgeGrp() const { return mEdgeGroupList.cend(); }
    
    
    DStringList getAllCompGroupName(const DComponentType &cType) const;
    DStringList getAllCompGroupInfo(const DComponentType &cType) const;
    
    bool isExistCompGrp(const DComponentType &cType, const std::string& name) const;
    bool addCompGrp(const DComponentType &cType, const std::string& name, const DPGroupData& attr);
    // Combine a selection on an unpublished output copy; never mutate shared masks.
    bool combineCompGrp(const DComponentType& cType, const std::string& name,
                        std::size_t count, const DPIdList& selection, int action, bool invert);
    bool removeCompGrp(const DComponentType &cType, const std::string& name);
    bool removeCompGrpAll(const DComponentType &cType);
    DPGroupData::ConstPtr getCompGrp(const DComponentType &cType, const std::string& name) const;
    size_t numberOfCompGroups(const DComponentType &cType) const;
    bool deleteCompGroupFromIds(const DComponentType &cType, const DPIdList &ids);
    bool sortCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids);
    
    bool remapCompGroupFromIds(const DComponentType &cType, const DPIndexList &ids);
	
    
    
    
protected:
    
    // The six Maya arrays are committed together without copying or allocating.
    struct ArrayStorage {
        ArrayStorage() = default;
        ArrayStorage(const ArrayStorage&) = delete;
        ArrayStorage& operator=(const ArrayStorage&) = delete;
        MIntArray mFaceList, mConnectList;
        MFloatArray m_u_vals, m_v_vals;
        MIntArray m_uv_count_list, m_uv_connect_list;
    };
    std::unique_ptr<ArrayStorage> arrays_{new ArrayStorage()};
    void swapState(DMeshInternalData& other) noexcept;
    
	//uv method
	void uvInitialize();
    
    
    //attribute method
    const DPAttrList *AttributeListConst(const DComponentType &cType) const;
    DPAttrList *AttributeList(const DComponentType &cType);
    
    
	//group method
    const DPGrpList *GroupListConst(const DComponentType &cType) const;
    DPGrpList *GroupList(const DComponentType &cType);
	
    
    //componet Attribute
    DPAttrList mFaceAttributeList;
    DPAttrList mPVtxAttributeList;
    
    
    //componet group
    DPGrpList mFaceGroupList;
    DPGrpList mEdgeGroupList;
    
    
    
    
    
};


typedef DMeshInternalData::Ptr DMItem;
typedef DMeshInternalData::CPtr ConstDMItem;




class DMeshData final : public MPxData
{
public:
	
	//curves
	typedef std::vector<DMItem> DMeshList;
	typedef std::vector<ConstDMItem> ConstDMeshList;
	typedef DMeshList::iterator DMeshIter;
	typedef DMeshList::const_iterator ConstDMeshIter;
	
	//attribute
	typedef std::map<std::string, DPAttributeData::Ptr> DPrimAttrList;
	typedef DPrimAttrList::iterator DPrimAttrListIter;
	typedef DPrimAttrList::const_iterator ConstDPrimAttrListIter;
	
	//group
	typedef std::unordered_map<std::string, DPGroupData::Ptr > DPrimGrpList;
	typedef DPrimGrpList::iterator DPrimGrpListIter;
	typedef DPrimGrpList::const_iterator ConstDPrimGrpListIter;
	
	
	
	
	/////////////////////Maya
	DMeshData();
	~DMeshData() override;
	MStatus         readASCII( const MArgList&, unsigned&  ) override;
	MStatus         readBinary( istream&, unsigned length ) override;
	MStatus         writeASCII( ostream&  ) override;
	MStatus         writeBinary( ostream&  ) override;
	void            copy ( const MPxData& ) override;
	// Internal callers must check this result before publishing or mutating data.
	// Geometry staging capacity is admitted before cloning. Failure leaves the
	// destination unchanged, including for allocation errors. Self-copy is a no-op.
	MStatus         copyFrom(const DMeshData& other);
	// Synchronous operation on each independently owned clone after the original
	// batch (copyIndex > 0). Do not retain references or mutate the source here.
	using MeshCopyOperation = std::function<MStatus(DMeshInternalData&, std::size_t copyIndex,
	                                               std::size_t meshIndex)>;
	MStatus copyRepeatedFrom(const DMeshData& source, std::size_t copies,
	                         const MeshCopyOperation& operation = MeshCopyOperation());
	MTypeId         typeId() const override;
	MString         name() const override;
	
	
	static void * creator();
	
	static const MString typeName;
	static const MTypeId id;
	
	
	
	
	
	/////////////////////
	// mesh iterator
	DMeshIter beginMeshes() {
		markBBoxDirty();
		return meshItemArray.begin();
	}
	DMeshIter endMeshes() {
		markBBoxDirty();
		return meshItemArray.end();
	}
	ConstDMeshIter beginMeshes() const { return meshItemArray.cbegin(); }
	ConstDMeshIter endMeshes() const { return meshItemArray.cend(); }
    DMeshIter beginPrims() {
		markBBoxDirty();
		return meshItemArray.begin();
	}
    DMeshIter endPrims() {
		markBBoxDirty();
		return meshItemArray.end();
	}
    ConstDMeshIter beginPrims() const { return meshItemArray.cbegin(); }
    ConstDMeshIter endPrims() const { return meshItemArray.cend(); }
	
	
	size_t numberOfMesh() const {return meshItemArray.size();}
	bool getCheckedTotalVertex(size_t &totalVtx) const;
	void getTotalVertex(size_t &totalVtx) const;
    size_t numTotalVertex() const;
    
	bool getCheckedTotalFace(size_t &totalFace) const;
	void getTotalFace(size_t &totalFace) const;
	bool getCheckedTotalConnect(size_t &totalConnect) const;
	void getTotalConnect(size_t &totalConnect) const;
    void getTotalVertexForDisplay(size_t &totalvtx) const;
    bool getCheckedTotalVertexForDisplay(size_t &totalvtx) const;
	bool hasValidMeshTopology() const;
    
    
    const DMeshList* getMeshListConst() const { return &meshItemArray; }
	const DMeshInternalData& getMesh(size_t index) const;
    DMItem getMeshPtr(size_t index);
	ConstDMItem getMeshPtrConst(size_t index) const;
	
    bool insert(const DMeshInternalData &data);
	bool insert(const DMItem&);
	bool insert(const ConstDMItem&);
	bool insert(const DMeshList&);
    bool expand(const DMItem&, size_t itemIndex);
    // Source indices refer to the pre-append primitive metadata. Commit once.
    bool expand(const DMeshList& meshes, const DPIndexList& sourceIndices);
    
	bool deleteFromIds(const DPIdList& ids);
	
	// Transactional two-input assignment. Inputs may alias this destination.
	// Empty inputs are identities; nonempty inputs retain the shared schemas.
	MStatus copyMergedFrom(const DMeshData& base, const DMeshData& target);
	bool mergeDMeshData(const DMeshData& data);
	
	void clear();
	
	
	
	//uv
	bool hasUVs() const;
	
	
	/////////////////////
	//Center
	void getPrimitiveCenterList(DPointList &centerList) const;
	// Staged double centers; invalid data or Maya allocation/copy failure throws.
	void getPrimitiveCenterList(MVectorArray &centerList) const;
	
	/////////////////////
	//Bounding box
	void updateBBox();
	MBoundingBox getBoundingBox() const;
	
	
	/////////////////////
	//mesh attribute
	DPAttributeData::Ptr operator[](const std::string&);
	DPAttributeData::ConstPtr operator[](const std::string&) const;
	
	template<typename T> typename T::Ptr getAttribute(const std::string&);
	template<typename T> typename T::ConstPtr getAttribute(const std::string&) const;
	
	template<typename T> const std::vector<T>& attrListValue(const std::string&) const;
	
	// Functions for iterating over the attribute
	DPrimAttrListIter beginAttr() { return mAttributeList.begin(); }
	DPrimAttrListIter endAttr() { return mAttributeList.end(); }
	ConstDPrimAttrListIter beginAttr() const { return mAttributeList.cbegin(); }
	ConstDPrimAttrListIter endAttr() const { return mAttributeList.cend(); }
	
	/////////////////////
	//primitive attribute
	bool isExistAttr(const std::string& name) const;
	void addAttr(const std::string& name, const DPAttributeData& attr);
	void removeAttr(const std::string& name);
	DPAttributeData::ConstPtr getAttr(const std::string& name) const;
	DPAttributeData::Ptr getAttrPtr(const std::string& name);
	DPAttributeData::ConstPtr getAttrPtr(
        const std::string& name) const;
	DString getAttributeType(const std::string& name) const;
	
	
	//add attribute
    template<typename T> void addDPAttributeValues(const std::string& name, const std::vector<T> &values);
    
    
    
    
    //set attribute
	void setDPAttributeValues(const std::string& name, const DScalarDataList &values);
    void setDPAttributeValues(const std::string& name, const DVec2DataList &values);
	void setDPAttributeValues(const std::string& name, const DVec3DataList &values);
	
    
    
	
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
    DStringList getPrimAttributeList() const;
	DStringList getPointAttributeList() const;
	
    //Component Attribute info
    bool isExistCompAttr(const DComponentType &cType, const std::string& name) const;
    size_t numberOfCompAttr(const DComponentType &cType) const;
    DStringList getCompAttrInfo(const DComponentType &cType) const;
    DStringList getAllCompAttrName(const DComponentType &cType) const;
	DStringList getAllAttributeList() const;
	DComponentType getAttributeComponentType(const std::string& name) const;
	
	/////////////////////
	//group
	// Functions for iterating over the group
	DPrimGrpListIter beginGrp() { return mGroupList.begin(); }
	DPrimGrpListIter endGrp() { return mGroupList.end(); }
	ConstDPrimGrpListIter beginGrp() const { return mGroupList.cbegin(); }
	ConstDPrimGrpListIter endGrp() const { return mGroupList.cend(); }
	
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
	// Synchronous input views only: no borrowed pointer is retained in the output.
	// The transaction validates capacity first and creates one owned copy per item.
	typedef std::vector<const DMeshInternalData*> MeshSourceList;
	bool appendMeshes(const MeshSourceList& meshes, const DPIndexList* sourceIndices = nullptr);
	
	DMeshList meshItemArray;
	MBoundingBox mBBox;
	bool mBBoxDirty;
	
	DPrimAttrList mAttributeList;
	DPrimGrpList mGroupList;
private:
	void markBBoxDirty() { mBBoxDirty = true; }
	MBoundingBox calculateBoundingBox() const;
};







inline DPAttributeData::Ptr
DMeshData::operator[](const std::string& name)
{
	DPrimAttrListIter iter = mAttributeList.find(name);
	return (iter == endAttr() ? DPAttributeData::Ptr() : iter->second);
}

inline DPAttributeData::ConstPtr
DMeshData::operator[](const std::string &name) const
{
	ConstDPrimAttrListIter iter = mAttributeList.find(name);
	return (iter == endAttr() ? DPAttributeData::Ptr() : iter->second);
}



template <typename T>
inline typename T::Ptr
DMeshData::getAttribute(const std::string &name)
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
DMeshData::getAttribute(const std::string &name) const
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
inline const std::vector<T>& DMeshData::attrListValue(const std::string &name) const
{
	typename TypedDPAttributeData<T>::ConstPtr m =
		getValidTypedAttributedata<T>(name);
	return m->value();
}



template <typename T>
inline typename TypedDPAttributeData<T>::Ptr
DMeshData::getValidTypedAttributedata(const std::string &name)
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
DMeshData::getValidTypedAttributedata(const std::string &name) const
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

#pragma once

// Complete production merge transactions with real attribute/group containers.
// Geometry/UV payload transport and native bounding boxes are scalar doubles.
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMeshCopyBudget.h"
#include <maya/MGlobal.h>
#include <maya/MVector.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>
#include <set>
#include <iterator>
#include <limits>
#include <vector>
#include "DDataUtility/Data/DPGroupData.h"

using DString = std::string;
using DStringList = std::vector<std::string>;
enum DComponentType {D_COMP_POINT, D_COMP_FACE, D_COMP_EDGE, D_COMP_PER_VTX};
using DPrimAttrList = DPAttrList;
using DPrimGrpList = DPGrpList;
using ConstDPrimGrpListIter = DPrimGrpList::const_iterator;
static int meshCopies = 0, attributeCopies = 0, groupCopies = 0, boundsCalls = 0;
static int copyFailure = 0, failCopyAt = -1;
static int failAttributeAt = -1, failGroupAt = -1, metadataFailure = 0;
static bool failBounds = false, failMetadata = false;
static std::size_t linearSearchCalls = 0U;
static int requestedCount = 3, matrixCalls = 0, bakeCalls = 0, setMatrixCalls = 0;
static bool failBake = false;
static MVector translation, rotation, scaling(1, 1, 1), pivotValue;
static double uniformScale = 1.0;
static short rotationOrder = 0;
static bool freezeTransform = false;

template <typename T> struct TrackedAttribute : TypedDPAttributeData<T> {
    using TypedDPAttributeData<T>::TypedDPAttributeData;
    DPAttributeData::Ptr copy() const override {
        ++attributeCopies;
        if (failMetadata) throw std::bad_alloc();
        if (attributeCopies == failAttributeAt && metadataFailure == 1) throw std::bad_alloc();
        if (attributeCopies == failAttributeAt && metadataFailure == 2) return {};
        auto result = std::make_shared<TrackedAttribute<T>>(*this);
        if (attributeCopies == failAttributeAt && metadataFailure == 3) result->incrementDefaultValue(1U);
        return result;
    }
};
struct TrackedGroup : DPGroupData {
    using DPGroupData::DPGroupData;
    DPGroupData::Ptr copy() const override {
        ++groupCopies;
        if (groupCopies == failGroupAt && metadataFailure == 1) throw std::bad_alloc();
        if (groupCopies == failGroupAt && metadataFailure == 2) return {};
        DPIdList values = getGroupIds();
        if (groupCopies == failGroupAt && metadataFailure == 3) values.push_back(false);
        return std::make_shared<TrackedGroup>(values);
    }
};

void CopyAttributeToAttribute(DPAttrList&, const DPAttrList&);
void CopyGroupToGroup(DPGrpList&, const DPGrpList&);
void ExpandGroupListSize(DPGrpList&, const std::size_t&);
bool MergeGroupToGroup(DPGrpList&, const DPGrpList&);
bool appendOtherDPAttribute(const DPAttributeData::Ptr&, const DPAttributeData::ConstPtr&);
void removeFromGroup(DPGrpList& groups, const std::string& name) {groups.erase(name);}
void IntersectDStringList(DStringList& output, const DStringList& input) {
    ++linearSearchCalls;
    // The historical helper is a deduplicating union despite its name.
    for (const auto& name : input)
        if (std::find(output.begin(), output.end(), name) == output.end()) output.push_back(name);
}
bool isContainDStringList(const DStringList& names, const std::string& name) {
    ++linearSearchCalls;
    return std::find(names.begin(), names.end(), name) != names.end();
}

struct DMeshInternalData {
    using Ptr = std::shared_ptr<DMeshInternalData>;
    DPAttrList pointAttrs, faceAttrs, cornerAttrs;
    DPGrpList groups, faceGroups, edgeGroups;
    std::vector<int> geometry{0, 1, 2}, uvIds{0, 1, 2};
    double transform = 1.0;
    MMatrix matrix;
    bool uvs = true, valid = true;
    std::size_t numberOfPoints() const {return geometry.size();}
    int numFace() const {return 1;}
    std::size_t numberOfConnections() const {return 3U;}
    std::size_t numberOfUVs() const {return uvs ? uvIds.size() : 0U;}
    bool hasUV() const {return uvs;}
    bool hasValidUVTopology() const {return !uvs || uvIds.size() == 3U;}
    bool hasValidMeshData() const {
        dommy::attribute::ValidationBudget cost;
        if (!valid || !dommy::attribute::isFiniteMatrix(matrix) || !hasValidUVTopology() ||
            !dommy::attribute::validateAttributeList(pointAttrs, geometry.size(), cost) ||
            !dommy::attribute::validateAttributeList(faceAttrs, 1U, cost) ||
            !dommy::attribute::validateAttributeList(cornerAttrs, 3U, cost)) return false;
        for (const auto* list : {&groups, &faceGroups, &edgeGroups})
            for (const auto& group : *list)
                if (!group.second || group.second->getGroupIds().size() !=
                    (list == &faceGroups ? 1U : 3U)) return false;
        return true;
    }
    DMeshInternalData() = default;
    DMeshInternalData(const DMeshInternalData& other) {
        const int index = meshCopies++;
        if (index == failCopyAt && copyFailure == 1) throw std::bad_alloc();
        geometry = other.geometry; uvIds = other.uvIds; transform = other.transform;
        matrix = other.matrix; uvs = other.uvs; valid = other.valid;
        CopyAttributeToAttribute(pointAttrs, other.pointAttrs);
        CopyAttributeToAttribute(faceAttrs, other.faceAttrs);
        CopyAttributeToAttribute(cornerAttrs, other.cornerAttrs);
        CopyGroupToGroup(groups, other.groups);
        CopyGroupToGroup(faceGroups, other.faceGroups);
        CopyGroupToGroup(edgeGroups, other.edgeGroups);
        if (index == failCopyAt && copyFailure == 3) valid = false;
    }
    Ptr copyNewMesh() const {
        if (meshCopies == failCopyAt && copyFailure == 2) {++meshCopies; return {};}
        return std::make_shared<DMeshInternalData>(*this);
    }
    MMatrix getTransformMatrix() const {return matrix;}
    void setTransformMatrix(const MMatrix& value) {++setMatrixCalls; matrix = value;}
    MStatus bakeTransform(const MMatrix& value) {
        ++bakeCalls; if (failBake) return MS::kFailure;
        transform = value[3][0]; matrix = MMatrix(); return MS::kSuccess;
    }
    Ptr copy() const {return copyNewMesh();}
    DStringList getAllAttributeName() const {
        DStringList result;
        for (const auto& field : pointAttrs) result.push_back(field.first);
        return result;
    }
    DStringList getAllCompGroupName(DComponentType type) const {
        DStringList result;
        const auto& list = type == D_COMP_POINT ? groups : type == D_COMP_FACE ? faceGroups : edgeGroups;
        for (const auto& group : list) result.push_back(group.first);
        return result;
    }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto field = pointAttrs.find(name);
        return field == pointAttrs.end() ? DPAttributeData::ConstPtr() : field->second;
    }
    DPAttrList::iterator beginAttr() {return pointAttrs.begin();}
    DPAttrList::iterator endAttr() {return pointAttrs.end();}
    DPAttrList::const_iterator beginAttr() const {return pointAttrs.begin();}
    DPAttrList::const_iterator endAttr() const {return pointAttrs.end();}
    ConstDPAttrIter beginFaceAttr() const {return faceAttrs.begin();}
    ConstDPAttrIter endFaceAttr() const {return faceAttrs.end();}
    ConstDPAttrIter beginPVtxAttr() const {return cornerAttrs.begin();}
    ConstDPAttrIter endPVtxAttr() const {return cornerAttrs.end();}
    DPGrpIter beginGrp() {return groups.begin();}
    DPGrpIter endGrp() {return groups.end();}
    ConstDPGrpIter beginGrp() const {return groups.begin();}
    ConstDPGrpIter endGrp() const {return groups.end();}
    DPGrpIter beginFaceGrp() {return faceGroups.begin();}
    DPGrpIter endFaceGrp() {return faceGroups.end();}
    ConstDPGrpIter beginFaceGrp() const {return faceGroups.begin();}
    ConstDPGrpIter endFaceGrp() const {return faceGroups.end();}
    DPGrpIter beginEdgeGrp() {return edgeGroups.begin();}
    DPGrpIter endEdgeGrp() {return edgeGroups.end();}
    ConstDPGrpIter beginEdgeGrp() const {return edgeGroups.begin();}
    ConstDPGrpIter endEdgeGrp() const {return edgeGroups.end();}
    void removeAttr(const std::string& name) {pointAttrs.erase(name);}
    void removeCompGrp(DComponentType type, const std::string& name) {
        (type == D_COMP_POINT ? groups : type == D_COMP_FACE ? faceGroups : edgeGroups).erase(name);
    }
    void clearUVs() {uvs = false; uvIds.clear();}
};
using DMItem = DMeshInternalData::Ptr;
using ConstDMItem = std::shared_ptr<const DMeshInternalData>;
using DMeshList = std::vector<DMItem>;
struct DMeshData {
    static const int id = 1;
    DMeshList meshItemArray;
    DPAttrList mAttributeList;
    DPGrpList mGroupList;
    std::size_t mBBox = 99U;
    bool mBBoxDirty = true;
    bool hasValidMeshTopology() const;
    MStatus copyFrom(const DMeshData&);
    MStatus copyMergedFrom(const DMeshData&, const DMeshData&);
    using MeshSourceList = std::vector<const DMeshInternalData*>;
    bool appendMeshes(const MeshSourceList&, const DPIndexList* = nullptr);
    using MeshCopyOperation = std::function<MStatus(DMeshInternalData&, std::size_t, std::size_t)>;
    MStatus copyRepeatedFrom(const DMeshData&, std::size_t, const MeshCopyOperation& = MeshCopyOperation());
    bool mergeDMeshData(const DMeshData&);
    bool hasUVs() const;
    DStringList getPointAttributeList() const;
    DP_ATTR_DATA_TYPE getPointAttributeType(const std::string&) const;
    DStringList getAllCompGroupName(const DComponentType&) const;
    bool isExistAttr(const std::string& name) const {return mAttributeList.count(name) != 0U;}
    DPAttributeData::Ptr getAttrPtr(const std::string& name) {return mAttributeList.at(name);}
    void removeAttr(const std::string& name) {mAttributeList.erase(name);}
    std::size_t numberOfMesh() const {return meshItemArray.size();}
    DMeshList::iterator beginMeshes() {return meshItemArray.begin();}
    DMeshList::iterator endMeshes() {return meshItemArray.end();}
    DMeshList::const_iterator beginMeshes() const {return meshItemArray.begin();}
    DMeshList::const_iterator endMeshes() const {return meshItemArray.end();}
    ConstDPAttrIter beginAttr() const {return mAttributeList.begin();}
    ConstDPAttrIter endAttr() const {return mAttributeList.end();}
    ConstDPGrpIter beginGrp() const {return mGroupList.begin();}
    ConstDPGrpIter endGrp() const {return mGroupList.end();}
    void updateBBox() {
        ++boundsCalls;
        if (failBounds) throw std::bad_alloc();
        mBBox = meshItemArray.size(); mBBoxDirty = false;
    }
};
// Complete Maya compute control flow; only host data handles and publication
// are doubled. The staging object uses the real extracted merge transaction.
static DMeshData inputData, targetData, stagingData, publishedData;
static int hostCalls = 0, hostFailAt = -1, hostThrowAt = -1;
static int creations = 0, publications = 0, cleanCalls = 0;
static MStatus hostCall() {
    const int call = hostCalls++;
    if (call == hostThrowAt) throw std::runtime_error("injected host exception");
    return call == hostFailAt ? MS::kFailure : MS::kSuccess;
}
struct MObject {
    std::string name;
    MObject(const char* value = "") : name(value) {}
};
struct MPlug {
    MObject object;
    bool operator==(const MObject& other) const {return object.name == other.name;}
    bool operator!=(const MObject& other) const {return !(*this == other);}
};
struct MDataHandle {
    bool valid;
    std::string name;
    int asInt() const {return requestedCount;}
    short asShort() const {return rotationOrder;}
    MVector asVector() const {
        return name == "aScale" ? scaling : name == "aTranslate" ? translation :
               name == "aRotate" ? rotation : pivotValue;
    }
    double asDouble() const {return uniformScale;}
    bool asBool() const {return freezeTransform;}
};
struct MDataBlock {
    MDataHandle inputValue(MObject object, MStatus* status) {*status = hostCall(); return {bool(*status), object.name};}
    MDataHandle outputValue(MObject, MStatus* status) {*status = hostCall(); return {bool(*status), "output"};}
    MStatus setClean(const MPlug&) {
        const auto status = hostCall(); if (status) ++cleanCalls; return status;
    }
};
struct MFnPluginData {
    void create(int, MStatus* status) {
        ++creations; *status = hostCall(); if (*status) stagingData = DMeshData();
    }
    MObject object() const {return {"staging"};}
};
static MStatus getValidInputDM(MObject object, MDataBlock&, const DMeshData*& result) {
    const auto status = hostCall();
    if (status) result = object.name == "input" ? &inputData : &targetData;
    return status;
}
namespace dommy { namespace maya {
template <typename T> T* pluginDataPointer(MFnPluginData&, MStatus* status) {
    *status = hostCall(); return *status ? &stagingData : nullptr;
}
template <typename T> MStatus publishTypedPluginDataObject(MDataHandle handle, MObject, int) {
    assert(handle.valid);
    const auto status = hostCall();
    if (status) {publishedData = stagingData; ++publications;}
    return status;
}
}}

// Matrix decomposition/rotation/pivot mathematics are a native host boundary.
// These doubles retain translation/scale and exercise all five API statuses.
using double3 = double[3];
enum {ROTATE_ORDER_XYZ, ROTATE_ORDER_YZX, ROTATE_ORDER_ZXY, ROTATE_ORDER_XZY, ROTATE_ORDER_YXZ, ROTATE_ORDER_ZYX};
namespace MSpace {enum {kObject};}
struct MTransformationMatrix {
    enum RotationOrder {kXYZ, kYZX, kZXY, kXZY, kYXZ, kZYX};
    MMatrix matrix;
    MTransformationMatrix() = default;
    explicit MTransformationMatrix(const MMatrix& value) : matrix(value) {}
    MStatus setRotatePivot(const MVector&, int, bool) {++matrixCalls; return hostCall();}
    MStatus setScalePivot(const MVector&, int, bool) {++matrixCalls; return hostCall();}
    MStatus addScale(const double3& value, int) {
        ++matrixCalls; const auto status = hostCall();
        if (status) for (int i = 0; i < 3; ++i) matrix[i][i] *= value[i];
        return status;
    }
    MStatus addRotation(const double3&, RotationOrder, int) {++matrixCalls; return hostCall();}
    MStatus addTranslation(const MVector& value, int) {
        ++matrixCalls; const auto status = hostCall();
        if (status) for (int i = 0; i < 3; ++i) matrix[3][i] += value[i];
        return status;
    }
    MMatrix asMatrix() const {return matrix;}
};
inline MMatrix operator*(const MMatrix& lhs, const MMatrix& rhs) {
    MMatrix result;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        result[r][c] = 0;
        for (int k = 0; k < 4; ++k) result[r][c] += lhs[r][k] * rhs[k][c];
    }
    return result;
}
namespace dommy {namespace math {
inline double degreesToRadians(double value) {return value * 0.017453292519943295;}
} namespace maya {
inline bool isFiniteVector(const MVector& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
inline bool isFiniteMatrix(const MMatrix& value) {return attribute::isFiniteMatrix(value);}
template <typename T> MStatus createPluginData(MFnPluginData& fn, int id, T*& output) {
    MStatus status; fn.create(id, &status);
    if (!status) return status;
    output = pluginDataPointer<T>(fn, &status); return status;
}
}}
static void reset() {
    meshCopies = attributeCopies = groupCopies = boundsCalls = 0;
    failCopyAt = -1; copyFailure = 0; failMetadata = failBounds = false;
    failAttributeAt = failGroupAt = -1; metadataFailure = 0;
    hostCalls = creations = publications = cleanCalls = 0; hostFailAt = hostThrowAt = -1;
    matrixCalls = bakeCalls = setMatrixCalls = 0; failBake = false;
    translation = rotation = pivotValue = MVector(); scaling = MVector(1, 1, 1);
    uniformScale = 1; rotationOrder = 0; freezeTransform = false; requestedCount = 3;
}
template <typename T> static std::shared_ptr<TrackedAttribute<T>> attr(const std::vector<T>& values,
                                                                  const T& def = T()) {
    return std::make_shared<TrackedAttribute<T>>(values, def);
}
static DMeshData fixture(int id = 1) {
    DMeshData data;
    auto mesh = std::make_shared<DMeshInternalData>();
    mesh->geometry = {id, id + 1, id + 2}; mesh->transform = id;
    mesh->pointAttrs["p"] = attr<int>({id, id + 1, id + 2});
    mesh->cornerAttrs["N"] = attr<Imath::V3f>({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}});
    mesh->faceAttrs["f"] = attr<int>({id});
    mesh->groups["g"] = std::make_shared<TrackedGroup>(DPIdList{true, false, true});
    mesh->faceGroups["g"] = std::make_shared<TrackedGroup>(DPIdList{true});
    mesh->edgeGroups["g"] = std::make_shared<TrackedGroup>(DPIdList{false, true, false});
    data.meshItemArray.push_back(mesh);
    data.mAttributeList["v"] = attr<int>({id}, -id);
    data.mGroupList["g"] = std::make_shared<TrackedGroup>(DPIdList{true});
    assert(data.hasValidMeshTopology());
    return data;
}
static void unchanged(const DMeshData& data, const DMeshData& old) {
    assert(data.meshItemArray == old.meshItemArray && data.mAttributeList == old.mAttributeList &&
           data.mGroupList == old.mGroupList && data.mBBox == old.mBBox && data.mBBoxDirty == old.mBBoxDirty);
}
static DMeshData twoMeshes() {
    auto data = fixture(1);
    data.meshItemArray.push_back(fixture(4).meshItemArray[0]);
    data.mAttributeList["v"] = attr<int>({1, 4}, -7);
    data.mGroupList["g"] = std::make_shared<TrackedGroup>(DPIdList{true, false});
    return data;
}

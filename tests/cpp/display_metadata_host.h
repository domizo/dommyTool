#pragma once

// Explicit procedural shapes reuse the existing failure-injectable Maya API.
#include "arnold_export_host.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DStatusOperation.h"
#ifdef DOMMY_SDK_CHECK
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#endif
using DStringList = std::vector<std::string>;
enum { D_COMP_POINT=1, D_COMP_EDGE=2, D_COMP_FACE=3, D_COMP_PER_VTX=4 };
struct DMeshData : AttributeOwner {
    static const MTypeId id;
    std::size_t meshCount = 1;
    std::map<std::string,std::shared_ptr<Group>> groups;
    DStringList getAllGroupList() const {
        DStringList result;
        for (const auto& value : groups) result.push_back(value.first);
        return result;
    }
    void getTotalVertex(std::size_t& count) const { count = meshCount*3; }
    std::size_t numberOfMesh() const { return meshCount; }
    bool hasUVs() const { return false; }
    std::size_t numberOfPointAttribute(bool) const { return 0; }
    DStringList getPointAttributeInfo() const { return {}; }
    std::size_t numberOfCompAttr(int) const { return 0; }
    DStringList getCompAttrInfo(int) const { return {}; }
    std::size_t numberOfGroups() const { return groups.size(); }
    auto beginGrp() const -> decltype(groups.begin()) { return groups.begin(); }
    auto endGrp() const -> decltype(groups.end()) { return groups.end(); }
    MBoundingBox getBoundingBox() const;
    std::size_t numberOfCompGroup(int) const { return 0; }
    DStringList getCompGroupInfo(int) const { return {}; }
};
const DMeshData* getInputDM(const MObject&,MDataBlock&);
MStatus getValidInputDP(const MObject&,MDataBlock&,const DPData*&);
MStatus getValidInputDM(const MObject&,MDataBlock&,const DMeshData*&);
MStatus getValidInputDC(const MObject&,MDataBlock&,const DCurveData*&);
const DMeshData* getInputDMfromPlug(const MPlug&,MObject&);
const DCurveData* getInputDCfromHandle(MDataHandle);

// Native plugin-data validation and OpenVDB grid metadata are explicit fixture
// boundaries; the production compute callback itself is compiled verbatim.
struct DOpenVDBData {
    bool valid = true;
    DStringList names{"density", "velocity"};
};
MStatus getValidInputVDB(const MObject&,MDataBlock&,const DOpenVDBData*&);
std::string getGridNames(const DOpenVDBData&);
MString buildVdbInfo(const DOpenVDBData&,const std::string&);

std::string mergeStringList(const DStringList& names);

#ifndef DOMMY_SDK_CHECK
const MTypeId DPData::id=1, DCurveData::id=2, DMeshData::id=3;
DPData metadataPoints;
DCurveData metadataCurves;
DMeshData metadataMeshes;
DOpenVDBData metadataVdb;
const DPData* getInputDPfromPlug(const MPlug&,MObject&) {
    return apiStatus("point.plug") && validInput ? &metadataPoints : nullptr;
}
MBoundingBox DPData::getBoundingBox() const {
    api("point.bounds");
    return MBoundingBox(MPoint(-2,-3,-4),MPoint(5,6,7));
}
const DMeshData* getInputDMfromPlug(const MPlug&,MObject&) {
    return apiStatus("mesh.plug") && validInput ? &metadataMeshes : nullptr;
}
const DCurveData* getInputDCfromHandle(MDataHandle) {
    api("curve.data");
    return validInput ? &metadataCurves : nullptr;
}
MBoundingBox DCurveData::getBoundingBox() const {
    api("curve.bounds");
    return MBoundingBox(MPoint(-2,-3,-4),MPoint(5,6,7));
}
MBoundingBox DMeshData::getBoundingBox() const {
    api("mesh.bounds");
    return MBoundingBox(MPoint(-2,-3,-4),MPoint(5,6,7));
}
namespace dommy { namespace maya {
// Explicit input-handle boundary; these tests do not emulate Maya evaluation.
struct ScopedPlugDataHandle {
    MDataHandle data;
    MDataHandle& handle() { return data; }
    MStatus read(const MPlug&) { return apiStatus("curve.handle"); }
};
}}
template <typename T>
const T* readMetadata(const MObject& attr,MDataBlock& data,const T& value) {
    MStatus status;
    data.inputValue(attr,&status);
    return status && apiStatus("data.read") && validInput ? &value : nullptr;
}
const DPData* getInputDP(const MObject& attr,MDataBlock& data) {
    return readMetadata(attr,data,metadataPoints);
}
const DCurveData* getInputDC(const MObject& attr,MDataBlock& data) {
    return readMetadata(attr,data,metadataCurves);
}
const DMeshData* getInputDM(const MObject& attr,MDataBlock& data) {
    return readMetadata(attr,data,metadataMeshes);
}
MStatus getValidInputDM(const MObject& attr,MDataBlock& data,const DMeshData*& output) {
    output = nullptr;
    MStatus status;
    data.inputValue(attr,&status);
    if (!status) return status;
    status = apiStatus("data.read");
    if (!status) return status;
    if (!validInput) return MS::kInvalidParameter;
    output = &metadataMeshes;
    return MS::kSuccess;
}
namespace dommy { namespace maya {
template <> MStatus getPluginDataFromHandle(const MDataHandle&, MTypeId id,
                                           const DPData*& output) {
    output = nullptr;
    const MStatus status = apiStatus("data.read");
    if (!status) return status;
    if (id != DPData::id || !validInput) return MS::kInvalidParameter;
    output = &metadataPoints;
    return MS::kSuccess;
}
}}
MStatus getValidInputDC(const MObject& attr,MDataBlock& data,const DCurveData*& output) {
    output = nullptr;
    MStatus status;
    data.inputValue(attr,&status);
    if (!status) return status;
    status = apiStatus("data.read");
    if (!status) return status;
    if (!validInput) return MS::kInvalidParameter;
    output = &metadataCurves;
    return MS::kSuccess;
}
MStatus getValidInputVDB(const MObject& attr,MDataBlock& data,const DOpenVDBData*& output) {
    output = nullptr;
    MStatus status;
    data.inputValue(attr,&status);
    if (!status) return status;
    status = apiStatus("data.read");
    if (!status) return status;
    if (!validInput || !metadataVdb.valid) return MS::kInvalidParameter;
    output = &metadataVdb;
    return MS::kSuccess;
}
std::string getGridNames(const DOpenVDBData& data) {
    api("vdb.names");
    return mergeStringList(data.names);
}
MString buildVdbInfo(const DOpenVDBData& data,const std::string& selection) {
    api("vdb.info");
    if (data.names.empty()) return "DOpenVDB Info\n\tGrid is empty.";
    return MString(("DOpenVDB Info\n\tgrid count: "+std::to_string(data.names.size())+
                    "\nselection: "+selection).c_str());
}
namespace dommy { namespace vdb {
MStatus reportComputeException(const char*,const std::exception_ptr&) noexcept {
    ++reports;
    return MS::kFailure;
}
}}
#endif

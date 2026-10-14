#pragma once

// Reuse the existing native-array/geometry boundary doubles. MFloatPoint is
// double-backed here: these tests cover status/ownership, not Maya float math.
#include "legacy_io_foam_doubles.h"
#include "common/utility/DMayaCheckedArray.h"
#include <array>
#include <memory>

#define CHECK_MSTATUS(s) ((void)(s))
inline void EchoWarning(const char*,const char*) {}
using DPointList=std::vector<std::array<double,3>>;
using DScalarDataList=std::vector<float>;
constexpr unsigned ID_DP_DATA=static_cast<unsigned>(MFnData::kAny);
struct MFnStringData {
    MObject create(const char* text,MStatus* status){
        *status=operation(std::string("string default ")+text);
        return *status ? MObject{"default"}:MObject::kNullObj;
    }
};
struct DPData {
    static constexpr MFnData::Type id=MFnData::kAny;
    DPointList points;
    std::map<std::string,DScalarDataList> attributes;
    bool valid=true;
    std::size_t numberOfPoints()const{return points.size();}
    const DPointList* getPositionPtrConst()const{return &points;}
    bool isExistAttr(const std::string& name)const{return attributes.count(name)!=0;}
    bool hasValidPointTopology()const{
        if(!valid)return false;
        for(const auto& point:points)for(double value:point)if(!std::isfinite(value))return false;
        return true;
    }
};
using DMItem=std::shared_ptr<MeshData>;
namespace {DPData inputPoints;bool missingPoints=false;}
inline const DPData* getInputDP(const MObject& attr,MDataBlock& data){
    MStatus status;const auto handle=data.inputValue(attr,&status);
    if(!status)return nullptr;handle.check();
    return !missingPoints && inputPoints.hasValidPointTopology()?&inputPoints:nullptr;
}
inline void getDScalarArrayFromAttribute(const DPData& data,const std::string& name,DScalarDataList& result){
    if(!api("attribute.read"))throw std::runtime_error("attribute read failed");
    result=data.attributes.at(name);
}
struct DMeshData {
    static constexpr MFnData::Type id=MFnData::kAny;
    std::vector<DMItem> values;
    bool insert(const DMItem& value){if(!api("mesh.insert"))return false;values.push_back(value);return true;}
    void updateBBox(){if(!api("mesh.bounds"))throw std::runtime_error("bounds failed");}
};
namespace {DMeshData publishedMeshes;DMeshData* pendingMeshes=nullptr;}
struct MFnPluginData {
    DMeshData value;
    MObject create(MFnData::Type,MStatus* status){
        *status=api("plugin.create");return !*status || nullMode==5 ? MObject::kNullObj:MObject{"plugin"};
    }
    MObject object()const{return nullMode==5 ? MObject::kNullObj:MObject{"plugin"};}
};
struct DMeshInternalData {
    static MStatus createFromMayaObject(const MObject& object,DMItem& result){
        const auto status=api("mesh.convert");
        if(!status)return status;if(object.isNull())return MS::kFailure;
        result=std::make_shared<MeshData>(meshes.at(object.name));return status;
    }
};
namespace dommy {namespace maya {
template<class T>MStatus getPluginDataFromHandle(const MDataHandle& handle,MFnData::Type,const T*& result){
    handle.check();const auto status=api("plugin.read");
    result=nullptr;if(!status)return status;
    if(missingPoints || handle.type()!=DPData::id)return MS::kFailure;
    result=&inputPoints;return status;
}
inline MStatus getPluginData(MFnPluginData& plugin,DMeshData*& result){
    const auto status=api("plugin.data");result=nullptr;
    if(status && !plugin.object().isNull()){result=&plugin.value;pendingMeshes=result;}
    return status;
}
inline MStatus createPluginData(MFnPluginData& plugin,MFnData::Type type,DMeshData*& result){
    MStatus status;result=nullptr;const auto object=plugin.create(type,&status);
    if(!status)return status;if(object.isNull())return MS::kFailure;
    return getPluginData(plugin,result);
}
template<class T>MStatus publishTypedPluginDataObject(MDataHandle& handle,const MObject& object,MFnData::Type){
    if(object.isNull() || !pendingMeshes)return MS::kFailure;
    const auto status=handle.set(object);if(status){publishedMeshes=*pendingMeshes;handle.setClean();}return status;
}
}}
namespace dommy {namespace math {
inline double distance(const MVector& a,const MVector& b){
    return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);
}
}}

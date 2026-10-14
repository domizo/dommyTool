// Production typed storage/converters/read branches. Maya arrays and renderer
// logging are explicit boundaries; no native graph or renderer is linked.
#ifdef DOMMY_SDK_CHECK
#include <maya/MDoubleArray.h>
#include <maya/MIntArray.h>
#include <maya/MVectorArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MGlobal.h>
#endif
#include "common/utility/DMayaAttributeArray.h"
#include <maya/MVector.h>
#include <cassert>
#include <map>

#ifndef DOMMY_SDK_CHECK
bool failResize=false,failCopy=false;
template<class T>struct ControlArray {
    std::vector<T> values;
    unsigned length()const{return static_cast<unsigned>(values.size());}
    T& operator[](unsigned i){return values.at(i);}
    const T& operator[](unsigned i)const{return values.at(i);}
    MStatus setLength(unsigned n){if(failResize)return MS::kFailure;values.resize(n);return MS::kSuccess;}
    MStatus copy(const ControlArray& source){if(failCopy)return MS::kFailure;values=source.values;return MS::kSuccess;}
};
using MDoubleArray=ControlArray<double>;
using MIntArray=ControlArray<int>;
using MVectorArray=ControlArray<MVector>;
using MMatrixArray=ControlArray<MMatrix>;
#endif
// Unused string accessor detail is omitted from this fixture's extracted
// namespace to avoid inventing a second string-array implementation.
using DScalarDataList=std::vector<float>;
using DVec3DataList=std::vector<Imath::V3f>;
struct DPData {
    std::map<std::string,DPAttributeData::ConstPtr> attributes;
    DPAttributeData::ConstPtr getAttr(const std::string& name)const{
        const auto found=attributes.find(name);return found==attributes.end()?nullptr:found->second;
    }
    bool isExistAttr(const std::string& name)const{return bool(getAttr(name));}
};
struct DrawData{bool useConeAngleVis=false;};
void AiMsgError(const char*,...){}
#ifndef DOMMY_SDK_CHECK
struct MGlobal{static void displayError(const char*){}static void displayWarning(const char*){}};
#endif
#include "renderer_control_methods.inc"

#ifndef DOMMY_SDK_CHECK
template<class T>DPAttributeData::ConstPtr field(std::vector<T> values){
    return std::make_shared<TypedDPAttributeData<T>>(values,T());
}
void check(bool(*stage)(DPAttributeData::ConstPtr,int),const std::string& family,bool optionalMode){
    assert(stage(nullptr,1));
    const auto wrong=field<std::string>({"wrong"});
    assert(!stage(wrong,1));
    if(optionalMode)assert(stage(wrong,0));
    if(family=="scalar" || family=="real") {
        for(const auto& numeric:{field<int32_t>({2}),field<uint32_t>({2}),field<int64_t>({2}),
                                field<float>({2}),field<double>({2})})assert(stage(numeric,1));
        assert(!stage(field<bool>({true}),1));
        assert(!stage(field<Imath::V3f>({{1,2,3}}),1));
        assert(!stage(field<double>({1,2}),1));
        assert(!stage(field<double>({std::numeric_limits<double>::infinity()}),1));
        if(family=="real") {
            failResize=true;assert(!stage(field<double>({2}),1));failResize=false;
            failCopy=true;assert(!stage(field<double>({2}),1));failCopy=false;
        }
    } else if(family=="vector") {
        assert(stage(field<Imath::V3i>({{1,2,3}}),1));
        assert(stage(field<Imath::V3f>({{1,2,3}}),1));
        assert(stage(field<Imath::V3d>({{1,2,3}}),1));
        assert(!stage(field<Imath::V2f>({{1,2}}),1));
        assert(!stage(field<double>({1}),1));
        assert(!stage(field<Imath::V3d>({}),1));
    } else {
        assert(stage(field<MMatrix>({MMatrix()}),1));
        assert(!stage(field<double>({1}),1));
        assert(!stage(field<MMatrix>({}),1));
        failCopy=true;assert(!stage(field<MMatrix>({MMatrix()}),1));failCopy=false;
    }
    assert(stage(nullptr,1));
}
int main(){
#include "renderer_control_cases.inc"
    DPData data;data.attributes["value"]=field<int64_t>({INT64_C(9007199254740991)});
    MDoubleArray values;assert(getCheckedMRealArrayFromAttribute(data,"value",1U,values));
    assert(values[0]==9007199254740991.0);
    assert(!getCheckedMDoubleArrayFromAttribute(data,"value",1U,values));
}
#endif

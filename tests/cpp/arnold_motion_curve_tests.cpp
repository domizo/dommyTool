// Production typed fields, Curve/Mesh exports and real motion handoffs.
// Bounded arrays expose indexing mistakes without emulating a native renderer.
#include "arnold_parameter_api.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributeExport.h"
#include "common/utility/DAttributeLookup.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DStatusOperation.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "DCurveArnoldLayout.h"
#include <cassert>
#include <cstring>
#include <map>
#include <limits>
#include <memory>
#include <sstream>
#ifdef DOMMY_SDK_CHECK
#include <maya/MString.h>
#include <maya/MStringArray.h>
#else
struct MStringArray;
struct MString{std::string text;MString(const char* v=""):text(v){}const char* asChar()const{return text.c_str();}
    unsigned length()const{return text.size();}MStatus split(char,MStringArray&)const;
    bool operator==(const char* value)const{return text==value;}};
struct MStringArray{std::vector<MString> values;unsigned length()const{return values.size();}const MString& operator[](unsigned i)const{return values.at(i);}};
MStatus MString::split(char c,MStringArray& out)const{std::istringstream s(text);std::string part;
    while(std::getline(s,part,c))if(!part.empty())out.values.emplace_back(part.c_str());return MS::kSuccess;}
#endif
struct AtString{const char* value;explicit AtString(const char* v):value(v){}};
struct AtVector{float x,y,z;};struct AtVector2{float x,y;};
int liveArrays=0,throwAt=-1,throwKind=0,failElementAt=-1,elementCalls=0;bool accepted=false,forceWriteFailure=false,mapFailure=false,failAllocation=false;
unsigned keyCalls=0;std::vector<std::string> calls;
void api(const char* name){calls.push_back(name);if(int(calls.size())-1==throwAt){
    if(throwKind)throw std::bad_alloc();throw std::runtime_error("renderer boundary");}}
struct AtArray{unsigned count;unsigned char keys,type;std::vector<std::array<double,3>> values;};
struct AtNode{unsigned char visibility=7;AtArray* motionTarget=nullptr;std::map<std::string,std::unique_ptr<AtArray>> arrays;};
AtArray motionSource{2,1,AI_TYPE_VECTOR,{}},motionTarget{2,2,AI_TYPE_VECTOR,{}};
AtArray* AiArrayAllocate(unsigned n,unsigned char k,int type){api("allocate");if(failAllocation)return nullptr;auto* a=new AtArray{n,k,static_cast<unsigned char>(type),std::vector<std::array<double,3>>(std::size_t(n)*k)};++liveArrays;return a;}
void AiArrayDestroy(AtArray* a){--liveArrays;delete a;}
bool elementWrite(const char* name){api(name);return elementCalls++!=failElementAt;}
bool AiArraySetInt(AtArray* a,std::size_t i,int v){if(!elementWrite("array.int"))return false;a->values.at(i)={{double(v),0,0}};return true;}
bool AiArraySetUInt(AtArray* a,std::size_t i,unsigned v){if(!elementWrite("array.uint"))return false;a->values.at(i)={{double(v),0,0}};return true;}
bool AiArraySetFlt(AtArray* a,std::size_t i,float v){if(!elementWrite("array.float"))return false;a->values.at(i)={{v,0,0}};return true;}
bool AiArraySetVec(AtArray* a,std::size_t i,AtVector v){if(!elementWrite("array.vector"))return false;a->values.at(i)={{v.x,v.y,v.z}};return true;}
bool AiArraySetVec2(AtArray* a,std::size_t i,AtVector2 v){if(!elementWrite("array.vector2"))return false;a->values.at(i)={{v.x,v.y,0}};return true;}
void* AiArrayMap(AtArray* a){api("map");return mapFailure?nullptr:a;}
void AiArrayUnmap(AtArray*){calls.push_back("unmap");}
std::uint32_t AiArrayGetNumElements(const AtArray* a){api("elements");return a->count;}
std::uint8_t AiArrayGetNumKeys(const AtArray* a){api("keys");return a->keys;}
std::uint8_t AiArrayGetType(const AtArray* a){api("type");return a->type;}
bool AiArraySetKey(AtArray* a,std::uint8_t k,const void* data){api("set.key");++keyCalls;
    const auto* source=static_cast<const AtArray*>(data);
    if(!a||k>=a->keys||a->count!=source->count||forceWriteFailure)return false;
    for(unsigned i=0;i<a->count;++i)a->values.at(std::size_t(k)*a->count+i)=source->values.at(i);
    return true;}
AtArray* AiNodeGetArray(AtNode* n,const char*){api("node.getarray");return n->motionTarget;}
AtArray* AiNodeGetArray(AtNode* n,AtString p){return AiNodeGetArray(n,p.value);}
void AiNodeSetByte(AtNode* n,AtString,unsigned char v){n->visibility=v;}
void AiNodeSetByte(AtNode* n,const char*,unsigned char v){n->visibility=v;}
void AiNodeSetStr(AtNode*,AtString,AtString){}void AiNodeSetFlt(AtNode*,AtString,float){}
void AiNodeSetArray(AtNode* n,const char* name,AtArray* a){std::unique_ptr<AtArray> owner(a);--liveArrays;api("node.array");n->arrays[name]=std::move(owner);}
void AiNodeSetArray(AtNode* n,AtString name,AtArray* a){AiNodeSetArray(n,name.value,a);}
void AiMsgError(const char*,...){}void AiMsgWarning(const char*,...){}
struct TestGlobal{static void displayError(const char*){}};
#define MGlobal TestGlobal
namespace dommy{namespace arnold{void requireUserParameter(AtNode*,const char*,const char*){api("declaration");}}}
using DPointList=std::vector<Imath::V3f>;using DPointListPtr=std::shared_ptr<DPointList>;
using DVec3DataList=std::vector<Imath::V3f>;
struct Fields{
    std::map<std::string,DPAttributeData::ConstPtr> attributes;
    DPAttributeData::ConstPtr getAttr(const std::string& n)const{auto i=attributes.find(n);return i==attributes.end()?nullptr:i->second;}
    bool isExistAttr(const std::string& n)const{return bool(getAttr(n));}
    template<class T>const std::vector<T>& attrListValue(const std::string& n)const;
    template<class T>typename T::ConstPtr getAttribute(const std::string& n)const;
    template<class T>typename TypedDPAttributeData<T>::ConstPtr getValidTypedAttributedata(const std::string& n)const;
    template<class T>void field(const std::string& n,std::vector<T> v,T defaultValue=T()){attributes[n]=std::make_shared<TypedDPAttributeData<T>>(v,defaultValue);}
};
#include "motion_curve_fields.inc"
struct Curve:Fields{
    DPointList points;std::size_t numberOfPoints()const{return points.size();}bool hasValidCurveTopology()const{return !points.empty();}
    bool isIdentity()const{return true;}MMatrix getTransformMatrix()const{return {};}
    DPointList getPointsInWorld()const{return points;}const DPointList* getPositionPtrConst()const{return &points;}
};
struct DCurveData:Fields{
    std::vector<std::shared_ptr<Curve>> curves;std::size_t numberOfCurves()const{return curves.size();}
    const std::vector<std::shared_ptr<Curve>>* getCurveListConst()const{return &curves;}
    auto beginCurves()const->decltype(curves.begin()){return curves.begin();}auto endCurves()const->decltype(curves.end()){return curves.end();}
    auto beginPrims()const->decltype(curves.begin()){return curves.begin();}auto endPrims()const->decltype(curves.end()){return curves.end();}
    bool isExistPointAttr(const std::string& n)const{return !curves.empty()&&curves.front()->isExistAttr(n);}
};
struct Mesh:Fields{std::size_t numberOfPoints()const{return 3U;}};
struct MeshCollection{
    std::vector<std::shared_ptr<Mesh>> meshes;
    auto beginMeshes()const->decltype(meshes.begin()){return meshes.begin();}
    auto endMeshes()const->decltype(meshes.end()){return meshes.end();}
    std::size_t numberOfPoints()const{return meshes.size()*3U;}
};
struct NotificationObject{};
struct NotificationNodeFn{explicit NotificationNodeFn(NotificationObject&,MStatus* status) {*status=MS::kSuccess;}};
struct NotificationPlug{
    MString name;
    MString partialName(bool,bool,bool,bool,bool,bool longNames,MStatus* status)const{assert(longNames);*status=MS::kSuccess;return name;}
};
struct CNodeTranslator{enum{AI_RECREATE_TRANSLATOR=7};};
struct MeshNotifier{
    unsigned requests=0;int mode=-1;
    void SetUpdateMode(int value){mode=value;}void RequestUpdate(){++requests;}
    void NodeChanged(NotificationObject&,NotificationPlug&);
};
#define DOMMY_MAYA_DEBUG(...) ((void)0)
#include "motion_curve_methods.inc"

#ifndef DOMMY_SDK_CHECK
void reset(){accepted=false;calls.clear();throwAt=-1;throwKind=0;keyCalls=0;forceWriteFailure=mapFailure=false;
    failElementAt=-1;elementCalls=0;failAllocation=false;
    motionSource={2,1,AI_TYPE_VECTOR,{{{1,2,3}},{{4,5,6}}}};
    motionTarget={2,2,AI_TYPE_VECTOR,std::vector<std::array<double,3>>(4)};assert(liveArrays==0);}
void motions(){for(auto stage:{motionPoint,motionCurve,motionMesh}){
    AtNode n;n.motionTarget=&motionTarget;reset();stage(&n,1,2);assert(accepted&&keyCalls==1U);
    const auto trace=calls;
    for(int invalid=0;invalid<12;++invalid){reset();n.visibility=7;n.motionTarget=&motionTarget;
        if(invalid==0)n.motionTarget=nullptr;if(invalid==1)motionTarget.count=4;
        if(invalid==2)motionTarget.type=AI_TYPE_FLOAT;if(invalid==3)motionSource.type=AI_TYPE_FLOAT;
        if(invalid==4)motionSource.keys=2;if(invalid==5)motionTarget.keys=1;
        if(invalid==9)motionTarget.count=1;
        if(invalid==11)motionSource.count=motionTarget.count=50000001U;
        stage(&n,invalid==6?2U:invalid==7?256U:1U,invalid==8?256U:invalid==10?0U:2U);
        assert(!accepted&&keyCalls==0U&&n.visibility==0);
    }
    reset();n.motionTarget=&motionTarget;forceWriteFailure=true;stage(&n,1,2);assert(!accepted&&keyCalls==1U&&n.visibility==0);
    reset();mapFailure=true;stage(&n,1,2);assert(!accepted&&keyCalls==0U);
    for(std::size_t i=0;i<trace.size();++i)if(trace[i]!="unmap")for(int k:{0,1}){
        reset();throwAt=int(i);throwKind=k;stage(&n,1,2);assert(!accepted&&n.visibility==0);
        reset();stage(&n,1,2);assert(accepted);
    }
}
    reset();assert(!dommy::arnold::setCheckedVectorMotionKey(&motionTarget,nullptr,1,2));
    assert(calls.empty());
}
DCurveData curves(){DCurveData d;const unsigned counts[]={3,4,2,5};
    for(unsigned i=0;i<4;++i){auto c=std::make_shared<Curve>();for(unsigned p=0;p<counts[i];++p)c->points.emplace_back(float(i+1),float(p),0);
        std::vector<float> v;for(unsigned p=0;p<counts[i];++p)v.push_back(float(100+i*10+p));
        c->field<float>("value",v);d.curves.push_back(c);}
    d.field<float>("width",{10,20,30,40});d.field<Imath::V3f>("orientation",{{1,0,0},{2,0,0},{3,0,0},{4,0,0}});
    d.field<int32_t>("id",{10,20,30,40});return d;}
template<class T> struct FieldValue {static T make(int v){return T(v);}};
template<class T> struct FieldValue<Imath::Vec2<T>> {
    static Imath::Vec2<T> make(int v){return {T(v),T(v+100)};}};
template<class T> struct FieldValue<Imath::Vec3<T>> {
    static Imath::Vec3<T> make(int v){return {T(v),T(v+100),T(v+200)};}};
template<class T> void addTypedFields(DCurveData& d,const std::string& suffix){
    std::vector<T> primitive;
    for(unsigned i=0;i<d.curves.size();++i){
        primitive.push_back(FieldValue<T>::make(int((i+1)*10)));
        std::vector<T> point;
        for(unsigned p=0;p<d.curves[i]->numberOfPoints();++p)
            point.push_back(FieldValue<T>::make(int(100+i*10+p)));
        d.curves[i]->field<T>("p_"+suffix,point);
    }
    d.field<T>("u_"+suffix,primitive);
}
const char* const typeNames[]={"int","float","double","v2i","v2f","v2d","v3i","v3f","v3d"};
MString fieldNames(const char* prefix){std::string out;
    for(const auto* name:typeNames){if(!out.empty())out+=' ';out+=prefix;out+=name;}
    return MString(out.c_str());}
void allTypedFields(DCurveData& d){
    addTypedFields<int32_t>(d,"int");addTypedFields<float>(d,"float");addTypedFields<double>(d,"double");
    addTypedFields<Imath::V2i>(d,"v2i");addTypedFields<Imath::V2f>(d,"v2f");addTypedFields<Imath::V2d>(d,"v2d");
    addTypedFields<Imath::V3i>(d,"v3i");addTypedFields<Imath::V3f>(d,"v3f");addTypedFields<Imath::V3d>(d,"v3d");
}
std::vector<double> scalars(const AtNode& n,const char* name){std::vector<double> v;for(const auto& a:n.arrays.at(name)->values)v.push_back(a[0]);return v;}
MeshCollection meshes(){MeshCollection d;for(unsigned i=0;i<3;++i)d.meshes.push_back(std::make_shared<Mesh>());return d;}
template<class T>void typedMeshCases(){
    using Components=dommy::numeric::ValueComponents<T>;
    MStringArray names;names.values={MString("value")};
    for(unsigned missing=0;missing<7U;++missing){
        auto d=meshes();unsigned first=0;while(missing&(1U<<first))++first;
        for(unsigned owner=0;owner<3;++owner)if(!(missing&(1U<<owner))){
            std::vector<T> values;for(int p=0;p<3;++p)values.push_back(FieldValue<T>::make(int(owner*10+p)));
            d.meshes[owner]->field<T>("value",values,FieldValue<T>::make(-7-int(owner)));
            dommy::attribute::ValidationBudget budget;
            assert(dommy::attribute::validateTypedAttribute<T>(d.meshes[owner]->getAttr("value"),3U,budget));
        }
        reset();AtNode n;meshAttributeStage(&n,&d,names);assert(accepted&&liveArrays==0);
        const auto output=n.arrays.at("value")->values;assert(output.size()==9U);
        for(unsigned owner=0;owner<3;++owner)for(unsigned p=0;p<3;++p){
            const T expected=FieldValue<T>::make(missing&(1U<<owner)?-7-int(first):int(owner*10+p));
            for(unsigned axis=0;axis<Components::count;++axis)
                assert(output.at(owner*3+p)[axis]==double(Components::get(expected,axis)));
        }
        const int writes=elementCalls;
        for(int failure:{0,writes/2,writes-1}){
            reset();failElementAt=failure;meshAttributeStage(&n,&d,names);
            assert(!accepted&&n.visibility==0&&liveArrays==0&&n.arrays.at("value")->values==output);
            reset();meshAttributeStage(&n,&d,names);assert(accepted&&liveArrays==0);
        }
    }
    for(unsigned count:{0U,2U,4U}){
        auto d=meshes();d.meshes[0]->field<T>("value",std::vector<T>(3U,FieldValue<T>::make(10)));
        d.meshes[2]->field<T>("value",std::vector<T>(count,FieldValue<T>::make(30)));
        reset();AtNode n;meshAttributeStage(&n,&d,names);
        assert(!accepted&&n.visibility==0&&n.arrays.empty()&&calls.empty());
    }
    auto d=meshes();d.meshes[0]->field<T>("value",std::vector<T>(3U,FieldValue<T>::make(10)));
    if(TypedDPAttributeData<T>::staticTypeName()==TypedDPAttributeData<double>::staticTypeName())
        d.meshes[1]->field<float>("value",{20,21,22});
    else d.meshes[1]->field<double>("value",{20,21,22});
    reset();AtNode n;meshAttributeStage(&n,&d,names);
    assert(!accepted&&n.visibility==0&&n.arrays.empty()&&calls.empty());
}
void meshCases(const std::string& phase){
    MStringArray names;names.values={MString("value")};
    auto d=meshes();
    d.meshes[0]->attributes["value"]=std::make_shared<TypedDPAttributeData<float>>(std::vector<float>{10,11,12},-7.0f);
    d.meshes[2]->field<float>("value",{30,31,32});
    if(phase=="all"||phase=="mesh"||phase=="mesh_missing"){
        reset();AtNode n;meshAttributeStage(&n,&d,names);assert(accepted);
        assert(scalars(n,"value")==std::vector<double>({10,11,12,-7,-7,-7,30,31,32}));
        reset();d.meshes[0]->attributes.erase("value");meshAttributeStage(&n,&d,names);assert(accepted);
        assert(scalars(n,"value")==std::vector<double>({0,0,0,0,0,0,30,31,32}));
        d.meshes[0]->field<float>("value",{10,11,12});
    }
    if(phase=="all"||phase=="mesh"||phase=="mesh_mixed"){
        reset();AtNode n;d.meshes[1]->field<double>("value",{20,21,22});
        meshAttributeStage(&n,&d,names);assert(!accepted&&n.visibility==0&&n.arrays.empty()&&liveArrays==0);
        d.meshes[1]->attributes.erase("value");
    }
    if(phase=="all"||phase=="mesh"||phase=="mesh_write")for(int at:{0,5,8}){
        reset();AtNode n;failElementAt=at;meshAttributeStage(&n,&d,names);
        assert(!accepted&&n.visibility==0&&n.arrays.empty()&&liveArrays==0);
        reset();meshAttributeStage(&n,&d,names);assert(accepted&&liveArrays==0);
    }
    if(phase=="all"||phase=="mesh"){
        typedMeshCases<int32_t>();typedMeshCases<float>();typedMeshCases<double>();
        typedMeshCases<Imath::V2i>();typedMeshCases<Imath::V2f>();typedMeshCases<Imath::V2d>();
        typedMeshCases<Imath::V3i>();typedMeshCases<Imath::V3f>();typedMeshCases<Imath::V3d>();
        reset();AtNode n;auto absent=meshes();meshAttributeStage(&n,&absent,names);
        assert(accepted&&n.arrays.empty()&&calls.empty());
        for(double invalid:{1.0e300,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            for(bool defaultOnly:{false,true}){
                auto wide=meshes();wide.meshes[2]->field<double>("value",{defaultOnly?1.0:invalid,2,3},defaultOnly?invalid:0.0);
                reset();meshAttributeStage(&n,&wide,names);
                assert(!accepted&&n.visibility==0&&n.arrays.empty()&&calls.empty());
            }
        d.meshes[1]->field<float>("value",{20,21,22});
        reset();failAllocation=true;meshAttributeStage(&n,&d,names);
        assert(!accepted&&n.visibility==0&&n.arrays.empty()&&liveArrays==0);
        reset();meshAttributeStage(&n,&d,names);assert(accepted&&liveArrays==0);
        const auto trace=calls;
        for(unsigned at=0;at<trace.size();++at)for(int kind:{0,1}){
            reset();throwAt=int(at);throwKind=kind;meshAttributeStage(&n,&d,names);
            assert(!accepted&&n.visibility==0&&liveArrays==0);
            reset();meshAttributeStage(&n,&d,names);assert(accepted&&liveArrays==0);
        }
    }
}
void notificationCases(){NotificationObject object;for(const char* name:{"ExportMeshAttribute","ExportPointAttribute"}){
    MeshNotifier notifier;NotificationPlug plug{MString(name)};notifier.NodeChanged(object,plug);
    assert(notifier.requests==1U&&notifier.mode==CNodeTranslator::AI_RECREATE_TRANSLATOR);
}MeshNotifier notifier;NotificationPlug plug{MString("unrelated")};notifier.NodeChanged(object,plug);assert(notifier.requests==0U);}
void curveCases(){
    reset();auto d=curves();{AtNode n;curveStage(&n,&d,1,false,1,2,1,"id","");assert(accepted);
        assert(scalars(n,"radius")==std::vector<double>({20,20,40,40,40}));
        assert(scalars(n,"id")==std::vector<double>({20,40}));assert(n.arrays.at("orientations")->count==9U);
        assert(scalars(n,"orientations")==std::vector<double>({2,2,2,2,4,4,4,4,4}));}
    reset();{AtNode n;curveStage(&n,&d,1,false,1,0,0,"id","value");assert(accepted);
        assert(scalars(n,"value")==std::vector<double>({111,112,131,132,133}));}
    for(short basis:{0,1,2})for(bool padded:{false,true}){
        reset();AtNode n;curveStage(&n,&d,basis,padded,1,2,1,"id","");assert(accepted);
        std::vector<double> radiusExpected,orientationExpected;
        for(unsigned i=0;i<d.curves.size();++i){
            const auto l=dommy::arnold::CurveLayout::create(d.curves[i]->numberOfPoints(),basis!=0,padded);
            if(!l.valid())continue;
            radiusExpected.insert(radiusExpected.end(),l.varyingCount(),(i+1)*10);
            orientationExpected.insert(orientationExpected.end(),l.controlVertexCount(),i+1);
        }
        assert(scalars(n,"radius")==radiusExpected&&scalars(n,"orientations")==orientationExpected);
    }
    allTypedFields(d);
    // Point width/orientation use the same retained source curves, but radius
    // is varying data and orientations are control-vertex data.
    for(unsigned i=0;i<d.curves.size();++i){
        d.curves[i]->field<float>("width",d.curves[i]->attrListValue<float>("value"));
        d.curves[i]->field<Imath::V3d>("orientation",d.curves[i]->attrListValue<Imath::V3d>("p_v3d"));
    }
    d.attributes.erase("width");
    for(short basis:{0,1,2})for(bool padded:{false,true}){
        reset();AtNode n;curveStage(&n,&d,basis,padded,1,2,2,fieldNames("u_"),fieldNames("p_"));assert(accepted);
        unsigned cv=0,varying=0,prims=0;
        std::vector<double> uniformExpected,varyingExpected,orientationExpected;
        for(unsigned i=0;i<d.curves.size();++i){const auto& c=d.curves[i];
            const auto l=dommy::arnold::CurveLayout::create(c->numberOfPoints(),basis!=0,padded);if(!l.valid())continue;
            cv+=l.controlVertexCount();varying+=l.varyingCount();++prims;
            uniformExpected.push_back((i+1)*10);
            for(unsigned p=0;p<l.varyingCount();++p)varyingExpected.push_back(100+i*10+l.varyingSourceIndex(p));
            for(unsigned p=0;p<l.controlVertexCount();++p)orientationExpected.push_back(100+i*10+l.controlVertexSourceIndex(p));
        }
        assert(n.arrays.at("points")->count==cv);
        for(const auto* name:typeNames){const std::string uniform=std::string("u_")+name,varyingName=std::string("p_")+name;
            assert(n.arrays.at(uniform)->count==prims&&scalars(n,uniform.c_str())==uniformExpected);
            assert(n.arrays.at(varyingName)->count==varying&&scalars(n,varyingName.c_str())==varyingExpected);
        }
        assert(scalars(n,"radius")==varyingExpected&&scalars(n,"orientations")==orientationExpected);
        n.motionTarget=n.arrays.at("points").get();reset();
        curveMotionStage(&n,&d,basis,padded,1,2);assert(accepted&&keyCalls==1);
        const auto& points=n.motionTarget->values;
        for(unsigned i=0;i<cv;++i)assert(points[i]==points[cv+i]);
        const int motionWrites=elementCalls;
        for(int failure:{0,motionWrites-1}){
            reset();failElementAt=failure;curveMotionStage(&n,&d,basis,padded,1,2);
            assert(!accepted&&n.visibility==0&&keyCalls==0&&liveArrays==0);
            reset();curveMotionStage(&n,&d,basis,padded,1,2);assert(accepted&&keyCalls==1&&liveArrays==0);
        }
        reset();n.motionTarget->count+=1;
        curveMotionStage(&n,&d,basis,padded,1,2);assert(!accepted&&keyCalls==0&&n.visibility==0);
        n.motionTarget->count-=1;reset();curveMotionStage(&n,&d,basis,padded,1,2);assert(accepted);
    }
    reset();{auto shortOnly=curves();shortOnly.curves.resize(1);AtNode n;
        curveStage(&n,&shortOnly,1,false,1,0,0,"","");assert(!accepted&&n.visibility==0&&calls.empty());}
    // Model renderer faults at every allocation, field write, declaration and
    // consuming handoff. This fixture owns arrays at setter entry; it does not
    // establish Arnold's native ownership behavior if a setter throws.
    reset();std::vector<std::string> trace;
    {AtNode n;curveStage(&n,&d,1,false,1,2,2,fieldNames("u_"),fieldNames("p_"));assert(accepted);trace=calls;}
    const auto writes=std::count_if(trace.begin(),trace.end(),[](const std::string& name){return name.find("array.")==0U;});
    for(int at=0;at<writes;++at){
        reset();AtNode n;failElementAt=at;
        curveStage(&n,&d,1,false,1,2,2,fieldNames("u_"),fieldNames("p_"));
        assert(!accepted&&n.visibility==0&&liveArrays==0);
        reset();curveStage(&n,&d,1,false,1,2,2,fieldNames("u_"),fieldNames("p_"));assert(accepted&&liveArrays==0);
    }
    for(unsigned i=0;i<trace.size();++i)for(int kind:{0,1}){
        reset();AtNode n;throwAt=int(i);throwKind=kind;
        curveStage(&n,&d,1,false,1,2,2,fieldNames("u_"),fieldNames("p_"));assert(!accepted&&n.visibility==0&&liveArrays==0);
        reset();curveStage(&n,&d,1,false,1,2,2,fieldNames("u_"),fieldNames("p_"));assert(accepted&&liveArrays==0);
    }
}
int main(int argc,char** argv){const std::string phase=argc>1?argv[1]:"all";
    if(phase=="all"||phase=="motion")motions();if(phase=="all"||phase=="curve")curveCases();
    if(phase=="all"||phase.find("mesh")==0U)meshCases(phase);
    if(phase=="all"||phase=="notification")notificationCases();assert(liveArrays==0);}
#endif

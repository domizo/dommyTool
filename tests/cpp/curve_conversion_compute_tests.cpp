// Actual converter compute, batch and DAG traversal; Maya geometry is a count double.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "modules/curve/DCurve/DCurveConversionPlan.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static int calls=0,failAt=-1,throwAt=-1,constructions=0,batches=0,singles=0,published=0,groupWrites=0;
static MStatus injected=MS::kFailure;
static bool failed=false,shortBatch=false,wrongCount=false,wrongKnots=false,nullCurve=false,missingGroup=false;
static std::vector<std::string> events;
static MStatus api(const std::string& name){
    assert(!failed);events.push_back(name);const int index=calls++;
    if(index==throwAt)throw std::runtime_error("injected compute exception");
    if(index==failAt){failed=true;return injected;}
    return MS::kSuccess;
}
struct MObject {std::string name;};
struct MObjectArray {
    std::vector<MObject> values;
    unsigned length()const{return static_cast<unsigned>(values.size());}
    const MObject& operator[](unsigned i)const{return values.at(i);}
    MStatus append(const MObject& value){auto s=api("append");if(s)values.push_back(value);return s;}
};
MString operator+(const MString& text,std::size_t number){return MString((std::string(text.asChar())+std::to_string(number)).c_str());}
struct Node {
    std::string type;
    std::vector<MObject> children;
    bool intermediate;
    int cvs,knots;
    unsigned countOverride;
    Node(std::string t="transform",std::vector<MObject> c={},bool i=false,int cv=2,int k=2)
        :type(std::move(t)),children(std::move(c)),intermediate(i),cvs(cv),knots(k),countOverride(0){}
};
static std::map<std::string,Node> graph;
struct MFnDagNode {
    MObject object;bool valid;
    MFnDagNode(const MObject& value,MStatus* status):object(value){*status=api("dag");valid=bool(*status);}
    unsigned childCount(MStatus* s)const {assert(valid);*s=api("children");const auto& n=graph.at(object.name);return n.countOverride ? n.countOverride : static_cast<unsigned>(n.children.size());}
    MObject child(unsigned i,MStatus* s)const {assert(valid);*s=api("child");return *s ? graph.at(object.name).children.at(i) : MObject{};}
    MString typeName(MStatus* s)const {assert(valid);*s=api("type");return graph.at(object.name).type.c_str();}
    bool isIntermediateObject(MStatus* s)const {assert(valid);*s=api("intermediate");return graph.at(object.name).intermediate;}
};
struct MFnNurbsCurve {
    MObject object;bool valid;
    MFnNurbsCurve(const MObject& value,MStatus* s):object(value){*s=api("curve");valid=bool(*s);}
    int numCVs(MStatus* s)const {assert(valid);*s=api("cvs");return graph.at(object.name).cvs;}
    int numKnots(MStatus* s)const {assert(valid);*s=api("knots");return graph.at(object.name).knots;}
};
using DPIdList=std::vector<bool>;
using DPIndexList=std::vector<std::size_t>;
struct DPGroupData {
    using ConstPtr=std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    explicit DPGroupData(const DPIdList& source):ids(source){}
    const DPIdList& getGroupIds()const{return ids;}
};
struct KnotArray {unsigned n;unsigned length()const{return n;}};
struct DCurveInternalData;
using DCItem=std::shared_ptr<DCurveInternalData>;
struct DCurveInternalData {
    std::string source;std::size_t points;unsigned knots;
    static MStatus createFromMayaObject(const MObject& object,DCItem& result){
        const auto status=api("construct");if(!status)return status;
        ++constructions;
        if(nullCurve){result.reset();return MS::kSuccess;}
        const auto& node=graph.at(object.name);
        result=std::make_shared<DCurveInternalData>();result->source=object.name;
        result->points=node.cvs+(wrongCount ? 1U : 0U);result->knots=node.knots+(wrongKnots ? 1U : 0U);
        return MS::kSuccess;
    }
    std::size_t numberOfPoints()const{return points;}
    KnotArray getKnots()const{return {knots};}
};
struct DCurveData {
    static const int id=1;
    using DCurveList=std::vector<DCItem>;
    DCurveList curves;std::map<std::string,std::shared_ptr<DPGroupData>> groups;
    bool insert(const DCurveList& source){
        ++batches;if(!api("batch"))return false;
        for(const auto& curve:source)curves.push_back(std::make_shared<DCurveInternalData>(*curve));
        if(shortBatch && !curves.empty())curves.pop_back();return true;
    }
    bool insert(const DCItem& source){++singles;curves.push_back(source);return true;}
    std::size_t numberOfCurves()const{assert(!failed);return curves.size();}
    bool getCheckedTotalCvs(std::size_t& total)const{assert(!failed && !shortBatch);total=0;for(const auto& c:curves)total+=c->points;return true;}
    void addGrp(const std::string& name,const DPIdList& mask){++groupWrites;if(!missingGroup)groups[name]=std::make_shared<DPGroupData>(mask);}
    DPGroupData::ConstPtr getGrp(const std::string& name)const {const auto i=groups.find(name);return i==groups.end() ? nullptr : i->second;}
    std::size_t numberOfGroups()const{return groups.size();}
};
static DCurveData committed;
static std::shared_ptr<DCurveData> staged;
namespace MFnData {enum {kNurbsCurve=1,kOther=2};}
struct MDataHandle {
    bool valid;short mode;MObject object;
    MTime asTime()const{assert(valid);return MTime();}
    short asShort()const{assert(valid);return mode;}
    int type()const{assert(valid);return object.name=="skip" ? MFnData::kOther : MFnData::kNurbsCurve;}
    MObject data(){assert(valid);return object;}
};
struct MArrayDataHandle {
    const std::vector<MObject>* values;unsigned overrideCount;bool valid;unsigned position;
    unsigned elementCount(MStatus* s){assert(valid);*s=api("elements");return overrideCount ? overrideCount : static_cast<unsigned>(values->size());}
    MStatus jumpToArrayElement(unsigned i){assert(valid);auto s=api("jump "+std::to_string(i));if(s)position=i;return s;}
    MDataHandle inputValue(MStatus* s){assert(valid);*s=api("element");return {bool(*s),0,values->at(position)};}
};
static bool connected=true;
struct MPlugArray;
struct MPlug {
    std::string name;
    MPlug(const MObject&,const MObject& attribute):name(attribute.name){}
    explicit MPlug(const char* value):name(value){}
    bool operator!=(const MObject& other)const{return name!=other.name;}
    bool isConnected(MStatus* s)const{*s=api("connected");return connected;}
    bool connectedTo(MPlugArray&,bool,bool,MStatus*)const;
    MObject node(MStatus* s)const{*s=api("node");return {"root"};}
};
struct MPlugArray {unsigned length()const{return connected ? 1U : 0U;}MPlug operator[](unsigned)const{return MPlug("root");}};
bool MPlug::connectedTo(MPlugArray&,bool,bool,MStatus* s)const{*s=api("connections");return connected;}
struct MDataBlock {
    short mode=0;std::vector<MObject> inputs{{"A"},{"skip"},{"B"},{"A"}};unsigned countOverride=0;
    MDataHandle inputValue(const MObject& attribute,MStatus* s){*s=api("input "+attribute.name);return {bool(*s),mode,{}};}
    MArrayDataHandle inputArrayValue(const MObject&,MStatus* s){*s=api("array");return {&inputs,countOverride,bool(*s),0};}
    MDataHandle outputValue(const MObject&,MStatus* s){*s=api("output");return {bool(*s),0,{}};}
    MStatus setClean(const MPlug&){return api("clean");}
};
struct MFnPluginData {MObject object()const{return {"plugin"};}};
namespace dommy {namespace maya {
template<class T>MStatus createPluginData(MFnPluginData&,int,T*& value){
    auto s=api("plugin");if(!s)return s;staged=std::make_shared<T>();value=staged.get();return s;
}
template<class T>MStatus publishTypedPluginDataObject(MDataHandle& handle,const MObject&,int){
    assert(handle.valid);auto s=api("publish");if(!s)return s;committed=*staged;++published;return s;
}
}}
#include "curve_conversion_compute.inc"

static void reset(){
    calls=0;failAt=throwAt=-1;constructions=batches=singles=published=groupWrites=0;events.clear();failed=false;
    shortBatch=wrongCount=wrongKnots=nullCurve=missingGroup=false;connected=true;committed=DCurveData();staged.reset();
    graph.clear();graph["A"]=Node("nurbsCurve");graph["B"]=Node("nurbsCurve",{},false,4,6);
    graph["D"]=Node("nurbsCurve");graph["I"]=Node("nurbsCurve",{},true);
    graph["N"]=Node("transform",{{"B"}});graph["G"]=Node("transform",{{"A"},{"N"},{"I"}});
    graph["H"]=Node("transform",{{"A"}});graph["E"]=Node();
    graph["root"]=Node("transform",{{"D"},{"G"},{"E"},{"H"}});
}
static std::vector<std::string> order(){std::vector<std::string> result;for(const auto& c:committed.curves)result.push_back(c->source);return result;}
int main(){
    DCConvertFromCurves node;MPlug plug("outDC");
    int injectedCases=0;
    for(short mode:{short(0),short(1)}){
        reset();MDataBlock data;data.mode=mode;
        assert(node.compute(plug,data)==MS::kSuccess && batches==1 && singles==0 && published==1);
        const auto baseline=events;const int total=calls;
        if(mode==0){assert((order()==std::vector<std::string>{"A","B","A"}));assert(committed.groups.empty());}
        else {
            assert((order()==std::vector<std::string>{"D","B","A","A"}));
            assert(committed.groups.size()==3);
            assert((committed.groups.at("piece0")->ids==DPIdList{true,false,false,false}));
            assert((committed.groups.at("piece1")->ids==DPIdList{false,true,true,false}));
            assert((committed.groups.at("piece2")->ids==DPIdList{false,false,false,true}));
        }
        assert(graph.at("A").cvs==2 && graph.at("B").knots==6 && data.inputs.size()==4);
        for(MStatus status:{MStatus(MS::kFailure),MStatus(MS::kInvalidParameter)})
            for(int at=0;at<total;++at){
                reset();failAt=at;injected=status;
                assert(node.compute(plug,data)!=MS::kSuccess && calls==at+1);
                if(baseline[at]!="clean")assert(published==0);
                ++injectedCases;
            }
        for(int at=0;at<total;++at){reset();throwAt=at;assert(node.compute(plug,data)==MS::kFailure && calls==at+1);}
        for(int bad=0;bad<5;++bad){
            reset();shortBatch=bad==0;wrongCount=bad==1;wrongKnots=bad==2;nullCurve=bad==3;missingGroup=bad==4;
            if(mode==0 && missingGroup)continue;
            assert(node.compute(plug,data)==MS::kFailure && published==0);
            if(bad>=1 && bad<=3)assert(constructions==1 && batches==0);
            if(bad==4)assert(groupWrites==1);
        }
    }
    reset();MDataBlock data;
    data.countOverride=dommy::generation::kMaxConnectedGeometryCount+1;
    assert(node.compute(plug,data)==MS::kInvalidParameter && constructions==0 && batches==0);
    reset();data.countOverride=0;graph["B"].cvs=2000000;graph["B"].knots=2000002;
    assert(node.compute(plug,data)==MS::kInvalidParameter && constructions==0);
    reset();data.mode=1;graph["root"].countOverride=dommy::generation::kMaxConnectedGeometryCount+1;
    assert(node.compute(plug,data)==MS::kInvalidParameter && constructions==0);
    reset();graph["G"].countOverride=dommy::generation::kMaxConnectedGeometryCount;
    assert(node.compute(plug,data)==MS::kInvalidParameter && constructions==0);
    reset();std::size_t remaining=3;MStatus status;MFnDagNode parent({"H"},&status);MObjectArray curves;
    assert(getNurbsCurveNodeFromHierarchy(parent,curves,&remaining)==MS::kSuccess && remaining==2);
    assert(getNurbsCurveNodeFromHierarchy(parent,curves,&remaining)==MS::kSuccess && remaining==1);
    assert(getNurbsCurveNodeFromHierarchy(parent,curves,&remaining)==MS::kSuccess && remaining==0);
    assert(getNurbsCurveNodeFromHierarchy(parent,curves,&remaining)==MS::kInvalidParameter);
    reset();data.mode=0;data.inputs.clear();assert(node.compute(plug,data)==MS::kFailure && published==0);
    reset();data.mode=1;connected=false;assert(node.compute(plug,data)==MS::kUnknownParameter && published==0);
    reset();data.mode=7;assert(node.compute(plug,data)==MS::kInvalidParameter && published==0);
    reset();assert(node.compute(MPlug("other"),data)==MS::kUnknownParameter && calls==0);
    std::cout<<"Curve conversion: array/hierarchy order, groups, one batch, admission, "<<injectedCases
             <<" status failures and compute exceptions passed\n";
}

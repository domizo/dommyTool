#include "point_edit_compute_doubles.h"
#include "point_edit_compute.inc"
#include "../fixtures/point_edit_smoothing_reference.inc"

template<typename T>void field(const std::string& name,const std::vector<T>& values,const T& def=T()) {
    inputPoints.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values,def);
}
std::vector<int32_t> ids(const DPData& data) {
    return std::dynamic_pointer_cast<const TypedDPAttributeData<int32_t>>(data.getAttr("id"))->value();
}
void setup() {
    resetCompute();inputConnected=true;inputPoints=DPData();outputPoints=DPData();
    inputPoints.points={{0,0,0},{.5f,0,0},{2,0,0}};
    field<int32_t>("id",{0,1,2},7);
    inputPoints.groups["g"]=std::make_shared<Group>();inputPoints.groups["g"]->ids={false,true,false};
    inputPoints.groups["h"]=std::make_shared<Group>();inputPoints.groups["h"]->ids={true,false,false};
    outputPoints.points={{-99,-99,-99}};
}
MDataBlock block() {
    MDataBlock data;data.inputs["inDP"].payload=&inputPoints;data.inputs["inDP"].type=DPData::id;
    return data;
}
MDataBlock deleteBlock(const char* groups="",const char* pattern="1") {
    auto data=block();data.inputs["aDeleteGroups"].text=groups;data.inputs["aDeleteNumber"].text=pattern;return data;
}
MDataBlock sortBlock(int type=1,bool reverse=false) {
    auto data=block();data.inputs["aSortType"].number=type;data.inputs["aIsReverse"].number=reverse;
    data.inputs["aSeed"].number=42;data.inputs["aOffset"].number=1;
    data.inputs["aAlongVector"].vector=Imath::V3d(1,0,0);
    data.inputs["aTargetPoint"].vector=Imath::V3d(1,0,0);
    data.inputs["aAttributeName"].text="id";data.inputs["aAttrComponent"].number=0;return data;
}
MDataBlock smoothBlock(int iterations=2) {
    auto data=block();data.inputs["aRadius"].number=1.5;
    data.inputs["aMaxEffectiveRadius"].number=.5;data.inputs["aExpandBound"].number=.5;
    data.inputs["aIteration"].number=iterations;return data;
}
MStatus removePoints(MDataBlock& data){return DPDelete().compute(MPlug(DPDelete::outDP),data);}
MStatus sortPoints(MDataBlock& data){return DPSort().compute(MPlug(DPSort::outDP),data);}
MStatus smoothPoints(MDataBlock& data){return DPSmooth().compute(MPlug(DPSmooth::outDP),data);}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
static int smoothFieldCopies=0;
struct SmoothObservedField : DPIntAttr {
    using DPIntAttr::DPIntAttr;
    DPAttributeData::Ptr copy()const override{++smoothFieldCopies;return std::make_shared<SmoothObservedField>(*this);}
};
void testSmoothOwnership(){
    for(int iterations:{0,2}){
        setup();smoothFieldCopies=0;inputPoints.attributes["id"]=std::make_shared<SmoothObservedField>(std::vector<int32_t>{0,1,2},7);
        auto data=smoothBlock(iterations);assert(smoothPoints(data)&&smoothFieldCopies==1&&geometryCopies==1&&positionCopies==0);
        assert(std::count(operations.begin(),operations.end(),"bounds.update")==1);
        assert(!called("point.copy")&&!called("point.replace"));
        assert(outputPoints.attributes.at("id")!=inputPoints.attributes.at("id"));
        assert(outputPoints.groups.at("g")!=inputPoints.groups.at("g"));
        const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("id"));
        assert(retained&&retained->defaultValue()==7&&ids(outputPoints)==ids(inputPoints));
        assert(outputPoints.groups.at("g")->ids==inputPoints.groups.at("g")->ids);
    }
    setup();inputPoints.points.clear();inputPoints.attributes["id"]=std::make_shared<DPIntAttr>(std::vector<int32_t>(),7);
    for(auto& group:inputPoints.groups)group.second->ids.clear();auto data=smoothBlock(-1);assert(smoothPoints(data));
    const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("id"));
    assert(retained&&retained->defaultValue()==7&&outputPoints.groups.size()==2);
}
void reducedSmoothBudgets(){
    for(int variant=0;variant<4;++variant){
        setup();auto data=smoothBlock();
        if(variant==0)field<std::string>("large",std::vector<std::string>(3,std::string(180,'x')));
        if(variant==1)field<DIntArray>("large",std::vector<DIntArray>(3,DIntArray(100,1)));
        if(variant==2)for(int i=0;i<40;++i)field<float>("field"+std::to_string(i),{1,2,3});
        if(variant==3)for(int i=0;i<10;++i){auto group=std::make_shared<Group>();group->ids={true,false,true};inputPoints.groups[std::string(80,'g')+std::to_string(i)]=group;}
        assert(!smoothPoints(data)&&!called("output.create")&&geometryCopies==0&&positionViews==0&&publications==0);
    }
    // Metadata and node arrays fit, but the valid grid's exact cell storage
    // crosses the combined allowance before allocation/output construction.
    setup();auto data=smoothBlock();data.inputs["aExpandBound"].number=8;
    assert(!smoothPoints(data)&&!called("output.create")&&geometryCopies==0);
}
void testDelete() {
    struct Case{const char* groups;const char* pattern;std::vector<int32_t> expected;};
    for(const auto& c:std::vector<Case>{{"","1",{0,2}},{"","! 1",{1}},
        {"","1-3",{0}},{"g","0",{2}},{"!g","",{1}},{"","",{}},
        {"   ","1",{0,2}},{"unknown","",{0,1,2}},{"g","",{0,2}},
        {"","99",{0,1,2}},{"",";",{}},{"","\t \n",{}},{"g g","1",{0,2}}}) {
        setup();auto data=deleteBlock(c.groups,c.pattern);assert(removePoints(data)&&publications==1);
        assert(ids(outputPoints)==c.expected);assert(ids(inputPoints)==std::vector<int32_t>({0,1,2}));
        for(std::size_t i=0;i<c.expected.size();++i) {
            assert(outputPoints.points[i][0]==inputPoints.points[c.expected[i]][0]);
            assert(outputPoints.groups.at("h")->ids[i]==(c.expected[i]==0));
        }
        if(std::string(c.groups)=="g")assert(!outputPoints.groups.count("g"));
    }
    for(const char* pattern:{"x","-1","1--3"}) {
        setup();auto data=deleteBlock("",pattern);assert(!removePoints(data)&&publications==0&&geometryCopies==0);
    }
    setup();inputPoints=DPData();auto data=deleteBlock("g","bad");
    assert(removePoints(data)&&outputPoints.points.empty()&&!called("input aDeleteNumber"));
}
void testSort() {
    for(int type=0;type<10;++type)for(bool reverse:{false,true}) {
        setup();auto data=sortBlock(type,reverse);std::vector<int32_t> expected={0,1,2};
        if(type==1||type==4||type==8||type==9)expected={2,1,0};
        if(type==5){std::default_random_engine engine(42);std::shuffle(expected.begin(),expected.end(),engine);}
        if(type==6)expected={2,0,1};
        if(type==7)expected={1,0,2};
        if(type&&reverse)std::reverse(expected.begin(),expected.end());
        assert(sortPoints(data)&&publications==1);assert(ids(outputPoints)==expected);
        for(std::size_t i=0;i<3;++i)assert(outputPoints.groups["g"]->ids[i]==(expected[i]==1));
    }
    setup();auto data=sortBlock(6);data.inputs["aOffset"].number=std::numeric_limits<int>::min();
    assert(sortPoints(data));assert(ids(outputPoints)==std::vector<int32_t>({2,0,1}));
    const float maximum=std::numeric_limits<float>::max();
    setup();inputPoints.points={{-maximum,0,0},{0,0,0},{maximum,0,0}};data=sortBlock(7);
    data.inputs["aTargetPoint"].vector=Imath::V3d(maximum,0,0);
    assert(sortPoints(data)&&ids(outputPoints)==std::vector<int32_t>({2,1,0}));
    setup();data=sortBlock(7);data.inputs["aTargetPoint"].vector=Imath::V3d(1e100,0,0);
    assert(sortPoints(data)&&ids(outputPoints)==std::vector<int32_t>({0,1,2}));
    setup();data=sortBlock(9);field<int64_t>("large",{9007199254740992LL,9007199254740993LL,9007199254740994LL});
    data.inputs["aAttributeName"].text="large";assert(sortPoints(data)&&ids(outputPoints)==std::vector<int32_t>({2,1,0}));
    setup();data=sortBlock(9);field<Imath::V3f>("vector",{Imath::V3f(0,3,0),Imath::V3f(0,1,0),Imath::V3f(0,2,0)});
    data.inputs["aAttributeName"].text="vector";data.inputs["aAttrComponent"].number=1;
    assert(sortPoints(data)&&ids(outputPoints)==std::vector<int32_t>({0,2,1}));
    setup();inputPoints=DPData();data=sortBlock(9);data.inputs["aAttributeName"].text="missing";
    assert(sortPoints(data)&&!called("input aAttributeName"));
    for(int variant=0;variant<6;++variant) {
        setup();data=sortBlock(variant<2?7:variant<4?8:9);
        if(variant==0)data.inputs["aTargetPoint"].vector[0]=NAN;
        if(variant==1)data.inputs["aTargetPoint"].vector=Imath::V3d(1.7e308,1.7e308,1.7e308);
        if(variant==2)data.inputs["aAlongVector"].vector=Imath::V3d();
        if(variant==3)data.inputs["aAlongVector"].vector[0]=INFINITY;
        if(variant==4)data.inputs["aAttributeName"].text="missing";
        if(variant==5)data.inputs["aSortType"].number=10;
        assert(!sortPoints(data)&&publications==0&&geometryCopies==0);
    }
}
void testSmooth() {
    for(int iterations:{0,1,3})for(float effective:{0.0f,.5f,2.0f}) {
        setup();auto data=smoothBlock(iterations);data.inputs["aMaxEffectiveRadius"].number=effective;
        DPData expected;expected.setDPData(inputPoints);
        assert(legacySmoothReference(expected,1.5f,effective,.5f,iterations));resetCompute();
        assert(smoothPoints(data)&&publications==1);assert(ids(outputPoints)==ids(inputPoints));
        for(std::size_t i=0;i<3;++i)for(unsigned axis=0;axis<3;++axis)
            assert(outputPoints.points[i][axis]==expected.points[i][axis]);
    }
    setup();inputPoints=DPData();auto data=smoothBlock(-1);
    assert(smoothPoints(data)&&!called("input aRadius"));
    for(int variant=0;variant<9;++variant) {
        setup();data=smoothBlock();
        if(variant==0)inputConnected=false;
        if(variant==1)data.inputs["aRadius"].number=0;
        if(variant==2)data.inputs["aRadius"].number=NAN;
        if(variant==3)data.inputs["aMaxEffectiveRadius"].number=-1;
        if(variant==4)data.inputs["aExpandBound"].number=-1;
        if(variant==5)data.inputs["aIteration"].number=-1;
        if(variant==6){
            data.inputs["aIteration"].number=100000000;
            // If preflight is removed, fail on the later bounds read rather
            // than running huge loops. Detached staging has already begun at
            // that point, so the view assertion rejects this late failure.
            failAt=7;
        }
        if(variant==7)inputPoints.points[0][0]=NAN;
        if(variant==8){inputPoints.points={{0,0,0},{128,128,128},{256,256,256}};data.inputs["aMaxEffectiveRadius"].number=1;}
        assert(!smoothPoints(data)&&publications==0);
        if(variant<8)assert(geometryCopies==0&&!called("output.create")&&positionViews==0);
    }
}
std::string signature(const DPData& data) {
    std::ostringstream out;for(const auto& p:data.points)for(unsigned axis=0;axis<3;++axis)out<<p[axis]<<',';
    if(data.getAttr("id"))for(auto id:ids(data))out<<id<<',';
    for(const auto& g:data.groups){out<<g.first;for(bool id:g.second->ids)out<<id;}
    return out.str();
}
template<typename Compute>int failures(MDataBlock data,Compute compute) {
    resetCompute();const auto source=signature(inputPoints);assert(compute(data));
    const auto trace=operations;const auto output=signature(outputPoints);int count=0;
    for(int index=0;index<int(trace.size());++index) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            resetCompute(index);injectedStatus=code;const auto result=compute(data);
            assert(!result&&callCount==index+1);
            const std::set<std::string> collapsed={"point.copy","point.delete","point.sort","groups.remove","bounds.read","bounds.update"};
            const auto expected=trace[index]=="point.replace"?MS::kInvalidParameter:collapsed.count(trace[index])?MS::kFailure:code;
            assert(result==expected);if(trace[index]!="output.clean")assert(publications==0);
            assert(signature(inputPoints)==source&&signature(outputPoints)==output);++count;
        }
        resetCompute();attributeThrowAt=index;assert(compute(data)==MS::kFailure&&callCount==index+1);
        if(trace[index]!="output.clean")assert(publications==0);
        assert(signature(inputPoints)==source&&signature(outputPoints)==output);++count;
    }
    resetCompute();assert(compute(data));return count;
}
int main(int argc,char**) {
    if(argc>1){reducedSmoothBudgets();return 0;}
    testDelete();testSort();testSmooth();testSmoothOwnership();int count=0;
    setup();count+=failures(deleteBlock("g","0"),removePoints);
    setup();count+=failures(deleteBlock(),removePoints);
    for(int type=0;type<10;++type){setup();count+=failures(sortBlock(type),sortPoints);}
    for(int iterations:{0,2}){setup();count+=failures(smoothBlock(iterations),smoothPoints);}
    for(auto compute:{removePoints,sortPoints,smoothPoints})for(int variant=0;variant<4;++variant) {
        setup();auto data=compute==removePoints?deleteBlock():compute==sortPoints?sortBlock():smoothBlock();
        if(variant==0)data.inputs["inDP"].payload=nullptr;
        if(variant==1)data.inputs["inDP"].type=999;
        if(variant==2)inputPoints.topology=false;if(variant==3)rejectFinal=true;
        assert(!compute(data)&&publications==0);
        if(variant==3)assert(geometryCopies==1&&outputPoints.points[0][0]==-99);
    }
    setup();auto data=block();assert(DPDelete().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);
    assert(DPSort().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);
    assert(DPSmooth().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
    std::cout<<"Point editing: delete/sort/smooth contracts and "<<count<<" compute status/exception injections passed\n";
}

// Real point-batch bodies and typed metadata. Only cached native bounds are
// doubled; these counters are not Maya performance or leak measurements.
#include "mesh_metadata_fixture.h"
#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DNumericValidation.h"
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
static std::size_t topologyChecks = 0U;
struct DPData {
    DPointList pointArray;
    DPAttrList mAttributeList;
    DPGrpList mGrooupList;
    std::size_t mBBox = 99U;
    bool mBBoxDirty = true;
    using PointCopyOperation = std::function<MStatus(DPoint&,std::size_t,std::size_t)>;
    std::size_t numberOfPoints() const {return pointArray.size();}
    ConstDPAttrIter beginAttr()const{return mAttributeList.begin();}
    ConstDPAttrIter endAttr()const{return mAttributeList.end();}
    ConstDPGrpIter beginGrp()const{return mGrooupList.begin();}
    ConstDPGrpIter endGrp()const{return mGrooupList.end();}
    bool hasValidPointTopology()const;
    bool appendMetadataFrom(const DPData&,std::size_t);
    MStatus mergeFrom(const std::vector<const DPData*>&);
    MStatus copyRepeatedFrom(const DPData&,std::size_t,const PointCopyOperation& = PointCopyOperation());
    void setDPData(const DPData&);
    bool mergeDPData(const DPData&);
    void updateBBox() {++boundsCalls;if(failBounds)throw std::bad_alloc();mBBox=pointArray.size();mBBoxDirty=false;}
};
#include "point_batch_transactions.inc"

DPData sample() {
    DPData source;source.pointArray={{1,2,3},{4,5,6}};
    source.mAttributeList["id"]=std::make_shared<TrackedAttribute<int32_t>>(std::vector<int32_t>{8,9},4);
    source.mGrooupList["g"]=std::make_shared<TrackedGroup>(DPIdList{true,false});
    return source;
}
template<class T>std::vector<T> values(const DPData& data,const std::string& name) {
    auto typed=std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(data.mAttributeList.at(name));
    assert(typed);return typed->value();
}
void unchanged(const DPData& data,const DPData& before) {
    assert(data.pointArray.size()==before.pointArray.size());
    for(std::size_t i=0;i<data.pointArray.size();++i)for(unsigned axis=0;axis<3;++axis)
        assert(data.pointArray[i][axis]==before.pointArray[i][axis]);
    assert(data.mAttributeList==before.mAttributeList &&
        data.mGrooupList==before.mGrooupList && data.mBBox==before.mBBox && data.mBBoxDirty==before.mBBoxDirty);
}
void testRepeated() {
    for(std::size_t count:{1U,100U,200U,1000U,20000U}) {
        reset();DPData source=sample(),output;
        std::size_t visits=0;
        assert(output.copyRepeatedFrom(source,count,[&](DPoint& p,std::size_t copy,std::size_t index){
            assert(visits==copy*2U+index);++visits;p[0]+=static_cast<float>(copy+1U);return MS::kSuccess;
        }));
        assert(output.numberOfPoints()==count*2U && visits==count*2U && boundsCalls==1);
        assert(attributeCopies==1 && groupCopies==1);
        const auto ids=values<int32_t>(output,"id");
        for(std::size_t copy=0;copy<count;++copy) {
            assert(output.pointArray[copy*2U][0]==float(copy+2U));
            assert(ids[copy*2U]==8);
            assert(output.mGrooupList.at("g")->getGroupIds()[copy*2U]);
        }
        assert(source.pointArray[0][0]==1 && source.numberOfPoints()==2U);
        assert(output.mAttributeList.at("id")!=source.mAttributeList.at("id"));
        assert(output.mGrooupList.at("g")!=source.mGrooupList.at("g"));
    }
    reset();DPData alias=sample();assert(alias.copyRepeatedFrom(alias,3U));
    assert(alias.numberOfPoints()==6U && attributeCopies==1 && groupCopies==1 && boundsCalls==1);
    reset();DPData empty,output=sample();
    empty.mAttributeList["id"]=std::make_shared<TrackedAttribute<int32_t>>(std::vector<int32_t>(),7);
    empty.mGrooupList["empty"]=std::make_shared<TrackedGroup>(DPIdList());
    assert(output.copyRepeatedFrom(empty,1000000U,[](DPoint&,std::size_t,std::size_t)->MStatus{assert(false);return MS::kFailure;}));
    assert(output.pointArray.empty() && output.mAttributeList.size()==1U && output.mGrooupList.size()==1U);
    assert(attributeCopies==1 && groupCopies==1 && boundsCalls==1);
}
void testMerge() {
    reset();DPData first=sample(),second,empty,output;
    second.pointArray={{7,8,9}};
    second.mAttributeList["other"]=std::make_shared<TrackedAttribute<float>>(std::vector<float>{2},5);
    second.mGrooupList["other"]=std::make_shared<TrackedGroup>(DPIdList{true});
    empty.mAttributeList["id"]=std::make_shared<TrackedAttribute<double>>(std::vector<double>(),99);
    empty.mAttributeList["ignored"]=std::make_shared<TrackedAttribute<double>>(std::vector<double>(),42);
    assert(output.mergeFrom({&first,&empty,&second}));
    assert(output.numberOfPoints()==3U && output.pointArray[0][0]==1 && output.pointArray[1][0]==4 && output.pointArray[2][0]==7);
    assert(values<int32_t>(output,"id")==std::vector<int32_t>({8,9,4}));
    assert(values<float>(output,"other")==std::vector<float>({5,5,2}));
    assert(output.mAttributeList.count("ignored")==0U);
    assert(output.mGrooupList.at("g")->getGroupIds()==DPIdList({true,false,false}));
    assert(output.mGrooupList.at("other")->getGroupIds()==DPIdList({false,false,true}));
    assert(attributeCopies==2 && groupCopies==2 && boundsCalls==1);
    DPData legacy;legacy.setDPData(first);assert(legacy.mergeDPData(empty)&&legacy.mergeDPData(second));
    assert(values<int32_t>(legacy,"id")==values<int32_t>(output,"id"));
    assert(values<float>(legacy,"other")==values<float>(output,"other"));
    for(const auto& group:output.mGrooupList)
        assert(legacy.mGrooupList.at(group.first)->getGroupIds()==group.second->getGroupIds());
    reset();first=sample();assert(first.mergeFrom({&first,&first}));
    assert(first.numberOfPoints()==4U && attributeCopies==1 && groupCopies==1 && boundsCalls==1);
    reset();DPData source=sample();std::vector<const DPData*> inputs(20000U,&source);
    assert(output.mergeFrom(inputs) && output.numberOfPoints()==40000U && attributeCopies==1 && groupCopies==1 && boundsCalls==1);
    reset();assert(output.mergeFrom({}) && output.pointArray.empty() && output.mAttributeList.empty() && boundsCalls==1);
    reset();DPData zero;zero.mAttributeList["a"]=std::make_shared<TrackedAttribute<float>>(std::vector<float>(),3);
    DPData missing;missing.pointArray={{0,0,0}};
    assert(output.mergeFrom({&zero,&missing}));assert(values<float>(output,"a")==std::vector<float>({3}));
}
template<class T>bool sameValue(const T& a,const T& b){return a==b;}
template<class T>bool sameValue(const Imath::Vec2<T>& a,const Imath::Vec2<T>& b){return a[0]==b[0]&&a[1]==b[1];}
template<class T>bool sameValue(const Imath::Vec3<T>& a,const Imath::Vec3<T>& b){return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];}
template<>bool sameValue(const MMatrix& a,const MMatrix& b){
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)if(a[r][c]!=b[r][c])return false;
    return true;
}
template<class T>void testType(const T& value,const T& defaultValue) {
    reset();DPData source,missing,output;source.pointArray={{1,2,3}};missing.pointArray={{4,5,6}};
    source.mAttributeList["a"]=std::make_shared<TrackedAttribute<T>>(std::vector<T>{value},defaultValue);
    assert(output.copyRepeatedFrom(source,3U));assert(values<T>(output,"a").size()==3U && attributeCopies==1);
    reset();assert(output.mergeFrom({&missing,&source,&missing}));
    auto field=std::dynamic_pointer_cast<TypedDPAttributeData<T>>(output.mAttributeList.at("a"));
    assert(field && field->attrSize()==3U && attributeCopies==1);
    const auto rows=field->value();
    assert(sameValue(T(rows[0]),defaultValue)&&sameValue(T(rows[1]),value)&&sameValue(T(rows[2]),defaultValue));
    assert(sameValue(field->defaultValue(),defaultValue));
}
void testAllTypes() {
    testType<bool>(true,false);testType<int32_t>(8,4);testType<uint32_t>(9,3);testType<int64_t>(10000000000LL,-7);
    testType<float>(2,3);testType<double>(4,5);
    testType<Imath::V2i>({1,2},{3,4});testType<Imath::V2f>({1,2},{3,4});testType<Imath::V2d>({1,2},{3,4});
    testType<Imath::V3i>({1,2,3},{4,5,6});testType<Imath::V3f>({1,2,3},{4,5,6});testType<Imath::V3d>({1,2,3},{4,5,6});
    testType<std::string>("row","default");testType<DIntArray>({1,2},{3,4});
    MMatrix row,defaultMatrix;row[3][0]=7;defaultMatrix[3][0]=9;testType<MMatrix>(row,defaultMatrix);
}
struct AliasAttribute:DPIntAttr {
    using DPIntAttr::DPIntAttr;std::weak_ptr<DPAttributeData> alias;
    DPAttributeData::Ptr copy()const override{return alias.lock();}
};
struct AliasGroup:DPGroupData {
    using DPGroupData::DPGroupData;std::weak_ptr<DPGroupData> alias;
    DPGroupData::Ptr copy()const override{return alias.lock();}
};
void testFailures() {
    for(int mode=0;mode<9;++mode) {
        reset();DPData source=sample(),output=sample();const DPData before=output;
        if(mode<3){failAttributeAt=1;metadataFailure=mode+1;}
        else if(mode<6){failGroupAt=1;metadataFailure=mode-2;}
        else if(mode==6)failBounds=true;
        MStatus result;
        if(mode==7)result=output.copyRepeatedFrom(source,3U,[](DPoint&,std::size_t step,std::size_t){return step==1?MS::kInvalidParameter:MS::kSuccess;});
        else if(mode==8)result=output.copyRepeatedFrom(source,3U,[](DPoint&,std::size_t,std::size_t)->MStatus{throw std::bad_alloc();});
        else result=output.copyRepeatedFrom(source,3U);
        assert(!result);unchanged(output,before);assert(source.pointArray[0][0]==1);
        reset();if(mode<3){failAttributeAt=1;metadataFailure=mode+1;}
        else if(mode<6){failGroupAt=1;metadataFailure=mode-2;}
        else if(mode==6)failBounds=true;
        if(mode<=6){assert(!output.mergeFrom({&source,&source}));unchanged(output,before);}
        reset();if(mode<3){failAttributeAt=1;metadataFailure=mode+1;}
        else if(mode<6){failGroupAt=1;metadataFailure=mode-2;}
        else if(mode==6)failBounds=true;
        if(mode<=6) {
            bool rejected=false;try{output.setDPData(source);}catch(const std::exception&){rejected=true;}
            assert(rejected);unchanged(output,before);
            reset();output.setDPData(source);
            assert(attributeCopies==1 && groupCopies==1 && boundsCalls==1);
            assert(output.mAttributeList.at("id")!=source.mAttributeList.at("id"));
            assert(output.mGrooupList.at("g")!=source.mGrooupList.at("g"));
        }
    }
    reset();DPData source=sample(),output=sample();const DPData before=output;
    for(std::size_t count:{0U,1000001U}){assert(!output.copyRepeatedFrom(source,count));unchanged(output,before);}
    assert(!output.mergeFrom({nullptr}));unchanged(output,before);
    DPData incompatible=source;incompatible.mAttributeList["id"]=std::make_shared<DPDoubleAttr>(std::vector<double>{1,2});
    assert(!output.mergeFrom({&source,&incompatible}));unchanged(output,before);
    source.pointArray[0][0]=NAN;assert(!output.copyRepeatedFrom(source,2U));unchanged(output,before);
    source=sample();assert(!output.copyRepeatedFrom(source,2U,[](DPoint& point,std::size_t,std::size_t){point[0]=INFINITY;return MS::kSuccess;}));unchanged(output,before);
    auto a=std::make_shared<AliasAttribute>(std::vector<int32_t>{1,2});a->alias=a;source.mAttributeList["id"]=a;
    assert(!output.copyRepeatedFrom(source,2U));unchanged(output,before);
    source=sample();auto g=std::make_shared<AliasGroup>(DPIdList{true,false});g->alias=g;source.mGrooupList["g"]=g;
    assert(!output.copyRepeatedFrom(source,2U));unchanged(output,before);
}
struct CountOwner {
    std::size_t count=0;DPAttrList fields;DPGrpList groups;
    std::size_t numberOfPoints()const{return count;}
    ConstDPAttrIter beginAttr()const{return fields.begin();}ConstDPAttrIter endAttr()const{return fields.end();}
    ConstDPGrpIter beginGrp()const{return groups.begin();}ConstDPGrpIter endGrp()const{return groups.end();}
};
void testBudgets() {
    CountOwner source;dommy::point::PointMergePlan plan;
    source.count=(256U*1024U*1024U/4U)/sizeof(Imath::V3f);
    assert(dommy::point::buildPointCopyPlan(source,1U,plan));const auto before=plan;
    ++source.count;assert(!dommy::point::buildPointCopyPlan(source,1U,plan));assert(plan.points==before.points&&plan.estimatedBytes==before.estimatedBytes);
    source.count=1;source.fields["a"]=std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{{1,2,3}});
    source.groups["g"]=std::make_shared<DPGroupData>(DPIdList{true});
    assert(dommy::point::buildPointCopyPlan(source,100U,plan));const std::size_t small=plan.workItems;
    assert(dommy::point::buildPointCopyPlan(source,200U,plan));assert(plan.workItems-small==1000U);
    CountOwner empty,missing;missing.count=100000U;
    empty.fields["a"]=std::make_shared<DPStringAttr>(std::vector<std::string>(),std::string(1001,'x'));
    const std::vector<const CountOwner*> inputs{&empty,&missing};assert(!dommy::point::buildPointMergePlan(inputs,plan));
    empty.fields.clear();empty.fields["a"]=std::make_shared<DPIntArrayAttr>(std::vector<DIntArray>(),DIntArray(10001,1));
    std::vector<const CountOwner*> ignored(10000U,&empty);ignored[0]=&source;
    assert(!dommy::point::buildPointMergePlan(ignored,plan));
}
void testStrings() {
    reset();DPData source,output=sample();source.pointArray={{1,2,3},{4,5,6}};
    source.mAttributeList["s"]=std::make_shared<DPStringAttr>(std::vector<std::string>{std::string(40,'x'),std::string(40,'y')});
    dommy::point::PointMergePlan plan;
    assert(!dommy::point::buildPointCopyPlan(source,2U,plan));
    const DPData before=output;assert(!output.copyRepeatedFrom(source,2U));unchanged(output,before);
    DPData empty,missing;missing.pointArray.assign(40U,DPoint(0,0,0));
    empty.mAttributeList["s"]=std::make_shared<DPStringAttr>(std::vector<std::string>(),std::string(4,'d'));
    assert(!dommy::point::buildPointMergePlan(std::vector<const DPData*>{&empty,&missing},plan));
    assert(!output.mergeFrom({&empty,&missing}));unchanged(output,before);
    // A legal virtual clone can still grow the retained payload across fields.
    struct Growing:DPIntArrayAttr {
        using DPIntArrayAttr::DPIntArrayAttr;
        DPAttributeData::Ptr copy()const override{return std::make_shared<DPIntArrayAttr>(std::vector<DIntArray>(2U),DIntArray(256U,1));}
    };
    source.mAttributeList.clear();
    source.mAttributeList["a"]=std::make_shared<Growing>(std::vector<DIntArray>(2U));
    assert(!output.copyRepeatedFrom(source,2U));unchanged(output,before);
    bool rejected=false;try{output.setDPData(source);}catch(const std::length_error&){rejected=true;}
    assert(rejected);unchanged(output,before);
    // Individually valid matrix fields exceed the shared aggregate allowance.
    source.mAttributeList.clear();
    for(const std::string name:{"a","b","c"})
        source.mAttributeList[name]=std::make_shared<TrackedAttribute<MMatrix>>(std::vector<MMatrix>(2U));
    assert(source.hasValidPointTopology());
    assert(!dommy::point::buildPointCopyPlan(source,1U,plan));
    reset();rejected=false;try{output.setDPData(source);}catch(const std::length_error&){rejected=true;}
    assert(rejected && attributeCopies==0 && groupCopies==0 && boundsCalls==0);unchanged(output,before);
    source.mAttributeList.erase("c");reset();output.setDPData(source);
    assert(attributeCopies==2 && boundsCalls==1);
    const auto field=output.mAttributeList.at("a");reset();output.setDPData(output);
    assert(attributeCopies==0 && boundsCalls==0 && output.mAttributeList.at("a")==field);
}
int main(int argc,char**) {
    if(argc>1){testStrings();std::cout<<"Point batch bounded strings: OK\n";return 0;}
    testRepeated();testMerge();testAllTypes();testFailures();testBudgets();
    std::cout<<"Point batches: owning merges, repetition, defaults, groups, failure rollback and linear clone/bounds counts: OK\n";
}

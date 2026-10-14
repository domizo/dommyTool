#include "point_utility_compute_doubles.h"
#include "point_utility_compute.inc"

template <typename T>
void field(DPData& data, const std::string& key, const std::vector<T>& values,
           const T& defaultValue = typeDefaultVal<T>()) {
    data.attributes[key] = std::make_shared<TypedDPAttributeData<T>>(values, defaultValue);
}
template <typename T> std::vector<T> values(const DPData& data, const std::string& key) {
    const auto a = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(data.getAttr(key));
    assert(a); return a->value();
}
MDataBlock particleBlock() {
    MDataBlock data;
    data.inputs["aIsVelocity"].number = 1;
    data.inputs["aVelocityAttribute"].text = "vel";
    data.inputs["aIsRadius"].number = 1;
    data.inputs["aRadiusAttribute"].text = "pscale";
    data.inputs["aRadius"].number = 0.25;
    data.inputs["aIsMass"].number = 1;
    data.inputs["aMassAttribute"].text = "mass";
    data.inputs["aMass"].number = 2;
    return data;
}
MDataBlock fillBlock() {
    MDataBlock data;
    data.inputs["samplingType"].number = 0;
    data.inputs["samplingCount"].number = 1;
    data.inputs["samplingSpace"].number = 1;
    data.inputs["isIDStepFill"].number = 1;
    data.inputs["nextStepSize"].number = 1;
    data.inputs["distance"].number = 3;
    return data;
}
MDataBlock particleCopyBlock() {
    auto data=particleBlock();
    field<Imath::V3d>(inputData,"vel",{Imath::V3d(1,2,3),Imath::V3d(4,5,6)});
    field<double>(inputData,"pscale",{3,5});field<int32_t>(inputData,"mass",{4,6});
    data.inputs["aVelocityType"].number=1;data.inputs["aVelocityTargetAttribute"].text="vel";
    data.inputs["aRadiusType"].number=1;data.inputs["aRadiusTargetAttribute"].text="pscale";
    data.inputs["aMassType"].number=1;data.inputs["aMassTargetAttribute"].text="mass";
    return data;
}
void setup();
template <typename T> void testScalarSource(const T& a,const T& b) {
    setup();auto data=particleBlock();field<T>(inputData,"src",{a,b});
    data.inputs["aRadiusType"].number=1;data.inputs["aRadiusTargetAttribute"].text="src";
    assert(DPParticleAttribute().compute(MPlug(DPParticleAttribute::outDP),data));
    const auto output=values<float>(outputData,"pscale");
    assert(output[0]==static_cast<float>(a) && output[1]==static_cast<float>(b));
}
template <typename T> void testVectorSource(const T& a) {
    setup();auto data=particleBlock();field<T>(inputData,"src",{a,a});
    data.inputs["aVelocityType"].number=1;data.inputs["aVelocityTargetAttribute"].text="src";
    assert(DPParticleAttribute().compute(MPlug(DPParticleAttribute::outDP),data));
    const auto output=values<Imath::V3f>(outputData,"vel");
    for(int axis=0;axis<3;++axis)assert(output[0][axis]==static_cast<float>(a[axis]));
}
void setup() {
    resetCompute(); inputData = DPData(); outputData = DPData(); arrayInputs.clear();
    inputData.points = {MPoint(0,0,0), MPoint(2,0,0)};
}
void testParticle() {
    setup(); auto data = particleBlock();
    const MPlug plug(DPParticleAttribute::outDP);
    assert(DPParticleAttribute().compute(plug,data));
    assert(publications == 1 && inputData.attributes.empty());
    assert(values<float>(outputData,"pscale") == std::vector<float>({0.25f,0.25f}));
    assert(values<float>(outputData,"mass") == std::vector<float>({2,2}));
    const auto velocity = values<Imath::V3f>(outputData,"vel");
    assert(velocity.size()==2 && velocity[0][1]==1 && velocity[0][0]==0);
    const auto stored = std::dynamic_pointer_cast<const TypedDPAttributeData<float>>(outputData.getAttr("pscale"));
    assert(stored->getDefaultValue()==0);

    // Same-name copying must read before replacing; storage stays historically float-backed.
    setup(); data = particleBlock();
    field<double>(inputData,"pscale",{3.0,5.0},9.0);
    data.inputs["aRadiusType"].number=1;
    data.inputs["aRadiusTargetAttribute"].text="pscale";
    assert(DPParticleAttribute().compute(plug,data));
    assert(values<float>(outputData,"pscale")==std::vector<float>({3,5}));
    assert(values<double>(inputData,"pscale")==std::vector<double>({3,5}));
    assert(std::dynamic_pointer_cast<const TypedDPAttributeData<double>>(inputData.getAttr("pscale"))->getDefaultValue()==9);

    // Keep sequential dependencies and allow a deliberate stored-type replacement.
    data.inputs["aRadiusType"].number=0;
    data.inputs["aMassType"].number=1;
    data.inputs["aMassTargetAttribute"].text="pscale";
    resetCompute(); assert(DPParticleAttribute().compute(plug,data));
    assert(values<float>(outputData,"mass")==std::vector<float>({0.25f,0.25f}));

    const char* modes[]={"aVelocityType","aRadiusType","aMassType"};
    const char* sources[]={"aVelocityTargetAttribute","aRadiusTargetAttribute","aMassTargetAttribute"};
    for(int channel=0;channel<3;++channel) {
        for(const char* source : {"missing", "wrong"}) {
            setup(); data=particleBlock(); field<std::string>(inputData,"wrong",{"a","b"});
            field<float>(inputData,"pscale",{3,5});
            data.inputs[modes[channel]].number=1;data.inputs[sources[channel]].text=source;
            assert(!DPParticleAttribute().compute(plug,data) && publications==0);
            assert(values<float>(inputData,"pscale")==std::vector<float>({3,5}));
        }
        setup();data=particleBlock();data.inputs[modes[channel]].number=2;
        if(channel==0)field<Imath::V3f>(inputData,"source",{Imath::V3f(1,2,3),Imath::V3f(4,5,6)});
        else field<float>(inputData,"source",{1,2});
        data.inputs[sources[channel]].text="source";
        assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    }
    for(const char* control : {"aRadius", "aMass"}) {
        setup();data=particleBlock();data.inputs[control].number=NAN;
        assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    }
    setup();data=particleBlock();data.inputs["aVelocityVector"].vector[2]=INFINITY;
    assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    setup();data=particleBlock();field<double>(inputData,"huge",{1e100,1e100});
    data.inputs["aRadiusType"].number=1;data.inputs["aRadiusTargetAttribute"].text="huge";
    assert(!DPParticleAttribute().compute(plug,data) && publications==0);

    setup();data=particleBlock();data.inputs["aRadiusAttribute"].text="";
    assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    setup();data=particleBlock();stringLength=dommy::generation::kMaxGeneratedStringByteCount+1U;
    assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    setup();data=particleBlock();data.inputs["aRadiusAttribute"].text="with space";
    assert(DPParticleAttribute().compute(plug,data));assert(outputData.getAttr("with space"));

    setup();data=particleBlock();inputData.points.clear();
    assert(DPParticleAttribute().compute(plug,data));
    for(const auto& op:operations)assert(op.find("input aIs")==std::string::npos);
    setup();data=particleBlock();data.inputs["aIsRadius"].number=0;data.inputs["aRadiusType"].number=99;
    data.inputs["aRadiusAttribute"].text="";
    assert(DPParticleAttribute().compute(plug,data));assert(!outputData.getAttr("pscale"));
    setup();data=particleBlock();rejectWrite=true;
    assert(!DPParticleAttribute().compute(plug,data) && publications==0);
    resetCompute();assert(DPParticleAttribute().compute(plug,data));
    setup();data=particleBlock();inputData.valid=false;
    assert(!DPParticleAttribute().compute(plug,data) && copies==0 && publications==0);
    setup();data=particleBlock();nullInput=true;
    assert(!DPParticleAttribute().compute(plug,data) && copies==0 && publications==0);
    setup();data=particleBlock();
    assert(DPParticleAttribute().compute(MPlug(MObject{"unrelated"}),data)==MS::kUnknownParameter);
    assert(callCount==0);
}

void testFill() {
    setup();auto data=fillBlock();const MPlug plug(DPFillSpace::outDP);
    assert(DPFillSpace().compute(plug,data));
    assert(publications==1 && outputData.points.size()==3 && outputData.points[2].x==1);
    setup();data=fillBlock();data.inputs["isIDStepFill"].number=0;data.inputs["isDistanceFill"].number=1;
    assert(DPFillSpace().compute(plug,data));assert(outputData.points.size()==3);
    setup();data=fillBlock();data.inputs["samplingType"].number=1;data.inputs["samplingSpace"].number=0.5;
    assert(DPFillSpace().compute(plug,data));assert(outputData.points.size()>2);
    setup();data=fillBlock();data.inputs["samplingSpace"].number=NAN;
    assert(!DPFillSpace().compute(plug,data)&&publications==0&&arrayCopies==0);
    setup();data=fillBlock();data.inputs["samplingCount"].number=-1;
    assert(!DPFillSpace().compute(plug,data)&&publications==0&&arrayCopies==0);
    setup();data=fillBlock();inputData.reportedCount=1000000U;
    data.inputs["isIDStepFill"].number=0;
    assert(!DPFillSpace().compute(plug,data)&&publications==0&&arrayCopies==0);
    setup();data=fillBlock();inputData.reportedCount=3U;
    assert(DPFillSpace().compute(plug,data)==MS::kFailure&&publications==0&&arrayCopies==1);
    setup();data=fillBlock();inputData.points.clear();assert(DPFillSpace().compute(plug,data));
    assert(outputData.points.empty());
}

void testMerge() {
    setup();MDataBlock data;const MPlug plug(DPMerge::outDP);
    DPData other;other.points={MPoint(4,0,0)};
    arrayInputs={&inputData,nullptr,&other};
    assert(DPMerge().compute(plug,data));
    assert(outputData.points.size()==3 && outputData.points[2].x==4 && copies==1 && merges==0);
    setup();assert(DPMerge().compute(plug,data));assert(outputData.points.empty());
    setup();arrayCount=dommy::generation::kMaxConnectedGeometryCount+1U;
    assert(!DPMerge().compute(plug,data) && copies==0 && publications==0);
    setup();DPData one;one.points={MPoint()};arrayInputs.assign(20000U,&one);
    assert(DPMerge().compute(plug,data) && copies==1 && merges==0 && publications==1 && outputData.points.size()==20000U);
    setup();arrayInputs={&inputData};nullInput=true;
    assert(!DPMerge().compute(plug,data) && publications==0);
    setup();DPData zero,many;many.reportedCount=100000U;
    field<std::string>(zero,"a",{},std::string(1001,'x'));arrayInputs={&zero,&many};
    assert(!DPMerge().compute(plug,data) && copies==0 && publications==0);
    assert(std::find(operations.begin(),operations.end(),"output.create")==operations.end());
}

void testMergeBudget() {
    using dommy::point::buildPointMergeBudget;
    DPData one;one.points={MPoint()};std::size_t work=99;
    std::vector<const DPData*> inputs(100U,&one);
    assert(buildPointMergeBudget(inputs,work) && work==500U);
    one.groups["g"]=std::make_shared<Group>();one.groups["g"]->ids={true};
    assert(buildPointMergeBudget(inputs,work) && work==900U);
    one.groups.clear();
    field<Imath::V3f>(one,"v",{Imath::V3f(1,2,3)});
    assert(buildPointMergeBudget(inputs,work) && work==1200U);
    one.attributes.clear();
    inputs.assign(20000U,&one);assert(buildPointMergeBudget(inputs,work) && work==100000U);
    inputs.assign(dommy::generation::kMaxConnectedGeometryCount+1U,&one);
    assert(!buildPointMergeBudget(inputs,work));
    DPData huge;huge.reportedCount=dommy::generation::kMaxGeneratedPointCount+1U;
    assert(!buildPointMergeBudget(std::vector<const DPData*>{&huge},work));
    DPData first;field<float>(first,"a",{},3.0f);
    DPData second;second.points={MPoint()};field<double>(second,"a",{2.0});
    assert(!buildPointMergeBudget(std::vector<const DPData*>{&first,&second},work));
    // Later empty payload metadata is ignored by the historical pairwise merge.
    assert(buildPointMergeBudget(std::vector<const DPData*>{&second,&first},work));
    field<std::string>(first,"a",{},std::string(1001,'x'));
    DPData many;many.reportedCount=100000U;
    assert(!buildPointMergeBudget(std::vector<const DPData*>{&first,&many},work));
    first.attributes.clear();field<DIntArray>(first,"a",{},DIntArray(10001,1));many.reportedCount=10000U;
    assert(!buildPointMergeBudget(std::vector<const DPData*>{&first,&many},work));
    // Ignored empty payloads still cost validation of their dynamic defaults.
    inputs.assign(10000U,&first);inputs[0]=&one;
    assert(!buildPointMergeBudget(inputs,work));
    first.attributes.clear();first.groups["g"]=std::make_shared<Group>();
    assert(buildPointMergeBudget(std::vector<const DPData*>{&first,&one},work));
    one.groups["g"]=std::make_shared<Group>();
    assert(!buildPointMergeBudget(std::vector<const DPData*>{&one},work));
}

template <typename Node,typename Prepare>
int injectFailures(const MObject& output,Prepare prepare) {
    setup();MDataBlock data=prepare();const MPlug plug(output);
    assert(Node().compute(plug,data));const int count=callCount;
    const auto baseline=operations;int tested=0;
    for(int i=0;i<count;++i) {
        for(const auto code:{MS::kFailure,MS::kInvalidParameter}) {
            setup();data=prepare();failAt=i;injectedStatus=code;
            const auto result=Node().compute(plug,data);
            assert(!result);assert(!invalidRead);
            if(std::is_same<Node,DPFillSpace>::value &&
               (baseline[i]=="input inDP"||baseline[i]=="point.bind"))assert(result==code);
            assert(publications==(baseline[i]=="output.clean"?1:0));++tested;
        }
        setup();data=prepare();attributeThrowAt=i;
        assert(!Node().compute(plug,data));assert(!invalidRead);++tested;
        setup();data=prepare();assert(Node().compute(plug,data));
    }
    return tested;
}
void fillBudgets() {
    // In the reduced policy, nine fixed ID samples fail before point export.
    setup();auto data=fillBlock();data.inputs["samplingCount"].number=9;
    assert(!DPFillSpace().compute(MPlug(DPFillSpace::outDP),data)&&arrayCopies==0&&publications==0);
    setup();data=fillBlock();data.inputs["samplingCount"].number=8;
    assert(DPFillSpace().compute(MPlug(DPFillSpace::outDP),data)&&outputData.points.size()==10);
    // Distance-filtered samples are admitted only for actual qualifying pairs.
    setup();data=fillBlock();data.inputs["isIDStepFill"].number=0;data.inputs["isDistanceFill"].number=1;
    data.inputs["samplingCount"].number=9;
    assert(!DPFillSpace().compute(MPlug(DPFillSpace::outDP),data)&&arrayCopies==1&&publications==0);
    assert(std::count(operations.begin(),operations.end(),"point.append")==2);
    setup();data=fillBlock();data.inputs["isIDStepFill"].number=0;data.inputs["isDistanceFill"].number=1;
    data.inputs["samplingCount"].number=100;data.inputs["distance"].number=1;
    assert(DPFillSpace().compute(MPlug(DPFillSpace::outDP),data)&&outputData.points.size()==2);
    std::cout<<"FillSpace known and incremental byte admission, excluded pairs and allocation-free rejection passed\n";
}
int main(int argc,char**) {
    if(argc>1){fillBudgets();return 0;}
    testParticle();testFill();testMerge();testMergeBudget();
    testScalarSource<int32_t>(-7,9);testScalarSource<uint32_t>(0,4000000000U);
    testScalarSource<int64_t>(-9007199254740991LL,9007199254740991LL);
    testScalarSource<float>(0.25f,1.5f);testScalarSource<double>(-2.5,7.25);
    testVectorSource<Imath::V3i>(Imath::V3i(1,2,3));
    testVectorSource<Imath::V3f>(Imath::V3f(1,2,3));
    testVectorSource<Imath::V3d>(Imath::V3d(1,2,3));
    int failures=injectFailures<DPParticleAttribute>(DPParticleAttribute::outDP,particleBlock);
    failures+=injectFailures<DPParticleAttribute>(DPParticleAttribute::outDP,particleCopyBlock);
    failures+=injectFailures<DPFillSpace>(DPFillSpace::outDP,fillBlock);
    failures+=injectFailures<DPMerge>(DPMerge::outDP,[]{arrayInputs={&inputData,&inputData};return MDataBlock();});
    std::cout<<"Point utilities: aliases, failed sources, fill, merge budgets and "
             <<failures<<" compute status/exception injections passed\n";
}

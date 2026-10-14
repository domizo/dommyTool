// Full production callback, typed fields, numeric kernels and publication.
// Native arrays/DG and custom payload ownership are injected, not Maya runs.
#define DOMMY_PROCEDURAL_API_TESTS
#include "copy_utility_compute_doubles.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DPointPayloadBudget.h"
#undef MMatrix
const int DPData::id;
using PointBoundary = DPData;
struct BlendPoint : PointBoundary {
    template<class T> const std::vector<T>& attrListValue(const std::string& name)const {
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name))->value();
    }
    template<class T> T attrDefaultValue(const std::string& name)const {
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name))->getDefaultValue();
    }
};
#define DPData BlendPoint
using DDoubleArray = std::vector<double>;
static int shortAt = -1, evaluations = 0;
struct MDoubleArray {
    std::vector<double> values;
    MDoubleArray() = default;
    MDoubleArray(unsigned n,double v) {
        const int at = callCount;
        voidApi("weights.construct");
        values.assign(at == shortAt && n ? n - 1U : n, v);
    }
    unsigned length()const {return static_cast<unsigned>(values.size());}
    MStatus setLength(unsigned n) {
        const int at = callCount;
        const auto status = operation("weights.length");
        if(status) values.resize(at == shortAt && n ? n - 1U : n);
        return status;
    }
    MStatus copy(const MDoubleArray& a) {
        const int at = callCount;
        const auto status = operation("weights.copy");
        if(status) {values = a.values; if(at == shortAt && !values.empty()) values.pop_back();}
        return status;
    }
    double& operator[](unsigned i) {return values.at(i);}
    const double& operator[](unsigned i)const {return values.at(i);}
};
static DPData source, staged, committed;
struct MFnPluginData {
    MObject create(int,MStatus* status) {
        *status = operation("factory.create");
        staged = DPData();
        return *status && !nullCreated ? MObject{"point"} : MObject::kNullObj;
    }
};
namespace dommy {namespace maya {
template<class T> MStatus getPluginData(MFnPluginData&,T*& out) {
    const auto status = operation("factory.bind");out = status ? &staged : nullptr;return status;
}
template<class T> bool validatePluginData(const T& v) {return !rejectFinal && v.hasValidPointTopology();}
}}
MStatus MDataHandle::set(const MObject&) {
    const auto status = operation("output.publish");
    if(status) {committed = staged;++publications;}return status;
}
#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
#include "attribute_blend_compute.inc"

static MDataBlock setup(std::size_t count=2U) {
    resetCompute();shortAt = -1;source = staged = committed = DPData();
    source.points.assign(count, DPoint(1,2,3));
    source.attributes["a"] = std::make_shared<DPDoubleAttr>(std::vector<double>(count,0),3.0);
    source.attributes["b"] = std::make_shared<DPDoubleAttr>(std::vector<double>(count,10),7.0);
    auto group = std::make_shared<Group>();group->ids.assign(count,true);source.groups["keep"] = group;
    committed.points = {{-99,0,0}};
    MDataBlock block;block.inputs["inDP"].payload = &source;block.inputs["inDP"].type = DPData::id;
    block.inputs["aSourceAttribute"].text = "a";block.inputs["aTargetAttribute"].text = "b";
    block.inputs["aWeightAttribute"].text = "weight";block.inputs["aWeight"].number = .25;
    block.inputs["aOutputAttribute"].text = "result";return block;
}
static MStatus compute(MDataBlock& block) {
    ++evaluations;return DPAttributeBlend().compute(MPlug(DPAttributeBlend::outDP),block);
}
static bool called(const char* label) {return std::find(operations.begin(),operations.end(),label)!=operations.end();}
static std::string state(const DPData& data) {
    std::ostringstream stream;
    for(const auto& p:data.points) stream<<p.x<<','<<p.y<<','<<p.z<<';';
    for(const auto& field:data.attributes) stream<<field.first<<':'<<field.second->str();
    for(const auto& group:data.groups) {stream<<group.first;for(bool id:group.second->ids)stream<<id;}
    return stream.str();
}
template<class T> static void numeric(const T& a,const T& b,const T& expected) {
    auto block = setup();
    source.attributes["a"] = std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{a,a},a);
    source.attributes["b"] = std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{b,b},b);
    const auto input = state(source);assert(compute(block));
    assert(std::count(operations.begin(),operations.end(),"handle.clean")==1);
    assert(committed.attrListValue<T>("result")==std::vector<T>({expected,expected}));
    assert(committed.attrDefaultValue<T>("result")==a && state(source)==input);
    assert(committed.getAttr("a")!=source.getAttr("a") && committed.groups["keep"]!=source.groups["keep"]);
}
template<class T> static void weightType() {
    auto block = setup();source.attributes["weight"] = std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{T(0),T(1)});
    assert(compute(block));assert(committed.attrListValue<double>("result")==std::vector<double>({0,10}));
}
template<class T,int Components> static void vectorType() {
    auto block=setup();T a,b;
    for(int i=0;i<Components;++i) {a[i]=0;b[i]=8;}
    source.attributes["a"]=std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{a,a},a);
    source.attributes["b"]=std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{b,b},b);
    assert(compute(block));
    for(const auto& value:committed.attrListValue<T>("result"))for(int i=0;i<Components;++i)assert(value[i]==2);
}
static void normal() {
    numeric<int32_t>(0,8,2);numeric<uint32_t>(0,8,2);numeric<int64_t>(0,8,2);
    numeric<int64_t>(9007199254740993LL,9007199254740993LL,9007199254740993LL);
    numeric<float>(0,10,2.5f);numeric<double>(0,10,2.5);
    vectorType<Imath::V2i,2>();vectorType<Imath::V2f,2>();vectorType<Imath::V2d,2>();
    vectorType<Imath::V3i,3>();vectorType<Imath::V3f,3>();vectorType<Imath::V3d,3>();
    auto matrixBlock=setup();MMatrix a,b;
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){a[row][column]=0;b[row][column]=8;}
    source.attributes["a"]=std::make_shared<DPMat4Attr>(std::vector<MMatrix>{a,a},a);
    source.attributes["b"]=std::make_shared<DPMat4Attr>(std::vector<MMatrix>{b,b},b);
    assert(compute(matrixBlock));
    for(const auto& value:committed.attrListValue<MMatrix>("result"))
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)assert(value[row][column]==2);
    for(auto name: {"", "missing", "weight"}) {auto block=setup();block.inputs["aWeightAttribute"].text=name;
        assert(compute(block)&&committed.attrListValue<double>("result")[0]==2.5);}
    weightType<int32_t>();weightType<uint32_t>();weightType<int64_t>();weightType<float>();weightType<double>();
    for(auto output: {"", "a", "b", "result"}) for(bool remove: {false,true}) {
        auto block=setup();block.inputs["aOutputAttribute"].text=output;block.inputs["aIsDeleteOriginal"].number=remove;
        const std::string name = *output ? output : "a";const auto input=state(source);
        assert(compute(block));assert(committed.attrListValue<double>(name)[0]==2.5&&state(source)==input);
        assert(committed.isExistAttr("a")==(!remove || name=="a"));
        assert(committed.isExistAttr("b")==(!remove || name=="b"));
    }
    auto block=setup();block.inputs["aWeight"].number=1.5;assert(compute(block));
    assert(committed.attrListValue<double>("result")[0]==15);
    block=setup(0);assert(compute(block)&&committed.points.empty()&&committed.attrDefaultValue<double>("result")==3);
}
static void invalid() {
    for(int variant=0;variant<12;++variant) {
        auto block=setup();
        switch(variant) {
        case 0:source.attributes["weight"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"x","y"});break;
        case 1:source.attributes["weight"]=std::make_shared<DPVec3fAttr>(DPointList{{1,2,3},{1,2,3}});break;
        case 2:source.attributes["weight"]=std::make_shared<TypedDPAttributeData<bool>>(std::vector<bool>{true,false});break;
        case 3:source.attributes["weight"]=std::make_shared<DPDoubleAttr>(std::vector<double>{NAN,1});break;
        case 4:source.attributes["weight"]=std::make_shared<DPDoubleAttr>(std::vector<double>{1});break;
        case 5:block.inputs["aSourceAttribute"].text="missing";break;
        case 6:block.inputs["aTargetAttribute"].text="a";break;
        case 7:block.inputs["aWeight"].number=INFINITY;break;
        case 8:source.attributes["b"]=std::make_shared<DPFloatAttr>(std::vector<float>{1,2});break;
        case 9:source.topology=false;break;
        case 10:block.inputs["inDP"].payload=nullptr;break;
        case 11:source.attributes["a"]=source.attributes["b"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"x","y"});break;
        }
        const auto previous=state(committed),input=state(source);
        assert(!compute(block)&&publications==0&&geometryCopies==0&&!called("weights.construct"));
        assert(state(committed)==previous&&state(source)==input);
    }
    auto block=setup();nullCreated=true;assert(compute(block)==MS::kFailure&&publications==0);
    block=setup();rejectFinal=true;assert(!compute(block)&&publications==0);
    block=setup();assert(DPAttributeBlend().compute(MPlug(MObject{"other"}),block)==MS::kUnknownParameter&&callCount==0);
}
static void failures(bool weights) {
    auto prepare=[weights](){auto block=setup();if(weights)source.attributes["weight"]=std::make_shared<DPDoubleAttr>(std::vector<double>{0,1});return block;};
    auto block=prepare();assert(compute(block));const auto trace=operations;
    for(int at=0;at<static_cast<int>(trace.size());++at) {
        // Our committed owning snapshot is a model, not a native API site.
        if(at>0&&trace[at]=="point.copy"&&trace[at-1]=="output.publish")continue;
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            block=prepare();const auto previous=state(committed),input=state(source);failAt=at;injectedStatus=code;
            const auto result=compute(block);assert(!result);
            const bool throws=trace[at]=="weights.construct"||trace[at]=="point.copy"||
                trace[at]=="point.attribute.add"||trace[at]=="point.attribute.remove"||
                trace[at]=="weights.length"||trace[at]=="weights.copy"||trace[at]=="handle.clean";
            if(!throws)assert(result==code);
            assert(state(source)==input);
            if(trace[at]!="output.clean"&&trace[at]!="handle.clean")assert(publications==0&&state(committed)==previous);
            resetCompute();assert(compute(block)&&publications==1);
        }
        if(trace[at]=="weights.construct"||trace[at]=="weights.length"||trace[at]=="weights.copy") {
            block=prepare();shortAt=at;assert(compute(block)==MS::kFailure&&publications==0&&geometryCopies==0);
        }
    }
}
static void budgets() {
    // Reduced 512-byte payload allowance: combined costs, not huge allocations.
    for(int mode=0;mode<3;++mode) {
        bool rejected=false;
        for(std::size_t n=1;n<20&&!rejected;++n) {
            auto block=setup(n);
            if(mode==0)source.attributes["padding"]=std::make_shared<DPStringAttr>(std::vector<std::string>(n,std::string(30,'x')));
            if(mode==1) {MMatrix matrix;source.attributes["a"]=source.attributes["b"]=std::make_shared<DPMat4Attr>(std::vector<MMatrix>(n,matrix));}
            if(mode==2)for(int i=0;i<5;++i)source.attributes["pad"+std::to_string(i)]=std::make_shared<DPFloatAttr>(std::vector<float>(n,1));
            const auto previous=state(committed);
            if(!compute(block)) {rejected=true;assert(!called("weights.construct")&&geometryCopies==0&&publications==0&&state(committed)==previous);}
        }
        assert(rejected);
    }
    // Isolate generated-field admission: input/weight budget fits, result does not.
    bool found=false;
    for(std::size_t padding=0;padding<200;++padding) {
        auto block=setup();source.attributes["pad"]=std::make_shared<DPStringAttr>(std::vector<std::string>(2,std::string(padding,'x')));
        dommy::point::PointPayloadBudget budget;
        try {dommy::point::preflightPointPayload(source,sizeof(DPoint)+2U*sizeof(double),budget);}
        catch(const std::length_error&) {continue;}
        try {dommy::point::preflightGeneratedPointAttribute(2,"result",sizeof(double),budget);}
        catch(const std::length_error&) {found=true;assert(!compute(block)&&!called("weights.construct")&&geometryCopies==0);break;}
    }
    assert(found);
}
int main(int argc,char**) {
    if(argc>1)budgets();else {normal();invalid();failures(false);failures(true);}
    std::cout<<"Complete Blend callback: "<<evaluations<<" evaluations; typed weights, aliases/defaults, exact integers, status/cardinality/retry and aggregate admission passed\n";
}

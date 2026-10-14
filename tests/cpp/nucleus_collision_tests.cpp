#include <maya/MStatus.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <vector>

// Fixed-size API doubles do not allocate while native registrations change.
// Global allocation injection exercises the actual std::vector bookkeeping.
static int allocationCount=0,failAllocationAt=-1;
static bool denyAllocation=false,denyAfterAdd=false;
void* operator new(std::size_t size) {
    const int call=allocationCount++;
    if(denyAllocation||call==failAllocationAt)throw std::bad_alloc();
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
enum Op {Array,Count,Read,Object,Pointer,Vertices,Next,Clear,Remove,Add,Make};
static std::array<Op,256> trace;
static int callCount=0,failAt=-1,throwAt=-1,secondFailAt=-1;
static MStatus injectedStatus=MS::kFailure;
static MStatus operation(Op op) {
    assert(callCount<int(trace.size()));const int call=callCount++;trace[call]=op;
    if(call==throwAt)throw std::runtime_error("native API exception");
    if(call==secondFailAt)return MS::kInvalidParameter;
    return call==failAt?injectedStatus:MStatus(MS::kSuccess);
}
static void resetCalls() {
    callCount=0;failAt=throwAt=secondFailAt=-1;
    allocationCount=0;failAllocationAt=-1;denyAllocation=denyAfterAdd=false;
}
struct MnRigid {
    unsigned count;
    explicit MnRigid(unsigned n=1):count(n){}
    MStatus getNumVertices(unsigned& out){const auto s=operation(Vertices);if(s)out=count;return s;}
};
struct MObject {
    MnRigid* rigid;bool null;
    explicit MObject(MnRigid* r=nullptr,bool n=false):rigid(r),null(n){}
    bool isNull()const{return null;}
};
static std::array<MObject,8> inputs;
static unsigned inputCount=0;
struct MDataHandle {
    MObject object;bool valid;
    MObject data()const{assert(valid);return object;}
};
struct MArrayDataHandle {
    bool valid;unsigned cursor;
    explicit MArrayDataHandle(bool v):valid(v),cursor(0){}
    unsigned elementCount(MStatus* status=nullptr) {
        assert(valid);const auto s=operation(Count);if(status)*status=s;
        return s?inputCount:0;
    }
    MDataHandle inputValue(MStatus* s) {
        assert(valid&&cursor<inputCount);*s=operation(Read);return {inputs[cursor],bool(*s)};
    }
    MStatus next(){assert(valid&&cursor+1<inputCount);const auto s=operation(Next);if(s)++cursor;return s;}
};
struct MDataBlock {
    MArrayDataHandle inputArrayValue(const MObject&,MStatus* s){*s=operation(Array);return MArrayDataHandle(bool(*s));}
};
struct MFnNObjectData {
    MObject object;bool valid;
    MFnNObjectData(MObject value,MStatus* s):object(value),valid(false){*s=operation(Object);valid=bool(*s);}
    MStatus getObjectPtr(MnRigid*& ptr){assert(valid);const auto s=operation(Pointer);if(s)ptr=object.rigid;return s;}
};
struct MnSolver {
    std::array<MnRigid*,8> objects{};unsigned count=0;
    MStatus removeAllCollisions(){return operation(Clear);}
    MStatus removeNObject(MnRigid* value) {
        const auto s=operation(Remove);if(!s)return s;
        const auto end=objects.begin()+count;const auto it=std::find(objects.begin(),end,value);
        assert(it!=end);std::move(it+1,end,it);--count;return s;
    }
    MStatus addNObject(MnRigid* value) {
        const auto s=operation(Add);if(!s)return s;
        assert(count<objects.size());objects[count++]=value;
        if(denyAfterAdd)denyAllocation=true;
        return s;
    }
    MStatus makeAllCollide(){return operation(Make);}
};
#include "nucleus_collision.inc"

static MnRigid previousA,previousB,first,second,third,empty(0);
struct Scenario {
    MnSolver solver;
    std::vector<MnRigid*> registered{&previousA,&previousB};
    Scenario(){solver.objects[0]=&previousA;solver.objects[1]=&previousB;solver.count=2;}
};
static void configureInputs() {
    inputs[0]=MObject(&first);inputs[1]=MObject(&second);inputs[2]=MObject(&third);inputCount=3;
}
static MStatus sync(Scenario& test) {
    MDataBlock data;MObject attr;
    try {return dommy::maya::syncNucleusRigidCollisions(data,attr,test.solver,test.registered);}
    catch (...) {return MS::kFailure;} // owning compute contains exceptions
}
static void consistent(const Scenario& test) {
    assert(test.registered.size()==test.solver.count);
    for(auto* object:test.registered)
        assert(std::count(test.solver.objects.begin(),test.solver.objects.begin()+test.solver.count,object)==1);
}
static void original(const Scenario& test) {
    consistent(test);assert(test.registered.size()==2);
    assert(test.registered[0]==&previousA&&test.registered[1]==&previousB);
}
int main() {
    configureInputs();Scenario baseline;resetCalls();assert(sync(baseline));consistent(baseline);
    assert((baseline.registered==std::vector<MnRigid*>{&first,&second,&third}));
    const int total=callCount;const auto baselineTrace=trace;
    const int firstMutation=std::find(trace.begin(),trace.begin()+total,Clear)-trace.begin();
    int faults=0;
    for(int i=0;i<total;++i)for(int mode=0;mode<3;++mode) {
        Scenario test;resetCalls();
        if(mode==2)throwAt=i;else {failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
        assert(sync(test)==(mode==1?MS::kInvalidParameter:MS::kFailure));
        consistent(test);
        if(i<firstMutation) {assert(callCount==i+1);original(test);}
        resetCalls();assert(sync(test));consistent(test);
        assert((test.registered==std::vector<MnRigid*>{&first,&second,&third}));++faults;
    }
    // Preserve the old set if any allocation fails. The final allocation is
    // registeredColliders.reserve, after reads but before native clear/remove.
    Scenario allocationBaseline;resetCalls();assert(sync(allocationBaseline));
    const int allocations=allocationCount;assert(allocations>=3);
    for(int i=0;i<allocations;++i) {
        Scenario test;resetCalls();failAllocationAt=i;assert(!sync(test));
        failAllocationAt=-1;original(test);
        assert(std::find(trace.begin(),trace.begin()+callCount,Clear)==trace.begin()+callCount);
    }
    Scenario noLateAllocation;resetCalls();denyAfterAdd=true;
    const MStatus noLateStatus=sync(noLateAllocation);denyAllocation=denyAfterAdd=false;
    assert(noLateStatus);consistent(noLateAllocation);
    // A failed cleanup must retain the still-registered entries for retry.
    int thirdAdd=-1;
    for(int i=0;i<total;++i)if(baselineTrace[i]==Add)thirdAdd=i;
    assert(thirdAdd>=0);
    for(int cleanupOffset:{1,2}) {
        Scenario test;resetCalls();injectedStatus=MS::kFailure;
        failAt=thirdAdd;secondFailAt=thirdAdd+cleanupOffset;
        assert(sync(test)==MS::kInvalidParameter);consistent(test);
        resetCalls();assert(sync(test));consistent(test);
    }
    // Collection is transactional even for invalid object payloads.
    inputs[0]=MObject(nullptr);inputCount=1;Scenario nullPointer;resetCalls();
    assert(sync(nullPointer)==MS::kInvalidParameter);original(nullPointer);
    // Skip null data/zero vertices, deduplicate pointers in first-seen order.
    inputs[0]=MObject(nullptr,true);inputs[1]=MObject(&empty);inputs[2]=MObject(&second);
    inputs[3]=MObject(&second);inputs[4]=MObject(&first);inputCount=5;
    Scenario filtered;resetCalls();assert(sync(filtered));consistent(filtered);
    assert((filtered.registered==std::vector<MnRigid*>{&second,&first}));
    inputCount=0;resetCalls();assert(sync(filtered));assert(filtered.registered.empty()&&filtered.solver.count==0);
    std::printf("Nucleus collisions: %d API faults/exceptions, %d allocation failures; retry, cleanup, filtering and tracking goldens passed\n",faults,allocations);
}

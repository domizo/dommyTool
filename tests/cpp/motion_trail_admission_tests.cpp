// Run the actual admission/selection prefix. Maya history, native curve creation
// and nonempty publication are deliberately outside this test boundary.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DGenerationLimits.h"
#include "math/DRandomGenerator.h"
#include <maya/MTime.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_IT(value) do {if (!(value)) return (value);} while (false)
struct MObject {std::string name; bool isNull() const {return name.empty();}};
struct MPlug {
    MObject attribute;
    bool operator!=(const MObject& other) const {return attribute.name != other.name;}
};
struct MPoint {
    double x=0, y=0, z=0, w=1;
    static const MPoint origin;
};
const MPoint MPoint::origin{};
namespace MSpace {enum Space{kObject};}
namespace MFnData {enum Type{kNurbsCurve,kNurbsSurface,kMesh,kOther};}
static int calls=0, failAt=-1, throwAt=-1, bulkReads=0, selectionOps=0;
static int admissions=0, emptyPublications=0;
static std::size_t admittedBytes=0;
static bool wrongSelectionSize=false;
static MS::MStatusCode failure=MS::kFailure;
static std::vector<std::string> operations;
static MStatus api(const std::string& name) {
    operations.push_back(name);
    const int current=calls++;
    if (current==throwAt) throw std::runtime_error("injected host exception");
    return current==failAt ? failure : MS::kSuccess;
}
struct MPointArray:std::vector<MPoint> {
    unsigned length() const {return static_cast<unsigned>(size());}
    MStatus append(const MPoint& point) {
        ++selectionOps;
        const auto status=api("selection.append");
        if (status && !wrongSelectionSize) push_back(point);
        return status;
    }
    MStatus copy(const MPointArray& points) {
        ++selectionOps;
        const auto status=api("selection.copy");
        if (status) {*this=points; if (wrongSelectionSize && !empty()) pop_back();}
        return status;
    }
};
static MPointArray selectedPositions;
struct Input {
    double scalar=0;
    MFnData::Type type=MFnData::kMesh;
    MObject object{"source"};
};
static std::map<std::string,Input> inputs;
struct MDataHandle {
    Input* input=nullptr;
    bool valid=false;
    void check() const {assert(valid && input);}
    MTime asTime() const {check();return MTime(input->scalar);}
    int asInt() const {check();return static_cast<int>(input->scalar);}
    double asDouble() const {check();return input->scalar;}
    MFnData::Type type() const {check();return input->type;}
    MObject asMesh() const {check();return input->object;}
    MObject asNurbsCurve() const {check();return input->object;}
    MObject asNurbsSurface() const {check();return input->object;}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute,MStatus* status) {
        *status=api("input "+attribute.name);
        MDataHandle handle;handle.input=&inputs[attribute.name];handle.valid=bool(*status);
        return handle;
    }
};
namespace dommy {namespace maya {
MStatus setObjectArrayOutput(MDataBlock&,const MObject&,const MPlug&,const std::vector<MObject>& values) {
    assert(values.empty());++emptyPublications;return MS::kSuccess;
}
}}
struct Geometry {
    int points=6, u=2, v=3;
    bool shortPoints=false, nonfinitePoints=false;
};
static Geometry geometry;
static int query(const char* name,int value,MStatus* status) {
    *status=api(name);return value;
}
static MStatus bind(const MObject& object,const char* name) {
    const auto status=api(name);
    return object.isNull() ? MStatus(MS::kFailure) : status;
}
static MStatus readPoints(MPointArray& output,const char* name) {
    ++bulkReads;
    const auto status=api(name);
    if (status) {
        // Excessive scalar reports must never reach this allocation boundary.
        assert(geometry.points>=0 && geometry.points<100);
        output.resize(geometry.points);
        for (unsigned i=0;i<output.length();++i) output[i].x=i+1;
        if (geometry.shortPoints && !output.empty()) output.pop_back();
        if (geometry.nonfinitePoints && !output.empty()) output[0].x=NAN;
    }
    return status;
}
struct MFnMesh {
    MStatus setObject(const MObject& o) {return bind(o,"mesh.bind");}
    int numVertices(MStatus* s) {return query("mesh.points",geometry.points,s);}
    MStatus getPoints(MPointArray& o,MSpace::Space) {return readPoints(o,"mesh.getPoints");}
};
struct MFnNurbsCurve {
    MStatus setObject(const MObject& o) {return bind(o,"curve.bind");}
    int numCVs(MStatus* s) {return query("curve.points",geometry.points,s);}
    MStatus getCVs(MPointArray& o,MSpace::Space) {return readPoints(o,"curve.getCVs");}
};
struct MFnNurbsSurface {
    MStatus setObject(const MObject& o) {return bind(o,"surface.bind");}
    int numCVsInU(MStatus* s) {return query("surface.u",geometry.u,s);}
    int numCVsInV(MStatus* s) {return query("surface.v",geometry.v,s);}
    MStatus getCVs(MPointArray& o,MSpace::Space) {return readPoints(o,"surface.getCVs");}
};
#include "motion_trail_admission.inc"

static void reset(MFnData::Type type=MFnData::kMesh,int mode=DT_TRAIL_POINT) {
    calls=0;failAt=-1;throwAt=-1;bulkReads=0;selectionOps=0;admissions=0;emptyPublications=0;
    wrongSelectionSize=false;operations.clear();inputs.clear();geometry=Geometry();
    inputs["inputObject"].type=type;
    inputs["trailType"].scalar=mode;inputs["timeSampling"].scalar=4;
    inputs["trailLength"].scalar=5;inputs["randomCount"].scalar=3;inputs["seed"].scalar=17;
}
static MStatus evaluate() {
    MDataBlock block;
    return DMotionTrailCurveNode().compute(MPlug{DMotionTrailCurveNode::outputObjects},block);
}
static std::vector<double> legacySelection(unsigned seed,int count) {
    // Historical partial Fisher-Yates order, with the same standard RNG backend.
    dommy::math::RandomGenerator generator(seed);
    std::vector<unsigned> indices{0,1,2,3,4,5};
    std::vector<double> result;
    for (int i=0;i<std::min(count,6);++i) {
        const auto other=i+generator.index(6-i);
        std::swap(indices[i],indices[other]);result.push_back(indices[i]+1);
    }
    return result;
}
static void testAdmission() {
    int injections=0;
    for (auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}) {
        for (int mode:{DT_TRAIL_OBJECT,DT_TRAIL_POINT,DT_TRAIL_POINT_RANDOM}) {
            reset(type,mode);assert(evaluate() && admissions==1 && bulkReads==1);
            const auto baseline=operations;
            const int total=calls;
            const auto bulk=std::find_if(baseline.begin(),baseline.end(),[](const std::string& item) {
                return item.find(".get")!=std::string::npos;
            })-baseline.begin();
            for (int at=0;at<total;++at) for (int kind=0;kind<3;++kind) {
                reset(type,mode);admittedBytes=123;
                if (kind==2) throwAt=at;
                else {failAt=at;failure=kind==0 ? MS::kFailure : MS::kInvalidParameter;}
                assert(!evaluate() && admissions==0 && emptyPublications==0);
                assert(calls==at+1 && admittedBytes==123);
                if (at<bulk) assert(bulkReads==0 && selectionOps==0);
                ++injections;
            }
            reset(type,mode);geometry.shortPoints=true;assert(!evaluate() && selectionOps==0);
            reset(type,mode);geometry.nonfinitePoints=true;assert(!evaluate() && selectionOps==0);
            reset(type,mode);wrongSelectionSize=true;assert(!evaluate() && admissions==0);
            for (int invalid:{-1,0,3,4097}) {
                reset(type,mode);inputs["timeSampling"].scalar=invalid;
                assert(!evaluate() && bulkReads==0 && selectionOps==0);
                assert(std::find(operations.begin(),operations.end(),"input inputObject")==operations.end());
            }
        }
        for (int seed:{0,1,17,123}) for (int count:{0,1,3,6,9}) {
            reset(type,DT_TRAIL_POINT_RANDOM);inputs["seed"].scalar=seed;inputs["randomCount"].scalar=count;
            assert(evaluate());
            if (!count) {assert(emptyPublications==1 && selectionOps==0);continue;}
            const auto expected=legacySelection(seed,count);
            assert(selectedPositions.length()==expected.size());
            for (unsigned i=0;i<selectedPositions.length();++i) assert(selectedPositions[i].x==expected[i]);
        }
        reset(type,DT_TRAIL_OBJECT);assert(evaluate() && selectedPositions.length()==1 && selectedPositions[0].x==0);
        reset(type);assert(evaluate() && selectedPositions.length()==6);
        for (unsigned i=0;i<6;++i) assert(selectedPositions[i].x==i+1);
        reset(type,DT_TRAIL_POINT_RANDOM);inputs["randomCount"].scalar=0;geometry.nonfinitePoints=true;
        assert(!evaluate() && emptyPublications==0);
        reset(type);geometry.points=geometry.u=0;assert(evaluate() && emptyPublications==1 && selectionOps==0);
    }
    std::cout<<"Motion Trail: "<<injections<<" status/exception injections; three geometry/mode paths and historical selection order passed\n";
}
static void testInvalidSizes() {
    for (auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}) {
        for (int bad:{-1,10000001,std::numeric_limits<int>::max()}) {
            reset(type);geometry.points=geometry.u=bad;
            assert(!evaluate() && bulkReads==0 && selectionOps==0);
        }
        reset(type);geometry.points=2000000;geometry.u=2000000;geometry.v=1;
        assert(!evaluate() && bulkReads==0); // Count failure before copies.
        reset(type,DT_TRAIL_OBJECT);geometry.points=5000000;geometry.u=5000000;geometry.v=1;
        assert(!evaluate() && bulkReads==0); // Source alone exceeds storage.
        reset(type);geometry.points=200000;geometry.u=200000;geometry.v=1;inputs["timeSampling"].scalar=10;
        assert(!evaluate() && bulkReads==0); // Counts fit; bytes do not.
    }
    reset(MFnData::kNurbsSurface);geometry.u=geometry.v=100000;assert(!evaluate() && bulkReads==0);
    reset(MFnData::kNurbsSurface);geometry.v=-1;assert(!evaluate() && bulkReads==0);
    reset();inputs["inputObject"].type=MFnData::kOther;assert(!evaluate() && bulkReads==0);
    reset();inputs["inputObject"].object=MObject{};assert(!evaluate() && bulkReads==0);
    reset();MDataBlock block;assert(DMotionTrailCurveNode().compute(MPlug{MObject{"other"}},block)==MS::kUnknownParameter && calls==0);
    for (const char* name:{"time","trailLength","trailLengthRandom"}) {
        reset();inputs[name].scalar=NAN;assert(!evaluate() && bulkReads==0);
    }
    for (const char* name:{"trailLength","trailLengthRandom","randomCount","trailType"}) {
        reset();inputs[name].scalar=-1;assert(!evaluate() && bulkReads==0);
    }
    reset();inputs["trailLengthRandom"].scalar=11;assert(!evaluate() && bulkReads==0);
    reset();inputs["trailLength"].scalar=inputs["trailLengthRandom"].scalar=std::numeric_limits<double>::max();
    assert(!evaluate() && bulkReads==0);
}
static void testBudget() {
    using namespace dommy::generation;
    MotionTrailBudget budget;
    assert(buildMotionTrailBudget(1,4,1,5,0,budget,6,false));
    assert(budget.estimatedBytes==3904);
    assert(buildMotionTrailBudget(1,4,1,5,0,budget,6,true));
    assert(budget.estimatedBytes==3904+6*sizeof(unsigned));
    assert(!buildMotionTrailBudget(1000000,10,1,5,0,budget,1000000,false));
    assert(buildMotionTrailBudget(1,4,1,5,0,budget,4000000,false));
    assert(!buildMotionTrailBudget(1,4,1,5,0,budget,4000000,true));
    assert(buildMotionTrailBudget(1,4,1,5,0,budget,4194249,false));
    assert(budget.estimatedBytes==kMaxMotionTrailBytes);
    const auto before=budget;
    assert(!buildMotionTrailBudget(1,4,1,5,0,budget,4194250,false));
    assert(budget.estimatedBytes==before.estimatedBytes && budget.curveCount==before.curveCount);
    assert(!buildMotionTrailBudget(1,4,1,5,0,budget,std::numeric_limits<std::size_t>::max(),true));
    assert(!buildMotionTrailBudget(-1,4,1,5,0,budget));
    assert(buildMotionTrailBudget(0,4,1,5,0,budget,6,true) && budget.estimatedBytes==384);
    assert(buildMotionTrailBudget(0,4,1,5,0,budget) && budget.estimatedBytes==0);
    assert(!buildMotionTrailBudget(0,0,1,5,0,budget));
    std::cout<<"Motion Trail: invalid controls/counts, mismatches, 256MiB exact boundary, empty requests and budget preservation passed\n";
}
int main() {testAdmission();testInvalidSizes();testBudget();}

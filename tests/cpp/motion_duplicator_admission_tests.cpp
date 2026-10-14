// Actual compute admission prefix; native copy/animation/output APIs are not
// emulated. Large reports are scalar counts and never allocate huge arrays.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DGenerationLimits.h"
#include <maya/MTime.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_IT(value) do {if (!(value)) return (value);} while (false)
struct MObject {
    std::string name;
    bool isNull() const {return name.empty();}
};
struct MPlug {
    MObject attribute;
    bool operator!=(const MObject& other) const {return attribute.name != other.name;}
};
struct MPoint {double x=0, y=0, z=0, w=1;};
struct MMatrix {
    double values[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const double* operator[](unsigned i) const {return values[i];}
};
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned length() const {return static_cast<unsigned>(this->size());}
};
using MPointArray=Array<MPoint>;
struct SetName {unsigned id=0, characters=4; unsigned length() const {return characters;}};
using MStringArray=Array<SetName>;
namespace MSpace {enum Space{kObject};}
namespace MFnData {enum Type{kNurbsCurve,kNurbsSurface,kMesh,kOther};}
static int calls=0, failAt=-1, throwAt=-1, bulkReads=0, nameReads=0, admissions=0, emptyPublications=0;
static MS::MStatusCode failure=MS::kFailure;
static std::vector<std::string> operations;
static std::size_t admittedBytes=0;
static MStatus api(const std::string& name) {
    operations.push_back(name);const int current=calls++;
    if(current==throwAt)throw std::runtime_error("injected host exception");
    return current==failAt?failure:MS::kSuccess;
}
struct Input {
    double scalar=0;
    std::array<float,3> floats{{0,0,0}};
    std::array<double,3> doubles{{0,0,0}};
    MMatrix matrix;
    MFnData::Type type=MFnData::kMesh;
    MObject object{"source"};
    bool nullTriple=false;
};
static std::map<std::string,Input> inputs;
struct MDataHandle {
    Input* input=nullptr;bool valid=false;
    void check() const {assert(valid && input);}
    MTime asTime() const {check();return MTime(input->scalar);}
    int asInt() const {check();return static_cast<int>(input->scalar);}
    short asShort() const {check();return static_cast<short>(input->scalar);}
    float asFloat() const {check();return static_cast<float>(input->scalar);}
    const float* asFloat3() const {check();return input->nullTriple?nullptr:input->floats.data();}
    const double* asDouble3() const {check();return input->nullTriple?nullptr:input->doubles.data();}
    MMatrix asMatrix() const {check();return input->matrix;}
    MFnData::Type type() const {check();return input->type;}
    MObject asMesh() const {check();return input->object;}
    MObject asNurbsCurve() const {check();return input->object;}
    MObject asNurbsSurface() const {check();return input->object;}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute,MStatus* status) {
        *status=api("input "+attribute.name);
        MDataHandle handle;handle.input=&inputs[attribute.name];handle.valid=bool(*status);return handle;
    }
};
namespace dommy {namespace maya {
MStatus setObjectArrayOutput(MDataBlock&,const MObject&,const MPlug&,const std::vector<MObject>& values) {
    assert(values.empty());++emptyPublications;return MS::kSuccess;
}
}}
struct Geometry {
    int points=4, faces=1, corners=4, normals=4, u=2, v=2, knots=6, knotsV=6;
    int uvSets=2, colorSets=1;
    std::vector<int> uvs{4,8}, colors{4};
    bool shortNames=false, shortPoints=false, nonfinitePoints=false;
    unsigned nameCharacters=4;
};
static Geometry geometry;
static int query(const char* name,int value,MStatus* status) {*status=api(name);return value;}
static MStatus points(MPointArray& output,const char* name) {
    ++bulkReads;const auto status=api(name);
    if(status) {
        assert(geometry.points>=0 && geometry.points<100); // Excessive reports must be rejected first.
        output.resize(geometry.points);
        if(geometry.shortPoints && !output.empty())output.pop_back();
        if(geometry.nonfinitePoints && !output.empty())output[0].x=NAN;
    }
    return status;
}
struct MFnMesh {
    MStatus setObject(const MObject&) {return api("mesh.bind");}
    int numVertices(MStatus* s) {return query("mesh.points",geometry.points,s);}
    int numPolygons(MStatus* s) {return query("mesh.faces",geometry.faces,s);}
    int numFaceVertices(MStatus* s) {return query("mesh.corners",geometry.corners,s);}
    int numNormals(MStatus* s) {return query("mesh.normals",geometry.normals,s);}
    int numUVSets(MStatus* s) {return query("mesh.uvSets",geometry.uvSets,s);}
    int numColorSets(MStatus* s) {return query("mesh.colorSets",geometry.colorSets,s);}
    MStatus names(MStringArray& output,int count,const char* label) {
        ++nameReads;const auto status=api(label);
        if(status) {assert(count>=0 && count<=1024);output.resize(count);
            for(unsigned i=0;i<output.length();++i){output[i].id=i;output[i].characters=geometry.nameCharacters;}
            if(geometry.shortNames && !output.empty())output.pop_back();}
        return status;
    }
    MStatus getUVSetNames(MStringArray& output) {return names(output,geometry.uvSets,"mesh.uvNames");}
    MStatus getColorSetNames(MStringArray& output) {return names(output,geometry.colorSets,"mesh.colorNames");}
    int numUVs(const SetName& name,MStatus* s) {return query("mesh.uvCount",geometry.uvs.at(name.id),s);}
    int numColors(const SetName& name,MStatus* s) {return query("mesh.colorCount",geometry.colors.at(name.id),s);}
    MStatus getPoints(MPointArray& output,MSpace::Space) {return points(output,"mesh.getPoints");}
};
struct MFnNurbsCurve {
    MStatus setObject(const MObject&) {return api("curve.bind");}
    int numCVs(MStatus* s) {return query("curve.points",geometry.points,s);}
    int numKnots(MStatus* s) {return query("curve.knots",geometry.knots,s);}
    MStatus getCVs(MPointArray& output,MSpace::Space) {return points(output,"curve.getCVs");}
};
struct MFnNurbsSurface {
    MStatus setObject(const MObject&) {return api("surface.bind");}
    int numCVsInU(MStatus* s) {return query("surface.u",geometry.u,s);}
    int numCVsInV(MStatus* s) {return query("surface.v",geometry.v,s);}
    int numKnotsInU(MStatus* s) {return query("surface.ku",geometry.knots,s);}
    int numKnotsInV(MStatus* s) {return query("surface.kv",geometry.knotsV,s);}
    MStatus getCVs(MPointArray& output,MSpace::Space) {return points(output,"surface.getCVs");}
};
#include "motion_duplicator_admission.inc"

static void reset(MFnData::Type type=MFnData::kMesh) {
    calls=0;failAt=-1;throwAt=-1;bulkReads=0;nameReads=0;admissions=0;emptyPublications=0;
    operations.clear();inputs.clear();geometry=Geometry();
    inputs["count"].scalar=2;
    inputs["scale"].floats={{1,1,1}};
    inputs["inputObject"].type=type;
}
static MStatus evaluate() {MDataBlock block;return DTMotionDuplicator().compute(MPlug{DTMotionDuplicator::outputObjects},block);}
static void testAdmission() {
    int injections=0;
    for(auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}) {
        reset(type);assert(evaluate() && admissions==1 && bulkReads==1);
        const auto baseline=operations;const int count=calls;
        for(int at=0;at<count;++at)for(int kind=0;kind<3;++kind) {
            reset(type);admittedBytes=123;
            if(kind==2)throwAt=at;else {failAt=at;failure=kind==0?MS::kFailure:MS::kInvalidParameter;}
            assert(!evaluate() && admissions==0 && emptyPublications==0);
            assert(calls==at+1 && admittedBytes==123);
            if(at<count-1)assert(bulkReads==0);
            ++injections;
        }
        reset(type);geometry.shortPoints=true;assert(!evaluate() && admissions==0);
        reset(type);geometry.nonfinitePoints=true;assert(!evaluate() && admissions==0);
        reset(type);inputs["count"].scalar=0;assert(evaluate() && emptyPublications==1 && bulkReads==0);
        assert(std::find(operations.begin(),operations.end(),"input inputObject")==operations.end());
        reset(type);inputs["count"].scalar=-1;assert(!evaluate() && bulkReads==0);
        reset(type);inputs["count"].scalar=1000001;assert(!evaluate() && bulkReads==0);
        reset(type);inputs["count"].scalar=300000;assert(!evaluate() && bulkReads==0);
    }
    reset();MDataBlock block;assert(DTMotionDuplicator().compute(MPlug{MObject{"other"}},block)==MS::kUnknownParameter && calls==0);
    reset();inputs["inputObject"].type=MFnData::kOther;assert(!evaluate() && bulkReads==0);
    reset();inputs["inputObject"].object=MObject{};assert(!evaluate() && bulkReads==0);
    for(const std::string& name:{"translateEvalTime","rotateEvalTime","scaleEvalTime","offsetEvalTime",
                                "offset","rotateCenter","scaleCenter","translate","rotate","scale"}) {
        reset();inputs[name].nullTriple=true;assert(!evaluate() && bulkReads==0);
    }
    for(const std::string& name:{"time","delay","seed","transformMatrixTimeRandom","translateDelay",
                               "translateTimeRandom","rotateDelay","rotateTimeRandom","scaleDelay","scaleTimeRandom"}) {
        reset();inputs[name].scalar=NAN;assert(!evaluate() && bulkReads==0);
    }
    reset();inputs["transformMatrix"].matrix.values[0][0]=NAN;assert(!evaluate() && bulkReads==0);
    std::cout<<"Motion admission: "<<injections<<" status/exception injections; three geometry types, zero/invalid count, null triples and invalid numerics passed\n";
}
static void testSizes() {
    for(int field=0;field<6;++field)for(int bad:{-1,std::numeric_limits<int>::max()}) {
        reset();int* values[]={&geometry.points,&geometry.faces,&geometry.corners,&geometry.normals,&geometry.uvSets,&geometry.colorSets};
        *values[field]=bad;assert(!evaluate() && bulkReads==0 && nameReads==0);
    }
    for(int field=0;field<3;++field) {
        reset();if(field==0)geometry.uvs[1]=6000000; // A non-current UV set must count.
        if(field==1)geometry.colors[0]=5000000;
        if(field==2)geometry.normals=5000000;
        assert(!evaluate() && bulkReads==0);
    }
    reset();geometry.uvs[1]=-1;assert(!evaluate() && bulkReads==0);
    reset();geometry.colors[0]=-1;assert(!evaluate() && bulkReads==0);
    reset();geometry.shortNames=true;assert(!evaluate() && bulkReads==0);
    reset();geometry.nameCharacters=30000000;assert(!evaluate() && bulkReads==0);
    reset();geometry.uvSets=600;geometry.colorSets=600;assert(!evaluate() && nameReads==0);
    reset();geometry.uvSets=0;geometry.colorSets=0;assert(evaluate() && nameReads==0);
    reset();geometry.points=1000000;geometry.faces=1000000;geometry.corners=4000000;assert(!evaluate() && bulkReads==0);
    for(auto type:{MFnData::kNurbsCurve,MFnData::kNurbsSurface})for(int bad:{-1,std::numeric_limits<int>::max()}) {
        reset(type);geometry.knots=bad;assert(!evaluate() && bulkReads==0);
        reset(type);if(type==MFnData::kNurbsCurve)geometry.points=bad;else geometry.u=bad;
        assert(!evaluate() && bulkReads==0);
    }
    reset(MFnData::kNurbsSurface);geometry.u=100000;geometry.v=100000;assert(!evaluate() && bulkReads==0);
    reset(MFnData::kNurbsSurface);geometry.knotsV=-1;assert(!evaluate() && bulkReads==0);
    using namespace dommy::generation;
    MotionGeometrySize source;source.points=1;MotionDuplicationBudget budget;
    assert(buildMotionDuplicationBudget(2,source,budget));
    assert(budget.pointCount==2 && budget.estimatedBytes==2432);
    const auto before=budget;source.uvs=std::numeric_limits<std::size_t>::max();
    assert(!buildMotionDuplicationBudget(2,source,budget) && budget.estimatedBytes==before.estimatedBytes);
    source=MotionGeometrySize();source.points=1048572; // 2 copies * 128 bytes + 1024.
    assert(buildMotionDuplicationBudget(1,source,budget) && budget.estimatedBytes==kMaxMotionDuplicationBytes);
    ++source.points;assert(!buildMotionDuplicationBudget(1,source,budget));
    std::cout<<"Motion budgets: all mesh sets/normals, knots, checked U*V, mismatched arrays and exact 256MiB boundary passed\n";
}
int main() {testAdmission();testSizes();}

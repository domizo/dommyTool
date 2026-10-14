// Maya allocation/geometry are seams. Serialization and atomic file I/O are real.
#include <maya/MStatus.h>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DLegacyTextData.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DPlugHandleOwner.h"
#include "common/utility/DStatusOperation.h"
#include "numeric_locale_fixture.h"

class MString {
    std::string value_;
public:
    MString(const char* value="") : value_(value) {}
    const char* asChar() const {return value_.c_str();}
    unsigned int length() const {return static_cast<unsigned int>(value_.size());}
    MString operator+(const MString& value) const {return MString((value_+value.value_).c_str());}
};
namespace export_host {
enum Kind {mesh,curve,surface,particle,pointData};
enum Fault {none,returnedFailure,invalidParameter,standardException,unknownException};
struct Node {
    Kind kind;
    int count=2, u=1, v=2, actual=2, reported=2;
    bool nonFinite=false, transform=false;
    explicit Node(Kind type) : kind(type) {}
};
std::vector<Node> nodes;
std::map<std::string,unsigned int> calls;
std::string failOperation;
std::string shortOperation;
unsigned int failOccurrence=1, writeAccesses=0, shortOccurrence=0;
bool allocationFailed=false;
unsigned int writesAfterFailedAllocation=0;
Fault fault=none;
int activeHandles=0;
std::size_t recordBudget=dommy::file::kMaxLegacyTextRecords;
std::size_t byteBudget=dommy::file::kMaxLegacyTextBytes;
void reset() {
    assert(activeHandles==0);
    nodes.clear();calls.clear();failOperation.clear();shortOperation.clear();fault=none;
    failOccurrence=1;writeAccesses=shortOccurrence=0;
    allocationFailed=false;writesAfterFailedAllocation=0;
    recordBudget=dommy::file::kMaxLegacyTextRecords;
    byteBudget=dommy::file::kMaxLegacyTextBytes;
}
MStatus step(const char* operation) {
    const unsigned int occurrence=++calls[operation];
    if (operation!=failOperation || occurrence!=failOccurrence) return MS::kSuccess;
    if (fault==standardException) throw std::bad_alloc();
    if (fault==unknownException) throw 42;
    return fault==invalidParameter ? MS::kInvalidParameter : MS::kFailure;
}
void fail(const std::string& operation,Fault failure,unsigned int occurrence=1) {
    failOperation=operation;fault=failure;failOccurrence=occurrence;
}
unsigned int payloads() {
    return calls["payload.mesh"]+calls["payload.curve"]+calls["payload.surface"]+
           calls["payload.particle"]+calls["payload.pointData"];
}
}
namespace host=export_host;
struct MVector {double x=0,y=0,z=0;};
struct MPoint {
    double x,y,z,w;
    MPoint(double a=0,double b=0,double c=0) : x(a),y(b),z(c),w(1) {}
    explicit MPoint(const MVector& v) : x(v.x),y(v.y),z(v.z),w(1) {}
};
template <typename T> struct Array {
    std::vector<T> values;
    unsigned int length() const {return static_cast<unsigned int>(values.size());}
    const T& operator[](unsigned int i) const {return values.at(i);}
    T& operator[](unsigned int i) {
        ++host::writeAccesses;
        if(host::allocationFailed)++host::writesAfterFailedAllocation;
        return values.at(i);
    }
    MStatus setLength(unsigned int count) {
        const MStatus status=host::step("array.allocate");
        if(!status)host::allocationFailed=true;
        if (status) values.resize(count-(host::shortOperation=="array.allocate" &&
            host::shortOccurrence==host::calls["array.allocate"] && count>0));
        return status;
    }
    MStatus copy(const Array& other) {
        const MStatus status=host::step("array.copy");
        if (!status) {values.assign(1,T());return status;}
        values=other.values;
        if (host::shortOperation=="array.copy" &&
            host::shortOccurrence==host::calls["array.copy"] && !values.empty()) values.pop_back();
        return status;
    }
};
using MPointArray=Array<MPoint>;
using MVectorArray=Array<MVector>;
namespace MFn {enum Type{kTransform,kMesh,kNurbsCurve,kNurbsSurface,kParticle};}
namespace MSpace {enum Space{kObject};}
struct MObject {
    int id=-1;
    bool isNull() const {return id<0;}
};
struct MDagPath {
    int id=-1;
    bool shape=false;
    bool isValid() const {return id>=0;}
    bool hasFn(MFn::Type type) const {
        if (!isValid()) return false;
        const host::Node& node=host::nodes.at(id);
        if (type==MFn::kTransform) return node.transform && !shape;
        if (node.transform && !shape) return false;
        return (type==MFn::kMesh && node.kind==host::mesh) ||
               (type==MFn::kNurbsCurve && node.kind==host::curve) ||
               (type==MFn::kNurbsSurface && node.kind==host::surface) ||
               (type==MFn::kParticle && node.kind==host::particle);
    }
    MStatus extendToShape() {const MStatus status=host::step("dag.extend");if(status)shape=true;return status;}
    MObject node(MStatus* status) const {*status=host::step("dag.node");MObject result;result.id=id;return result;}
};
struct MSelectionList {
    int id=-1;
    MStatus add(const MString& name) {
        const MStatus status=host::step("selection.add");
        if (status && name.length()>1) id=std::stoi(std::string(name.asChar()).substr(1));
        return id>=0 && static_cast<std::size_t>(id)<host::nodes.size() ? status : MStatus(MS::kFailure);
    }
    unsigned int length() const {return id>=0 ? 1U : 0U;}
    MStatus getDependNode(unsigned int,MObject& object) const {
        const MStatus status=host::step("selection.object");if(status)object.id=id;return status;
    }
    MStatus getDagPath(unsigned int,MDagPath& path) const {
        // Non-DAG point data intentionally has no DAG path; this is not an
        // injected API error or a native DAG behavior model.
        if (host::nodes.at(id).kind==host::pointData) return MS::kFailure;
        const MStatus status=host::step("selection.path");if(status)path.id=id;return status;
    }
};
struct MGlobal {static void displayError(const MString&) {}};
#include "common/utility/DCommandGuard.h"
struct MDataHandle {MObject value;MObject data() const {return value;}};
struct MPlug {
    int id=-1;
    bool isNull() const {return id<0;}
    MStatus getValue(MDataHandle& handle) const {
        const MStatus status=host::step("handle.read");
        if(status){handle.value.id=id;++host::activeHandles;}return status;
    }
    void destructHandle(MDataHandle&) const {--host::activeHandles;}
};
namespace dommy {namespace maya {using ScopedPlugDataHandle=PlugHandleOwner<MPlug,MDataHandle>;}}
struct Function {
    int id;
    Function(int value,MStatus* status) : id(value) {*status=host::step("bind");}
    MString name(MStatus* status) const {*status=host::step("name");return MString(("n"+std::to_string(id)).c_str());}
    MStatus payload(const char* operation,MPointArray& output) const {
        const MStatus status=host::step(operation);
        if (!status) {output.values.assign(1,MPoint());return status;}
        const auto& node=host::nodes.at(id);
        assert(node.actual>=0 && node.actual<=2048);
        output.values.clear();
        for(int i=0;i<node.actual;++i) output.values.emplace_back(i+.25,-i-.5,1);
        if(node.nonFinite && !output.values.empty()) output.values.back().x=std::numeric_limits<double>::infinity();
        return status;
    }
};
struct MFnMesh : Function {
    MFnMesh(const MDagPath& path,MStatus* status) : Function(path.id,status) {}
    int numVertices(MStatus* status) const {*status=host::step("count.mesh");return host::nodes.at(id).count;}
    MStatus getPoints(MPointArray& out,MSpace::Space) const {return payload("payload.mesh",out);}
};
struct MFnNurbsCurve : Function {
    MFnNurbsCurve(const MDagPath& path,MStatus* status) : Function(path.id,status) {}
    int numCVs(MStatus* status) const {*status=host::step("count.curve");return host::nodes.at(id).count;}
    MStatus getCVs(MPointArray& out,MSpace::Space) const {return payload("payload.curve",out);}
};
struct MFnNurbsSurface : Function {
    MFnNurbsSurface(const MDagPath& path,MStatus* status) : Function(path.id,status) {}
    int numCVsInU(MStatus* status) const {*status=host::step("count.surface.u");return host::nodes.at(id).u;}
    int numCVsInV(MStatus* status) const {*status=host::step("count.surface.v");return host::nodes.at(id).v;}
    MStatus getCVs(MPointArray& out,MSpace::Space) const {return payload("payload.surface",out);}
};
struct MFnParticleSystem : Function {
    MFnParticleSystem(const MDagPath& path,MStatus* status) : Function(path.id,status) {}
    unsigned int count() const {return static_cast<unsigned int>(host::nodes.at(id).count);}
    unsigned int getPerParticleAttribute(const char*,MVectorArray& output,MStatus* status) const {
        *status=host::step("payload.particle");
        if(*status){MPointArray points;payloadValues(points);output.values.resize(points.length());
            for(unsigned int i=0;i<points.length();++i){output.values[i].x=points[i].x;output.values[i].y=points[i].y;output.values[i].z=points[i].z;}}
        return static_cast<unsigned int>(host::nodes.at(id).reported);
    }
    void payloadValues(MPointArray& points) const {
        const auto& node=host::nodes.at(id);
        assert(node.actual>=0 && node.actual<=2048);
        for(int i=0;i<node.actual;++i)points.values.emplace_back(i+.25,-i-.5,1);
        if(node.nonFinite && !points.values.empty())points.values.back().x=std::numeric_limits<double>::infinity();
    }
};
struct MFnDependencyNode : Function {
    MFnDependencyNode(const MObject& object,MStatus* status) : Function(object.id,status) {}
    MPlug findPlug(const char*,bool,MStatus* status) const {
        *status=host::step("plug.find");MPlug plug;
        if(*status && host::nodes.at(id).kind==host::pointData)plug.id=id;
        return plug;
    }
};
struct MFnPointArrayData : Function {
    MFnPointArrayData(const MObject& object,MStatus* status) : Function(object.id,status) {}
    unsigned int length(MStatus* status) const {*status=host::step("count.pointData");return static_cast<unsigned int>(host::nodes.at(id).count);}
    MPointArray array(MStatus* status) const {MPointArray output;*status=payload("payload.pointData",output);return output;}
};
struct MArgList {};
class DPointDataExportCmd {
public:
    MString filePath;
    std::vector<MString> savedNodeNames;
    MStatus parseArgs(const MArgList&) {return host::step("parse");}
    MStatus doIt(const MArgList&);
};
#include "point_export_methods.inc"

MStatus collect(std::size_t maximum=dommy::generation::kMaxGeneratedPointCount) {
    try {MPointArray points;MString type,name;return collectPoints("n0",maximum,points,type,name);}
    catch (...) {return MS::kFailure;}
}
std::string read(const std::string& path) {
    std::ifstream input(path);return std::string(std::istreambuf_iterator<char>(input),{});
}
void seed(const std::string& path) {std::ofstream output(path);output<<"sentinel";assert(output.good());}
DPointDataExportCmd command(const std::string& path) {
    DPointDataExportCmd result;result.filePath=MString(path.c_str());
    for(std::size_t i=0;i<host::nodes.size();++i)result.savedNodeNames.emplace_back(("n"+std::to_string(i)).c_str());
    return result;
}
void admissionTests() {
    for(host::Kind kind : {host::mesh,host::curve,host::surface,host::particle,host::pointData}) {
        for(int count : {0,-1,10000001}) {
            host::reset();host::nodes.emplace_back(kind);host::nodes[0].count=count;host::nodes[0].v=count;
            assert(collect()==MS::kInvalidParameter && host::payloads()==0 && host::activeHandles==0);
        }
        host::reset();host::nodes.emplace_back(kind);
        assert(collect(1)==MS::kInvalidParameter && host::payloads()==0);
        host::reset();host::nodes.emplace_back(kind);assert(collect(2)==MS::kSuccess);
        host::reset();host::nodes.emplace_back(kind);assert(collect(0)==MS::kInvalidParameter && host::calls.empty());
        for(int actual : {1,3}) {
            host::reset();host::nodes.emplace_back(kind);host::nodes[0].actual=actual;
            assert(collect()==MS::kFailure && host::activeHandles==0);
        }
        host::reset();host::nodes.emplace_back(kind);host::nodes[0].nonFinite=true;
        assert(!collect() && host::activeHandles==0);
    }
    host::reset();host::nodes.emplace_back(host::surface);host::nodes[0].u=4;host::nodes[0].v=3;
    assert(collect(10)==MS::kInvalidParameter && host::payloads()==0);
    host::reset();host::nodes.emplace_back(host::surface);host::nodes[0].u=INT_MAX;host::nodes[0].v=INT_MAX;
    assert(collect()==MS::kInvalidParameter && host::payloads()==0);
    for(int count : {0,-1}) {
        host::reset();host::nodes.emplace_back(host::surface);host::nodes[0].u=count;
        assert(collect()==MS::kInvalidParameter && host::payloads()==0);
    }
    host::reset();host::nodes.emplace_back(host::particle);host::nodes[0].reported=1;
    assert(collect()==MS::kFailure);
    for(const char* operation : {"array.allocate","array.copy"}) {
        host::reset();host::nodes.emplace_back(host::particle);
        host::shortOperation=operation;host::shortOccurrence=1;
        assert(collect()==MS::kFailure);
    }
    for(host::Kind kind : {host::particle,host::pointData}) {
        for(const char* operation : {"array.allocate","array.copy"}) {
            if(kind==host::pointData && std::string(operation)=="array.allocate")continue;
            host::reset();host::nodes.emplace_back(kind);
            host::fail(operation,host::invalidParameter);
            assert(collect()==MS::kInvalidParameter);
        }
    }
    host::reset();host::nodes.emplace_back(host::particle);host::fail("array.allocate",host::returnedFailure);
    assert(collect()==MS::kFailure && host::calls["array.copy"]==0 &&
        host::writesAfterFailedAllocation==0);
    host::reset();host::byteBudget=25;
    std::ostringstream boundary;boundary<<std::string(25,'x');
    assert(isWithinLegacyTextBudget(boundary));boundary<<'x';
    assert(!isWithinLegacyTextBudget(boundary));
}
void commandTests(const std::string& root) {
    const std::string path=root+"/export.txt";
    const char* tags[]={"MESH","CURVE","SURFACE","PARTICLE","DPOINT"};
    for(host::Kind kind : {host::mesh,host::curve,host::surface,host::particle,host::pointData}) {
        host::reset();host::nodes.emplace_back(kind);host::nodes[0].transform=kind!=host::pointData;
        DPointDataExportCmd healthy=command(path);
        assert(healthy.doIt(MArgList())==MS::kSuccess && host::activeHandles==0);
        const std::string expected=std::string("#D POINT DATA 1.0\n\nPOINT_DATA_FROM_")+tags[kind]+" n0 2\nP 0.25 -0.5 1 1.25 -1.5 1\n";
        assert(read(path)==expected);
        const auto baseline=host::calls;
        for(host::Fault failure : {host::returnedFailure,host::invalidParameter,host::standardException,host::unknownException}) {
            for(const auto& entry : baseline) {
                for(unsigned int point=1;point<=entry.second;++point) {
                    host::reset();host::nodes.emplace_back(kind);host::nodes[0].transform=kind!=host::pointData;
                    host::fail(entry.first,failure,point);seed(path);
                    DPointDataExportCmd rejected=command(path);
                    assert(!rejected.doIt(MArgList()) && read(path)=="sentinel" && host::activeHandles==0);
                    if(entry.first=="array.allocate")assert(host::calls["array.copy"]==0 &&
                        host::writesAfterFailedAllocation==0);
                    host::reset();host::nodes.emplace_back(kind);
                    assert(rejected.doIt(MArgList())==MS::kSuccess);
                    assert(read(path)==expected && host::activeHandles==0);
                }
            }
        }
    }
    host::reset();host::nodes.emplace_back(host::mesh);host::nodes.emplace_back(host::particle);
    host::recordBudget=3;seed(path);
    assert(!command(path).doIt(MArgList()) && read(path)=="sentinel");
    assert(host::calls["payload.mesh"]==1 && host::calls["payload.particle"]==0);
    host::reset();host::nodes.emplace_back(host::mesh);host::nodes.emplace_back(host::particle);
    host::recordBudget=4;assert(command(path).doIt(MArgList())==MS::kSuccess);
    std::istringstream combined(read(path));std::vector<dommy::file::Triple> parsed;
    assert(dommy::file::readPointData(combined,parsed) && parsed.size()==4);
    host::reset();host::nodes.emplace_back(host::mesh);host::byteBudget=25;seed(path);
    assert(!command(path).doIt(MArgList()) && read(path)=="sentinel");
    host::reset();host::nodes.emplace_back(host::mesh);host::byteBudget=1;seed(path);
    assert(!command(path).doIt(MArgList()) && host::payloads()==0 && read(path)=="sentinel");
    host::reset();host::nodes.emplace_back(host::mesh);seed(path);
    DPointDataExportCmd missingPath=command(path);missingPath.filePath="";
    assert(!missingPath.doIt(MArgList()) && host::payloads()==0 && read(path)=="sentinel");
    DPointDataExportCmd missingNodes=command(path);missingNodes.savedNodeNames.clear();
    assert(!missingNodes.doIt(MArgList()) && host::payloads()==0 && read(path)=="sentinel");
    host::reset();host::nodes.emplace_back(host::particle);
    host::nodes[0].count=host::nodes[0].actual=host::nodes[0].reported=1025;
    assert(command(path).doIt(MArgList())==MS::kSuccess);
    const std::string payload=read(path);assert(std::count(payload.begin(),payload.end(),'\n')==5);
    std::istringstream split(payload);assert(dommy::file::readPointData(split,parsed) && parsed.size()==1025);
    host::reset();host::nodes.emplace_back(host::mesh);
    assert(!command(root+"/missing-parent/export.txt").doIt(MArgList()));
    assert(!command(root+"/destination-directory").doIt(MArgList()));
    assert(!command(root+"/../unsafe.txt").doIt(MArgList()));
}
void localeTests(const std::string& root) {
    const std::string path=root+"/locale-export.txt";
    const std::locale original;
    std::string expected;
    {
        dommy_test::ScopedNumericLocale classic(std::locale::classic());
        host::reset();host::nodes.emplace_back(host::particle);
        host::nodes[0].count=host::nodes[0].actual=host::nodes[0].reported=1025;
        assert(command(path).doIt(MArgList())==MS::kSuccess);
        expected=read(path);
        assert(expected.find(" n0 1025\n")!=std::string::npos);
        assert(std::locale()==classic.selected());
    }
    for(unsigned int variant=0U;variant<2U;++variant) {
        dommy_test::ScopedNumericLocale localized(variant==0U ? ',' : '.',
                                                 variant==0U ? '.' : ',');
        host::reset();host::nodes.emplace_back(host::particle);
        host::nodes[0].count=host::nodes[0].actual=host::nodes[0].reported=1025;
        assert(command(path).doIt(MArgList())==MS::kSuccess);
        const std::string payload=read(path);
        assert(payload==expected && host::activeHandles==0);
        std::istringstream input(payload);
        input.imbue(localized.selected());
        std::vector<dommy::file::Triple> parsed;
        assert(dommy::file::readPointData(input,parsed) && parsed.size()==1025U);
        assert(parsed.back()[0]==1024.25 && parsed.back()[1]==-1024.5);
        assert(input.getloc()==localized.selected() && std::locale()==localized.selected());
    }
    assert(std::locale()==original);
}
int main(int argc,char** argv) {
    assert(argc==2);admissionTests();commandTests(argv[1]);localeTests(argv[1]);
    std::cout<<"Production point export: five source types, preflight/remaining quota, count/numeric/array failures, recovery, exact legacy bytes, locale invariance and real atomic files passed (Maya/parser/limit seams)\n";
}

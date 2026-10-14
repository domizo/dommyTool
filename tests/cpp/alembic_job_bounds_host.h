#pragma once
#include <maya/MStatus.h>
#include "alembic_box_double.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DAbcRootAdmission.h"
#include "common/utility/DAbcSamplingPlan.h"
#include "common/file/DAtomicFile.h"
#include <cassert>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <sstream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#ifdef DOMMY_SDK_CHECK
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MTypeId.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MFileIO.h>
#include <maya/MStringArray.h>
#else
#include <maya/MMatrix.h>
MMatrix operator*(const MMatrix& a,const MMatrix& b) {
    MMatrix c;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){c[i][j]=0;for(unsigned k=0;k<4;++k)c[i][j]+=a[i][k]*b[k][j];}return c;
}
std::vector<std::string> calls;int failAt=-1,kind=0;
MStatus query(const char* label,MStatus* output=nullptr) {
    calls.push_back(label);const bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();if(failed&&kind==3)throw std::runtime_error("job fault");
    const MStatus s=failed?MS::kFailure:MS::kSuccess;if(output)*output=s;return s;
}
void transport(const char* label){if(!query(label))throw std::runtime_error("writer fault");}
struct MStringArray;
struct MString {std::string text;MString(const char* s=""):text(s){}const char* asChar()const{return text.c_str();}
    unsigned length()const{return text.size();}
    MStatus split(char separator,MStringArray&)const;
    MString operator+(const char* s)const{return MString((text+s).c_str());}MString& operator+=(const MString& s){text+=s.text;return *this;}
    MString& operator+=(const char* s){text+=s;return *this;}
    MString& operator+=(int n){text+=std::to_string(n);return *this;}};
struct MStringArray {std::vector<MString> values;unsigned length()const{return values.size();}
    const MString& operator[](unsigned i)const{return values.at(i);}};
MStatus MString::split(char separator,MStringArray& result)const {
    const auto status=query("namespace.split");if(!status)return status;std::size_t start=0U;
    for(std::size_t i=0U;i<=text.size();++i) {
        if(i!=text.size()&&text[i]!=separator)continue;
        if(i!=start)result.values.emplace_back(text.substr(start,i-start).c_str());
        start=i+1U;
    }
    return status;
}
namespace MFn {enum Type{kTransform=1,kLocator=2};}
struct MTypeId {int value;MTypeId(int v=0):value(v){}bool operator==(const MTypeId& t)const{return value==t.value;}};
struct HostNode {int type=1;std::vector<unsigned> children;bool selected=true,connected=true;MMatrix matrix;bool inherits=true;
    MBoundingBox bounds{MPoint(1,2,3),MPoint(4,5,6)};};
std::vector<HostNode> nodes;
struct MObject {unsigned index;MObject(unsigned i=0):index(i){}bool hasFn(MFn::Type t)const{return nodes.at(index).type==int(t);}
    MString apiTypeStr()const{return "mock";}static const MObject kNullObj;};
const MObject MObject::kNullObj={};
struct MDagPath {std::vector<unsigned> path{0};MObject node(MStatus* s=nullptr)const{query("dag.node",s);return {path.back()};}
    unsigned childCount(MStatus* s=nullptr)const{query("dag.count",s);return nodes.at(path.back()).children.size();}
    MObject child(unsigned i,MStatus* s=nullptr)const{query("dag.child",s);return {nodes.at(path.back()).children.at(i)};}
    MStatus push(MObject o){const auto s=query("dag.push");if(s)path.push_back(o.index);return s;}
    MStatus pop(){const auto s=query("dag.pop");if(s)path.pop_back();return s;}
    MString fullPathName(MStatus* s=nullptr)const{
        query("dag.name",s);std::string name;for(auto i:path)name+="|node"+std::to_string(i);return name.c_str();}
    bool operator<(const MDagPath& other)const{return path<other.path;}};
struct MPlug {MObject object;bool null=false;MPlug()=default;MPlug(MObject o,MObject):object(o){}
    bool isNull()const{return null;}bool isConnected(MStatus* s=nullptr)const{query("plug.connected",s);return nodes.at(object.index).connected;}};
struct MFnDependencyNode {MObject object;MFnDependencyNode(MObject o,MStatus* s=nullptr):object(o){query("node.bind",s);}
    MTypeId typeId(MStatus* s=nullptr)const{query("node.type",s);return {nodes.at(object.index).type};}
    MString name(MStatus* s=nullptr)const{query("node.name",s);return MString(("node"+std::to_string(object.index)).c_str());}};
struct MFnDagNode:MFnDependencyNode {using MFnDependencyNode::MFnDependencyNode;
    bool isIntermediateObject(MStatus* s=nullptr)const{query("node.intermediate",s);return false;}};
struct MSelectionList {bool hasItem(const MDagPath& p,const MObject&,MStatus* s=nullptr)const{query("selection",s);return nodes.at(p.path.back()).selected;}};
struct MGlobal {static void displayWarning(const MString&){}static void displayError(const MString&){}
    static MString mayaVersion(){return "mock";} };
struct MFileIO {static MString currentFile(){return "mock";}};
#endif
namespace Alembic {namespace Util {using uint32_t=unsigned;template<class T>using shared_ptr=std::shared_ptr<T>;}
namespace AbcCoreAbstract {
struct TimeSamplingType {enum AcyclicFlag{kAcyclic};unsigned count=1;double period=1;bool acyclic=false;
    explicit TimeSamplingType(AcyclicFlag):acyclic(true){}TimeSamplingType(unsigned n,double p):count(n),period(p){}};
struct TimeSampling {TimeSamplingType type{1,1};std::vector<double> times{0};
    TimeSampling()=default;TimeSampling(double p,double t):type(1,p),times{t}{}
    TimeSampling(TimeSamplingType t,const std::vector<double>& values):type(t),times(values){}
    bool operator==(const TimeSampling& s)const{return type.count==s.type.count&&type.period==s.type.period&&
        type.acyclic==s.type.acyclic&&times==s.times;}
    double getSampleTime(std::size_t n)const{return type.acyclic?times.at(n):times.at(n%type.count)+(n/type.count)*type.period;}};
using TimeSamplingPtr=std::shared_ptr<TimeSampling>;}
namespace Abc {
bool boxSetFails=false;
struct V3d {double x,y,z;V3d(double a,double b,double c):x(a),y(b),z(c){}};
struct Box3d {V3d min,max;Box3d(V3d a,V3d b):min(a),max(b){}};
struct ArchiveState {bool valid=false;std::vector<AbcCoreAbstract::TimeSampling> times{AbcCoreAbstract::TimeSampling()};
    std::map<std::string,unsigned> counts;std::map<std::string,std::string> strings;};
struct OCompoundProperty {std::shared_ptr<ArchiveState> state;};
struct OObject {std::shared_ptr<ArchiveState> state;OCompoundProperty getProperties()const{return {state};}};
struct OArchive {std::shared_ptr<ArchiveState> state=std::make_shared<ArchiveState>();
    OObject getTop()const{return {state};}bool valid()const{return state->valid;}
    unsigned addTimeSampling(const AbcCoreAbstract::TimeSampling& t){
        for(unsigned i=0;i<state->times.size();++i)if(state->times[i]==t)return i;
        state->times.push_back(t);return state->times.size()-1;}};
struct OBox3dProperty {unsigned timeIndex=0;std::vector<Box3d> boxes;
    void set(const Box3d& b){if(boxSetFails)throw std::runtime_error("box set failed");boxes.push_back(b);}};
struct OStringProperty {OCompoundProperty parent;std::string name;
    OStringProperty(OCompoundProperty p,const char* n):parent(p),name(n){}
    void set(const std::string& value){parent.state->strings[name]=value;}};
struct OUInt32Property {OCompoundProperty parent;std::string name;
    OUInt32Property(OCompoundProperty p,const char* n):parent(p),name(n){}
    void set(unsigned value){parent.state->counts[name]=value;}};
struct ErrorHandler {enum Policy{kThrowPolicy};};
}
namespace AbcGeom {inline Abc::OBox3dProperty CreateOArchiveBounds(const Abc::OArchive&,unsigned index){
    Abc::OBox3dProperty prop;prop.timeIndex=index;return prop;}}
namespace AbcCoreOgawa {struct WriteArchive {};}}
namespace AbcA=Alembic::AbcCoreAbstract;
inline Alembic::Abc::OArchive CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive,const std::string&,
    const std::string&,const std::string&,Alembic::Abc::ErrorHandler::Policy){
    Alembic::Abc::OArchive archive;archive.state->valid=true;return archive;}
#define DCABCEXPORT_VERSION "fixture"
struct JobArgs {bool useSelectionList=false,worldSpace=false;unsigned stripNamespace=0;
    std::shared_ptr<dommy::exporting::RootAdmission> exportRootAdmission=
        std::make_shared<dommy::exporting::RootAdmission>();
    std::set<MDagPath> dagPaths;std::string melPerFrameCallback,pythonPerFrameCallback,melPostCallback,pythonPostCallback;};
struct MayaAttributeWriter {bool isAnimated()const{return false;}void write(){}};
using MayaAttributeWriterPtr=std::shared_ptr<MayaAttributeWriter>;
struct MayaTransformWriter {MMatrix matrix;bool inherits=true,animated=true;
    MayaTransformWriter(Alembic::Abc::OObject&,MDagPath&,unsigned,const JobArgs&);
    MayaTransformWriter(MayaTransformWriter&,MDagPath&,unsigned,const JobArgs&);
    bool isAnimated()const{return animated;}MayaAttributeWriterPtr getAttrs()const{return std::make_shared<MayaAttributeWriter>();}
    Alembic::Abc::OObject getObject()const{return {};}
    MMatrix getSampleMatrix()const;
    bool inheritsSampleTransform()const{return inherits;}void write(){}};
using MayaTransformWriterPtr=std::shared_ptr<MayaTransformWriter>;
struct DPData {static MTypeId id;std::size_t count=1;std::size_t numberOfPoints()const{return count;}};
struct DCurveData {static MTypeId id;std::size_t count=1;std::size_t numberOfCurves()const{return count;}};
struct DMeshData {static MTypeId id;std::size_t count=1;std::size_t numberOfMesh()const{return count;}};
struct DPointDisplay {static MTypeId typeId;static MObject inDP;};
struct DCurveDisplay {static MTypeId typeId;static MObject inDC;};
struct DMeshDisplay {static MTypeId typeId;static MObject inDM;};
struct ShapeWriter {MBoundingBox bounds;bool animated=true;std::size_t pointCount=1,curveCVs=1,writes=0;
    ShapeWriter(MDagPath&,Alembic::Abc::OObject&,unsigned,const JobArgs&);
    bool isAnimated()const{return animated;}MBoundingBox getSampleBounds()const;
    void write(){++writes;}std::size_t getNumPoints()const{return pointCount;}
    std::size_t getNumCVs()const{return curveCVs;}
    MayaAttributeWriterPtr getAttrs()const{return std::make_shared<MayaAttributeWriter>();}};
using DPointWriter=ShapeWriter;using DCurveWriter=ShapeWriter;using DMeshWriter=ShapeWriter;using MayaLocatorWriter=ShapeWriter;
using DPointWriterPtr=std::shared_ptr<DPointWriter>;using DCurveWriterPtr=std::shared_ptr<DCurveWriter>;
using DMeshWriterPtr=std::shared_ptr<DMeshWriter>;using MayaLocatorWriterPtr=std::shared_ptr<MayaLocatorWriter>;
namespace dommy {namespace maya {template<typename T>MStatus getPluginDataFromPlug(const MPlug&,MTypeId,MObject&,const T*&);}}
namespace ddata_export_util {MObject exportDagNode(const MDagPath&);bool isIntermediate(const MObject&);}
namespace ddata_export_util {
using ShapeSet=std::set<MDagPath>;
MString stripNamespaces(const MString&,unsigned);MString exportObjectName(const MFnDependencyNode&,unsigned);
void requireUniqueExportRootNames(const ShapeSet&,unsigned,dommy::exporting::RootAdmission&);
}
namespace ddata_export_util {inline double spf(){return 1.0/24.0;}}
std::vector<double> callbackFrames;
inline void processBestEffortCallback(const std::string& text,bool,double frame,const MBoundingBox&){
    if(!text.empty())callbackFrames.push_back(frame);}
#define DOMMY_MAYA_DEBUG(...)
#ifndef DOMMY_SDK_CHECK
MTypeId DPData::id=10,DCurveData::id=11,DMeshData::id=12;
MTypeId DPointDisplay::typeId=10,DCurveDisplay::typeId=11,DMeshDisplay::typeId=12;
MObject DPointDisplay::inDP,DCurveDisplay::inDC,DMeshDisplay::inDM;
DPData dp;DCurveData dc;DMeshData dm;
template<typename T>const T* data();template<>const DPData* data<DPData>(){return &dp;}
template<>const DCurveData* data<DCurveData>(){return &dc;}template<>const DMeshData* data<DMeshData>(){return &dm;}
template<typename T>MStatus dommy::maya::getPluginDataFromPlug(const MPlug&,MTypeId,MObject&,const T*& out){out=data<T>();return query("data.read");}
MayaTransformWriter::MayaTransformWriter(Alembic::Abc::OObject&,MDagPath& path,unsigned,const JobArgs&) {
    transport("transform.new");const auto& n=nodes.at(path.path.back());matrix=n.matrix;inherits=n.inherits;
}
MayaTransformWriter::MayaTransformWriter(MayaTransformWriter&,MDagPath& path,unsigned time,const JobArgs& args) {
    Alembic::Abc::OObject parent;MayaTransformWriter value(parent,path,time,args);*this=value;
}
MMatrix MayaTransformWriter::getSampleMatrix()const{transport("transform.sample");return matrix;}
ShapeWriter::ShapeWriter(MDagPath& path,Alembic::Abc::OObject&,unsigned,const JobArgs&) {
    transport("shape.new");bounds=nodes.at(path.path.back()).bounds;
}
MBoundingBox ShapeWriter::getSampleBounds()const{transport("shape.bounds");return bounds;}
#endif

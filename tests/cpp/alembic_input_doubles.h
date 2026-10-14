#pragma once
// Real sampling/admission and file transaction; fake DG and archive producer.
#include <maya/MStatus.h>
#include "common/file/DAtomicFile.h"
#include "common/utility/DWorkBudget.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "common/utility/DMayaStateGuard.h"

#ifdef DOMMY_SDK_CHECK
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
extern int admittedShapes;
#else
namespace MFn {enum Type {kInvalid,kAnimCurveTimeToAngular,kAnimCurveTimeToDistance,
    kAnimCurveTimeToTime,kAnimCurveTimeToUnitless,kMute};}
struct PlugState {
    bool array=false,compound=false,connected=false,boolean=true;
    int node=0;std::vector<int> sources,elements,children;
    unsigned countOverride=0;
};
struct NodeState {MFn::Type type=MFn::kInvalid;std::map<std::string,int> attributes;};
std::map<int,PlugState> plugs;
std::map<int,NodeState> nodes;
std::vector<std::string> operations;
int failStep=-1,failureKind=0,admittedShapes=0;
MStatus operation(const char* name,MStatus* output=nullptr) {
    operations.push_back(name);
    const bool failed=int(operations.size())-1==failStep;
    if(failed && failureKind==2)throw std::bad_alloc();
    if(failed && failureKind==3)throw std::runtime_error("injected Alembic input failure");
    const MStatus status=failed?MS::kInvalidParameter:MS::kSuccess;
    if(output)*output=status;return status;
}
struct MObject {
    int id=0;explicit MObject(int value=0):id(value){}
    MFn::Type apiType()const{return nodes.at(id).type;}
};
struct MPlugArray;
struct MPlug {
    int id=0;explicit MPlug(int key=0):id(key){}
    MPlug(const MObject&,const MObject& attribute):id(attribute.id){}
    bool isNull()const{return id==0;}
    bool connectedTo(MPlugArray&,bool,bool,MStatus* status=nullptr)const;
    bool isConnected(MStatus* status=nullptr)const{operation("plug.connected",status);return plugs.at(id).connected;}
    bool isArray(MStatus* status=nullptr)const{operation("plug.array",status);return plugs.at(id).array;}
    bool isCompound(MStatus* status=nullptr)const{operation("plug.compound",status);return plugs.at(id).compound;}
    unsigned numConnectedElements(MStatus* status=nullptr)const{operation("plug.elements",status);return plugs.at(id).countOverride?plugs.at(id).countOverride:unsigned(plugs.at(id).elements.size());}
    unsigned numConnectedChildren(MStatus* status=nullptr)const{
        operation("plug.connectedChildren",status);unsigned count=0;
        for(int child:plugs.at(id).children)count+=!plugs.at(child).sources.empty();return count;
    }
    unsigned numChildren(MStatus* status=nullptr)const{operation("plug.children",status);return plugs.at(id).countOverride?plugs.at(id).countOverride:unsigned(plugs.at(id).children.size());}
    MPlug child(unsigned index,MStatus* status=nullptr)const{operation("plug.child",status);return MPlug(plugs.at(id).children.at(index));}
    MPlug connectionByPhysicalIndex(unsigned index,MStatus* status=nullptr)const{operation("plug.element",status);return MPlug(plugs.at(id).elements.at(index));}
    MObject node(MStatus* status=nullptr)const{operation("plug.node",status);return MObject(plugs.at(id).node);}
    bool asBool(MStatus* status=nullptr)const{operation("plug.boolean",status);return plugs.at(id).boolean;}
};
struct MPlugArray {std::vector<MPlug> values;unsigned length()const{return unsigned(values.size());}MPlug operator[](unsigned i)const{return values.at(i);}};
bool MPlug::connectedTo(MPlugArray& output,bool,bool,MStatus* status)const {
    operation("plug.connections",status);output.values.clear();
    for(int source:plugs.at(id).sources)output.values.push_back(MPlug(source));return !output.values.empty();
}
struct MFnDependencyNode {
    MObject object;
    MStatus setObject(const MObject& value){const MStatus status=operation("node.setObject");object=value;return status;}
    MPlug findPlug(const char* name,bool,MStatus* status=nullptr)const{
        operation("node.findPlug",status);return MPlug(nodes.at(object.id).attributes.at(name));
    }
};
#endif

namespace ddata_export_util {int getSampledType(const MPlug&);int getVisibilityType(const MPlug&);}
namespace ddata_export_util {int getSampledType(const MPlug&,dommy::generation::WorkBudget&);}
struct DPointDisplay {static MObject inDP;};
struct DCurveDisplay {static MObject inDC;};
struct DMeshDisplay {static MObject inDM;};
void shapeAdmissionPoint(const MObject&);
void shapeAdmissionCurve(const MObject&);
void shapeAdmissionMesh(const MObject&);
namespace AbcA {struct TimeSampling {};using TimeSamplingPtr=std::shared_ptr<TimeSampling>;}
AbcA::TimeSamplingPtr observedBoundsTime;
struct JobArgs {};
struct DCAbcWriteJob {
    std::string path;
    DCAbcWriteJob(const char*,bool,const std::set<double>&,AbcA::TimeSamplingPtr,
                 const std::set<double>&,AbcA::TimeSamplingPtr,AbcA::TimeSamplingPtr,const JobArgs&);
    ~DCAbcWriteJob();
    bool eval(double);
};
using DCAbcWriteJobPtr=std::shared_ptr<DCAbcWriteJob>;

#ifndef DOMMY_SDK_CHECK
MObject DPointDisplay::inDP(1),DCurveDisplay::inDC(2),DMeshDisplay::inDM(3);
int currentShape=0;
std::string temporaryPath;
std::vector<std::string> temporaryPaths;
unsigned closedArchives=0U;
std::vector<std::string> lifecycleEvents;
DCAbcWriteJob::DCAbcWriteJob(const char* filename,bool,const std::set<double>&,AbcA::TimeSamplingPtr,
    const std::set<double>&,AbcA::TimeSamplingPtr,AbcA::TimeSamplingPtr bounds,const JobArgs&):path(filename){
    temporaryPath=filename;temporaryPaths.push_back(filename);observedBoundsTime=bounds;}
DCAbcWriteJob::~DCAbcWriteJob(){++closedArchives;lifecycleEvents.push_back("archive.close");}
#endif

#pragma once
#include <maya/MStatus.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DWorkBudget.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DMelString.h"
#include <set>
#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef DOMMY_SDK_CHECK
#include <maya/MItDependencyNodes.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
extern int admitted;
MStatus readPlugValue(const MPlug&,MString&);
#else
int admitted=0,failAt=-1,kind=0;
bool nullPlug=false;
bool testingPaths=false,directoryExists=true;
std::vector<std::string> calls;
struct SceneNode {std::string type,path;bool exists;
    SceneNode(const char* t,const char* p,bool e):type(t),path(p),exists(e){}};
std::vector<SceneNode> scene;
MStatus query(const char* name,MStatus* out=nullptr){calls.push_back(name);const bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();if(failed&&kind==3)throw std::runtime_error("destination fault");
    const MStatus s=failed?MS::kInvalidParameter:MS::kSuccess;if(out)*out=s;return s;}
struct MString {std::string text;MString(const char* s=""):text(s){}bool operator==(const char* s)const{return text==s;}
    bool operator==(const MString& s)const{return text==s.text;}
    const char* asChar()const{return text.c_str();}
    MString operator+(const char* s)const{return MString((text+s).c_str());}
    MString& operator+=(const MString& s){text+=s.text;return *this;}
    void format(const char* pattern,const char* value){text=pattern;text+=value;}
    void format(const char* pattern,const MString& value){format(pattern,value.asChar());}};
struct MObject {std::size_t index=0U;};
namespace MFn {enum Type{kPluginDependNode};}
struct MItDependencyNodes {std::size_t index=0U;
    MItDependencyNodes(MFn::Type,MStatus* s=nullptr){query("iterator.bind",s);}
    bool isDone(MStatus* s=nullptr)const{query("iterator.done",s);return index==scene.size();}
    MObject thisNode(MStatus* s=nullptr)const{query("iterator.node",s);MObject o;o.index=index;return o;}
    MStatus next(){const auto s=query("iterator.next");++index;return s;}
};
struct MPlug {std::size_t index;bool isNull()const{return nullPlug;}};
struct MFnDependencyNode {std::size_t index;
    MFnDependencyNode(MObject o,MStatus* s=nullptr):index(o.index){query("node.bind",s);}
    MString typeName(MStatus* s=nullptr)const{query("node.type",s);return MString(scene.at(index).type.c_str());}
    MPlug findPlug(const char*,bool,MStatus* s=nullptr)const{query("node.plug",s);return MPlug{index};}
};
MStatus readPlugValue(const MPlug& p,MString& out){const auto s=query("plug.string");out=MString(scene.at(p.index).path.c_str());return s;}
struct MFileObject {MString path;
    MStatus setRawFullName(const MString& s){const auto status=query("file.path");path=s;return status;}
    bool exists()const{if(testingPaths)return directoryExists;for(const auto& row:scene)if(row.path==path.text)return row.exists;return false;}
    MString resolvedFullName()const{return path;}
    MString resolvedPath()const{return MString(path.text.substr(0,path.text.find_last_of('/')).c_str());}
    static bool isAbsolutePath(const MString& p){return !p.text.empty()&&p.text.front()=='/';}
};
struct MGlobal {static void displayError(const MString&){}
    static MStatus executeCommand(const MString&){const auto status=query("folder.create");if(status)directoryExists=true;return status;}};
#endif

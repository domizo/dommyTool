#pragma once
#include <maya/MStatus.h>
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DAbcRootAdmission.h"
#include "common/utility/DExportAttributeState.h"
#include "common/file/DAtomicFile.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <maya/MMatrix.h>

#ifdef DOMMY_SDK_CHECK
#include <maya/MFnTransform.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MStringArray.h>
#else
std::vector<std::string> calls;
std::vector<std::string> archiveNames;
std::string nodeName="node";
int failAt=-1,failureKind=0;
bool spline=false,fbik=false,invalidOrder=false,missingPlug=false,ancestorFailure=false;
bool inheritsOnly=false,badEuler=false;
bool badSampleMatrix=false;
bool controlsOnly=false;
unsigned dagDepth=2U;
std::map<std::string,double> values;
MStatus query(const char* label,MStatus* output=nullptr){
    calls.push_back(label);const bool failed=int(calls.size())-1==failAt;
    if(failed&&failureKind==2)throw std::bad_alloc();
    if(failed&&failureKind==3)throw std::runtime_error("transform fault");
    const MStatus status=failed?MS::kInvalidParameter:MS::kSuccess;
    if(output)*output=status;return status;
}
void backendQuery(const char* label){if(!query(label))throw std::runtime_error("backend fault");}
struct MStringArray;
struct MString {std::string value;MString(const char* s=""):value(s){}
    const char* asChar()const{return value.c_str();}unsigned length()const{return value.size();}
    MString& operator+=(const MString& other){value+=other.value;return *this;}
    MStatus split(char separator,MStringArray&)const;};
struct MStringArray {std::vector<MString> values;unsigned length()const{return values.size();}
    const MString& operator[](unsigned index)const{return values.at(index);}};
MStatus MString::split(char separator,MStringArray& output)const{
    const auto status=query("namespace.split");if(!status)return status;std::size_t start=0;
    for(std::size_t i=0;i<=value.size();++i) {
        if(i!=value.size()&&value[i]!=separator)continue;
        if(i!=start)output.values.emplace_back(value.substr(start,i-start).c_str());
        start=i+1U;
    }
    return status;
}
namespace MFn {enum Type {kJoint=1,kIkHandle,kSplineSolver,kTransform};}
struct MObject {MFn::Type type;MObject(MFn::Type t=MFn::kTransform):type(t){}
    bool hasFn(MFn::Type t)const{return type==t;}};
struct MPlugArray;
struct MPlug {
    std::string key;MPlug()=default;MPlug(const char* s):key(s){}
    bool isNull()const{return key.empty();}
    MObject node(MStatus* s=nullptr)const{query("plug.node",s);return key=="handle"?MObject(MFn::kIkHandle):MObject(MFn::kSplineSolver);}
    bool connectedTo(MPlugArray&,bool,bool,MStatus* =nullptr)const;
};
struct MPlugArray {std::vector<MPlug> plugs;unsigned length()const{return plugs.size();}
    MPlug operator[](unsigned i)const{return plugs.at(i);}};
bool MPlug::connectedTo(MPlugArray& result,bool,bool,MStatus* s)const{
    query("plug.connections",s);if(spline)result.plugs.push_back(MPlug(key=="message"?"handle":"solver"));return !result.plugs.empty();}
struct MDagPath {unsigned depth=dagDepth;bool joint=true;
    bool hasFn(MFn::Type t,MStatus* s=nullptr)const{query("path.kind",s);return t==MFn::kJoint&&joint;}
    unsigned length(MStatus* s=nullptr)const{query("path.length",s);return depth;}
    MStatus pop(){const auto s=query("path.pop");if(ancestorFailure)return MS::kFailure;if(s&&depth) --depth;return s;}};
struct MFnDependencyNode {MObject bound;
    MFnDependencyNode(const MObject& o,MStatus* s=nullptr):bound(o){query("node.bind",s);}
    MFnDependencyNode(const MDagPath&,MStatus* s=nullptr){query("node.bind",s);}
    MPlug findPlug(const MString& name,bool,MStatus* s=nullptr)const{query("node.plug",s);return missingPlug?MPlug():MPlug(name.asChar());}
    MString name(MStatus* s=nullptr)const{query("node.name",s);return nodeName.c_str();}
    MObject object()const{return bound;}};
struct MTransformationMatrix {enum RotationOrder {kInvalid,kXYZ,kYZX,kZXY,kXZY,kYXZ,kZYX,kLast};};
struct MFnTransform:MFnDependencyNode {
    using MFnDependencyNode::MFnDependencyNode;
    MTransformationMatrix::RotationOrder rotationOrder(MStatus* s=nullptr)const{
        query("transform.order",s);return invalidOrder?MTransformationMatrix::kInvalid:
            static_cast<MTransformationMatrix::RotationOrder>(values.count("rotateOrder")?
                values.at("rotateOrder")+1:MTransformationMatrix::kXYZ);}};
struct MFnDagNode:MFnDependencyNode {using MFnDependencyNode::MFnDependencyNode;};
struct MFnIkJoint:MFnTransform {
    using MFnTransform::MFnTransform;
    MString hikJointName(MStatus* s=nullptr)const{query("joint.hik",s);return fbik?"animated":"";}
    MStatus getOrientation(double[3],MTransformationMatrix::RotationOrder& order)const{
        const auto s=query("joint.orientation");order=MTransformationMatrix::kXYZ;return s;}
    MStatus getScaleOrientation(double[3],MTransformationMatrix::RotationOrder& order)const{
        const auto s=query("joint.axis");order=MTransformationMatrix::kXYZ;return s;}
};
struct MEulerRotation {enum RotationOrder {kXYZ,kYZX,kZXY,kXZY,kYXZ,kZYX};
    double x=0,y=0,z=0;RotationOrder order=kXYZ;
    MEulerRotation()=default;MEulerRotation(double a,double b,double c,RotationOrder o):x(a),y(b),z(c),order(o){}
    void setValue(double a,double b,double c,RotationOrder o){x=a;y=b;z=c;order=o;}
    void setToClosestSolution(const MEulerRotation&){if(badEuler)x=std::numeric_limits<double>::infinity();}
};
#endif
MStatus readPlugValue(const MPlug&,double&);MStatus readPlugValue(const MPlug&,bool&);
MStatus readPlugValue(const MPlug&,int&);
#ifndef DOMMY_SDK_CHECK
MStatus readPlugValue(const MPlug& plug,double& value){const auto s=query("plug.double");
    const bool scale=plug.key=="scaleX"||plug.key=="scaleY"||plug.key=="scaleZ"||
        plug.key=="inverseScaleX"||plug.key=="inverseScaleY"||plug.key=="inverseScaleZ";
    value=values.count(plug.key)?values.at(plug.key):scale?1.0:0.0;return s;}
MStatus readPlugValue(const MPlug& plug,bool& value){const auto s=query("plug.bool");value=values.count(plug.key)?values.at(plug.key)!=0:true;return s;}
MStatus readPlugValue(const MPlug& plug,int& value){const auto s=query("plug.int");value=values.count(plug.key)?int(values.at(plug.key)):0;return s;}
#endif

// Archive transport/Euler math are explicit seams; no native Alembic library is linked.
namespace Alembic {namespace Util {using uint8_t=unsigned char;using uint32_t=unsigned;template<class T>using shared_ptr=std::shared_ptr<T>;}
namespace Abc {struct OCompoundProperty {};struct OObject {};
struct M44d {double data[4][4]{};M44d(){makeIdentity();}void makeIdentity(){for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)data[i][j]=i==j?1:0;}
    const double* operator[](unsigned i)const{return data[i];}};}
namespace AbcGeom {
using OObject=Abc::OObject;
enum XformOperationType {kTranslateOperation,kScaleOperation,kMatrixOperation,kRotateXOperation,kRotateYOperation,kRotateZOperation};
enum {kTranslateHint,kRotatePivotTranslationHint,kRotatePivotPointHint,kScalePivotTranslationHint,
      kScalePivotPointHint,kRotateHint,kRotateOrientationHint,kScaleHint,kMayaShearHint};
inline double RadiansToDegrees(double x){return x*57.29577951308232;}
inline double DegreesToRadians(double x){return x/57.29577951308232;}
struct XformOp {std::vector<double> values=std::vector<double>(16U,0.0);
    XformOp(XformOperationType,unsigned char){}void setMatrix(const Abc::M44d&){}
    void setChannelValue(unsigned i,double value){values.at(i)=value;}
    double getChannelValue(unsigned i)const{return values.at(i);}};
struct XformSample {std::vector<XformOp> ops;bool inherits=true;
    unsigned getNumOps()const{return ops.size();}std::size_t addOp(const XformOp& op){ops.push_back(op);return ops.size()-1U;}
    XformOp& operator[](std::size_t i){return ops.at(i);}const XformOp& operator[](std::size_t i)const{return ops.at(i);}
    void setInheritsXforms(bool value){inherits=value;}bool getInheritsXforms()const{return inherits;}
    Abc::M44d getMatrix()const{Abc::M44d result;
#ifndef DOMMY_SDK_CHECK
        backendQuery("sample.matrix");
        if(badSampleMatrix)result.data[0][0]=std::numeric_limits<double>::infinity();
#endif
        return result;}};
struct SchemaState {std::vector<XformSample> samples;};
struct OXformSchema {
    std::shared_ptr<SchemaState> state=std::make_shared<SchemaState>();
    void set(const XformSample& sample){
#ifndef DOMMY_SDK_CHECK
        backendQuery("archive.set");
#endif
        state->samples.push_back(sample);}
    void setTimeSampling(unsigned){}Abc::OObject getObject()const{return {};}
    Abc::OCompoundProperty getArbGeomParams()const{return {};}Abc::OCompoundProperty getUserProperties()const{return {};}
};
struct OXform:Abc::OObject {OXform(const Abc::OObject&,const char* name,unsigned){
#ifndef DOMMY_SDK_CHECK
    archiveNames.push_back(name);
#else
    (void)name;
#endif
    }OXformSchema getSchema()const{return {};}};
}}
struct JobArgs {bool filterEulerRotations=false,worldSpace=false;unsigned stripNamespace=0;
    std::shared_ptr<dommy::attribute::ExportAttributeBudget> exportAttributeBudget =
        std::make_shared<dommy::attribute::ExportAttributeBudget>();};
struct MayaAttributeWriter {
    template<typename... Args>MayaAttributeWriter(Args&&...){
#ifndef DOMMY_SDK_CHECK
        backendQuery("attributes.new");
#endif
    }
    static bool hasAnyAttr(const MFnDependencyNode&,const JobArgs&){return false;}
};
using MayaAttributeWriterPtr=std::shared_ptr<MayaAttributeWriter>;
struct AnimChan {MPlug plug;double scale;std::size_t opNum;unsigned channelNum;};
struct MayaTransformWriter {
    Alembic::AbcGeom::OXformSchema mSchema;MayaAttributeWriterPtr mAttrs;
    Alembic::AbcGeom::XformSample mSample;std::vector<AnimChan> mAnimChanList;MPlug mInheritsPlug;
    struct StructuralControl {MPlug plug;int value;bool boolean;};
    std::vector<StructuralControl> mStructuralControls;
    bool mStructuralControlsAnimated=false;
    std::shared_ptr<dommy::attribute::ExportAttributeBudget> mInspectionBudget;
    void trackStructuralControl(const MFnDependencyNode&,const char*,bool,bool);
    void requireStructuralControls()const;
    std::size_t mJointOrientOpIndex[3],mRotateOpIndex[3],mRotateAxisOpIndex[3];
    bool mFilterEulerRotations=false;MEulerRotation mPrevJointOrientSolution,mPrevRotateSolution,mPrevRotateAxisSolution;
    MayaTransformWriter(Alembic::Abc::OObject&,MDagPath&,unsigned,const JobArgs&);
    MayaTransformWriter(MayaTransformWriter&,MDagPath&,unsigned,const JobArgs&);
    ~MayaTransformWriter();void write();bool isAnimated()const;MMatrix getSampleMatrix()const;
    Alembic::Abc::OObject getObject(){return mSchema.getObject();}
    void pushTransformStack(const MFnTransform&,bool);void pushTransformStack(const MFnIkJoint&,bool);
};
namespace ddata_export_util {
void requireExportQuery(const MStatus&,const char*);
int getSampledType(const MPlug&);MString stripNamespaces(const MString&,unsigned);
MString exportObjectName(const MFnDependencyNode&,unsigned);
int getSampledType(const MPlug&,dommy::generation::WorkBudget&);
bool getRotOrder(MTransformationMatrix::RotationOrder,unsigned&,unsigned&,unsigned&);
bool isDrivenByFBIK(const MFnIkJoint&);bool isDrivenBySplineIK(const MFnIkJoint&);
}
#ifndef DOMMY_SDK_CHECK
int ddata_export_util::getSampledType(const MPlug& plug){if(!query("sampling"))throw std::runtime_error("sample fault");
    if(controlsOnly)return plug.key=="rotateOrder"||plug.key=="segmentScaleCompensate"?1:0;
    return !inheritsOnly||plug.key=="inheritsTransform"?1:0;}
int ddata_export_util::getSampledType(const MPlug& plug,dommy::generation::WorkBudget& work){
    if(!work.consume(1U))throw std::length_error("sample work exhausted");return getSampledType(plug);}
#endif

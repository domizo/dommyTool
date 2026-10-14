// Complete computes; Maya/DG/collection copies and sampling are explicit doubles.
#define DOMMY_POINT_NOISE_TESTS
#define DOMMY_PROCEDURAL_API_TESTS
#include "point_force_compute_doubles.h"
#undef MMatrix
#include "common/utility/DAttributePublication.h"
#include <perlin.h>
// FastNoise's generator is an external seam; only its parameter holder is
// needed here. The separate noise suite exercises its configuration boundary.
using FN_DECIMAL=float;
struct FastNoise {};
enum {DP_ATTR_PRE_FLOAT=0,DP_ATTR_PRE_DOUBLE=1};
static std::vector<int> sampledSeeds;
static std::shared_ptr<Payload> pendingPayload;
struct MFnPluginData {
    std::shared_ptr<Payload> candidate;
    MObject object()const{return {"data"};}
};
namespace dommy {namespace maya {
template<typename T>MStatus createPluginData(MFnPluginData& plugin,int,T*& output) {
    const auto status=operation("native.data.create");if(!status)return status;
    if(nullCreated){output=nullptr;return MS::kSuccess;}
    plugin.candidate=std::make_shared<T>();output=static_cast<T*>(plugin.candidate.get());
    pendingPayload=plugin.candidate;return MS::kSuccess;
}
template<typename T>MStatus publishTypedPluginDataObject(MDataHandle& handle,const MObject&,int) {
    assert(handle.valid);const auto* candidate=dynamic_cast<const T*>(pendingPayload.get());
    if(!candidate || rejectFinal || !candidate->valid())return MS::kInvalidParameter;
    const auto status=operation("output.publish");if(!status)return status;
    outputFor(static_cast<T*>(nullptr))=*candidate;++publications;return MS::kSuccess;
}
}}
struct NoiseCurveFn : MFnNurbsCurve {
    NoiseCurveFn(const MObject&,MStatus* status){*status=operation("native.curve.bind");}
};
#define MFnNurbsCurve NoiseCurveFn
struct MFnMesh {
    MFnMesh(const MObject&,MStatus* status){*status=operation("native.mesh.bind");}
    MStatus getVertexNormal(int,MVector& value,MSpace::Space){
        const auto status=operation("native.normal");if(status)value=MVector(0,1,0);return status;
    }
};
static const double kMFnNurbsEpsilon=.0001;
inline MPoint DPToMP(const DPoint& point){return MPoint(point[0],point[1],point[2]);}
enum class DMeshPointSpace{Local};
inline MStatus generateMMeshFromDmesh(const Primitive&,MObject& object,DMeshPointSpace){
    const auto status=operation("native.mesh.generate");if(status)object={"mesh"};return status;
}
namespace dommy {namespace curve {
inline MStatus directionAtClosestPoint(NoiseCurveFn&,const MPoint&,bool,double,MSpace::Space,MVector& value){
    const auto status=operation("native.normal");if(status)value=MVector(0,1,0);return status;
}
}}
struct PyroObjectWorker {
    DScalarDataList* values;
    template<typename Params>PyroObjectWorker(noise::module::Perlin&,const Params&,float,float,
                                              const MObject&,const DPointList&,DScalarDataList* v):values(v){}
    MStatus operator()(const tbb::blocked_range<unsigned>&){
        const auto status=operation("native.pyro");if(status)std::fill(values->begin(),values->end(),.75f);return status;
    }
};
using PyroNoiseFromCurve=PyroObjectWorker;using PyroNoiseFromMesh=PyroObjectWorker;
#include "point_noise_output.inc"

static int fieldCopies=0;
template<typename T>struct ObservedField:TypedDPAttributeData<T> {
    using TypedDPAttributeData<T>::TypedDPAttributeData;
    DPAttributeData::Ptr copy()const override {
        voidApi("metadata.clone");++fieldCopies;return std::make_shared<ObservedField>(*this);
    }
};
void setup(std::size_t count=2U) {
    resetCompute();fieldCopies=0;sampledSeeds.clear();pendingPayload.reset();connectedPlugs.clear();
    inputPoints=DPData();outputPoints=DPData();inputCurves=DCurveData();outputCurves=DCurveData();
    inputMeshes=DMeshData();outputMeshes=DMeshData();
    inputPoints.points.assign(count,DPoint(1,2,3));outputPoints.points={{-99,0,0}};
    inputPoints.attributes["keep"]=std::make_shared<ObservedField<int32_t>>(std::vector<int32_t>(count,7),9);
    auto group=std::make_shared<Group>();group->ids.assign(count,true);inputPoints.groups["keep"]=group;
}
MDataBlock block(int node,short family=0,bool field=true,bool deform=true,short precision=DP_ATTR_PRE_FLOAT) {
    MDataBlock data;Input& input=data.inputs[node==2?"inDP":"inDD"];
    input.payload=family==0?static_cast<const Payload*>(&inputPoints):
        (family==1?static_cast<const Payload*>(&inputCurves):static_cast<const Payload*>(&inputMeshes));
    input.type=input.payload->type();
    data.inputs["aIsNoiseAttribute"].number=field;data.inputs["aNoiseAttributeName"].text="noise";
    data.inputs["aDeformType"].number=deform?NOISE_DEFROM_VECTOR:NOISE_DEFROM_NONE;
    data.inputs["aDeformVector"].vector=Imath::V3d(1,2,3);
    data.inputs["aIsDeform"].number=deform;data.inputs["aPrecision"].number=precision;
    data.inputs["aSeed"].number=4;data.inputs["aAmplitube"].number=2;
    data.inputs["aNoiseType"].number=1;
    data.inputs["aOctave"].number=data.inputs["aTurbulenceRoughnesss"].number=1;
    data.inputs["aScale"].vector=Imath::V3d(1,1,1);data.inputs["aUniformScale"].number=1;
    data.inputs["aFrequency"].number=1;data.inputs["aEpsilon"].number=.001;
    data.inputs["aNormalVector"].vector=Imath::V3d(0,1,0);
    data.inputs["aOutAttribute"].text="density";data.inputs["aRadius"].number=1;
    data.inputs["inTargetDP"].text=MObject::kNullObj.name;
    return data;
}
MStatus compute(int node,MDataBlock& data) {
    if(node==0)return DPFastNoise().compute(MPlug(DPFastNoise::outDD),data);
    if(node==1)return DPDFNoise().compute(MPlug(DPDFNoise::outDD),data);
    return DPPyroNoise().compute(MPlug(DPPyroNoise::outDP),data);
}
std::string state(const DPData& value) {
    std::ostringstream stream;for(const auto& point:value.points)for(unsigned axis=0;axis<3;++axis)stream<<point[axis]<<',';
    for(const auto& field:value.attributes)stream<<field.first<<':'<<field.second->str();
    for(const auto& group:value.groups){stream<<group.first;for(bool id:group.second->ids)stream<<id;}
    return stream.str();
}
bool called(const char* name){return std::find(operations.begin(),operations.end(),name)!=operations.end();}
void pointValues() {
    for(int node=0;node<3;++node)for(bool field:{false,true})for(bool deform:{false,true})
        for(short precision:{DP_ATTR_PRE_FLOAT,DP_ATTR_PRE_DOUBLE}) {
            setup();auto data=block(node,0,field,deform,precision);const auto before=state(inputPoints);
            const auto result=compute(node,data);
            if(!result || publications!=1 || geometryCopies!=1 || fieldCopies!=1) {
                std::cerr<<"noise case "<<node<<' '<<field<<' '<<deform<<' '<<precision
                         <<" copies "<<geometryCopies<<' '<<fieldCopies<<'\n';
                for(const auto& op:operations)std::cerr<<op<<'\n';
            }
            assert(result && publications==1 && geometryCopies==1 && fieldCopies==1);
            assert(state(inputPoints)==before && outputPoints.attributes.at("keep")!=inputPoints.attributes.at("keep"));
            assert(outputPoints.groups.at("keep")!=inputPoints.groups.at("keep"));
            assert(std::count(operations.begin(),operations.end(),"bounds.update")== (node==2?0:1));
            if(node!=2)assert(called("point.batch") && !called("point.copy") && !called("point.replace") && positionCopies==0);
            for(std::size_t i=0;i<inputPoints.points.size();++i)for(unsigned axis=0;axis<3;++axis) {
                const float offset=node==2||!deform?0:float(i+1U)*float(axis+1U)*.5f;
                assert(outputPoints.points[i][axis]==inputPoints.points[i][axis]+offset);
            }
            if(node==0 && field) {
                auto attr=std::dynamic_pointer_cast<const DPFloatAttr>(outputPoints.getAttr("noise"));
                assert(attr && attr->value()==std::vector<float>({.25f,.5f}));
            }
            if(node==1 && field) {
                std::vector<Imath::V3d> values;assert(dommy::attribute::convertVector3Attribute(outputPoints.getAttr("noise"),2U,values));
                assert(values[0][0]==.25 && values[1][2]==1.5);
                assert(outputPoints.getAttr("noise")->getAttrType()==(precision==DP_ATTR_PRE_FLOAT?DP_ATTR_VEC3F:DP_ATTR_VEC3D));
            }
            if(node==2) {
                auto attr=std::dynamic_pointer_cast<const DPFloatAttr>(outputPoints.getAttr("density"));
                assert(attr && attr->value()[0]==.4f);
            }
        }
    for(int node=0;node<3;++node){setup(0);auto data=block(node);assert(compute(node,data)&&outputPoints.points.empty());}
    setup();auto data=block(0);data.inputs["aDeformType"].number=NOISE_DEFROM_ATTRIBUTE;
    data.inputs["aVectorAttributeName"].text=data.inputs["aNoiseAttributeName"].text="vector";
    inputPoints.attributes["vector"]=std::make_shared<ObservedField<Imath::V3d>>(std::vector<Imath::V3d>{{2,0,0},{3,0,0}});
    assert(compute(0,data) && outputPoints.points[0][0]==2 && outputPoints.points[1][0]==4);
    assert(outputPoints.getAttr("vector")->getAttrType()==DP_ATTR_FLOAT && fieldCopies==2);
    for(int node:{0,1}){setup();data=block(node);data.inputs["aNoiseAttributeName"].text="keep";assert(compute(node,data));}
}
void primitiveValues() {
    for(int node:{0,1})for(short family:{1,2}) {
        setup();auto& list=family==1?inputCurves.primitives:inputMeshes.primitives;
        for(unsigned i=0;i<3;++i){auto p=std::make_shared<Primitive>();p->points={{1,2,3},{4,5,6}};
            p->attributes["keep"]=std::make_shared<ObservedField<int32_t>>(std::vector<int32_t>{7,8},9);list.push_back(p);}
        auto data=block(node,family);data.inputs["aUseSeedOffset"].number=1;
        assert(compute(node,data) && sampledSeeds==std::vector<int>({4,5,7}) && fieldCopies==3);
        const auto& output=family==1?outputCurves.primitives:outputMeshes.primitives;
        assert(output.size()==3 && output[0]!=list[0] && list[0]->points[0][0]==1 && output[0]->points[0][0]==1.5f);
        assert(output[0]->attributes.at("keep")!=list[0]->attributes.at("keep"));
        if(node==0){setup();auto& normals=family==1?inputCurves.primitives:inputMeshes.primitives;
            normals.push_back(std::make_shared<Primitive>());data=block(node,family);
            data.inputs["aDeformType"].number=family==1?NOISE_DEFROM_CURVE_NORMAL:NOISE_DEFROM_MESH_NORMAL;
            assert(compute(node,data) && called("native.normal"));}
    }
}
void pyroViews() {
    setup();DPData target;target.points={{1,0,0},{3,0,0},{2,0,0}};
    auto data=block(2);data.inputs["inTargetDP"].payload=&target;
    data.inputs["inTargetDP"].type=DPData::id;data.inputs["inTargetDP"].text="target";
    assert(compute(2,data) && sampledSeeds==std::vector<int>({4,5,6}));
    assert(positionCopies==0 && positionViews==2 && target.points[1][0]==3);
    const auto density=std::dynamic_pointer_cast<const DPFloatAttr>(outputPoints.getAttr("density"));
    assert(density && density->value()[0]==3.5f);
    for(auto type:{MFnData::kNurbsCurve,MFnData::kMesh}) {
        setup();data=block(2);data.inputs["aPyroType"].number=1;
        data.inputs["inObject"].type=type;data.inputs["inObject"].text="object";
        assert(compute(2,data) && called("native.pyro") && positionCopies==0);
        const auto trace=operations;const auto found=std::find(trace.begin(),trace.end(),"native.pyro");
        assert(found!=trace.end());setup();data=block(2);data.inputs["aPyroType"].number=1;
        data.inputs["inObject"].type=type;data.inputs["inObject"].text="object";
        const auto before=state(outputPoints);resetCalls(static_cast<int>(found-trace.begin()));
        assert(!compute(2,data) && publications==0 && state(outputPoints)==before);
        resetCalls();assert(compute(2,data) && publications==1);
    }
}
void failures() {
    for(int node=0;node<3;++node) {
        setup();auto data=block(node);assert(compute(node,data));const auto trace=operations;
        for(std::size_t index=0;index<trace.size();++index)for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            setup();data=block(node);const auto source=state(inputPoints),destination=state(outputPoints);
            resetCalls(static_cast<int>(index));injectedStatus=code;
            const auto status=compute(node,data);assert(!status && state(inputPoints)==source);
            if(trace[index]=="output.clean") assert(publications==1);
            else assert(publications==0 && state(outputPoints)==destination);
            assert(callCount==static_cast<int>(index)+1);
            resetCalls();sampledSeeds.clear();publications=0;assert(compute(node,data)&&publications==1);
        }
        setup();data=block(node);nullCreated=true;assert(!compute(node,data)&&publications==0);
        setup();data=block(node);rejectFinal=true;assert(!compute(node,data)&&publications==0);
    }
}
void budgets() {
    for(int node=0;node<3;++node) {
        setup();auto data=block(node);
        for(const std::string name:{"matrix","matrix2"})
            inputPoints.attributes[name]=std::make_shared<ObservedField<MMatrix>>(std::vector<MMatrix>(2U));
        const auto before=state(inputPoints);assert(!compute(node,data)&&geometryCopies==0&&fieldCopies==0&&publications==0);
        assert(state(inputPoints)==before && !called("noise.fast")&&!called("noise.df")&&!called("noise.pyro"));
        inputPoints.attributes.erase("matrix");inputPoints.attributes.erase("matrix2");
        resetCalls();assert(compute(node,data)&&publications==1);
        setup();data=block(node);bool found=false;
        const std::size_t pointBytes=node==0?sizeof(DPoint)+sizeof(float)+sizeof(Imath::V3d):
            (node==1?sizeof(DPoint)+sizeof(DVec3Data):sizeof(DPoint)+2U*sizeof(float));
        const std::size_t fieldWidth=node==1?sizeof(Imath::V3f):sizeof(float);
        const std::string name=node==2?"density":"noise";
        for(std::size_t padding=0U;padding<256U;++padding) {
            inputPoints.attributes["padding"]=std::make_shared<DPStringAttr>(std::vector<std::string>(2U,std::string(padding,'x')));
            dommy::point::PointPayloadBudget workspace;
            try{dommy::point::preflightPointPayload(inputPoints,pointBytes,workspace);}
            catch(const std::length_error&){continue;}
            try{dommy::point::preflightGeneratedPointAttribute(2U,name,fieldWidth,workspace);}
            catch(const std::length_error&){found=true;break;}
        }
        assert(found);assert(!compute(node,data)&&geometryCopies==0&&sampledSeeds.empty());
    }
    for(int node:{0,1})for(short family:{1,2}) {
        setup();auto& list=family==1?inputCurves.primitives:inputMeshes.primitives;
        for(unsigned i=0;i<2;++i){auto primitive=std::make_shared<Primitive>();primitive->points.assign(2U,DPoint(1,2,3));
            primitive->attributes["matrix"]=std::make_shared<ObservedField<MMatrix>>(std::vector<MMatrix>(2U));list.push_back(primitive);}
        DCurveData one;one.primitives.push_back(list[0]);dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPrimitivePointPayload(one,1U,
            node==0?sizeof(DPoint)+sizeof(float)+sizeof(Imath::V3d):sizeof(DPoint)+sizeof(DVec3Data),
            "noise",node==0?sizeof(float):sizeof(Imath::V3f),workspace);
        auto data=block(node,family);assert(!compute(node,data)&&geometryCopies==0&&fieldCopies==0&&sampledSeeds.empty());
    }
}
int main(int argc,char**) {
    // Initialize attribute object names exactly as the source-extracted declarations require.
    if(argc>1){budgets();std::cout<<"Noise aggregate point/collection admission and retry: OK\n";return 0;}
    pointValues();primitiveValues();pyroViews();failures();
    std::cout<<"Complete noise outputs: values, precision, aliases, seeds, ownership and failure rollback: OK\n";
}

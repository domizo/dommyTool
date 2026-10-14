// Full computes and real typed fields/math/budgets. DG ownership, native
// matrices/geometry and TBB are injectable boundaries, not host emulation.
#define DOMMY_PROCEDURAL_API_TESTS
#define DOMMY_AFFECT_SPIRAL_TESTS
#include "point_force_compute_doubles.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaEntryPointGuard.h"

enum { ID_DP_DATA=DPData::id, ID_DC_DATA=DCurveData::id, ID_DM_DATA=DMeshData::id };
using DReal=float;
struct DeformPoint {
    double x,y,z;
    DeformPoint(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
    DeformPoint operator*(const MMatrix& matrix)const {
        return {x*matrix[0][0]+y*matrix[1][0]+z*matrix[2][0]+matrix[3][0],
                x*matrix[0][1]+y*matrix[1][1]+z*matrix[2][1]+matrix[3][1],
                x*matrix[0][2]+y*matrix[1][2]+z*matrix[2][2]+matrix[3][2]};
    }
};
#define MPoint DeformPoint
struct DeformTransform : MTransformationMatrix {
    MStatus setScale(const double* values,MSpace::Space space){return addScale(values,space);}
    MStatus setRotatePivot(const DeformPoint& value,MSpace::Space space,bool balance){
        assert(balance);return MTransformationMatrix::setRotatePivot(MVector(value.x,value.y,value.z),space,false);
    }
    MStatus setRotation(const double* values,RotationOrder order){return addRotation(values,order,MSpace::kWorld);}
    MStatus setTranslation(const MVector& value,MSpace::Space space){return addTranslation(value,space);}
};
#define MTransformationMatrix DeformTransform
namespace dommy {namespace maya {
inline bool isFinitePoint(const DeformPoint& point){return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);}
inline bool isFloatRepresentablePoint(const DeformPoint& point){
    return numeric::canRepresentAsFloat(point.x)&&numeric::canRepresentAsFloat(point.y)&&numeric::canRepresentAsFloat(point.z);
}
}}
// Private construction and publication are distinct owning-data boundaries.
// All three payload branches execute; native Maya ownership is not emulated.
static DCurveData stagedCurves;
static DMeshData stagedMeshes;
static DCurveData& stagedFor(DCurveData*){return stagedCurves;}
static DMeshData& stagedFor(DMeshData*){return stagedMeshes;}
struct MFnPluginData {
    MObject create(int,MStatus* status){*status=operation("native.data.create");return *status&&!nullCreated?MObject{"data"}:MObject::kNullObj;}
    MObject object()const{return {"data"};}
};
namespace dommy {namespace maya {
template<typename T>MStatus getPluginData(MFnPluginData&,T*& output){
    const auto status=operation("native.data.bind");output=status?&stagedFor(static_cast<T*>(nullptr)):nullptr;return status;
}
template<typename T>MStatus publishPluginDataObject(MDataHandle&,const MObject& object,const T& candidate){
    assert(!object.isNull()&&candidate.valid());
    const auto status=operation("native.data.publish");
    if(status){outputFor(static_cast<T*>(nullptr))=candidate;++publications;}return status;
}
}}
#include "point_deformation_output.inc"

static int fieldCopies=0;
template<typename T>struct ObservedField : TypedDPAttributeData<T> {
    using TypedDPAttributeData<T>::TypedDPAttributeData;
    DPAttributeData::Ptr copy()const override{voidApi("metadata.clone");++fieldCopies;return std::make_shared<ObservedField>(*this);}
};
void setup(std::size_t count=2U){
    resetCompute();fieldCopies=0;connectedPlugs.clear();
    inputPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();outputCurves=DCurveData();stagedCurves=DCurveData();
    inputMeshes=DMeshData();outputMeshes=DMeshData();stagedMeshes=DMeshData();
    inputPoints.points.assign(count,DPoint(1,2,3));
    inputPoints.attributes["keep"]=std::make_shared<ObservedField<int32_t>>(std::vector<int32_t>(count,7),9);
    auto group=std::make_shared<Group>();group->ids.assign(count,true);inputPoints.groups["keep"]=group;
    outputPoints.points={{-99,0,0}};
}
MDataBlock vectorBlock(){
    MDataBlock data;data.inputs["inDP"].payload=&inputPoints;data.inputs["inDP"].type=DPData::id;
    data.inputs["aVectorType"].number=0;data.inputs["aBaseVector"].vector=Imath::V3d(2,0,0);
    data.inputs["aVectorAttributeName"].text="vector";data.inputs["aMagnitude"].number=3;
    data.inputs["aWeightAttributeName"].text="weight";data.inputs["aWeight"].number=.5;
    data.inputs["aMaxStrength"].number=1;return data;
}
MDataBlock transformBlock(bool matrix=false){
    MDataBlock data;data.inputs["inDD"].payload=&inputPoints;data.inputs["inDD"].type=DPData::id;
    data.inputs["aTranslate"].vector=Imath::V3d(2,3,4);data.inputs["aScale"].vector=Imath::V3d(1,1,1);
    data.inputs["transformMatrix"].matrix[3][0]=5;
    if(matrix)connectedPlugs.insert("transformMatrix");return data;
}
MStatus vectorCompute(MDataBlock& data){return DPVectorDeform().compute(MPlug(DPVectorDeform::outDP),data);}
MStatus transformCompute(MDataBlock& data){return DPTransform().compute(MPlug(DPTransform::outDD),data);}
MDataBlock collectionBlock(int type,std::size_t count=2U,bool matrix=false){
    auto data=transformBlock(matrix);
    for(std::size_t i=0;i<count;++i){
        auto curve=std::make_shared<Primitive>();curve->points={{1,2,3},{4,5,6}};
        curve->attributes["keep"]=std::make_shared<DPIntAttr>(std::vector<int32_t>{7,8},9);
        (type==DCurveData::id?inputCurves.primitives:inputMeshes.primitives).push_back(curve);
    }
    outputCurves.primitives.push_back(std::make_shared<Primitive>());
    outputMeshes.primitives.push_back(std::make_shared<Primitive>());
    outputCurves.primitives[0]->points={{-99,0,0}};outputMeshes.primitives[0]->points={{-99,0,0}};
    data.inputs["inDD"].type=type;
    data.inputs["inDD"].payload=type==DCurveData::id?static_cast<const Payload*>(&inputCurves):&inputMeshes;
    return data;
}
template<class T>std::string collectionState(const T& data){
    std::ostringstream stream;
    for(const auto& p:data.primitives){
        if(!p){stream<<"null";continue;}
        for(const auto& point:p->points)stream<<point.x<<','<<point.y<<','<<point.z<<';';
        for(const auto& field:p->attributes)stream<<field.first<<':'<<field.second->str();
    }return stream.str();
}
std::string collectionInput(int type){return type==DCurveData::id?collectionState(inputCurves):collectionState(inputMeshes);}
std::string collectionOutput(int type){return type==DCurveData::id?collectionState(outputCurves):collectionState(outputMeshes);}
std::string state(const DPData& data){
    std::ostringstream stream;for(const auto& point:data.points)for(unsigned axis=0;axis<3;++axis)stream<<point[axis]<<',';
    for(const auto& field:data.attributes)stream<<field.first<<':'<<field.second->str();
    for(const auto& group:data.groups){stream<<group.first;for(bool value:group.second->ids)stream<<value;}
    return stream.str();
}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
void owningOutput(){
    assert(geometryCopies==1&&positionCopies==0&&fieldCopies==int(inputPoints.attributes.size()));
    assert(std::count(operations.begin(),operations.end(),"bounds.update")==1);
    assert(called("point.batch")&&!called("point.copy")&&!called("point.replace"));
    assert(outputPoints.groups.at("keep")!=inputPoints.groups.at("keep"));
    for(const auto& field:inputPoints.attributes)assert(outputPoints.attributes.at(field.first)!=field.second);
    const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("keep"));
    assert(retained&&retained->defaultValue()==9);
}
void values(){
    for(std::size_t count:{0U,2U,4097U})for(bool clamp:{false,true}){
        setup(count);auto data=vectorBlock();data.inputs["aUseClamp"].number=clamp;const auto source=state(inputPoints);
        assert(vectorCompute(data)&&publications==1&&state(inputPoints)==source);owningOutput();
        for(const auto& point:outputPoints.points)assert(point[0]==(clamp?2.5f:4.0f)&&point[1]==2&&point[2]==3);
    }
    for(int type=0;type<3;++type){
        setup();auto data=vectorBlock();data.inputs["aVectorType"].number=1;
        if(type==0)inputPoints.attributes["vector"]=std::make_shared<ObservedField<Imath::V3f>>(DPointList{{0,2,0},{0,3,0}});
        if(type==1)inputPoints.attributes["vector"]=std::make_shared<ObservedField<Imath::V3d>>(std::vector<Imath::V3d>{{0,2,0},{0,3,0}});
        if(type==2)inputPoints.attributes["vector"]=std::make_shared<ObservedField<Imath::V3i>>(std::vector<Imath::V3i>{{0,2,0},{0,3,0}});
        inputPoints.attributes["weight"]=std::make_shared<ObservedField<double>>(std::vector<double>{.5,1});
        assert(vectorCompute(data));owningOutput();assert(outputPoints.points[0][1]==3.5f&&outputPoints.points[1][1]==6.5f);
    }
    for(bool matrix:{false,true}){
        setup();auto data=transformBlock(matrix);const auto source=state(inputPoints);
        assert(transformCompute(data)&&publications==1&&state(inputPoints)==source);owningOutput();
        assert(outputPoints.points[0][0]==(matrix?6:3));assert(outputPoints.points[0][1]==(matrix?2:5));
        setup(0);data=transformBlock(matrix);assert(transformCompute(data)&&outputPoints.points.empty());owningOutput();
    }
    auto* node=static_cast<DPTransform*>(DPTransform::creator());assert(node);delete node;
}
template<typename Block,typename Compute>void failures(Block makeBlock,Compute compute){
    setup();auto data=makeBlock();assert(compute(data));const auto trace=operations;
    for(int at=0;at<int(trace.size());++at)for(bool exception:{false,true})for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        setup();data=makeBlock();const auto source=state(inputPoints),previous=state(outputPoints);
        if(exception)attributeThrowAt=at;else failAt=at;injectedStatus=code;
        assert(!compute(data)&&state(inputPoints)==source);
        if(trace[at]!="output.clean")assert(publications==0&&state(outputPoints)==previous);
        resetCompute();assert(compute(data)&&publications==1&&state(inputPoints)==source);
    }
}
void invalid(){
    for(int variant=0;variant<9;++variant){
        setup();auto data=vectorBlock();
        if(variant==0)data.inputs["aVectorType"].number=2;
        if(variant==1)data.inputs["aMagnitude"].number=NAN;
        if(variant==2){data.inputs["aUseClamp"].number=1;data.inputs["aMaxStrength"].number=-1;}
        if(variant==3)inputPoints.attributes["weight"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"a","b"});
        if(variant==4){data.inputs["aVectorType"].number=1;inputPoints.attributes["vector"]=std::make_shared<DPFloatAttr>(std::vector<float>{1,2});}
        if(variant==5)data.inputs["aWeight"].number=INFINITY;
        if(variant==6){inputPoints.points[0][0]=std::numeric_limits<float>::max();data.inputs["aBaseVector"].vector[0]=std::numeric_limits<float>::max();data.inputs["aMagnitude"].number=2;}
        if(variant==7)nullCreated=true;
        if(variant==8)rejectFinal=true;
        const auto source=state(inputPoints),previous=state(outputPoints);
        assert(!vectorCompute(data)&&publications==0&&state(inputPoints)==source&&state(outputPoints)==previous);
    }
    setup();auto data=transformBlock(true);data.inputs["transformMatrix"].matrix[0][0]=NAN;
    assert(!transformCompute(data)&&!called("output.create"));
    setup();data=vectorBlock();assert(DPVectorDeform().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
    setup();data=transformBlock();assert(DPTransform().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
}
void collectionCases(){
    for(int type:{DCurveData::id,DMeshData::id})for(bool matrix:{false,true})for(std::size_t count:{0U,2U}){
        setup();auto data=collectionBlock(type,count,matrix);const auto source=collectionInput(type);
        assert(transformCompute(data)==MS::kSuccess&&publications==1&&collectionInput(type)==source);
        const auto& output=type==DCurveData::id?outputCurves.primitives:outputMeshes.primitives;
        const auto& input=type==DCurveData::id?inputCurves.primitives:inputMeshes.primitives;
        assert(output.size()==count);
        for(std::size_t i=0;i<count;++i){
            assert(output[i]!=input[i]&&output[i]->attributes.at("keep")!=input[i]->attributes.at("keep"));
            assert(output[i]->points[0].x==(matrix?6:3)&&output[i]->points[0].y==(matrix?2:5)&&output[i]->points[0].z==(matrix?3:7));
        }
        const auto trace=operations;
        for(int at=0;at<int(trace.size());++at)for(bool exception:{false,true})for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            setup();data=collectionBlock(type,count,matrix);
            const auto beforeInput=collectionInput(type),beforeOutput=collectionOutput(type);
            if(exception)attributeThrowAt=at;else failAt=at;injectedStatus=code;
            assert(!transformCompute(data)&&collectionInput(type)==beforeInput);
            if(trace[at]!="output.clean")assert(publications==0&&collectionOutput(type)==beforeOutput);
            resetCompute();assert(transformCompute(data)&&publications==1&&collectionInput(type)==beforeInput);
        }
    }
}
void exactAdmissionStatus(){
    for(int at:{0,1})for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
        setup();auto data=vectorBlock();failAt=at;injectedStatus=code;
        assert(vectorCompute(data)==code&&publications==0&&positionViews==0&&callCount==at+1);
        resetCompute();assert(vectorCompute(data)&&publications==1);
    }
    for(int type:{DPData::id,DCurveData::id,DMeshData::id}){
        setup();auto data=type==DPData::id?transformBlock():collectionBlock(type);
        assert(transformCompute(data));const auto trace=operations;
        for(int at=0;at<int(trace.size());++at){
            if(trace[at]!="input inDD"&&trace[at]!="input.bind"&&trace[at]!="native.data.create")continue;
            for(auto code:{MS::kFailure,MS::kInvalidParameter}){
                setup();data=type==DPData::id?transformBlock():collectionBlock(type);failAt=at;injectedStatus=code;
                assert(transformCompute(data)==code&&publications==0&&callCount==at+1);
                resetCompute();assert(transformCompute(data)&&publications==1);
            }
        }
        setup();data=type==DPData::id?transformBlock():collectionBlock(type);
        if(type==DPData::id)inputPoints.topology=false;
        else if(type==DCurveData::id)inputCurves.topology=false;
        else inputMeshes.topology=false;
        assert(transformCompute(data)==MS::kInvalidParameter&&publications==0);
        setup();data=type==DPData::id?transformBlock():collectionBlock(type);data.inputs["inDD"].payload=nullptr;
        assert(transformCompute(data)==MS::kFailure&&publications==0);
        setup();data=type==DPData::id?transformBlock():collectionBlock(type);nullCreated=true;
        assert(transformCompute(data)==MS::kFailure&&publications==0);
    }
}
template<typename Block,typename Compute>void reduced(Block makeBlock,Compute compute){
    for(int variant=0;variant<4;++variant){
        setup();auto data=makeBlock();
        if(variant==0)inputPoints.attributes["large"]=std::make_shared<DPStringAttr>(std::vector<std::string>(2,std::string(256,'x')));
        if(variant==1)inputPoints.attributes["large"]=std::make_shared<DPIntArrayAttr>(std::vector<DIntArray>(2,DIntArray(100,1)));
        if(variant==2)for(int i=0;i<40;++i)inputPoints.attributes["field"+std::to_string(i)]=std::make_shared<DPFloatAttr>(std::vector<float>{1,2});
        if(variant==3)for(int i=0;i<10;++i){auto group=std::make_shared<Group>();group->ids={true,false};inputPoints.groups[std::string(80,'g')+std::to_string(i)]=group;}
        const auto source=state(inputPoints),previous=state(outputPoints);
        assert(!compute(data)&&!called("output.create")&&positionViews==0&&geometryCopies==0);
        assert(state(inputPoints)==source&&state(outputPoints)==previous);
    }
}
void vectorCombinedBudget(){
    setup();bool found=false;
    for(std::size_t padding=0;padding<256;++padding){
        inputPoints.attributes["padding"]=std::make_shared<DPStringAttr>(std::vector<std::string>(2,std::string(padding,'x')));
        dommy::point::PointPayloadBudget budget;
        try{dommy::point::preflightPointPayload(inputPoints,sizeof(DPoint)+sizeof(Imath::V3d),budget);}
        catch(const std::length_error&){continue;}
        try{budget.add(2U,sizeof(double));}catch(const std::length_error&){found=true;break;}
    }
    assert(found);auto data=vectorBlock();assert(!vectorCompute(data)&&positionViews==0&&!called("output.create"));
}
int main(int argc,char**){
    if(argc>1){reduced(vectorBlock,vectorCompute);reduced([]{return transformBlock();},transformCompute);vectorCombinedBudget();}
    else{values();failures(vectorBlock,vectorCompute);failures([]{return transformBlock();},transformCompute);failures([]{return transformBlock(true);},transformCompute);invalid();collectionCases();exactAdmissionStatus();}
    std::cout<<"Point deformation: full computes, owning metadata, capacity, failure/retry checks passed\n";
}

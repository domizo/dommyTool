#include "collision_distance_compute_doubles.h"
#include "collision_distance_compute.inc"

template<typename Data,typename T>void field(Data& data,const char* name,const std::vector<T>& values){
    data.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values);
}
std::string fields(const DPAttrList& attrs){
    std::ostringstream s;s<<std::setprecision(17);
    for(const auto& a:attrs){s<<a.first<<':'<<a.second->typeName()<<':'<<a.second->attrSize();
        std::vector<Imath::V3d> vectors;
        if(dommy::attribute::convertVector3Attribute(a.second,a.second->attrSize(),vectors))
            for(const auto& v:vectors)for(int k=0;k<3;++k)s<<v[k]<<',';
        std::vector<double> values;
        if(dommy::attribute::convertScalarAttribute(a.second,a.second->attrSize(),values))
            for(double v:values)s<<v<<',';
    }return s.str();
}
std::string signature(const DPData& data){
    std::ostringstream s;s<<fields(data.attributes);
    for(const auto& p:data.points)for(int k=0;k<3;++k)s<<p[k]<<',';return s.str();
}
template<int Id>std::string signature(const PrimitiveData<Id>& data){
    std::ostringstream s;s<<fields(data.attributes);
    for(const auto& p:data.primitives){s<<fields(p->attributes);
        for(const auto& v:p->points)for(int k=0;k<3;++k)s<<v[k]<<',';
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)s<<p->matrix[r][c]<<',';
    }return s.str();
}
std::string inputs(){return signature(inputPoints)+signature(targetPoints)+signature(inputCurves)+signature(inputMeshes);}
std::string outputs(){return signature(outputPoints)+signature(outputCurves)+signature(outputMeshes);}
void setup(){
    resetCompute();badNeighbor=rejectNative=false;nativeQueries=0;
    tbb::recordOnly=false;tbb::scheduledPoints=0;
    inputPoints=DPData();targetPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();outputCurves=DCurveData();inputMeshes=DMeshData();outputMeshes=DMeshData();
    inputPoints.points={{0,0,0},{1,0,0}};targetPoints.points={{-1,0,0},{1,0,0}};
    field(inputPoints,"vel",DPointList{{1,0,0},{-1,0,0}});
    field(inputPoints,"keep",std::vector<int>{7,8});
    for(int i=0;i<2;++i){
        auto p=std::make_shared<Primitive>();p->points={{float(i),0,0},{100,0,0}};
        p->matrix[3][0]=100+i;
        field(*p,"dist",DPointList{{7,8,9},{10,11,12}});
        field(*p,"nearid",DPointList{{7,8,9},{10,11,12}});
        inputCurves.primitives.push_back(std::make_shared<Primitive>(*p));
        inputMeshes.primitives.push_back(p);
    }
    field(inputCurves,"dist",DPointList{{1,2,3},{4,5,6}});
    field(inputCurves,"nearid",DPointList{{1,2,3},{4,5,6}});
    field(inputMeshes,"dist",DPointList{{1,2,3},{4,5,6}});
    field(inputMeshes,"nearid",DPointList{{1,2,3},{4,5,6}});
}
MDataBlock block(int kind=0,int targetMode=0,int component=1){
    MDataBlock data;Input source;
    source.payload=kind==0?static_cast<Payload*>(&inputPoints):
        kind==1?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    source.type=source.payload->type();data.inputs["inDD"]=data.inputs["inDP"]=source;
    Input target;target.payload=&targetPoints;target.type=DPData::id;data.inputs["inTargetDP"]=target;
    data.inputs["aSearchRadius"].number=3;data.inputs["aRadius"].number=1;
    data.inputs["aRadiusAttribute"].text="pscale";data.inputs["aMassAttribute"].text="mass";
    data.inputs["aDamping"].number=1;data.inputs["aAvoidPower"].number=0;
    data.inputs["aComponentType"].number=component;data.inputs["targetType"].number=targetMode;
    data.inputs["targetPoint"].vector=Imath::V3d(4,0,0);
    data.inputs["distAttrName"].text="dist";data.inputs["aAddNearID"].number=1;
    data.inputs["aNearIDAttributeName"].text="nearid";data.inputs["inObject"].type=MFnData::kMesh;
    return data;
}
MStatus compute(MDataBlock& data,bool collision){
    return collision?DPSelfCollision().compute(MPlug(DPSelfCollision::outDP),data):
        DPDistance().compute(MPlug(DPDistance::outDD),data);
}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
int failures(MDataBlock data,bool collision){
    resetCompute();const auto before=inputs();assert(compute(data,collision));const auto trace=operations;
    const auto expected=outputs();int count=0;
    for(int index=0;index<int(trace.size());++index){
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            resetCompute(index);injectedStatus=code;
            assert(!compute(data,collision)&&callCount==index+1);
            assert(inputs()==before&&outputs()==expected);
            if(trace[index]!="output.clean")assert(publications==0);++count;
        }
        resetCompute();attributeThrowAt=index;
        assert(compute(data,collision)==MS::kFailure&&callCount==index+1);
        assert(inputs()==before&&outputs()==expected);
        if(trace[index]!="output.clean")assert(publications==0);++count;
    }
    return count;
}
Imath::V3d velocity(std::size_t index,const char* name="vel"){
    std::vector<Imath::V3d> values;
    assert(dommy::attribute::convertVector3Attribute(outputPoints.getAttr(name),outputPoints.points.size(),values));
    return values[index];
}
void collisions(){
    for(float distance:{1.0f,1e-4f,1e20f,std::numeric_limits<float>::denorm_min()}){
        for(float mass:{1.0f,1e-20f,std::numeric_limits<float>::max()}){
            setup();inputPoints.points[1][0]=distance;
            field(inputPoints,"mass",std::vector<float>{mass,mass});
            auto data=block();data.inputs["aUseMass"].number=1;
            data.inputs["aRadius"].number=distance;data.inputs["aSearchRadius"].number=double(distance)*2;
            const auto before=inputs();assert(compute(data,true)&&inputs()==before);
            assert(velocity(0)[0]==-1&&velocity(1)[0]==1);
        }
    }
    // Preserve the old mass weighting, avoid term, separating and exact-contact rules.
    setup();auto data=block();field(inputPoints,"mass",std::vector<float>{2,1});
    data.inputs["aUseMass"].number=1;data.inputs["aDamping"].number=0.5;
    data.inputs["aAvoidPower"].number=0.75;
    assert(compute(data,true)&&velocity(0)[0]==-0.5&&velocity(1)[0]==1.25);
    setup();data=block();inputPoints.points[1][0]=2;assert(compute(data,true)&&velocity(0)[0]==-1);
    setup();data=block();inputPoints.points[1][0]=2.5;assert(compute(data,true)&&velocity(0)[0]==1);
    setup();data=block();inputPoints.points[1][0]=0;assert(compute(data,true)&&velocity(0)[0]==1);
    setup();data=block();field(inputPoints,"mass",std::vector<float>{0,0});data.inputs["aUseMass"].number=1;
    assert(compute(data,true)&&velocity(0)[0]==1);
    setup();data=block();field(inputPoints,"vel",DPointList{{-1,0,0},{1,0,0}});
    assert(compute(data,true)&&velocity(0)[0]==-1);
    setup();data=block();field(inputPoints,"vel",std::vector<Imath::V3d>{{1,0,0},{-1,0,0}});
    field(inputPoints,"v",DPointList{{9,0,0},{9,0,0}});
    assert(compute(data,true)&&velocity(0)[0]==-1&&velocity(0,"v")[0]==9);
    assert(outputPoints.getAttr("vel")->getAttrType()==DP_ATTR_VEC3D);
    inputPoints.attributes.erase("vel");resetCompute();assert(compute(data,true)&&velocity(0,"v")[0]==9);
    inputPoints.attributes.erase("v");resetCompute();assert(compute(data,true)&&!outputPoints.isExistAttr("vel"));
    setup();data=block();field(inputPoints,"vel",std::vector<Imath::V3i>{{1,0,0},{-1,0,0}});
    assert(compute(data,true)&&velocity(0)[0]==-1&&outputPoints.getAttr("vel")->getAttrType()==DP_ATTR_VEC3I);
    setup();data=block();field(inputPoints,"mass",DPointList{{7,8,9},{10,11,12}});
    field(inputPoints,"pscale",DPointList{{7,8,9},{10,11,12}});data.inputs["aUseMass"].number=1;
    assert(compute(data,true)&&velocity(0)[0]==-1); // Non-scalar legacy fallback.
    for(int variant=0;variant<10;++variant){
        setup();data=block();
        if(variant==0)data.inputs["aDamping"].number=NAN;
        if(variant==1)data.inputs["aRadius"].number=-1;
        if(variant==2)data.inputs["aSearchRadius"].number=0;
        if(variant==3)field(inputPoints,"vel",std::vector<int>{1,2});
        if(variant==4)field(inputPoints,"vel",DPointList{{1,0,0}});
        if(variant==5)field(inputPoints,"pscale",std::vector<float>{-1,1});
        if(variant==6){data.inputs["aUseMass"].number=1;field(inputPoints,"mass",std::vector<float>{-1,1});}
        if(variant==7)badNeighbor=true;
        if(variant==8){data.inputs["aDamping"].number=std::numeric_limits<float>::max();
            field(inputPoints,"vel",DPointList{{1e30f,0,0},{-1e30f,0,0}});}
        if(variant==9)inputPoints.points[0][0]=NAN;
        assert(!compute(data,true)&&geometryCopies==0&&publications==0&&!called("output.create"));
    }
    setup();inputPoints=DPData();data=block();data.inputs["aSearchRadius"].number=0;
    assert(compute(data,true)&&!outputPoints.isExistAttr("vel"));
    // A malformed worker input must fail instead of returning a partial result.
    setup();DScalarDataList radii{1},masses{1,1};std::vector<Imath::V3d> vel(2);
    DVec3DataList output(2);IDsListArray neighbors(2);std::atomic<bool> good(true);
    DPSelfCollideOp operation(inputPoints.points,radii,masses,vel,output,neighbors,good);
    operation(tbb::blocked_range<std::size_t>(0,2));assert(!good);
}
template<typename Data>std::vector<float> scalars(const Data& data,const char* name){
    std::vector<float> result;const auto attr=data.getAttr(name);assert(attr);
    assert(dommy::attribute::convertScalarAttribute(attr,attr->attrSize(),result));return result;
}
void distances(){
    for(int kind=0;kind<3;++kind)for(int component:{0,1})for(int target:{0,2}){
        setup();auto data=block(kind,target,component);const auto before=inputs();
        assert(compute(data,false)&&inputs()==before&&publications==1);
        assert(std::count(operations.begin(),operations.end(),"input inDD")==1);
        if(kind==0){assert(scalars(outputPoints,"dist")[0]==(target==0?4:1));
            if(target==2)assert(scalars(outputPoints,"nearid")[0]==0);}
        else {
            const auto& parts=kind==1?outputCurves.primitives:outputMeshes.primitives;
            const auto prim=kind==1?outputCurves.getAttr("dist"):outputMeshes.getAttr("dist");
            const auto values=component==0?(kind==1?scalars(outputCurves,"dist"):scalars(outputMeshes,"dist")):
                scalars(*parts[0],"dist");
            assert(values[0]==(target==0?4:1));
            if(component==0)assert(parts[0]->getAttr("dist")->getAttrType()==DP_ATTR_VEC3F);
            else assert(prim->getAttrType()==DP_ATTR_VEC3F&&values[1]==(target==0?96:99));
            assert(parts[0]->matrix[3][0]==100);
        }
    }
    for(int kind=0;kind<3;++kind)for(int component:{0,1})
        for(auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}){
            setup();auto data=block(kind,1,component);data.inputs["inObject"].type=type;
            assert(compute(data,false)&&nativeQueries==(kind==0||component==0?1:2));
            resetCompute();rejectNative=true;assert(!compute(data,false)&&geometryCopies==0&&publications==0);
        }
    for(int kind=0;kind<3;++kind){
        setup();inputPoints=DPData();inputCurves=DCurveData();inputMeshes=DMeshData();auto data=block(kind);
        assert(compute(data,false));
        if(kind==0)assert(outputPoints.isExistAttr("dist"));
        else assert(!outputCurves.isExistAttr("dist")&&!outputMeshes.isExistAttr("dist"));
    }
    for(int variant=0;variant<6;++variant){
        setup();auto data=block(0,2);
        if(variant==0)data.inputs["aComponentType"].number=2;
        if(variant==1)data.inputs["targetType"].number=3;
        if(variant==2)data.inputs["distAttrName"].text="";
        if(variant==3)data.inputs["aNearIDAttributeName"].text="dist";
        if(variant==4)targetPoints.points.clear();
        if(variant==5)targetPoints.points[0][0]=INFINITY;
        assert(!compute(data,false)&&geometryCopies==0&&publications==0);
    }
    setup();auto data=block(1,0,0);inputCurves.primitives[0]->points.clear();
    assert(!compute(data,false)&&geometryCopies==0);
}
void budgets(){
    setup();targetPoints.points.resize(10000);DistanceTarget target;target.type=2;target.points=&targetPoints.points;
    assert(validDistanceBudget(target,9000,2));assert(!validDistanceBudget(target,10000,2));
    assert(!validDistanceBudget(target,std::numeric_limits<std::size_t>::max(),2));
    assert(!validDistanceBudget(target,1,dommy::generation::kMaxConnectedGeometryCount+1));
    target.type=0;assert(!validDistanceBudget(target,3000000,1));target.type=2;
    DPointList first(5001),second(5001);std::vector<const DPointList*> points{&first,&second};
    std::vector<DScalarDataList> values(2,DScalarDataList(5001));std::vector<DIntArray> ids(2,DIntArray(5001));
    tbb::recordOnly=true;DistanceGen evaluate(target,points,values,ids);
    assert(!evaluate()&&tbb::scheduledPoints==0);
    // Linear/native target modes must enforce the dispatcher workspace limit,
    // too, independently of compute. No native work may begin on rejection.
    {
        DPointList large(3000000);
        std::vector<const DPointList*> partitions{&large};
        std::vector<DScalarDataList> result(1,DScalarDataList(large.size()));
        for(short mode:{short(0),short(1)}){
            target.type=mode;target.object={"mesh"};target.objectType=MFnData::kMesh;
            DistanceGen oversized(target,partitions,result,ids);
            assert(!oversized()&&tbb::scheduledPoints==0&&nativeQueries==0);
        }
    }
    for(int kind=0;kind<3;++kind){
        setup();targetPoints.points.resize(10000);auto data=block(kind,2);
        if(kind==0){inputPoints.attributes.clear();inputPoints.points.resize(10002);}
        else {auto& parts=kind==1?inputCurves.primitives:inputMeshes.primitives;
            for(auto& p:parts){p->attributes.clear();p->points.resize(5001);}}
        tbb::recordOnly=true;assert(!compute(data,false)&&geometryCopies==0&&!called("output.create"));
        assert(tbb::scheduledPoints==0&&positionViews==1);
    }
    setup();inputPoints.attributes.clear();inputPoints.points.resize(10001);auto data=block();
    tbb::recordOnly=true;assert(!compute(data,true)&&!called("collision.search")&&geometryCopies==0);
}
int main(){
    collisions();distances();budgets();int count=0;
    setup();count+=failures(block(),true);
    for(int kind=0;kind<3;++kind)for(int component:{0,1})for(int target:{0,1,2}){
        setup();count+=failures(block(kind,target,component),false);
    }
    for(bool collision:{false,true}){
        setup();auto data=block();nullCreated=true;assert(!compute(data,collision)&&publications==0);
        setup();data=block();rejectFinal=true;assert(!compute(data,collision)&&publications==0);
    }
    std::cout<<"Collision/distance: numeric/namespace/budget contracts and "<<count<<" boundary failures passed\n";
}

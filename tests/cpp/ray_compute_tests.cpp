#include "ray_compute_doubles.h"
#include "ray_compute.inc"

template<typename Data,typename T>void field(Data& data,const std::vector<T>& values){
    data.attributes["dir"]=std::make_shared<TypedDPAttributeData<T>>(values);
}
std::string fields(const DPAttrList& attrs){
    std::ostringstream s;s<<std::setprecision(17);
    for(const auto& a:attrs){s<<a.first<<':';if(!a.second){s<<"null";continue;}
        s<<a.second->typeName()<<':'<<a.second->attrSize();
        std::vector<Imath::V3d> vectors;
        if(dommy::attribute::convertVector3Attribute(a.second,a.second->attrSize(),vectors))
            for(const auto& v:vectors)for(int k=0;k<3;++k)s<<v[k]<<',';
        std::vector<double> values;
        if(dommy::attribute::convertScalarAttribute(a.second,a.second->attrSize(),values))
            for(double v:values)s<<v<<',';
    }return s.str();
}
template<typename Groups>std::string masks(const Groups& groups){
    std::ostringstream s;for(const auto& g:groups){s<<g.first<<':';
        if(g.second)for(bool b:g.second->ids)s<<b;else s<<"null";}return s.str();
}
std::string signature(const DPData& data){
    std::ostringstream s;s<<fields(data.attributes)<<masks(data.groups);
    for(const auto& p:data.points)for(int k=0;k<3;++k)s<<p[k]<<',';return s.str();
}
template<int Id>std::string signature(const PrimitiveData<Id>& data){
    std::ostringstream s;s<<fields(data.attributes)<<masks(data.groups);
    for(const auto& p:data.primitives){if(!p){s<<"null";continue;}
        s<<fields(p->attributes)<<masks(p->groups);
        for(const auto& g:p->componentGroups)s<<g.first<<masks(g.second);
        for(const auto& v:p->points)for(int k=0;k<3;++k)s<<v[k]<<',';
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)s<<p->matrix[r][c]<<',';
    }return s.str();
}
std::string inputs(){return signature(inputPoints)+signature(inputCurves)+signature(inputMeshes);}
std::string outputs(){return signature(outputPoints)+signature(outputCurves)+signature(outputMeshes);}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
void setup(){
    resetCompute();connectedPlugs={"inObject"};
    noHit=badNativePoint=badNativeArray=false;
    rayQueries=closestQueries=acceleratorReleases=0;queryTolerance=0;
    inputPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();outputCurves=DCurveData();inputMeshes=DMeshData();outputMeshes=DMeshData();
    inputPoints.points={{0,0,0},{1,0,0}};
    inputPoints.attributes["hit"]=std::make_shared<DPFloatAttr>(std::vector<float>{42,43});
    auto group=std::make_shared<Group>();group->ids={false,false};inputPoints.groups["hit"]=group;
    for(int i=0;i<2;++i){
        auto p=std::make_shared<Primitive>();p->points={{float(i),0,0},{float(i+2),0,0}};
        p->matrix[3][0]=7;
        p->attributes=inputPoints.attributes;p->groups["hit"]=std::make_shared<Group>(*group);
        inputCurves.primitives.push_back(std::make_shared<Primitive>(*p));inputMeshes.primitives.push_back(p);
    }
}
MDataBlock block(int kind=0,int mode=DP_RAY_MINIMUM,MFnData::Type object=MFnData::kMesh){
    MDataBlock data;Input source;
    source.payload=kind==0?static_cast<Payload*>(&inputPoints):
        kind==1?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    source.type=source.payload->type();data.inputs["inDD"]=source;
    data.inputs["inObject"].type=object;data.inputs["inObject"].text="object";
    data.inputs["aMethod"].number=mode;data.inputs["aRayTolerance"].number=0.01;
    data.inputs["aIsTransform"].number=data.inputs["aIsCreateRayGroup"].number=1;
    data.inputs["aScale"].number=1;data.inputs["aRayDirection"].vector=Imath::V3d(0,1,0);
    data.inputs["aRayGroupName"].text="hit";data.inputs["aVectorAttribute"].text="dir";
    data.inputs["aFaceGroup"].text="selected";return data;
}
MStatus compute(MDataBlock& data){return DPRay().compute(MPlug(DPRay::outDD),data);}
void rejected(MDataBlock& data){
    const auto before=inputs(),old=outputs();assert(!compute(data));
    assert(inputs()==before&&outputs()==old&&publications==0);
}
int failures(MDataBlock data){
    resetCompute();const auto before=inputs();assert(compute(data));const auto trace=operations;
    const auto expected=outputs();int count=0;
    for(int index=0;index<int(trace.size());++index){
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            resetCompute(index);injectedStatus=code;
            assert(!compute(data)&&callCount==index+1);
            assert(inputs()==before&&outputs()==expected);
            if(trace[index]!="output.clean")assert(publications==0);++count;
        }
        resetCompute();attributeThrowAt=index;
        assert(compute(data)==MS::kFailure&&callCount==index+1);
        assert(inputs()==before&&outputs()==expected);
        if(trace[index]!="output.clean")assert(publications==0);++count;
    }return count;
}
void contracts(){
    for(int kind=0;kind<3;++kind)for(int mode=0;mode<2;++mode)
    for(auto object:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        setup();auto data=block(kind,mode,object);const auto before=inputs();
        if(mode==DP_RAY_PROJECTION&&object==MFnData::kNurbsCurve){rejected(data);continue;}
        assert(compute(data)&&inputs()==before&&publications==1);
        assert(std::count(operations.begin(),operations.end(),"input inDD")==1);
        assert(std::count(operations.begin(),operations.end(),"input.bind")==1);
        const float expected=mode==DP_RAY_PROJECTION?2:1;
        if(kind==0){assert(outputPoints.points[0][1]==expected&&outputPoints.groups["hit"]->ids==DPIdList(2,true));
            assert(fields(inputPoints.attributes)==fields(outputPoints.attributes));}
        else {const auto& result=kind==1?outputCurves.primitives:outputMeshes.primitives;
            for(const auto& p:result){assert(p->points[0][1]==expected&&p->matrix[3][0]==7);
                assert(p->groups.at("hit")->ids==DPIdList(2,true));
                assert(fields(p->attributes)==fields(inputPoints.attributes));}}
        if(mode==DP_RAY_PROJECTION&&object==MFnData::kMesh)assert(acceleratorReleases==(kind==0?1:2));
    }
    for(auto object:{MFnData::kNurbsSurface,MFnData::kMesh}){
        setup();auto data=block(0,DP_RAY_PROJECTION,object);data.inputs["aIsFarthest"].number=1;
        assert(compute(data)&&outputPoints.points[0][1]==4);
        setup();data=block(0,DP_RAY_PROJECTION,object);noHit=true;
        assert(compute(data)&&outputPoints.points[0][1]==0&&outputPoints.groups["hit"]->ids==DPIdList(2,false));
    }
    setup();auto data=block();data.inputs["aIsTransform"].number=0;data.inputs["aLift"].number=2;
    assert(compute(data)&&outputPoints.points[0][1]==2); // Historical independent lift.
    setup();data=block();data.inputs["aScale"].number=0.5;
    assert(compute(data)&&outputPoints.points[0][1]==0.5f);
    setup();data=block();data.inputs["aIsMaxDistance"].number=1;data.inputs["aMaxDistance"].number=0.5;
    assert(compute(data)&&outputPoints.points[0][1]==0);
    for(int kind=0;kind<3;++kind){
        setup();data=block(kind);inputPoints.points.clear();inputPoints.attributes.clear();inputPoints.groups.clear();
        inputCurves.primitives.clear();inputMeshes.primitives.clear();
        assert(compute(data)&&publications==1&&rayQueries==0&&closestQueries==0);
        assert(called("mesh.bind")== (kind==0));
    }
}
void directions(){
    for(int type=0;type<3;++type){
        setup();auto data=block(2);
        for(auto& p:inputMeshes.primitives){
            if(type==0)field(*p,std::vector<Imath::V3i>(2,Imath::V3i(0,1,0)));
            if(type==1)field(*p,std::vector<Imath::V3f>(2,Imath::V3f(0,1,0)));
            if(type==2)field(*p,std::vector<Imath::V3d>(2,Imath::V3d(0,1,0)));
        }
        assert(compute(data)&&rayQueries==4&&closestQueries==0&&outputMeshes.primitives[0]->points[0][1]==2);
    }
    for(int invalid=0;invalid<7;++invalid)for(int mode=0;mode<2;++mode){
        setup();auto data=block(2,mode);auto& p=*inputMeshes.primitives[0];
        if(invalid==0)field(p,std::vector<float>{1,1});
        if(invalid==1)field(p,std::vector<Imath::V2f>(2,Imath::V2f(1,1)));
        if(invalid==2)field(p,std::vector<std::string>{"x","y"});
        if(invalid==3)field(p,std::vector<Imath::V3f>(1,Imath::V3f(0,1,0)));
        if(invalid==4)field(p,std::vector<Imath::V3f>(2,Imath::V3f(0,NAN,0)));
        if(invalid==5)field(p,std::vector<Imath::V3d>(2,Imath::V3d(0,1e100,0)));
        if(invalid==6)p.attributes["dir"]=nullptr;
        rejected(data);assert(rayQueries==0&&closestQueries==0);
    }
    setup();auto data=block(2);
    field(*inputMeshes.primitives[0],std::vector<Imath::V3f>(2,Imath::V3f(0,1,0)));
    assert(compute(data)&&rayQueries==0&&closestQueries==4); // Mixed partitions: all-or-none fallback.
    setup();data=block(2);
    for(auto& p:inputMeshes.primitives)field(*p,std::vector<Imath::V3f>(2,Imath::V3f(0,0,0)));
    assert(compute(data)&&rayQueries==0&&closestQueries==0&&outputMeshes.primitives[0]->points[0][1]==0);
    setup();data=block(2);noHit=true;
    for(auto& p:inputMeshes.primitives)field(*p,std::vector<Imath::V3f>(2,Imath::V3f(0,1,0)));
    assert(compute(data)&&rayQueries==4&&closestQueries==4&&outputMeshes.primitives[0]->points[0][1]==1);
    for(int mode=0;mode<2;++mode){
        setup();data=block(2,mode);
        for(auto& p:inputMeshes.primitives){auto g=std::make_shared<Group>();g->ids={false};
            p->componentGroups[D_COMP_FACE]["selected"]=g;}
        assert(compute(data));assert(outputMeshes.primitives[0]->points[0][1]==(mode==DP_RAY_MINIMUM?0:2));
    }
}
void invalidAndTolerance(){
    for(int invalid=0;invalid<9;++invalid){
        setup();auto data=block();
        if(invalid==0)connectedPlugs.clear();
        if(invalid==1)data.inputs["inObject"].text="null";
        if(invalid==2)data.inputs["inObject"].type=MFnData::kInvalid;
        if(invalid==3)data.inputs["inDD"].type=-1;
        if(invalid==4)data.inputs["inDD"].payload=nullptr;
        if(invalid==5)data.inputs["aRayGroupName"].text="";
        if(invalid==6)data.inputs["aMethod"].number=9;
        if(invalid==7)inputPoints.topology=false;
        if(invalid==8)data.inputs["aRayTolerance"].number=NAN;
        rejected(data);assert(geometryCopies==0);
    }
    setup();auto data=block();assert(DPRay().compute(MPlug({"unknown"}),data)==MS::kUnknownParameter&&callCount==0);
    for(int kind=0;kind<3;++kind)for(int mode=0;mode<2;++mode)
    for(auto object:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        if(mode==DP_RAY_PROJECTION&&object==MFnData::kNurbsCurve)continue;
        setup();data=block(kind,mode,object);data.inputs["aRayTolerance"].number=1e100;
        if(mode==DP_RAY_PROJECTION&&object==MFnData::kMesh){
            rejected(data);assert(!called("output.create")&&!called("mesh.bind"));
        }else {assert(compute(data));if(object!=MFnData::kMesh)assert(queryTolerance==1e100);}
    }
    for(double tolerance:{-1.0,double(NAN),double(INFINITY),1e100,double(std::numeric_limits<float>::max())}){
        setup();DPRayParams params;params.m_object={"mesh"};params.mRayTolerance=tolerance;
        DPointList points{{0,0,0}};DPIdList hits(1,false);
        if(tolerance==double(std::numeric_limits<float>::max())){
            assert(projectToMesh(params,points,hits));assert(queryTolerance==tolerance&&hits[0]);
        }else {assert(projectToMesh(params,points,hits)==MS::kInvalidParameter);
            assert(callCount==0&&!hits[0]&&points[0][1]==0);}
    }
    for(auto object:{MFnData::kNurbsSurface,MFnData::kMesh})for(bool badArray:{false,true}){
        setup();data=block(0,DP_RAY_PROJECTION,object);
        badNativeArray=badArray;badNativePoint=!badArray;rejected(data);
    }
    setup();data=block();nullCreated=true;rejected(data);
    setup();data=block();rejectFinal=true;rejected(data);
}
int main(){
    contracts();directions();invalidAndTolerance();int count=0;
    for(int kind=0;kind<3;++kind)for(int mode=0;mode<2;++mode)
    for(auto object:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        if(mode==DP_RAY_PROJECTION&&object==MFnData::kNurbsCurve)continue;
        setup();auto data=block(kind,mode,object);data.inputs["aLift"].number=0.1;count+=failures(data);
    }
    setup();auto data=block(2);
    for(auto& p:inputMeshes.primitives)field(*p,std::vector<Imath::V3f>(2,Imath::V3f(0,1,0)));
    count+=failures(data);
    std::cout<<"Ray: dispatch/direction/tolerance/group contracts; "<<count<<" injected compute failures passed\n";
}

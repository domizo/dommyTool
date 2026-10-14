#include "affect_spiral_compute_doubles.h"
#include "affect_spiral_compute.inc"

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
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
void setup(){
    resetCompute();connectedPlugs.clear();connectedPlugs.insert("inObject");
    nanParameter=nanRamp=colorRamp=forbidWork=shortCenters=false;rampSamples=tangentQueries=nativeQueries=0;
    inputPoints=DPData();targetPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();outputCurves=DCurveData();inputMeshes=DMeshData();outputMeshes=DMeshData();
    inputPoints.points={{0,0,0},{1,0,0}};targetPoints.points={{0,0,0},{4,0,0}};
    field(inputPoints,"aval",std::vector<double>{42,43});
    field(inputPoints,"keep",std::vector<int>{7,8});
    for(int i=0;i<2;++i){
        auto p=std::make_shared<Primitive>();p->points={{float(i),0,0},{float(i+2),0,0}};
        p->matrix[3][0]=1;
        field(*p,"aval",std::vector<double>{42,43});
        field(*p,"keep",std::vector<int>{7,8});
        inputCurves.primitives.push_back(std::make_shared<Primitive>(*p));inputMeshes.primitives.push_back(p);
    }
    field(inputCurves,"aval",std::vector<double>{42,43});
    field(inputMeshes,"aval",std::vector<double>{42,43});
}
MDataBlock block(int kind=0,int mode=AFFECT_POINT,int component=DP_ATTR_CLASS_POINT){
    MDataBlock data;Input source;
    source.payload=kind==0?static_cast<Payload*>(&inputPoints):
        kind==1?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    source.type=source.payload->type();data.inputs["inDD"]=data.inputs["inDP"]=source;
    Input target;target.payload=&targetPoints;target.type=DPData::id;data.inputs["aTargetDP"]=target;
    data.inputs["aAffectType"].number=mode;data.inputs["aAttributeClass"].number=component;
    data.inputs["aDistance"].vector=Imath::V3d(0,5,0);
    data.inputs["aValueRange"].vector=Imath::V3d(0,1,0);
    data.inputs["aScale"].number=1;data.inputs["aTolerance"].number=0.1;
    data.inputs["aMeshResolution"].number=1500;data.inputs["aInnerCheckAngle"].vector=Imath::V3d(1,0,0);
    data.inputs["aInObject"].type=MFnData::kMesh;data.inputs["aInObject"].text="mesh";
    data.inputs["aAxisTarget"].vector=Imath::V3d(5,0,0);
    data.inputs["aAxisTargetMatrix"].matrix[3][0]=5;
    data.inputs["aOutAttribute"].text="aval";
    data.inputs["aDistanceAttribute"].text="dist";
    data.inputs["aSlideAttribute"].text="slide";data.inputs["aScaleAttribute"].text="scale";
    data.inputs["aAttributeName"].text="aval";
    data.inputs["inObject"].type=MFnData::kNurbsCurve;data.inputs["inObject"].text="curve";
    data.inputs["aRadiusRange"].vector=Imath::V3d(0,10,0);
    data.inputs["aCenterStreamRange"].vector=data.inputs["aUpStraemRange"].vector=Imath::V3d(0,1,0);
    data.inputs["aCenterDistanceRange"].vector=data.inputs["aUpDistanceRange"].vector=Imath::V3d(0,5,0);
    data.inputs["aMagnitude"].number=1;
    return data;
}
MStatus compute(MDataBlock& data,bool spiral=false){
    return spiral?DPSpiralField().compute(MPlug(DPSpiralField::outDP),data):
        DPAffect().compute(MPlug(DPAffect::outDD),data);
}
int failures(MDataBlock data,bool spiral=false){
    resetCompute();const auto before=inputs();assert(compute(data,spiral));const auto trace=operations;
    const auto expected=outputs();int count=0;
    for(int index=0;index<int(trace.size());++index){
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            resetCompute(index);injectedStatus=code;
            assert(!compute(data,spiral)&&callCount==index+1);
            assert(inputs()==before&&outputs()==expected);
            if(trace[index]!="output.clean")assert(publications==0);++count;
        }
        resetCompute();attributeThrowAt=index;
        assert(compute(data,spiral)==MS::kFailure&&callCount==index+1);
        assert(inputs()==before&&outputs()==expected);
        if(trace[index]!="output.clean")assert(publications==0);++count;
    }return count;
}
float scalar(const DPAttributeData::ConstPtr& attr,std::size_t index){
    assert(attr&&attr->getAttrType()==DP_ATTR_FLOAT);std::vector<float> values;
    assert(dommy::attribute::convertScalarAttribute(attr,attr->attrSize(),values));return values[index];
}
void close(double a,double b){assert(std::abs(a-b)<1e-6);}
void affectContracts(){
    for(int kind=0;kind<3;++kind)for(int component=0;component<2;++component){
        setup();auto data=block(kind,AFFECT_POINT,component);const auto before=inputs();
        assert(compute(data)&&inputs()==before&&publications==1);
        if(kind==0){close(scalar(outputPoints.getAttr("aval"),0),1);close(scalar(outputPoints.getAttr("aval"),1),0.8);}
        else {
            const auto& attrs=kind==1?outputCurves.attributes:outputMeshes.attributes;
            const auto& primitives=kind==1?outputCurves.primitives:outputMeshes.primitives;
            if(component==DP_ATTR_CLASS_PRIMITIVE){
                close(scalar(attrs.at("aval"),0),0.6);close(scalar(attrs.at("aval"),1),0.4);
                assert(primitives[0]->getAttr("aval")->getAttrType()==DP_ATTR_DOUBLE);
            }else{
                assert(attrs.at("aval")->getAttrType()==DP_ATTR_DOUBLE);
                close(scalar(primitives[0]->getAttr("aval"),0),1);
                close(scalar(primitives[1]->getAttr("aval"),1),0.4);
            }
        }
    }
    for(bool matrices:{false,true}){
        setup();auto data=block(0,AFFECT_AXIS_LINE);data.inputs["aUseAxisDistance"].number=1;
        if(matrices){connectedPlugs.insert("aAxisSourceMatrix");connectedPlugs.insert("aAxisTargetMatrix");}
        assert(compute(data));close(scalar(outputPoints.getAttr("aval"),1),0.8);
    }
    for(int kind:{1,2}){
        setup();auto data=block(kind,AFFECT_POINT,DP_ATTR_CLASS_PRIMITIVE);shortCenters=true;
        assert(!compute(data)&&geometryCopies==0&&publications==0);
    }
    setup();auto data=block();data.inputs["aValueRange"].vector=Imath::V3d(3,1,0);
    data.inputs["aScale"].number=2;assert(compute(data));close(scalar(outputPoints.getAttr("aval"),0),6);
    setup();data=block(0,AFFECT_DPOINT);data.inputs["aValueRange"].vector=Imath::V3d(1,3,0);
    data.inputs["aScale"].number=2;assert(compute(data));close(scalar(outputPoints.getAttr("aval"),0),5);
    setup();data=block(0,AFFECT_DPOINT);
    field(targetPoints,"dist",std::vector<double>{2,2});field(targetPoints,"scale",std::vector<int>{1,1});
    assert(compute(data));close(scalar(outputPoints.getAttr("aval"),1),0.5);
    setup();data=block(0,AFFECT_DPOINT);field(targetPoints,"dist",DPointList{{9,9,9},{9,9,9}});
    assert(compute(data));close(scalar(outputPoints.getAttr("aval"),1),0.8); // Legacy non-scalar fallback.
    const float maximum=std::numeric_limits<float>::max();
    setup();data=block();inputPoints.points={{2.5f,0,0}};
    field(inputPoints,"aval",std::vector<double>{42});field(inputPoints,"keep",std::vector<int>{7});
    data.inputs["aValueRange"].vector=Imath::V3d(-maximum,maximum,0);
    assert(compute(data));close(scalar(outputPoints.getAttr("aval"),0),0);
    for(float position:{0.0f,5.0f}){
        setup();data=block();inputPoints.points={{position,0,0}};
        field(inputPoints,"aval",std::vector<double>{42});field(inputPoints,"keep",std::vector<int>{7});
        data.inputs["aValueRange"].vector=Imath::V3d(-maximum,maximum,0);
        assert(compute(data));assert(scalar(outputPoints.getAttr("aval"),0)==(position==0?maximum:-maximum));
    }
    setup();data=block(0,AFFECT_DPOINT);inputPoints.points={{2.5f,0,0}};
    field(inputPoints,"aval",std::vector<double>{42});field(inputPoints,"keep",std::vector<int>{7});
    targetPoints.points={{0,0,0}};
    data.inputs["aValueRange"].vector=Imath::V3d(-maximum,maximum,0);
    assert(compute(data));close(scalar(outputPoints.getAttr("aval"),0),0);
    setup();data=block();data.inputs["aOutAttribute"].text="";assert(compute(data));
    assert(outputPoints.getAttr("affect")&&outputPoints.getAttr("aval")->getAttrType()==DP_ATTR_DOUBLE);
    for(int kind=0;kind<3;++kind)for(int component=0;component<2;++component){
        setup();data=block(kind,AFFECT_DPOINT,component);targetPoints=DPData();assert(compute(data));
        if(kind==0)assert(!outputPoints.getAttr("aval"));
        else if(component==DP_ATTR_CLASS_PRIMITIVE)
            close(scalar(kind==1?outputCurves.getAttr("aval"):outputMeshes.getAttr("aval"),0),0);
        else close(scalar(kind==1?outputCurves.primitives[0]->getAttr("aval"):
            outputMeshes.primitives[0]->getAttr("aval"),0),0);
    }
    for(int kind=0;kind<3;++kind){
        setup();inputPoints=DPData();inputCurves=DCurveData();inputMeshes=DMeshData();
        data=block(kind,AFFECT_DPOINT);data.inputs["aTargetDP"].payload=nullptr;
        assert(compute(data)&&!called("input aTargetDP"));
    }
    for(int variant=0;variant<11;++variant){
        setup();data=block(0,AFFECT_DPOINT);
        if(variant==0)data.inputs["aTargetDP"].payload=nullptr;
        if(variant==1)data.inputs["aTargetDP"].type=DCurveData::id;
        if(variant==2)field(targetPoints,"dist",std::vector<double>{1});
        if(variant==3)field(targetPoints,"dist",std::vector<double>{1,1e100});
        if(variant==4)data.inputs["aSourcePoint"].vector[0]=NAN;
        if(variant==5)data.inputs["aScale"].number=NAN;
        if(variant==6)data.inputs["aAffectType"].number=4;
        if(variant==7)data.inputs["aAttributeClass"].number=2;
        if(variant==8){connectedPlugs.insert("aAxisSourceMatrix");data.inputs["aAxisSourceMatrix"].matrix[0][0]=NAN;}
        if(variant==9)nanRamp=true;
        if(variant==10)inputPoints.topology=false;
        assert(!compute(data)&&publications==0&&geometryCopies==0&&!called("output.create"));
    }
}
Imath::V3d vectorValue(std::size_t index){
    const auto attr=outputPoints.getAttr("aval");assert(attr&&attr->getAttrType()==DP_ATTR_VEC3F);
    std::vector<Imath::V3d> values;
    assert(dommy::attribute::convertVector3Attribute(attr,attr->attrSize(),values));return values[index];
}
void spiralContracts(){
    setup();inputPoints.points={{1,5,0},{2,5,0}};auto data=block();const auto before=inputs();
    assert(compute(data,true)&&inputs()==before);auto v=vectorValue(0);
    close(v[0],-0.5);close(v[1],0.5);close(v[2],1);
    v=vectorValue(1);close(v[0],-2);close(v[1],1);close(v[2],1);
    resetCompute();data.inputs["aInvert"].number=1;data.inputs["aNormalize"].number=1;
    data.inputs["aMagnitude"].number=2;assert(compute(data,true));v=vectorValue(0);
    close(v.length(),2);assert(v[0]>0&&v[1]<0&&v[2]<0);
    resetCompute();data.inputs["aIsMaxDistance"].number=1;data.inputs["aMaxDistance"].number=0.5;
    assert(compute(data,true));close(vectorValue(0).length(),0);close(vectorValue(1).length(),0);
    setup();inputPoints.points={{1,5,0},{0,5,0}};data=block();data.inputs["aRadiusRange"].vector=Imath::V3d();
    assert(compute(data,true));close(vectorValue(0)[0],-2.5);close(vectorValue(1).length(),0);
    setup();data=block();nanParameter=true;
    assert(!compute(data,true)&&tangentQueries==0&&rampSamples==0&&geometryCopies==0&&publications==0);
    for(int variant=0;variant<8;++variant){
        setup();data=block();
        if(variant==0)connectedPlugs.clear();
        if(variant==1)data.inputs["inObject"].type=MFnData::kMesh;
        if(variant==2)data.inputs["inDP"].payload=nullptr;
        if(variant==3)data.inputs["aAttributeName"].text="";
        if(variant==4)colorRamp=true;
        if(variant==5)nanRamp=true;
        if(variant==6)data.inputs["aRadiusRange"].vector[0]=-1;
        if(variant==7)data.inputs["aMagnitude"].number=NAN;
        assert(!compute(data,true)&&geometryCopies==0&&publications==0&&!called("output.create"));
    }
    setup();inputPoints=DPData();data=block();assert(compute(data,true)&&!outputPoints.getAttr("aval"));
}
void budgets(){
    assert(validAffectBudget(5001,1,10000)&&!validAffectBudget(10002,2,10000));
    assert(!validAffectBudget(SIZE_MAX,1,1)&&!validAffectBudget(1,1,SIZE_MAX));
    assert(!validAffectBudget(4000000,1,1000000)); // Excessive query-by-target work.
    assert(!validAffectBudget(0,1000000,5000000)); // Empty partitions still need target/table storage.
    assert(validSpiralBudget(1000,1)&&!validSpiralBudget(1000000,1));
    assert(!validSpiralBudget(SIZE_MAX,1)&&!validSpiralBudget(1,SIZE_MAX));
    setup();DPointList points(5001),targets(10000);
    std::vector<const DPointList*> partitions{&points,&points};
    std::vector<DScalarDataList> values(2,DScalarDataList(points.size()));
    AffectParams params;params.m_type=AFFECT_DPOINT;params.m_targetPts=&targets;
    params.m_maxDistArray.resize(10000,5);params.m_slideArray.resize(10000);params.m_scaleArray.resize(10000,1);
    forbidWork=true;assert(!AffectGen(params,partitions,values)()&&rampSamples==0);
    setup();inputPoints=DPData();inputPoints.points.resize(10002);targetPoints=DPData();targetPoints.points.resize(10000);
    auto data=block(0,AFFECT_DPOINT);forbidWork=true;
    assert(!compute(data)&&geometryCopies==0&&publications==0&&!called("output.create")&&positionViews==1);
    for(int kind:{1,2}){
        setup();targetPoints=DPData();targetPoints.points.resize(10000);
        auto& partitions=kind==1?inputCurves.primitives:inputMeshes.primitives;
        for(auto& partition:partitions){partition->attributes.clear();partition->points.resize(5001);}
        data=block(kind,AFFECT_DPOINT);forbidWork=true;
        assert(!compute(data)&&geometryCopies==0&&publications==0&&positionViews==1);
    }
    setup();inputPoints=DPData();inputPoints.points.resize(1000000);data=block();forbidWork=true;
    assert(!compute(data,true)&&geometryCopies==0&&publications==0&&!called("output.create")&&positionViews==0);
    setup();points.resize(400000);partitions={&points,&points};
    std::vector<DVec3DataList> vectors(2,DVec3DataList(points.size()));SpiralParams spiral;
    forbidWork=true;assert(!SpiralGen(spiral,partitions,vectors)()&&tangentQueries==0);
}
int main(){
    affectContracts();spiralContracts();budgets();int injected=0;
    for(int kind=0;kind<3;++kind)for(int component=0;component<2;++component)for(int mode=0;mode<4;++mode){
        setup();auto data=block(kind,mode,component);injected+=failures(data);
    }
    setup();connectedPlugs.insert("aAxisSourceMatrix");connectedPlugs.insert("aAxisTargetMatrix");
    injected+=failures(block(0,AFFECT_AXIS_LINE));
    setup();inputPoints.points={{1,5,0},{2,5,0}};injected+=failures(block(),true);
    setup();MDataBlock data;assert(DPAffect().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);
    assert(DPSpiralField().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&callCount==0);
    std::cout<<"Affect/spiral: numeric/field/empty contracts, aggregate budgets, NaN query rejection, "
             <<injected<<" injected compute failures passed\n";
}

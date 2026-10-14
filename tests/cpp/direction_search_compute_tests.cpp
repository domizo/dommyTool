#include "direction_search_compute_doubles.h"
#include "direction_search_compute.inc"

template<typename Data,typename T>void field(Data& data,const std::string& name,const std::vector<T>& values){
    data.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values);
}
std::vector<Imath::V3f> vectors(const DPAttrList& attrs,const std::string& name="N"){
    const auto attr=std::dynamic_pointer_cast<const DPVec3fAttr>(attrs.at(name));assert(attr);return attr->value();
}
bool equal(const DPoint& a,const DPoint& b){
    for(unsigned k=0;k<3;++k)if(std::abs(double(a[k])-b[k])>1e-5)return false;return true;
}
void setup(){
    resetCompute();nativeQueries=0;rejectNative=badNeighbor=false;
    inputPoints=DPData();targetPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();inputMeshes=DMeshData();outputCurves=DCurveData();outputMeshes=DMeshData();
    inputPoints.points={{0,0,0},{2,0,0}};targetPoints.points={{-1,0,0},{1,0,0}};
    field(inputPoints,"N",DPointList{{7,8,9},{7,8,9}});
    field(targetPoints,"N",std::vector<Imath::V3d>{{0,2,0},{0,0,3}});
    for(int i=0;i<2;++i){
        auto p=std::make_shared<Primitive>();p->points={{float(i),0,0},{float(i+2),0,0}};
        field(*p,"N",DPointList{{7,8,9},{7,8,9}});field(*p,"keep",std::vector<int32_t>{3,4});
        inputCurves.primitives.push_back(std::make_shared<Primitive>(*p));
        inputMeshes.primitives.push_back(p);
    }
    field(inputCurves,"N",DPointList{{8,8,8},{8,8,8}});
    field(inputMeshes,"N",DPointList{{8,8,8},{8,8,8}});
    outputPoints.points={{-99,-99,-99}};
}
MDataBlock directionBlock(int kind=0,int mode=3,int targetMode=0,int attributeClass=0){
    MDataBlock data;
    data.inputs["inDD"].payload=kind==0?static_cast<Payload*>(&inputPoints):
        kind==1?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    data.inputs["inDD"].type=kind==0?DPData::id:kind==1?DCurveData::id:DMeshData::id;
    data.inputs["dirAttrName"].text="N";data.inputs["aDirectionType"].number=mode;
    data.inputs["baseVector"].vector=Imath::V3d(0,1,0);data.inputs["targetVector"].vector=Imath::V3d(1,2,3);
    data.inputs["aScale"].number=1;data.inputs["aAttributeClass"].number=attributeClass;
    data.inputs["aDistance"].number=5;data.inputs["aTargetDPCalcType"].number=targetMode;
    data.inputs["inTargetDP"].payload=&targetPoints;data.inputs["inTargetDP"].type=DPData::id;
    data.inputs["aTargetDPAttrName"].text="N";data.inputs["inObject"].type=MFnData::kMesh;
    return data;
}
MDataBlock searchBlock(int kind=0,bool nearest=false){
    auto data=directionBlock(kind);data.inputs["aTargetDP"]=data.inputs["inTargetDP"];
    data.inputs["aIndexAttributeNames"].text="idx";data.inputs["aRadius"].number=2.5;
    data.inputs["aMaxCount"].number=2;data.inputs["aDetectNearestPoint"].number=nearest;return data;
}
MStatus direction(MDataBlock& data){return DPDirection().compute(MPlug(DPDirection::outDD),data);}
MStatus search(MDataBlock& data){return DPSearchPoint().compute(MPlug(DPSearchPoint::outDD),data);}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
std::vector<DPointList> partitions(int kind,int attributeClass){
    if(kind==0)return {inputPoints.points};
    std::vector<DPointList> result;
    if(kind==2&&attributeClass==1){DPointList centers;inputMeshes.getPrimitiveCenterList(centers);return {centers};}
    for(const auto& p:kind==1?inputCurves.primitives:inputMeshes.primitives)result.push_back(p->points);
    return result;
}
std::vector<DPointList> results(int kind,int attributeClass){
    if(kind==0)return {vectors(outputPoints.attributes)};
    if(kind==2&&attributeClass==1)return {vectors(outputMeshes.attributes)};
    std::vector<DPointList> result;
    for(const auto& p:kind==1?outputCurves.primitives:outputMeshes.primitives)result.push_back(vectors(p->attributes));
    return result;
}
DPoint reference(const DPoint& point,std::size_t index,int mode,int targetMode,bool limited,bool inverted,bool normalized,double scale){
    Imath::V3d value(0,1,0),target(1,2,3);bool found=false;std::size_t chosen=0;
    if(mode==1)found=!limited||(Imath::V3d(point)-target).length()<1.0;
    if(mode==3){
        if(targetMode==2){chosen=index;found=index<targetPoints.points.size();}
        else{double best=INFINITY;for(std::size_t j=0;j<targetPoints.points.size();++j){
            const double distance=(Imath::V3d(point)-Imath::V3d(targetPoints.points[j])).length();
            if((!limited||distance<1.0)&&distance<best){best=distance;chosen=j;found=true;}
        }}
        if(found)target=Imath::V3d(targetPoints.points[chosen]);
    }
    if(found){
        if(mode==3&&targetMode==1)value=chosen==0?Imath::V3d(0,2,0):Imath::V3d(0,0,3);
        else value=target-Imath::V3d(point);
    }
    if(inverted)value=-value;
    if(normalized&&value.length()>std::numeric_limits<double>::epsilon())value=value/value.length();
    return DPoint(value*scale);
}
void testDirection(){
    for(int kind=0;kind<3;++kind)for(int cls=0;cls<2;++cls)for(int mode:{0,1,3})
    for(int targetMode=0;targetMode<3;++targetMode)for(bool limited:{false,true}){
        setup();auto data=directionBlock(kind,mode,targetMode,cls);
        data.inputs["aUseDistance"].number=limited;data.inputs["aDistance"].number=1;
        data.inputs["aInvert"].number=true;data.inputs["aNormalize"].number=true;data.inputs["aScale"].number=-2;
        const auto source=partitions(kind,cls);resetCompute();assert(direction(data)&&publications==1);
        const auto actual=results(kind,cls);assert(actual.size()==source.size());
        for(std::size_t p=0;p<source.size();++p)for(std::size_t i=0;i<source[p].size();++i)
            assert(equal(actual[p][i],reference(source[p][i],i,mode,targetMode,limited,true,true,-2)));
        if(kind==2&&cls==1){
            for(const auto& p:outputMeshes.primitives){assert(equal(vectors(p->attributes)[0],DPoint(7,8,9)));assert(p->attributes.count("keep"));}
            assert(!called("primitive.point.remove"));
        }
        if(kind==1)assert(equal(vectors(outputCurves.attributes)[0],DPoint(8,8,8)));
    }
    setup();field(inputMeshes,"N",std::vector<int32_t>{4,5});auto data=directionBlock(2,0,0,1);
    assert(direction(data)&&equal(vectors(outputMeshes.attributes)[0],DPoint(0,1,0)));
    for(int kind=0;kind<3;++kind)for(auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}){
        setup();data=directionBlock(kind,2);data.inputs["inObject"].type=type;
        assert(direction(data)&&nativeQueries==(kind==0?1:2));
    }
    setup();data=directionBlock(0,3,2);targetPoints.points.resize(1);targetPoints.attributes.clear();
    assert(direction(data)&&equal(vectors(outputPoints.attributes)[1],DPoint(0,1,0)));
    setup();data=directionBlock(0,3,0);targetPoints=DPData();assert(direction(data));
    assert(equal(vectors(outputPoints.attributes)[0],DPoint(0,1,0)));
}
void testBudget(){
    assert(validDirectionBudget(9999,2,3,0,10000));
    assert(!validDirectionBudget(10000,2,3,0,10000));
    assert(validDirectionBudget(10001,2,3,2,10001));
    assert(!validDirectionBudget(10000000,1,0,0,0));
    assert(!validDirectionBudget(1,1000001,0,0,0));
    assert(!validDirectionBudget(1,1,3,0,std::numeric_limits<std::size_t>::max()));
    for(int kind=0;kind<3;++kind){
        setup();inputPoints.attributes.clear();inputPoints.points.resize(10001);
        targetPoints.attributes.clear();targetPoints.points.resize(10001);
        for(auto& p:inputCurves.primitives){p->attributes.clear();p->points.resize(5001);}
        for(auto& p:inputMeshes.primitives){p->attributes.clear();p->points.resize(5001);}
        auto data=directionBlock(kind);assert(!direction(data)&&geometryCopies==0&&!called("output.create"));
        assert(positionViews==1); // Target view only: no query workspace before preflight.
    }
    setup();DPointList a(5001),b(5001),targets(10000);std::vector<const DPointList*> points{&a,&b};
    std::vector<DPointList> values{a,b};DirectionGen generator(3,points,values);generator.m_targetPoints=&targets;
    assert(generator()==MS::kInvalidParameter);
}
void testSearch(){
    for(int kind=0;kind<3;++kind)for(bool nearest:{false,true}){
        setup();auto data=searchBlock(kind,nearest);assert(search(data)&&publications==1);
        const auto& attrs=kind==0?outputPoints.attributes:kind==1?outputCurves.attributes:outputMeshes.attributes;
        auto values=std::dynamic_pointer_cast<const DPIntArrayAttr>(attrs.at("idx"))->value();
        assert(values.size()==2);assert(values[1]==DIntArray{1});
        assert(values[0]==(nearest?DIntArray{kind==0?0:1}:kind==0?DIntArray{0,1}:DIntArray{1,0}));
    }
    setup();auto data=searchBlock();data.inputs["aIndexAttributeNames"].text="";targetPoints=DPData();
    assert(search(data));auto empty=std::dynamic_pointer_cast<const DPIntArrayAttr>(outputPoints.getAttr("idx"));
    assert(empty&&empty->value().size()==2&&empty->value()[0].empty());
    setup();data=searchBlock(0,true);data.inputs["aRadius"].number=NAN;assert(search(data));
}
void testInvalidAndEmpty(){
    for(int variant=0;variant<12;++variant){
        setup();auto data=directionBlock();
        if(variant==0)data.inputs["inDD"].payload=nullptr;
        if(variant==1)data.inputs["inDD"].type=999;
        if(variant==2)inputPoints.topology=false;
        if(variant==3)data.inputs["aDirectionType"].number=4;
        if(variant==4)data.inputs["baseVector"].vector[0]=INFINITY;
        if(variant==5)data.inputs["aScale"].number=NAN;
        if(variant==6)data.inputs["aTargetDPCalcType"].number=3;
        if(variant==7)data.inputs["inTargetDP"].payload=nullptr;
        if(variant==8)targetPoints.topology=false;
        if(variant==9){data.inputs["aTargetDPCalcType"].number=1;targetPoints.attributes.clear();}
        if(variant==10){data.inputs["aDirectionType"].number=2;rejectNative=true;}
        if(variant==11){data.inputs["aUseDistance"].number=true;data.inputs["aDistance"].number=-1;}
        assert(!direction(data)&&publications==0&&geometryCopies==0);
    }
    for(int kind=0;kind<3;++kind){
        setup();inputPoints=DPData();inputCurves=DCurveData();inputMeshes=DMeshData();
        auto data=directionBlock(kind,3);data.inputs["aTargetDPCalcType"].number=9;
        assert(direction(data)&&!called("input inTargetDP")&&!called("input aTargetDPCalcType"));
        resetCompute();data=searchBlock(kind);assert(search(data));
    }
    for(int variant=0;variant<5;++variant){
        setup();auto data=searchBlock();
        if(variant==0)data.inputs["aRadius"].number=0;
        if(variant==1)data.inputs["aMaxCount"].number=0;
        if(variant==2)data.inputs["aTargetDP"].payload=nullptr;
        if(variant==3)badNeighbor=true;
        if(variant==4){badNeighbor=true;data.inputs["aDetectNearestPoint"].number=1;}
        assert(!search(data)&&publications==0&&geometryCopies==0);
    }
}
std::string signature(const DPAttrList& attrs){
    std::ostringstream out;
    for(const auto& a:attrs){out<<a.first<<':'<<a.second->typeName()<<':'<<a.second->attrSize();
        if(auto v=std::dynamic_pointer_cast<const DPVec3fAttr>(a.second))for(const auto& p:v->value())out<<p[0]<<','<<p[1]<<','<<p[2];
        if(auto v=std::dynamic_pointer_cast<const DPIntArrayAttr>(a.second))for(const auto& row:v->value())for(auto i:row)out<<i<<',';
    }return out.str();
}
template<typename Data>std::string primitiveSignature(const Data& data){
    std::ostringstream s;s<<signature(data.attributes);
    for(const auto& p:data.primitives){
        s<<signature(p->attributes);
        for(const auto& v:p->points)for(unsigned axis=0;axis<3;++axis)s<<v[axis]<<',';
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)s<<p->matrix[row][col]<<',';
    }return s.str();
}
std::string pointSignature(const DPData& data){
    std::ostringstream s;s<<signature(data.attributes);
    for(const auto& v:data.points)for(unsigned axis=0;axis<3;++axis)s<<v[axis]<<',';
    return s.str();
}
std::string inputsSignature(){return pointSignature(inputPoints)+pointSignature(targetPoints)+primitiveSignature(inputCurves)+primitiveSignature(inputMeshes);}
std::string outputsSignature(){return pointSignature(outputPoints)+primitiveSignature(outputCurves)+primitiveSignature(outputMeshes);}
template<typename Compute>int failures(MDataBlock data,Compute compute){
    resetCompute();const auto before=inputsSignature();assert(compute(data));const auto trace=operations;
    const auto expected=outputsSignature();int count=0;
    for(int index=0;index<int(trace.size());++index){
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            resetCompute(index);injectedStatus=code;assert(!compute(data)&&callCount==index+1);
            assert(inputsSignature()==before&&outputsSignature()==expected);
            if(trace[index]!="output.clean")assert(publications==0);++count;
        }
        resetCompute();attributeThrowAt=index;assert(compute(data)==MS::kFailure&&callCount==index+1);
        assert(inputsSignature()==before&&outputsSignature()==expected);
        if(trace[index]!="output.clean")assert(publications==0);++count;
    }
    resetCompute();assert(compute(data));return count;
}
int main(){
    testDirection();testBudget();testSearch();testInvalidAndEmpty();int injected=0;
    for(int kind=0;kind<3;++kind){
        for(int mode:{0,1,2,3})for(int cls:{0,1}){setup();injected+=failures(directionBlock(kind,mode,1,cls),direction);}
        for(bool nearest:{false,true}){setup();injected+=failures(searchBlock(kind,nearest),search);}
        for(bool directionNode:{false,true})for(bool reject:{false,true}){
            setup();auto data=directionNode?directionBlock(kind):searchBlock(kind);
            const auto before=inputsSignature(),output=outputsSignature();
            rejectFinal=reject;nullCreated=!reject;
            assert(!(directionNode?direction(data):search(data))&&publications==0);
            assert(inputsSignature()==before&&outputsSignature()==output);
        }
    }
    setup();auto data=directionBlock();assert(DPDirection().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);
    assert(DPSearchPoint().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
    std::cout<<"Direction/search: domain, workload, selection and "<<injected<<" compute failure injections passed\n";
}

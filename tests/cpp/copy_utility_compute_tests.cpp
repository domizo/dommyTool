#include "copy_utility_compute_doubles.h"
#include "copy_utility_compute.inc"

template<typename T>
void field(DPData& data,const std::string& name,const std::vector<T>& values,const T& defaultValue=T()) {
    data.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values,defaultValue);
}
template<typename T>std::vector<T> values(const DPData& data,const std::string& name) {
    const auto attr=std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(data.attributes.at(name));
    assert(attr);return attr->value();
}
void pointEqual(const DPoint& a,const DPoint& b) {
    for(unsigned axis=0;axis<3;++axis)assert(std::abs(a[axis]-b[axis])<1e-4f);
}
void matrixEqual(const MMatrix& a,const MMatrix& b) {
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)assert(std::abs(a[r][c]-b[r][c])<1e-12);
}
bool called(const char* operationName) {
    return std::find(operations.begin(),operations.end(),operationName)!=operations.end();
}
void setup() {
    resetCompute();inputPoints=DPData();targetPoints=DPData();outputPoints=DPData();
    inputPoints.points={{1,2,3},{4,5,6}};targetPoints.points={{10,20,30},{-5,-6,-7}};
    field<int32_t>(inputPoints,"id",{8,9},4);
    inputPoints.groups["group"]=std::make_shared<Group>();inputPoints.groups["group"]->ids={true,false};
    inputCurves=DCurveData();inputMeshes=DMeshData();
    for(unsigned i=0;i<2;++i) {
        auto p=std::make_shared<Primitive>();p->matrix[3][0]=i*10;
        p->points[0][0]+=i;p->tag=10+i;p->member=i==0;
        inputCurves.primitives.push_back(p);inputMeshes.primitives.push_back(std::make_shared<Primitive>(*p));
    }
}
MDataBlock duplicatorBlock() {
    MDataBlock data;
    data.inputs["inDP"].payload=&inputPoints;data.inputs["inDP"].type=DPData::id;
    data.inputs["aTargetDP"].payload=&targetPoints;data.inputs["aTargetDP"].type=DPData::id;
    return data;
}
MDataBlock transformBlock(int kind=DPData::id,int count=3,bool freeze=false,int order=0) {
    MDataBlock data;
    data.inputs["inDD"].payload=kind==DPData::id?static_cast<Payload*>(&inputPoints):
        kind==DCurveData::id?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    data.inputs["inDD"].type=kind;data.inputs["aCount"].number=count;
    data.inputs["aTranslate"].vector=MVector(2,3,4);
    data.inputs["aRotate"].vector=MVector(20,30,40);
    data.inputs["aScale"].vector=MVector(1.2,.8,1.1);
    data.inputs["aPivot"].vector=MVector(3,-2,1);
    data.inputs["aUniformScale"].number=.75;data.inputs["aFreezeTransform"].number=freeze;
    data.inputs["aRotateOrder"].number=order;
    return data;
}
MStatus duplicate(MDataBlock& data) {return DPDuplicator().compute(MPlug(DPDuplicator::outDP),data);}
MStatus transform(MDataBlock& data) {return DPCopyTransform().compute(MPlug(DPCopyTransform::outDD),data);}
MDataBlock blendBlock() {
    MDataBlock data=duplicatorBlock();data.inputs["aWeight"].number=1;
    Input element;
    element.children["aBlend"].number=.5;
    element.children["aTargetDP"].payload=&targetPoints;
    element.children["aTargetDP"].type=DPData::id;
    element.children["aBlendAttribute"].text="weight";
    data.elements.push_back(element);return data;
}
MStatus blend(MDataBlock& data) {return DPBlend().compute(MPlug(DPBlend::outDP),data);}

static int blendFieldCopies=0;
struct BlendObservedField : DPIntAttr {
    using DPIntAttr::DPIntAttr;
    DPAttributeData::Ptr copy()const override{++blendFieldCopies;return std::make_shared<BlendObservedField>(*this);}
};
void testBlendOwnership(){
    for(bool enabled:{false,true}){
        setup();blendFieldCopies=0;
        inputPoints.attributes["id"]=std::make_shared<BlendObservedField>(std::vector<int32_t>{8,9},4);
        auto data=blendBlock();data.inputs["aWeight"].number=enabled;
        assert(blend(data)&&geometryCopies==1&&blendFieldCopies==1&&positionCopies==0);
        assert(std::count(operations.begin(),operations.end(),"bounds.update")==1);
        assert(!called("point.copy")&&!called("point.replace"));
        assert(outputPoints.attributes.at("id")!=inputPoints.attributes.at("id"));
        assert(outputPoints.groups.at("group")!=inputPoints.groups.at("group"));
        const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("id"));
        assert(retained&&retained->defaultValue()==4);
        assert(values<int32_t>(outputPoints,"id")==std::vector<int32_t>({8,9}));
        assert(outputPoints.groups.at("group")->ids==inputPoints.groups.at("group")->ids);
    }
    setup();inputPoints.points.clear();inputPoints.attributes["id"]=std::make_shared<DPIntAttr>(std::vector<int32_t>(),4);
    inputPoints.groups["group"]->ids.clear();auto data=blendBlock();assert(blend(data));
    const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("id"));
    assert(retained&&retained->defaultValue()==4&&outputPoints.groups.count("group"));
}
void reducedBlendBudgets(){
    for(int variant=0;variant<4;++variant){
        setup();auto data=blendBlock();
        if(variant==0)field<std::string>(inputPoints,"large",std::vector<std::string>(2,std::string(256,'x')));
        if(variant==1)field<DIntArray>(inputPoints,"large",std::vector<DIntArray>(2,DIntArray(100,1)));
        if(variant==2)for(int i=0;i<40;++i)field<float>(inputPoints,"field"+std::to_string(i),{1,2});
        if(variant==3)for(int i=0;i<10;++i){auto group=std::make_shared<Group>();group->ids={true,false};inputPoints.groups[std::string(80,'g')+std::to_string(i)]=group;}
        const auto first=inputPoints.points[0];
        assert(!blend(data)&&!called("output.create")&&geometryCopies==0&&positionViews==0);
        pointEqual(inputPoints.points[0],first);assert(publications==0);
    }
    // Source/staging fits, but retained active target descriptors do not.
    setup();auto data=blendBlock();const Input element=data.elements[0];data.elements.assign(20,element);
    assert(!blend(data)&&!called("output.create")&&positionViews==0);
}

void testBlend() {
    setup();auto data=blendBlock();assert(blend(data)&&publications==1);
    pointEqual(outputPoints.points[0],DPoint(5.5,11,16.5));
    assert(values<int32_t>(outputPoints,"id")==std::vector<int32_t>({8,9}));
    assert(outputPoints.groups["group"]->ids==inputPoints.groups["group"]->ids);
    // Physical element order, sequential blending, and wrapped target points.
    setup();data=blendBlock();targetPoints.points.resize(1);data.elements.push_back(data.elements[0]);
    assert(blend(data));pointEqual(outputPoints.points[1],DPoint(8.5,16.25,24));
    setup();data=blendBlock();data.elements[0].children["aBlend"].number=-1;
    assert(blend(data));pointEqual(outputPoints.points[0],DPoint(-8,-16,-24));
    setup();data=blendBlock();data.elements[0].children["aTargetDP"].payload=&inputPoints;
    assert(blend(data));pointEqual(outputPoints.points[0],inputPoints.points[0]);
    for(int type=0;type<7;++type) {
        setup();data=blendBlock();
        if(type==0)field<int32_t>(targetPoints,"weight",{1,0});
        if(type==1)field<uint32_t>(targetPoints,"weight",{1,0});
        if(type==2)field<int64_t>(targetPoints,"weight",{1,0});
        if(type==3)field<float>(targetPoints,"weight",{1,0});
        if(type==4)field<double>(targetPoints,"weight",{1,0});
        if(type==5)field<std::string>(targetPoints,"weight",{"a","b"});
        if(type==6)data.elements[0].children["aBlendAttribute"].text="";
        assert(blend(data));
        pointEqual(outputPoints.points[0],type<5?targetPoints.points[0]:DPoint(5.5,11,16.5));
        pointEqual(outputPoints.points[1],type<5?inputPoints.points[1]:DPoint(-.5,-.5,-.5));
    }
    const float maximum=std::numeric_limits<float>::max();
    for(float weight:{0.0f,.5f,1.0f}) {
        setup();data=blendBlock();inputPoints.points[0][0]=maximum;targetPoints.points[0][0]=-maximum;
        field<float>(targetPoints,"weight",{weight,weight});
        assert(blend(data));assert(outputPoints.points[0][0]==(weight==0?maximum:weight==1?-maximum:0));
    }
    for(int variant=0;variant<10;++variant) {
        setup();data=blendBlock();
        if(variant==0)data.elements[0].children["aTargetDP"].payload=nullptr;
        if(variant==1)data.elements[0].children["aTargetDP"].type=DMeshData::id;
        if(variant==2)targetPoints.topology=false;
        if(variant==3)targetPoints.points.clear();
        if(variant==4)data.elements[0].children["aBlend"].number=NAN;
        if(variant==5)data.inputs["aWeight"].number=NAN;
        if(variant==6){targetPoints.points.resize(1);field<float>(targetPoints,"weight",{1});}
        if(variant==7)field<double>(targetPoints,"weight",{1e300,1});
        if(variant==8){data.inputs["aWeight"].number=maximum;data.elements[0].children["aBlend"].number=maximum;}
        if(variant==9){inputPoints.points[0][0]=maximum;targetPoints.points[0][0]=-maximum;data.elements[0].children["aBlend"].number=-1;}
        assert(!blend(data)&&publications==0);
    }
    for(int disabled=0;disabled<3;++disabled) {
        setup();data=blendBlock();data.elements[0].children["aTargetDP"].payload=nullptr;
        if(disabled==0)inputPoints=DPData();
        if(disabled==1)data.inputs["aWeight"].number=-1;
        if(disabled==2)data.elements[0].children["aBlend"].number=0;
        assert(blend(data)&&publications==1);
    }
    setup();data=blendBlock();data.reportedCount=1000001;
    assert(!blend(data)&&geometryCopies==0&&!called("output.create"));
    setup();data=blendBlock();inputPoints=DPData();inputPoints.points.resize(2000000,DPoint(0,0,0));
    data.elements.resize(20,data.elements[0]);
    assert(!blend(data)&&geometryCopies==0&&!called("output.create"));
    setup();data=blendBlock();
    field<std::string>(targetPoints,"payload",{"",""},std::string(100000,'x'));
    data.elements.resize(1001,data.elements[0]);
    assert(!blend(data)&&geometryCopies==0&&!called("output.create"));
    setup();data=blendBlock();
    assert(DPBlend().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
}

void testPrimitiveBudget() {
    for(int kind:{DCurveData::id,DMeshData::id}) {
        setup();auto data=transformBlock(kind,1000);
        dommy::mesh::MeshMergePlan inputPlan,largePlan;
        if(kind==DCurveData::id)assert(dommy::curve::buildCurveCopyPlan(inputCurves,20000U,largePlan));
        else {
            assert(dommy::mesh::appendMeshRangePlan(inputMeshes.beginMeshes(),inputMeshes.endMeshes(),inputPlan));
            assert(dommy::mesh::repeatMeshMergePlan(inputPlan,20000U,largePlan));
            assert(dommy::mesh::validMeshOperationStorage(largePlan));
            assert(dommy::mesh::validateRepeatedMeshMetadata(inputMeshes,20000U,largePlan));
        }
        data.inputs["aRotate"].vector=MVector();data.inputs["aTranslate"].vector=MVector();
        data.inputs["aScale"].vector=MVector(1,1,1);data.inputs["aUniformScale"].number=1;
        assert(transform(data)&&publications==1&&geometryCopies==1);
        assert(!called("primitive.expand"));
        setup();data=transformBlock(kind,1000001);
        assert(!transform(data)&&geometryCopies==0&&!called("output.create"));
        setup();data=transformBlock(kind,2);
        if(kind==DCurveData::id){inputCurves.primitives.resize(1);inputCurves.primitives[0]->reportedPoints=10000001;}
        else {inputMeshes.primitives.resize(1);inputMeshes.primitives[0]->reportedPoints=5000001;}
        assert(!transform(data)&&geometryCopies==0&&!called("output.create"));
    }
    for(int variant=0;variant<3;++variant) {
        setup();auto data=transformBlock(DMeshData::id,2);inputMeshes.primitives.resize(1);
        if(variant==0)inputMeshes.primitives[0]->faces=1000001;
        if(variant==1)inputMeshes.primitives[0]->connections=50000001;
        if(variant==2)inputMeshes.primitives[0]->uvs=50000001;
        assert(!transform(data)&&geometryCopies==0&&!called("output.create"));
    }
    setup();dommy::attribute::ValidationBudget base,extra;
    assert(dommy::mesh::addMeshMetadataCost(inputMeshes.beginMeshes(),inputMeshes.endMeshes(),base));
    auto& primitive=*inputMeshes.primitives[0];
    primitive.componentAttributes[D_COMP_PER_VTX]["v"]=std::make_shared<TypedDPAttributeData<Imath::V3f>>(
        std::vector<Imath::V3f>(3,Imath::V3f(1,2,3)),Imath::V3f());
    dommy::attribute::ValidationBudget withAttribute;
    assert(dommy::mesh::addMeshMetadataCost(inputMeshes.beginMeshes(),inputMeshes.endMeshes(),withAttribute)&&withAttribute.workItems>base.workItems);
    primitive.componentGroups[D_COMP_EDGE]["edges"]=std::make_shared<Group>();
    primitive.componentGroups[D_COMP_EDGE]["edges"]->ids={true,true,true};
    assert(dommy::mesh::addMeshMetadataCost(inputMeshes.beginMeshes(),inputMeshes.endMeshes(),extra)&&extra.workItems-withAttribute.workItems>=3U);
    for(int variant=0;variant<5;++variant) {
        setup();auto data=transformBlock(DMeshData::id,100);
        auto value=std::make_shared<TypedDPAttributeData<std::string>>(
            std::vector<std::string>(1,std::string(600000,'x')),std::string(600000,'x'));
        if(variant==0)inputMeshes.primitives[0]->attributes["s"]=value;
        if(variant==1)inputMeshes.primitives[0]->componentAttributes[D_COMP_FACE]["s"]=value;
        if(variant==2)inputMeshes.primitives[0]->componentAttributes[D_COMP_PER_VTX]["s"]=
            std::make_shared<TypedDPAttributeData<std::string>>(std::vector<std::string>(3,std::string(600000,'x')),std::string(600000,'x'));
        if(variant==3)inputMeshes.attributes["s"]=std::make_shared<TypedDPAttributeData<std::string>>(
            std::vector<std::string>(2,std::string(600000,'x')),std::string(12000,'x'));
        if(variant==4)inputMeshes.primitives[0]->attributes["a"]=std::make_shared<TypedDPAttributeData<DIntArray>>(
            std::vector<DIntArray>(1,DIntArray(600000,1)),DIntArray(600000,1));
        dommy::mesh::MeshMergePlan sourcePlan,outputPlan;
        assert(dommy::mesh::appendMeshRangePlan(inputMeshes.beginMeshes(),inputMeshes.endMeshes(),sourcePlan));
        assert(dommy::mesh::repeatMeshMergePlan(sourcePlan,100,outputPlan));
        assert(!dommy::mesh::validateRepeatedMeshMetadata(inputMeshes,100,outputPlan));
        assert(!transform(data)&&geometryCopies==0&&!called("output.create"));
    }
}

// Frozen pre-refactor transform call sequence/formulas, exercised against the
// same deterministic host boundary for all six rotation orders.
MMatrix legacyMatrix(const CopyTransformRequest& request,std::size_t i,MVector& prescale) {
    MTransformationMatrix newMat;
    const MVector movVec=request.translation*static_cast<double>(i);
    newMat.setRotatePivot(movVec+request.pivot,MSpace::kObject,false);
    newMat.setScalePivot(movVec+request.pivot,MSpace::kObject,false);
    prescale=MVector(prescale[0]*request.scale[0],prescale[1]*request.scale[1],prescale[2]*request.scale[2]);
    const double s[3]={prescale.x,prescale.y,prescale.z};newMat.addScale(s,MSpace::kObject);
    const double r[3]={dommy::math::degreesToRadians(request.rotation[0])*static_cast<double>(i),
        dommy::math::degreesToRadians(request.rotation[1])*static_cast<double>(i),
        dommy::math::degreesToRadians(request.rotation[2])*static_cast<double>(i)};
    newMat.addRotation(r,request.order,MSpace::kObject);newMat.addTranslation(movVec,MSpace::kObject);
    return newMat.asMatrix();
}
DPoint expectedPoint(const DPoint& p,const MMatrix& matrix) {
    DPoint result;
    for(unsigned a=0;a<3;++a)result[a]=static_cast<float>(
        double(p[0])*matrix[0][a]+double(p[1])*matrix[1][a]+double(p[2])*matrix[2][a]+matrix[3][a]);
    return result;
}
void testDuplicator() {
    setup();auto data=duplicatorBlock();assert(duplicate(data)&&publications==1);
    assert(outputPoints.points.size()==4);
    for(unsigned target=0;target<2;++target)for(unsigned p=0;p<2;++p)
        pointEqual(outputPoints.points[target*2+p],inputPoints.points[p]+targetPoints.points[target]);
    assert(values<int32_t>(outputPoints,"id")==std::vector<int32_t>({8,9,8,9}));
    assert(outputPoints.groups["group"]->ids==std::vector<bool>({true,false,true,false}));
    assert(values<int32_t>(inputPoints,"id")==std::vector<int32_t>({8,9}));
    setup();data=duplicatorBlock();inputPoints=DPData();data.inputs["aTargetDP"].payload=nullptr;
    assert(duplicate(data)&&outputPoints.points.empty()&&!called("input aTargetDP"));
    setup();data=duplicatorBlock();targetPoints=DPData();assert(duplicate(data)&&outputPoints.points.empty());
    for(int variant=0;variant<5;++variant) {
        setup();data=duplicatorBlock();
        if(variant==0)data.inputs["aTargetDP"].payload=nullptr;
        if(variant==1)data.inputs["aTargetDP"].type=DCurveData::id;
        if(variant==2)targetPoints.topology=false;
        if(variant==3)targetPoints.points[0][0]=NAN;
        if(variant==4){inputPoints.points[0][0]=std::numeric_limits<float>::max();targetPoints.points[0][0]=inputPoints.points[0][0];}
        assert(!duplicate(data)&&publications==0);
    }
    setup();data=duplicatorBlock();targetPoints.points.resize(20000,DPoint(0,0,0));
    assert(duplicate(data)&&publications==1&&geometryCopies==1&&outputPoints.points.size()==40000U);
    assert(!called("point.merge")&&!called("point.replace"));
    setup();data=duplicatorBlock();targetPoints.points.resize(20000,DPoint(0,0,0));
    field<std::string>(inputPoints,"large",{std::string(10000,'x'),std::string(10000,'y')});
    assert(!duplicate(data)&&!called("output.create")&&geometryCopies==0);
    setup();data=transformBlock(DPData::id,20000);
    data.inputs["aTranslate"].vector=MVector();data.inputs["aRotate"].vector=MVector();
    data.inputs["aScale"].vector=MVector(1,1,1);data.inputs["aUniformScale"].number=1;
    assert(transform(data)&&publications==1&&geometryCopies==1&&outputPoints.points.size()==40000U);
    assert(!called("point.merge")&&!called("point.replace"));
}
void testTransforms() {
    for(int kind:{DPData::id,DCurveData::id,DMeshData::id})
    for(bool freeze:{false,true})for(int count:{0,1,3})for(int order=0;order<6;++order) {
        setup();auto data=transformBlock(kind,count,freeze,order);
        CopyTransformRequest request;assert(readCopyTransformRequest(data,request));
        assert(request.order==static_cast<MTransformationMatrix::RotationOrder>(order));resetCompute();
        assert(transform(data)&&publications==1);
        const unsigned effective=count==0?1:unsigned(count);
        MVector scale(1,1,1);
        if(kind==DPData::id) {
            assert(outputPoints.points.size()==2*effective);
            for(unsigned step=0;step<effective;++step) {
                const auto matrix=step?legacyMatrix(request,step,scale):MMatrix();
                for(unsigned p=0;p<2;++p)pointEqual(outputPoints.points[step*2+p],expectedPoint(inputPoints.points[p],matrix));
            }
            auto ids=values<int32_t>(outputPoints,"id");assert(ids.size()==2*effective);
            for(unsigned i=0;i<ids.size();++i)assert(ids[i]==8+int(i%2));
        } else {
            const auto& output=kind==DCurveData::id?outputCurves.primitives:outputMeshes.primitives;
            const auto& input=kind==DCurveData::id?inputCurves.primitives:inputMeshes.primitives;
            assert(output.size()==2*effective);
            for(unsigned step=0;step<effective;++step) {
                const auto delta=step?legacyMatrix(request,step,scale):MMatrix();
                for(unsigned i=0;i<2;++i) {
                    const auto& p=*output[step*2+i];const auto matrix=input[i]->matrix*delta;
                    assert(p.tag==input[i]->tag&&p.member==input[i]->member);
                    matrixEqual(p.matrix,step&&freeze?MMatrix():matrix);
                    pointEqual(p.points[0],step&&freeze?expectedPoint(input[i]->points[0],matrix):input[i]->points[0]);
                }
            }
        }
        pointEqual(inputPoints.points[0],DPoint(1,2,3));
        pointEqual(inputCurves.primitives[0]->points[0],DPoint(1,2,3));
        assert(inputCurves.primitives[0]->matrix[3][0]==0);
    }
    for(int kind:{DPData::id,DCurveData::id,DMeshData::id}) {
        setup();auto data=transformBlock(kind);inputPoints=DPData();inputCurves=DCurveData();inputMeshes=DMeshData();
        assert(transform(data)&&publications==1);
    }
}
void testFrozenMeshNormals() {
    for(int variant=0;variant<5;++variant) {
        setup();auto data=transformBlock(DMeshData::id,2,true);
        data.inputs["aTranslate"].vector=MVector(0,0,0);
        data.inputs["aRotate"].vector=MVector(0,0,0);
        data.inputs["aScale"].vector=MVector(1,1,1);
        data.inputs["aUniformScale"].number=1;
        auto source=inputMeshes.primitives[0];
        MMatrix matrix;
        if(variant==0){matrix[0][0]=0;matrix[0][1]=1;matrix[1][0]=-1;matrix[1][1]=0;}
        if(variant==1){matrix[0][0]=2;matrix[1][1]=.5;}
        if(variant==2)matrix[0][1]=.75;
        if(variant==3)matrix[0][0]=-1;
        if(variant==4)matrix[0][0]=0;
        source->matrix=matrix;
        const DPointList normals{{1,1,0},{0,0,0},{0,0,1}};
        source->componentAttributes[D_COMP_PER_VTX]["__dommy_corner_normal"]=
            std::make_shared<DPVec3fAttr>(normals,DPoint(0,0,0));
        if(variant==4) {
            assert(!transform(data)&&publications==0);
        } else {
            assert(transform(data)&&publications==1);
            const auto frozen=outputMeshes.primitives[2];
            auto stored=std::dynamic_pointer_cast<DPVec3fAttr>(
                frozen->componentAttributes.at(D_COMP_PER_VTX).at("__dommy_corner_normal"));
            matrixEqual(frozen->matrix,MMatrix::identity);
            for(std::size_t i=0;i<normals.size();++i) {
                std::array<float,3> expected;
                assert(dommy::numeric::checkedTransformNormal(normals[i],matrix,expected));
                pointEqual(stored->value()[i],DPoint(expected[0],expected[1],expected[2]));
            }
            const auto original=std::dynamic_pointer_cast<DPVec3fAttr>(
                outputMeshes.primitives[0]->componentAttributes.at(D_COMP_PER_VTX).at("__dommy_corner_normal"));
            for(std::size_t i=0;i<normals.size();++i)pointEqual(original->value()[i],normals[i]);
            assert(stored!=original);
        }
        const auto unchanged=std::dynamic_pointer_cast<DPVec3fAttr>(
            source->componentAttributes.at(D_COMP_PER_VTX).at("__dommy_corner_normal"));
        for(std::size_t i=0;i<normals.size();++i)pointEqual(unchanged->value()[i],normals[i]);
        matrixEqual(source->matrix,matrix);
    }
}
void testInvalid() {
    for(int kind:{DPData::id,DCurveData::id,DMeshData::id})for(int variant=0;variant<12;++variant) {
        setup();auto data=transformBlock(kind,3,true);
        if(variant==0)data.inputs["inDD"].payload=nullptr;
        if(variant==1)data.inputs["inDD"].type=999;
        if(variant==2)data.inputs["aCount"].number=-1;
        if(variant==3)data.inputs["aCount"].number=1000001;
        if(variant==4)data.inputs["aRotateOrder"].number=6;
        if(variant==5)data.inputs["aTranslate"].vector[0]=NAN;
        if(variant==6)data.inputs["aUniformScale"].number=INFINITY;
        if(variant==7){data.inputs["aScale"].vector[0]=1e300;data.inputs["aUniformScale"].number=1e100;}
        if(variant==8)data.inputs["aPivot"].vector[0]=NAN;
        if(variant==9){inputPoints.topology=false;inputCurves.topology=false;inputMeshes.topology=false;}
        if(variant==10)badMatrix=true;
        if(variant==11)rejectFinal=true;
        assert(!transform(data)&&publications==0);
    }
    setup();auto data=transformBlock(DPData::id,20000);
    field<std::string>(inputPoints,"large",{std::string(10000,'x'),std::string(10000,'y')});
    assert(!transform(data)&&!called("output.create")&&geometryCopies==0);
    setup();data=transformBlock();nullCreated=true;assert(!transform(data)&&publications==0);
    setup();data=transformBlock(DCurveData::id);data.inputs["inDD"].payload=&inputPoints;
    assert(!transform(data)&&publications==0);
    setup();data=transformBlock(DMeshData::id);inputMeshes.primitives[0]->faces=-1;
    assert(!transform(data)&&publications==0);
    setup();data=transformBlock();
    assert(DPCopyTransform().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
    assert(DPDuplicator().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
}
void testMatrixBoundary() {
    setup();CopyTransformRequest request;auto data=transformBlock();assert(readCopyTransformRequest(data,request));
    for(int variant=0;variant<6;++variant) {
        resetCompute();
        CopyTransformRequest bad=request;MVector scale(1,1,1);MMatrix output;output[3][0]=99;
        if(variant==0)bad.translation[0]=std::numeric_limits<double>::max();
        if(variant==1){bad.translation[0]=8e307;bad.pivot[0]=1e308;}
        if(variant==2){scale[0]=1e308;bad.scale[0]=1e308;}
        if(variant==3)bad.rotation[0]=std::numeric_limits<double>::max();
        if(variant==4)badMatrix=true;
        if(variant==5)bad.scale[0]=NAN;
        const MVector before=scale;
        assert(!buildCopyTransform(bad,variant==3?1000U:2U,scale,output));
        if(variant!=4)assert(operations.empty());
        for(unsigned i=0;i<3;++i)assert(scale[i]==before[i]);
        assert(output[3][0]==99);resetCompute();
    }
    DPoint huge(std::numeric_limits<float>::max(),0,0);MMatrix scale;scale[0][0]=2;
    assert(!transformCopyPoint(huge,scale));
    assert(huge[0]==std::numeric_limits<float>::max());
}
void testBudget() {
    setup();std::size_t base=0,withGroup=0,withVector=0;
    DPData point;point.points={{0,0,0}};
    assert(dommy::point::buildPointCopyBudget(point,100,base));
    point.groups["g"]=std::make_shared<Group>();point.groups["g"]->ids={true};
    assert(dommy::point::buildPointCopyBudget(point,100,withGroup)&&withGroup-base>=200U);
    field<Imath::V3f>(point,"v",{Imath::V3f(1,2,3)});
    assert(dommy::point::buildPointCopyBudget(point,100,withVector)&&withVector-withGroup>=500U);
    std::size_t work=777;
    assert(dommy::point::buildPointCopyBudget(point,20000,work));
    work=777;
    assert(!dommy::point::buildPointCopyBudget(point,std::numeric_limits<std::size_t>::max(),work)&&work==777);
    // A complete repeated field needs no missing-row default expansion.
    // Its default and name are retained once, not per copy.
    point.attributes.clear();field<std::string>(point,"s",{""},std::string(12000,'x'));
    assert(dommy::point::buildPointCopyBudget(point,100,work));
    point.attributes.clear();field<DIntArray>(point,"a",{DIntArray()},DIntArray(50000,1));
    assert(dommy::point::buildPointCopyBudget(point,100,work));
    point.attributes.clear();point.groups.clear();
    field<int32_t>(point,std::string(1000000,'x'),{1});
    assert(dommy::point::buildPointCopyBudget(point,100,work));
    point.attributes.clear();field<std::string>(point,"s",{std::string(1000000,'x')});work=777;
    assert(!dommy::point::buildPointCopyBudget(point,100,work)&&work==777);
    point.attributes.clear();field<DIntArray>(point,"a",{DIntArray(1000000,1)});
    assert(!dommy::point::buildPointCopyBudget(point,100,work)&&work==777);
}
std::string payloadSignature(const DPData& points,const DCurveData& curves,const DMeshData& meshes) {
    std::ostringstream out;
    for(const auto& p:points.points)for(unsigned axis=0;axis<3;++axis)out<<p[axis]<<',';
    for(const auto& a:points.attributes) {
        out<<a.first<<':'<<a.second->typeName()<<':';
        const auto ids=std::dynamic_pointer_cast<const TypedDPAttributeData<int32_t>>(a.second);
        if(ids)for(auto id:ids->value())out<<id<<',';
    }
    for(const auto& g:points.groups) {out<<g.first;for(bool value:g.second->ids)out<<value;}
    for(const auto* prims:{&curves.primitives,&meshes.primitives})for(const auto& p:*prims) {
        out<<p->tag<<':'<<p->member;
        for(const auto& point:p->points)for(unsigned a=0;a<3;++a)out<<point[a]<<',';
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)out<<p->matrix[r][c]<<',';
    }
    return out.str();
}
template<typename Compute>int injectFailures(MDataBlock data,Compute compute) {
    resetCompute();assert(compute(data));const auto trace=operations;int cases=0;
    const auto source=payloadSignature(inputPoints,inputCurves,inputMeshes);
    const auto output=payloadSignature(outputPoints,outputCurves,outputMeshes);
    const auto unchanged=[&](){
        assert(payloadSignature(inputPoints,inputCurves,inputMeshes)==source);
        assert(payloadSignature(outputPoints,outputCurves,outputMeshes)==output);
    };
    for(int index=0;index<int(trace.size());++index) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            resetCompute(index);injectedStatus=code;
            const auto result=compute(data);assert(!result&&callCount==index+1);
            const std::set<std::string> collapsed={"point.copy","bounds.update","matrix.result",
                "primitive.matrix","point.replace","point.merge","primitive.expand"};
            assert(result==(collapsed.count(trace[index])?MS::kFailure:code));
            if(trace[index]!="output.clean")assert(publications==0);
            unchanged();
            ++cases;
        }
        resetCompute();attributeThrowAt=index;
        assert(!compute(data)&&callCount==index+1);
        if(trace[index]!="output.clean")assert(publications==0);
        unchanged();
        ++cases;
    }
    resetCompute();assert(compute(data));return cases;
}
int main(int argc,char**) {
    if(argc>1){reducedBlendBudgets();return 0;}
    testDuplicator();testTransforms();testFrozenMeshNormals();testInvalid();testMatrixBoundary();testBudget();
    testBlend();testBlendOwnership();testPrimitiveBudget();
    setup();int failures=injectFailures(duplicatorBlock(),duplicate);
    setup();failures+=injectFailures(blendBlock(),blend);
    for(int kind:{DPData::id,DCurveData::id,DMeshData::id})for(bool freeze:{false,true}) {
        setup();failures+=injectFailures(transformBlock(kind,3,freeze),transform);
    }
    std::cout<<"Copy utilities: point/curve/mesh contracts, bounded copies, "<<failures
             <<" compute status/exception injections passed\n";
}

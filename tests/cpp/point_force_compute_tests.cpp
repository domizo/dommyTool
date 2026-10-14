#include "point_force_compute_doubles.h"
#include "point_force_compute.inc"
#include <cfenv>

template<typename Data,typename T>void field(Data& data,const char* name,const std::vector<T>& values){
    data.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values);
}
static DCurveData targetCurves;
void setup(){
    resetCompute();nativeQueries=0;rejectNative=false;
    inputPoints=DPData();targetPoints=DPData();outputPoints=DPData();
    inputCurves=DCurveData();inputMeshes=DMeshData();targetCurves=DCurveData();
    outputCurves=DCurveData();outputMeshes=DMeshData();
    inputPoints.points={{0.5f,0,0},{2,0,0}};targetPoints.points={{0,0,0},{1,0,0}};
    field(inputPoints,"keep",std::vector<int32_t>{7,8});
    for(int i=0;i<2;++i){
        auto p=std::make_shared<Primitive>();p->points=inputPoints.points;
        p->matrix[3][0]=10+i;field(*p,"keep",std::vector<int32_t>{7,8});
        inputCurves.primitives.push_back(std::make_shared<Primitive>(*p));
        inputMeshes.primitives.push_back(std::make_shared<Primitive>(*p));
        targetCurves.primitives.push_back(p);
    }
    field(inputCurves,"keep",std::vector<int32_t>{9,10});
    field(inputMeshes,"keep",std::vector<int32_t>{9,10});
    outputPoints.points={{-99,0,0}};
}
MDataBlock block(int kind=0,int mode=0){
    MDataBlock data;
    Input source;source.payload=kind==0?static_cast<Payload*>(&inputPoints):
        kind==1?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputMeshes);
    source.type=source.payload->type();data.inputs["inDD"]=data.inputs["inDP"]=source;
    Input target;target.payload=&targetPoints;target.type=DPData::id;
    data.inputs["inTargetDP"]=data.inputs["aTargetDP"]=target;
    target.payload=&targetCurves;target.type=DCurveData::id;data.inputs["inTargetDC"]=target;
    data.inputs["aAvoidType"].number=data.inputs["aMagnetType"].number=data.inputs["aGoalType"].number=mode;
    data.inputs["aRadius"].number=2;data.inputs["aIterate"].number=4;
    data.inputs["aRadiusAtribute"].text="rad";data.inputs["aWeightAttributeName"].text="weight";
    data.inputs["aWeight"].number=0.5;data.inputs["aMagnitude"].number=1;
    data.inputs["aClampRange"].vector=Imath::V3d(0,1,0);
    data.inputs["aTargetPoint"].vector=Imath::V3d(4,0,0);
    data.inputs["inTargetObject"].type=MFnData::kMesh;
    data.inputs["aVelocityAttribute"].text="vel";data.inputs["aMassAttribute"].text="mass";
    data.inputs["aDamping"].number=1;data.inputs["aSpringConstant"].number=1;
    data.inputs["aPosition"].vector=Imath::V3d(4,0,0);
    return data;
}
MStatus compute(MDataBlock& data){
#ifdef DPGOAL
    return ForceNode().compute(MPlug(ForceNode::outDP),data);
#else
    return ForceNode().compute(MPlug(ForceNode::outDD),data);
#endif
}
bool called(const char* label){return std::find(operations.begin(),operations.end(),label)!=operations.end();}
std::string fields(const DPAttrList& attrs){
    std::ostringstream s;s<<std::setprecision(17);
    for(const auto& a:attrs){s<<a.first<<':'<<a.second->typeName()<<':'<<a.second->attrSize();
        std::vector<Imath::V3d> vectors;
        if(dommy::attribute::convertVector3Attribute(a.second,a.second->attrSize(),vectors))
            for(const auto& v:vectors)for(int k=0;k<3;++k)s<<v[k]<<',';
        std::vector<double> values;
        if(dommy::attribute::convertScalarAttribute(a.second,a.second->attrSize(),values))for(double v:values)s<<v<<',';
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
std::string inputs(){return signature(inputPoints)+signature(targetPoints)+signature(inputCurves)+signature(inputMeshes)+signature(targetCurves);}
std::string outputs(){return signature(outputPoints)+signature(outputCurves)+signature(outputMeshes);}
int failures(MDataBlock data){
    resetCompute();const auto before=inputs();assert(compute(data));const auto trace=operations;
    const auto expected=outputs();int count=0;
    for(int index=0;index<int(trace.size());++index){
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            resetCompute(index);injectedStatus=code;assert(!compute(data)&&callCount==index+1);
            assert(inputs()==before&&outputs()==expected);
            if(trace[index]!="output.clean")assert(publications==0);++count;
        }
        resetCompute();attributeThrowAt=index;assert(compute(data)==MS::kFailure&&callCount==index+1);
        assert(inputs()==before&&outputs()==expected);
        if(trace[index]!="output.clean")assert(publications==0);++count;
    }
    resetCompute();assert(compute(data));return count;
}

#ifdef DPGOAL
Imath::V3d velocity(std::size_t index){
    std::vector<Imath::V3d> values;
    assert(dommy::attribute::convertVector3Attribute(outputPoints.getAttr("vel"),outputPoints.points.size(),values));
    return values[index];
}
void testValues(){
    setup();inputPoints.points={{0,0,0}};inputPoints.attributes.clear();
    field(inputPoints,"vel",std::vector<Imath::V3d>{{16777217,0,0}});
    auto data=block();data.inputs["aPosition"].vector=Imath::V3d(16777216,0,0);
    const auto before=inputs();assert(compute(data)&&velocity(0)[0]==-1&&inputs()==before);
    assert(outputPoints.getAttr("vel")->getAttrType()==DP_ATTR_VEC3D);
    field(inputPoints,"vel",std::vector<Imath::V3d>{{1e40,0,0}});
    data.inputs["aDamping"].number=1e-10;data.inputs["aPosition"].vector=Imath::V3d();
    assert(compute(data)&&std::abs(velocity(0)[0]/-1e30-1)<1e-6);
    field(inputPoints,"vel",std::vector<Imath::V3d>{{1e308,0,0}});
    data.inputs["aDamping"].number=2; data.inputs["aSpringConstant"].number=0;
    data.inputs["aMagnitude"].number=0;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(compute(data)&&velocity(0)[0]==0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID)==0);
    setup();data=block(0,1);field(inputPoints,"vel",DPointList{{1,0,0},{2,0,0}});
    field(inputPoints,"mass",std::vector<double>{2,3});data.inputs["aUseMass"].number=1;
    assert(compute(data)&&velocity(0)[0]==-3&&velocity(1)[0]==-9);
    assert(outputPoints.getAttr("vel")->getAttrType()==DP_ATTR_VEC3F);
    setup();data=block();assert(compute(data)&&velocity(0)[0]==3.5&&velocity(1)[0]==2);
    for(int variant=0;variant<7;++variant){
        setup();data=block(0,1);
        if(variant==0)targetPoints.points.pop_back();
        if(variant==1)data.inputs["aDamping"].number=NAN;
        if(variant==2)field(inputPoints,"vel",std::vector<int32_t>{1,2});
        if(variant==3)field(inputPoints,"vel",DPointList{{1,0,0}});
        if(variant==4)field(inputPoints,"vel",std::vector<Imath::V3d>{{INFINITY,0,0},{0,0,0}});
        if(variant==5){data.inputs["aUseMass"].number=1;field(inputPoints,"mass",DPointList{{1,0,0},{2,0,0}});}
        if(variant==6){data.inputs["aUseMass"].number=1;data.inputs["aMassAttribute"].text="";}
        assert(!compute(data)&&geometryCopies==0&&publications==0);
    }
    setup();inputPoints=DPData();targetPoints=DPData();data=block();assert(compute(data)&&!outputPoints.isExistAttr("vel"));
}
#else
std::vector<DPointList> positions(int kind,bool output){
    if(kind==0)return {output?outputPoints.points:inputPoints.points};
    std::vector<DPointList> values;
    const auto& parts=kind==1?(output?outputCurves.primitives:inputCurves.primitives):
        (output?outputMeshes.primitives:inputMeshes.primitives);
    for(const auto& p:parts)values.push_back(p->points);return values;
}
DPoint reference(DPoint point,int mode,float weight,int iterations){
#ifdef DPAVOID
    for(int iteration=0;iteration<(mode==0?1:iterations);++iteration){
        const DPointList targets=mode==0?DPointList{{4,0,0}}:targetPoints.points;
        for(const auto& target:targets){
            const Imath::V3d delta=Imath::V3d(point)-Imath::V3d(target);const double distance=delta.length();
            if(distance>=2||distance<=std::numeric_limits<double>::epsilon())continue;
            point=DPoint(Imath::V3d(point)+delta*((2-distance)*weight/distance));
        }
    }
    return point;
#else
    (void)iterations;DPoint target(4,0,0);
    if(mode==2){double best=INFINITY;for(const auto& candidate:targetPoints.points){
        const double distance=(Imath::V3d(point)-Imath::V3d(candidate)).length();
        if(distance<best){best=distance;target=candidate;}
    }}
    return DPoint(Imath::V3d(point)+(Imath::V3d(target)-Imath::V3d(point))*weight);
#endif
}
void testValues(){
    for(int kind=0;kind<3;++kind)for(int mode:{0,2})for(int iterations:{1,4}){
        setup();auto data=block(kind,mode);data.inputs["aIterate"].number=iterations;
        const auto before=inputs(),original=fields(inputPoints.attributes);const auto source=positions(kind,false);
        assert(compute(data)&&publications==1&&inputs()==before);
        const auto actual=positions(kind,true);assert(actual.size()==source.size());
        for(std::size_t p=0;p<source.size();++p)for(std::size_t i=0;i<source[p].size();++i){
            const DPoint expected=reference(source[p][i],mode,0.5f,iterations);
            for(int axis=0;axis<3;++axis)assert(std::abs(actual[p][i][axis]-expected[axis])<1e-5);
        }
        if(kind==0) {
            assert(fields(outputPoints.attributes)==original && geometryCopies==1);
            assert(std::count(operations.begin(),operations.end(),"bounds.update")==1);
            assert(!called("point.copy") && !called("point.replace") && called("point.batch"));
            assert(outputPoints.getAttr("keep") != inputPoints.getAttr("keep"));
        }
        else {const auto& out=kind==1?outputCurves.primitives:outputMeshes.primitives;
            assert(out[0]->matrix[3][0]==10&&fields(out[0]->attributes)==original);}
        assert(std::count(operations.begin(),operations.end(),"input inDD")==1);
    }
    for(int kind=0;kind<3;++kind)for(int mode:{1,3})for(auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}){
        setup();auto data=block(kind,mode);data.inputs["inTargetObject"].type=type;
        assert(compute(data)&&nativeQueries==(kind==0?1:2));
        resetCompute();rejectNative=true;assert(!compute(data)&&geometryCopies==0&&publications==0);
    }
    for(int variant=0;variant<7;++variant){
        setup();auto data=block();
        if(variant==0)data.inputs["aWeight"].number=NAN;
        if(variant==1)data.inputs["inDD"].payload=nullptr;
        if(variant==2)data.inputs["inDD"].type=999;
        if(variant==3)inputPoints.topology=false;
        if(variant==4)field(inputPoints,"weight",std::vector<double>{INFINITY,1});
        if(variant==5)field(inputPoints,"weight",std::vector<float>{1});
        if(variant==6)data.inputs["aAvoidType"].number=data.inputs["aMagnetType"].number=4;
        assert(!compute(data)&&publications==0&&geometryCopies==0);
    }
    setup();auto data=block();field(inputPoints,"weight",std::vector<std::string>{"x","y"});
    assert(compute(data)); // Existing non-scalar field retains the global fallback.
    for(int kind=0;kind<3;++kind){
        setup();data=block(kind);field(inputPoints,"weight",std::vector<double>{0,1});
        for(auto& p:inputCurves.primitives)field(*p,"weight",std::vector<double>{0,1});
        for(auto& p:inputMeshes.primitives)field(*p,"weight",std::vector<double>{0,1});
        const auto source=positions(kind,false);assert(compute(data));const auto actual=positions(kind,true);
        for(std::size_t p=0;p<source.size();++p){
            assert(actual[p][0][0]==source[p][0][0]);
            assert(std::abs(actual[p][1][0]-reference(source[p][1],0,1,4)[0])<1e-5);
        }
    }
#ifdef DPAVOID
    setup();data=block(0,2);data.inputs["aWeight"].number=1;
    targetPoints.points={{0,0,0}};field(targetPoints,"rad",std::vector<double>{3});
    assert(compute(data)&&outputPoints.points[0][0]==3&&outputPoints.points[1][0]==3);
    for(double radius:{0.0,-1.0,std::numeric_limits<double>::infinity()}){
        field(targetPoints,"rad",std::vector<double>{radius});resetCompute();
        assert(!compute(data)&&geometryCopies==0&&publications==0);
    }
    setup();data=block(0,2);targetPoints=DPData();assert(compute(data));
    assert(outputPoints.points[0][0]==inputPoints.points[0][0]);
    setup();data=block(0,3);targetCurves=DCurveData();assert(compute(data));
    assert(outputPoints.points[0][0]==inputPoints.points[0][0]&&outputPoints.points[0][1]==0&&nativeQueries==0);
#else
    setup();data=block();data.inputs["aIsClamp"].number=1;data.inputs["aClampRange"].vector=Imath::V3d(0.25,0,0);
    data.inputs["aMagnitude"].number=4;
    assert(compute(data)&&std::abs(outputPoints.points[0][0]-1.375)<1e-6);
    setup();data=block(0,2);targetPoints=DPData();assert(!compute(data)&&geometryCopies==0);
#endif
    for(int kind=0;kind<3;++kind){
        setup();inputPoints=DPData();inputCurves=DCurveData();inputMeshes=DMeshData();auto empty=block(kind);
        assert(compute(empty)&&publications==1&&nativeQueries==0);
    }
}
void testBudget(){
    using dommy::point::validPointForceBudget;
    assert(validPointForceBudget(999,2,1000,100));
    assert(!validPointForceBudget(1000,2,1000,100));
    assert(!validPointForceBudget(1,1,std::numeric_limits<std::size_t>::max(),1));
    assert(!validPointForceBudget(2,1,2,std::numeric_limits<std::size_t>::max()));
    assert(!validPointForceBudget(1,1,1,0));
    assert(!validPointForceBudget(1,1000001,1,1));
    assert(!validPointForceBudget(5000000,1,0,1));
    for(int kind=0;kind<3;++kind){
        setup();inputPoints.attributes.clear();targetPoints.attributes.clear();
#ifdef DPAVOID
        const std::size_t half=3000,targets=5000;
#else
        const std::size_t half=6000,targets=10000;
#endif
        inputPoints.points.resize(half*2);targetPoints.points.resize(targets);
        for(auto& p:inputCurves.primitives){p->attributes.clear();p->points.resize(half);}
        for(auto& p:inputMeshes.primitives){p->attributes.clear();p->points.resize(half);}
        auto data=block(kind,2);assert(!compute(data)&&geometryCopies==0&&!called("output.create"));
        assert(positionViews==1); // Target only: no source position copy before preflight.
    }
    setup();DPointList a(6000),b(6000),targets(10000);std::vector<DPointList*> parts{&a,&b};
    std::vector<DScalarDataList> weights(2,DScalarDataList(6000,1));
#ifdef DPAVOID
    AvoidParams params;params.m_type=2;params.m_targetPts=&targets;
    AvoidGen generator(parts,weights);generator.mParam=&params;
#else
    MagneteParams params;params.m_type=2;params.m_targetPts=&targets;
    MagnetGen generator(params,parts,weights);
#endif
    assert(generator()==MS::kInvalidParameter);
}
void testRetainedPayloadBudget() {
    for (int kind=0; kind<3; ++kind) for (int variant=0; variant<4; ++variant) {
        setup(); auto data=block(kind);
        DPAttrList& attrs=kind==0?inputPoints.attributes:
            kind==1?inputCurves.attributes:inputMeshes.attributes;
        if (variant==0) attrs["large"]=std::make_shared<DPStringAttr>(
            std::vector<std::string>(2,std::string(256,'x')));
        if (variant==1) attrs["large"]=std::make_shared<DPIntArrayAttr>(
            std::vector<DIntArray>(2,DIntArray(100,1)));
        if (variant==2) for (int field=0;field<30;++field)
            attrs["field"+std::to_string(field)]=std::make_shared<DPFloatAttr>(std::vector<float>{1,2});
        if (variant==3) {
            // Each partition fits by itself, but retained collection/point
            // metadata plus staging must be admitted as one workspace.
            if (kind==0) {
                for(int group=0;group<10;++group) {
                    auto value=std::make_shared<Group>();value->ids={true,false};
                    inputPoints.groups[std::string(100,'g')+std::to_string(group)]=value;
                }
            } else {
                auto& primitives=kind==1?inputCurves.primitives:inputMeshes.primitives;
                for(auto& primitive:primitives)
                    primitive->attributes["large"]=std::make_shared<DPStringAttr>(
                        std::vector<std::string>(2,std::string(100,'x')));
            }
        }
        const auto before=inputs(), previous=outputs();
        assert(!compute(data) && geometryCopies==0 && publications==0 && nativeQueries==0);
        assert(!called("output.create") && inputs()==before && outputs()==previous);
    }
}
void testCoordinateKernelShape() {
    for (std::size_t size : {0U, 1U, 3U}) {
        setup(); auto data=block(); dommy::point::PointForceInput input;
        input.points=&inputPoints; input.pointCount=2U; input.partitionCount=1U;
        const auto before=inputs(), previous=outputs();
        const auto status=dommy::point::evaluatePointForceOutput(input,data,MObject{"outDP"},"weight",1.0f,
            [&](std::vector<DPointList*>& positions,std::vector<DScalarDataList>&) -> MStatus {
                positions[0]->resize(size); return MS::kSuccess;
            });
        assert(status==MS::kInvalidParameter && !called("output.create"));
        assert(inputs()==before && outputs()==previous && publications==0);
    }
}
#endif

#ifdef DPMAGNET
void testMagnetNumericBoundaries() {
    const float limit = std::numeric_limits<float>::max();
    int valuesChecked = 0, failuresChecked = 0;
    for (int kind : {0, 1, 2}) for (int mode : {0, 2}) {
        for (float start : {-limit, limit}) for (float end : {-1.0f, 1.0f}) {
            for (double weight : {0.0, 1.0}) {
                setup();
                const DPoint source(start, -0.0f, 2.0f), target(end, 3.0f, -0.0f);
                inputPoints.points[0] = source;
                for (auto& p : inputCurves.primitives) p->points[0] = source;
                for (auto& p : inputMeshes.primitives) p->points[0] = source;
                targetPoints.points = {target, target};
                auto data = block(kind, mode);
                data.inputs["aWeight"].number = weight;
                data.inputs["aTargetPoint"].vector = Imath::V3d(target);
                const auto before = inputs();
                assert(compute(data) == MS::kSuccess && publications == 1 && inputs() == before);
                const DPoint expected = weight == 0.0 ? source : target;
                for (const auto& part : positions(kind, true)) {
                    for (unsigned int axis = 0U; axis < 3U; ++axis) {
                        assert(part[0][axis] == expected[axis]);
                        assert(std::signbit(part[0][axis]) == std::signbit(expected[axis]));
                        ++valuesChecked;
                    }
                }
            }
        }
        for (float weight : {-0.5f, 0.25f, 0.75f, 1.5f}) {
            setup();
            auto data = block(kind, mode);
            data.inputs["aWeight"].number = weight;
            const auto source = positions(kind, false);
            const auto before = inputs();
            assert(compute(data) == MS::kSuccess && inputs() == before);
            const auto actual = positions(kind, true);
            for (std::size_t p = 0U; p < source.size(); ++p) {
                for (std::size_t i = 0U; i < source[p].size(); ++i) {
                    const DPoint expected = reference(source[p][i], mode, weight, 1);
                    for (unsigned int axis = 0U; axis < 3U; ++axis) {
                        assert(actual[p][i][axis] == expected[axis]);
                        ++valuesChecked;
                    }
                }
            }
        }
        for (bool clamp : {false, true}) {
            setup();
            inputPoints.points[0][0] = -limit;
            for (auto& p : inputCurves.primitives) p->points[0][0] = -limit;
            for (auto& p : inputMeshes.primitives) p->points[0][0] = -limit;
            targetPoints.points = {{1, 0, 0}, {1, 0, 0}};
            auto data = block(kind, mode);
            data.inputs["aTargetPoint"].vector = Imath::V3d(1, 0, 0);
            data.inputs["aWeight"].number = 2;
            data.inputs["aMagnitude"].number = clamp ? 1.5 : 0.5;
            data.inputs["aIsClamp"].number = clamp;
            assert(compute(data) == MS::kSuccess);
            for (const auto& part : positions(kind, true)) {
                assert(part[0][0] == 1.0f);
                ++valuesChecked;
            }
        }
        for (int variant = 0; variant < 4; ++variant) {
            setup();
            auto data = block(kind, mode);
            assert(compute(data) == MS::kSuccess);
            const auto previous = outputs();
            resetCompute();
            if (variant < 2) {
                data.inputs["aWeight"].number = variant == 0 ? NAN : INFINITY;
            } else {
                // The second point fails only after a valid first result is staged.
                inputPoints.points[0][0] = limit;
                inputPoints.points[1][0] = -limit;
                for (auto& p : inputCurves.primitives) {
                    p->points[0][0] = limit;
                    p->points[1][0] = -limit;
                }
                for (auto& p : inputMeshes.primitives) {
                    p->points[0][0] = limit;
                    p->points[1][0] = -limit;
                }
                data.inputs["aWeight"].number = variant == 2 ? -1.0 : 2.0;
                data.inputs["aTargetPoint"].vector = Imath::V3d(limit, 0, 0);
                targetPoints.points = {{limit, 0, 0}, {limit, 0, 0}};
            }
            const auto before = inputs();
            assert(compute(data) != MS::kSuccess && publications == 0);
            assert(inputs() == before && outputs() == previous);
            ++failuresChecked;
            resetCompute();
            data.inputs["aWeight"].number = 1.0;
            assert(compute(data) == MS::kSuccess && publications == 1 && inputs() == before);
        }
    }
    std::cout << "Magnet numeric checks: " << valuesChecked << " boundary/golden values and "
              << failuresChecked << " failure/retry cases passed\n";
}
#endif

int main(int argc,char**){
#ifndef DPGOAL
    if(argc>1) {testRetainedPayloadBudget();return 0;}
#else
    (void)argc;
#endif
    testValues();int injected=0;
#ifdef DPMAGNET
    testMagnetNumericBoundaries();
#endif
#ifdef DPGOAL
    for(int mode=0;mode<2;++mode)for(bool mass:{false,true}){
        setup();auto data=block(0,mode);data.inputs["aUseMass"].number=mass;
        field(inputPoints,"vel",std::vector<Imath::V3d>{{1,0,0},{2,0,0}});
        field(inputPoints,"mass",std::vector<float>{1,2});injected+=failures(data);
    }
#else
    testBudget();
    testCoordinateKernelShape();
    for(int kind=0;kind<3;++kind)for(int mode=0;mode<4;++mode){
        setup();injected+=failures(block(kind,mode));
    }
#endif
    for(bool reject:{false,true}){
        setup();auto data=block();const auto before=inputs(),output=outputs();
        rejectFinal=reject;nullCreated=!reject;assert(!compute(data)&&publications==0);
        assert(inputs()==before&&outputs()==output);
    }
    setup();auto data=block();assert(ForceNode().compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&operations.empty());
    std::cout<<"Point force: values, budgets, recovery and "<<injected<<" compute failure injections passed\n";
}

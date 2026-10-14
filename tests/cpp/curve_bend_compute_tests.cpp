// Native curve queries/matrices are explicit control-flow models. The complete
// callback, checked input/output helpers, array conversion and bend planner run.
#define DOMMY_BEND_TESTS
#include "curve_generator_compute_doubles.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DCurveBendPlan.h"
namespace dommy {namespace maya {
inline bool isFinitePoint(const MPoint& p){return isFiniteVector(p);}
inline bool isFiniteMatrix(const MMatrix& m){return dommy::attribute::isFiniteMatrix(m);}
}}
static void getDCPAttributeMVectorArrayFromVec3(const DCItem& curve,const std::string& name,MVectorArray& values) {
    std::vector<Imath::V3d> converted;
    if(!dommy::attribute::convertVector3Attribute(curve->getAttr(name),curve->points.size(),converted))throw std::invalid_argument("axis");
    values.values.clear();for(const auto& v:converted)values.values.emplace_back(v);
}
static void getDCPAttributeMDoubleArray(const DCItem& curve,const std::string& name,MDoubleArray& values) {
    if(!dommy::attribute::convertScalarAttribute(curve->getAttr(name),curve->points.size(),values.values))throw std::invalid_argument("weights");
}
#include "curve_bend_compute.inc"
static MDataBlock setup(int kind=1,bool reverse=false,bool active=true) {
    resetCompute();nativeObjects.clear();shortArrayAt=-1;
    generatorInput=generatorStaged=generatorOutput=DCurveData();
    auto curve=std::make_shared<GeneratorCurve>();curve->points={{0,0,0},{1,0,0},{2,0,0}};
    generatorInput.curves.push_back(curve);
    auto previous=std::make_shared<GeneratorCurve>();previous->points={{-99,0,0}};generatorOutput.curves.push_back(previous);
    MDataBlock data;data.inputs["inDC"].payload=&generatorInput;data.inputs["inDC"].type=DCurveData::id;
    data.inputs["aBend"].number=active?90:0;data.inputs["aBendVectorType"].number=kind;
    data.inputs["aRotateVector"].vector={0,0,1};data.inputs["aReverse"].number=reverse;
    data.inputs["aWeight"].number=1;data.inputs["aWeightAttriute"].text="weight";
    data.inputs["aRotateVectorAttribute"].text="axis";
    if(kind==2)curve->attributes["axis"]=std::make_shared<DPVec3fAttr>(DPointList(3,DPoint(0,0,1)));
    return data;
}
static MStatus compute(MDataBlock& data){return DCBend().compute(MPlug(DCBend::outDC),data);}
int main() {
    int tested=0;
    for(int kind:{0,1,2})for(bool reverse:{false,true})for(bool active:{false,true}) {
        auto data=setup(kind,reverse,active);assert(compute(data)&&publications==1);
        assert(generatorInput.curves[0]->points[1].x==1&&generatorOutput.curves[0]!=generatorInput.curves[0]);
        if(active&&kind==1&&!reverse){const auto& p=generatorOutput.curves[0]->points;
            assert(std::abs(p[1].x)<1e-5&&std::abs(p[1].y-1)<1e-5&&std::abs(p[2].x+1)<1e-5&&std::abs(p[2].y-1)<1e-5);}
        const auto trace=operations;
        for(int at=0;at<int(trace.size());++at)for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            data=setup(kind,reverse,active);failAt=at;injectedStatus=code;const auto result=compute(data);assert(!result);
            if(trace[at]=="input inDC"||trace[at]=="input.bind"||trace[at]=="factory.create"||trace[at]=="factory.bind")assert(result==code);
            if(trace[at]!="output.clean"&&trace[at]!="handle.clean")assert(publications==0&&generatorOutput.curves[0]->points[0].x==-99);
            assert(generatorInput.curves[0]->points[1].x==1&&callCount==at+1);
            resetCompute();assert(compute(data)&&publications==1);++tested;
        }
        for(int at=0;at<int(trace.size());++at)if(trace[at]=="array.length"||trace[at]=="array.copy") {
            data=setup(kind,reverse,active);shortArrayAt=at;assert(!compute(data)&&publications==0);
        }
        for(int at=0;at<int(trace.size());++at) {
            data=setup(kind,reverse,active);attributeThrowAt=at;
            assert(!compute(data)&&callCount==at+1);
            if(trace[at]!="output.clean"&&trace[at]!="handle.clean")assert(publications==0);
            assert(generatorInput.curves[0]->points[1].x==1);
            resetCompute();assert(compute(data)&&publications==1);++tested;
        }
    }
    auto data=setup();data.inputs["inDC"].payload=nullptr;assert(!compute(data)&&publications==0&&geometryCopies==0);
    data=setup();generatorInput.topology=false;assert(!compute(data)&&publications==0);
    data=setup();generatorInput.curves.clear();assert(compute(data)&&generatorOutput.curves.empty());
    data=setup();nullCreated=true;assert(compute(data)==MS::kFailure&&publications==0);nullCreated=false;
    std::cout<<"Complete DCBend: "<<tested<<" status/retry cases, three axis modes, both directions, no-op and short-array isolation passed\n";
}

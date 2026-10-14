#include "curve_generator_compute_doubles.h"
#include "curve_generator_compute.inc"

static int evaluations=0,failureCases=0,shortCases=0;
static void setup(std::size_t count=2U){
    resetCompute();shortArrayAt=-1;shortRamp=badRamp=shortBatch=nullNative=false;
    generatorPoints=DPData();generatorInput=DCurveData();generatorStaged=DCurveData();generatorOutput=DCurveData();
    nativeOutput={{"old"}};nativeStaged.clear();nativeObjects.clear();
    for(std::size_t i=0;i<count;++i){
        generatorPoints.points.emplace_back(float(i),0,0);
        auto curve=std::make_shared<GeneratorCurve>();curve->points={{float(i),0,0},{float(i),2,0}};
        curve->knots={0,1};generatorInput.curves.push_back(curve);
    }
    auto previous=std::make_shared<GeneratorCurve>();previous->points={{-99,0,0}};generatorOutput.curves={previous};
}
static MDataBlock furBlock(){
    MDataBlock block;block.inputs["inDP"].payload=&generatorPoints;block.inputs["inDP"].type=DPData::id;
    block.inputs["aSeed"].number=1;block.inputs["aSegment"].number=4;block.inputs["aLength"].number=2;
    block.inputs["aScale"].number=1;
    block.inputs["aLengthAttribute"].text="";block.inputs["aNormalAttribute"].text="";block.inputs["aTransferAttribute"].text="";
    return block;
}
static MDataBlock ahoBlock(){
    MDataBlock block;block.inputs["inDC"].payload=&generatorInput;block.inputs["inDC"].type=DCurveData::id;
    block.inputs["aSeed"].number=1;block.inputs["aSegment"].number=4;block.inputs["aLength"].number=2;
    block.inputs["aCount"].number=2;block.inputs["aNormal"].vector=Imath::V3d(0,1,0);return block;
}
static MStatus fur(MDataBlock& block){++evaluations;return DCFurGen().compute(MPlug(DCFurGen::outDC),block);}
static MStatus aho(MDataBlock& block){++evaluations;return DCAhoGen().compute(MPlug(DCAhoGen::outDC),block);}
static MStatus native(MDataBlock& block){++evaluations;return DCToCurves().compute(MPlug(DCToCurves::outCurves),block);}
static std::string state(const DCurveData& value){
    std::ostringstream stream;for(const auto& curve:value.curves){if(!curve){stream<<"null";continue;}
        stream<<curve->degree;for(const auto& point:curve->points)stream<<point.x<<','<<point.y<<','<<point.z<<';';for(double knot:curve->knots)stream<<knot<<',';}
    for(const auto& field:value.attributes)stream<<field.first<<':'<<field.second->str();return stream.str();
}
static std::string inputState(){
    std::ostringstream stream;stream<<state(generatorInput);for(const auto& point:generatorPoints.points)stream<<point.x<<','<<point.y<<','<<point.z<<';';
    for(const auto& field:generatorPoints.attributes)stream<<field.first<<':'<<field.second->str();return stream.str();
}
static std::string outputState(){std::string result=state(generatorOutput);for(const auto& object:nativeOutput)result+=object.name;return result;}
static bool called(const char* name){return std::find(operations.begin(),operations.end(),name)!=operations.end();}
static void values(){
    for(std::size_t count:{0U,2U}){
        setup(count);auto block=furBlock();const auto source=inputState();
        assert(fur(block)==MS::kSuccess&&publications==1&&inputState()==source);
        assert(generatorOutput.curves.size()==count);
        for(std::size_t i=0;i<count;++i){const auto& curve=*generatorOutput.curves[i];assert(curve.points.size()==5&&curve.degree==3&&curve.knots.size()==7);
            for(std::size_t p=0;p<5;++p)assert(curve.points[p].x==float(i)&&curve.points[p].y==float(p)*.5f&&curve.points[p].z==0);}
        for(int distribution=0;distribution<2;++distribution)for(int angle=0;angle<3;++angle){
            setup(count);block=ahoBlock();block.inputs["aDistributeType"].number=distribution;block.inputs["aAngleType"].number=angle;
            assert(aho(block)==MS::kSuccess&&publications==1&&generatorOutput.curves.size()==count*2);
            const auto first=state(generatorOutput);resetCompute();assert(aho(block)&&state(generatorOutput)==first);
            for(const auto& curve:generatorOutput.curves){assert(curve->points.size()==4&&curve->degree==3);
                assert(std::abs((curve->points.back().y-curve->points.front().y)-2)<1e-5);}
        }
        setup(count);block=ahoBlock();assert(native(block)&&nativeOutput.size()==count&&publications==1);
        for(const auto& object:nativeOutput)assert(!object.isNull());
    }
    setup();auto block=furBlock();block.inputs["aTransferAttribute"].text="weight label";
    generatorPoints.attributes["weight"]=std::make_shared<DPFloatAttr>(std::vector<float>{3,4},9);
    generatorPoints.attributes["label"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"a","b"},"default");
    block.inputs["aLengthAttribute"].text="weight";
    assert(fur(block)&&generatorOutput.attributes.size()==2);
    const auto weight=std::dynamic_pointer_cast<DPFloatAttr>(generatorOutput.attributes.at("weight"));
    assert(weight&&weight->value()==std::vector<float>({3,4})&&weight->getDefaultValue()==9);
    assert(generatorOutput.attributes.at("weight")!=generatorPoints.attributes.at("weight"));
    assert(generatorOutput.curves[0]->points.back().y==5&&generatorOutput.curves[1]->points.back().y==6);
    setup();block=furBlock();block.inputs["aLength"].number=0;assert(fur(block)&&generatorOutput.curves.empty());
    setup();block=ahoBlock();block.inputs["aCount"].number=0;assert(aho(block)&&generatorOutput.curves.empty());
    setup();block=furBlock();block.inputs["aNormalAttribute"].text="N";
    generatorPoints.attributes["N"]=std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{{1,0,0},{0,0,1}});
    assert(fur(block)&&generatorOutput.curves[0]->points.back().x==2&&generatorOutput.curves[1]->points.back().z==2);
    setup();block=furBlock();block.inputs["aLengthAttribute"].text="length";block.inputs["aTransferAttribute"].text="label";
    generatorPoints.attributes["length"]=std::make_shared<DPDoubleAttr>(std::vector<double>{-3,1});
    generatorPoints.attributes["label"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"skip","keep"},"default");
    assert(fur(block)&&generatorOutput.curves.size()==1&&generatorOutput.curves[0]->points.back().y==3);
    const auto label=std::dynamic_pointer_cast<DPStringAttr>(generatorOutput.attributes.at("label"));
    assert(label&&label->value()==std::vector<std::string>({"keep"})&&label->getDefaultValue()=="default");
    setup();block=furBlock();block.inputs["aThinning"].number=1;assert(fur(block)&&generatorOutput.curves.empty());
    for(bool isFur:{false,true}){
        setup();block=isFur?furBlock():ahoBlock();block.inputs["aIsStruggle"].number=1;
        block.inputs["aStruggleRange"].vector=Imath::V3d(.1,.2,.3);block.inputs["aStruggleStrength"].number=.5;
        block.inputs["aLengthRandom"].number=.5;
        if(isFur)block.inputs["aBend"].number=15;else block.inputs["aAngleSpread"].number=.1;
        assert((isFur?fur(block):aho(block))&&generatorOutput.valid());const auto first=state(generatorOutput);
        resetCompute();assert((isFur?fur(block):aho(block))&&state(generatorOutput)==first);
    }
}
template<class Make,class Compute>static void failures(Make make,Compute compute){
    setup();auto block=make();assert(compute(block));const auto trace=operations;
    for(int at=0;at<int(trace.size());++at)for(bool exception:{false,true})for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        setup();block=make();const auto source=inputState(),previous=outputState();
        if(exception)attributeThrowAt=at;else failAt=at;injectedStatus=code;
        const auto status=compute(block);assert(!status&&inputState()==source);
        const bool afterPublish=trace[at]=="handle.clean"||trace[at]=="output.clean"||trace[at]=="array.clean";
        if(!afterPublish)assert(publications==0&&outputState()==previous);
        // Required acquisition/binding/factory errors retain their exact code.
        if(!exception&&(trace[at]=="input inDP"||trace[at]=="input inDC"||trace[at]=="input.bind"||trace[at]=="factory.create"||trace[at]=="factory.bind"))assert(status==code);
        ++failureCases;resetCompute();assert(compute(block)&&publications==1&&inputState()==source);
    }
    for(int at=0;at<int(trace.size());++at){
        if(trace[at]!="array.length"&&trace[at]!="array.copy"&&trace[at]!="array.construct"&&trace[at]!="array.append")continue;
        setup();block=make();shortArrayAt=at;const auto previous=outputState();
        assert(!compute(block)&&publications==0&&outputState()==previous);++shortCases;
        shortArrayAt=-1;resetCompute();assert(compute(block)&&publications==1);
    }
}
template<class Make,class Compute>static void admission(Make make,Compute compute,bool point){
    for(int bad=0;bad<5;++bad){
        setup();auto block=make();const std::string attribute=point?"inDP":"inDC";
        if(bad==0)block.inputs[attribute].payload=nullptr;
        if(bad==1)block.inputs[attribute].type=999;
        if(bad==2)block.inputs[attribute].payload=point?static_cast<const Payload*>(&generatorInput):&generatorPoints;
        if(bad==3){if(point)generatorPoints.topology=false;else generatorInput.topology=false;}
        if(bad==4){if(point)generatorPoints.points[0].x=NAN;else generatorInput.curves[0].reset();}
        const auto previous=outputState();
        assert(compute(block)==(bad>=3?MS::kInvalidParameter:MS::kFailure)&&publications==0&&outputState()==previous);
        assert(!called("factory.create")&&!called("native.curve"));
    }
}
static void malformed(){
    for(int bad=0;bad<6;++bad){
        setup();auto block=furBlock();shortRamp=bad==0;badRamp=bad==1;nullCreated=bad==2;shortBatch=bad==3;rejectFinal=bad==4;
        if(bad==5)block.inputs["aSegment"].number=1;
        const auto source=inputState(),previous=outputState();assert(!fur(block)&&publications==0&&inputState()==source&&outputState()==previous);
    }
    for(int bad=0;bad<7;++bad){
        setup();auto block=ahoBlock();shortRamp=bad==0;badRamp=bad==1;nullCreated=bad==2;shortBatch=bad==3;rejectFinal=bad==4;nullNative=bad==5;
        if(bad==6)block.inputs["aLength"].number=NAN;
        const auto previous=outputState();assert(!aho(block)&&publications==0&&outputState()==previous);
    }
    setup();auto block=ahoBlock();nullNative=true;assert(native(block)==MS::kFailure&&publications==0);
    setup();block=furBlock();assert(DCFurGen().compute(MPlug(MObject{"other"}),block)==MS::kUnknownParameter&&operations.empty());
    assert(DCAhoGen().compute(MPlug(MObject{"other"}),block)==MS::kUnknownParameter&&operations.empty());
    assert(DCToCurves().compute(MPlug(MObject{"other"}),block)==MS::kUnknownParameter&&operations.empty());
}
int main(){
    values();failures(furBlock,fur);failures(ahoBlock,aho);failures(ahoBlock,native);
    admission(furBlock,fur,true);admission(ahoBlock,aho,false);admission(ahoBlock,native,false);malformed();
    std::cout<<"Curve callbacks: "<<evaluations<<" evaluations, "<<failureCases<<" API failure/retry cases, "<<shortCases<<" short-array cases passed\n";
}

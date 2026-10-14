#include "curve_weight_map_doubles.h"
#include "curve_weight_map.inc"

namespace {
DCurveData source;
void setup() {
    reset();
    assert(DCWeightMap::initialize() == MS::kSuccess);
    source = DCurveData();
    source.curves.push_back(std::make_shared<DCurveInternalData>(
        MPointArray{{0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0}}, 1));
    source.curves[0]->attributes["keep"] = {1,2,3,4,5};
    source.curves[0]->primitiveMatrix[3][0] = 7;
    inputs.clear();
    inputs["inDC"].plugin = &source;
    inputs["ParamType"].number = 0;
    inputs["BaseParam"].number = 0.5;
    inputs["ParamAttributeName"].string = "param";
    inputs["Distance"].number = 0.2f;
    inputs["Offset"].number = 0;
    inputs["WrapMode"].number = 1;
    inputs["WeightAttributeName"].string = "weight";
    inputs["WeightScale"].number = 1;
    inputs["RepeatWeight"].number = 0;
    rampStart = 1; rampEnd = 0; invalidRamp = false;
    sampledPositions.clear();
    reset();
}
MStatus compute() {
    MDataBlock data;
    return DCWeightMap().compute(MPlug(MObject{"node"}, DCWeightMap::outDC), data);
}
void unchanged() {
    assert(source.curves.size() == 1 && source.curves[0]->attributes.size() == 1);
    assert(source.curves[0]->attributes.at("keep") == std::vector<float>({1,2,3,4,5}));
    assert(source.curves[0]->primitiveMatrix[3][0] == 7);
    for (unsigned i=0; i<5; ++i) assert(source.curves[0]->points[i].x == i);
}
void weights(const std::vector<float>& expected) {
    assert(published == 1 && cleaned == 1 && outputData.curves.size() == 1);
    const auto& curve = outputData.curves[0];
    const auto& actual = curve->attributes.at(inputs["WeightAttributeName"].string.empty()
        ? "weight" : inputs["WeightAttributeName"].string);
    assert(actual.size() == expected.size());
    for (unsigned i=0; i<expected.size(); ++i) assert(std::fabs(actual[i]-expected[i]) < 2e-6f);
    assert(curve->attributes.at("keep") == source.curves[0]->attributes.at("keep"));
    assert(curve->primitiveMatrix[3][0] == 7);
    unchanged();
}
void contracts() {
    setup(); assert(compute()); weights({0,0,1,0,0});
    assert(sampledPositions == std::vector<float>({1,1,0,1,1}));
    setup(); inputs["Distance"].number=1; assert(compute()); weights({0.5f,0.75f,1,0.75f,0.5f});
    setup(); rampEnd=0.25f; assert(compute()); weights({0.25f,0.25f,1,0.25f,0.25f});
    setup(); inputs["RepeatWeight"].number=1; assert(compute()); weights({0.5f,0.25f,1,0.25f,0.5f});
    for (double offset : {std::ldexp(1.0,60), -std::ldexp(1.0,60)}) {
        setup(); inputs["BaseParam"].number=0.25; inputs["Offset"].number=offset;
        inputs["WrapMode"].number=2; inputs["Distance"].number=1;
        assert(compute()); weights({0.75f,1,0.75f,0.5f,0.25f});
    }
    for (float parameter : {std::ldexp(1.0f,60), -std::ldexp(1.0f,60)}) {
        setup(); inputs["ParamType"].number=1; source.attributes["param"]={parameter};
        inputs["WrapMode"].number=0; inputs["Distance"].number=1; inputs["RepeatWeight"].number=1;
        assert(compute()); weights({1,0.75f,0.5f,0.25f,0});
    }
    setup(); inputs["WeightAttributeName"].string=""; inputs["WeightScale"].number=-2;
    assert(compute()); weights({0,0,-2,0,0});
    setup(); MDataBlock data;
    assert(DCWeightMap().compute(MPlug(MObject{"node"}, MObject{"other"}), data)==MS::kUnknownParameter);
    assert(operations.empty() && !published);
}
void invalidInputs() {
    for (int variant=0; variant<10; ++variant) {
        setup();
        if(variant==0) inputs["Distance"].number=0;
        if(variant==1) inputs["Distance"].number=std::numeric_limits<float>::epsilon();
        if(variant==2) inputs["Offset"].number=NAN;
        if(variant==3) inputs["WrapMode"].number=3;
        if(variant==4) {inputs["ParamType"].number=1; source.attributes["param"]={0,1};}
        if(variant==5) {inputs["ParamType"].number=1; source.attributes["param"]={NAN};}
        if(variant==6) invalidRamp=true;
        if(variant==7) {rampStart=std::numeric_limits<float>::max(); inputs["WeightScale"].number=2;}
        if(variant==8) inputs["BaseParam"].number=INFINITY;
        if(variant==9) inputs["ParamType"].number=2;
        assert(!compute() && !published); unchanged();
    }
}
void failures() {
    int statusCases=0, exceptions=0;
    for (bool attribute : {false,true}) {
        setup();
        if(attribute) {inputs["ParamType"].number=1; source.attributes["param"]={0.5f};}
        assert(compute()); const auto trace=operations;
        for(int index=0;index<int(trace.size());++index) {
            for (auto status : {MS::kFailure,MS::kInvalidParameter}) {
                reset(index); injectedStatus=status;
                assert(!compute() && callCount==index+1); unchanged();
                if(trace[index]!="clean") assert(!published);
                ++statusCases;
            }
            reset(); throwAt=index;
            assert(compute()==MS::kFailure && callCount==index+1); unchanged();
            if(trace[index]!="clean") assert(!published);
            ++exceptions;
        }
        reset(); assert(compute()); weights({0,0,1,0,0});
    }
    std::cout << "DCWeightMap: " << statusCases << " status failures, " << exceptions
              << " exceptions, recovery and numeric contracts passed\n";
}
} // namespace
int main() {
    contracts(); invalidInputs(); failures();
}

#include "mesh_emitter_doubles.h"
#include <iostream>

using DPIdList=std::vector<bool>;
MObject thisMObject(){return {"node"};}
MString name(){return "packing input probe";}
struct PackingRamp {
    std::string attribute="old";
    bool valid=true;
    PackingRamp()=default;
    PackingRamp(const MPlug& plug,MStatus* status=nullptr) {
        const auto result=api("ramp.bind "+plug.attribute);if(status)*status=result;
        attribute=plug.attribute;valid=bool(result);
    }
};
#define MRampAttribute PackingRamp

#include "packing_input.inc"

DPData prePacked;
void fixture(bool connected=true) {
    reset();injectedStatus=MS::kFailure;inputs.clear();
    prePacked=DPData();prePacked.points={{{1,2,3}},{{4,5,6}}};
    inputs["pre"].connected=connected;inputs["pre"].plugin=&prePacked;
    inputs["group"].string="seed";
    inputs["map"].connected=true;
    inputs["geometry"].connected=true;inputs["geometry"].type=MFnData::kMesh;
    inputs["geometry"].object={"mesh"};
    emitterMeshes.clear();emitterMeshes["mesh"]=EmitterMesh();
}
int mapConnections() {
    int failures=0;MDataBlock block;
    for(auto probe:mapProbes)for(bool threeDimensions:{false,true}) {
        const short mode=probe==mapDPointVolumeEmit?D_PACK_3D_MAP:D_PACK_MAP;
        fixture();bool admitted=false;
        assert(probe(block,mode,threeDimensions,admitted)==MS::kSuccess && admitted);
        const int count=callCount;
        for(int index=0;index<count;++index) {
            for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                fixture();failAt=index;injectedStatus=code;admitted=false;
                assert(probe(block,mode,threeDimensions,admitted)==code && !admitted);
                assert(callCount==index+1);++failures;
            }
            fixture();throwAt=index;admitted=false;
            assert(probe(block,mode,threeDimensions,admitted)==MS::kFailure && !admitted);
            fixture();assert(probe(block,mode,threeDimensions,admitted)==MS::kSuccess && admitted);
            ++failures;
        }
        fixture();inputs["map"].connected=false;admitted=false;
        assert(probe(block,mode,threeDimensions,admitted)!=MS::kSuccess && !admitted);
        for(short inactive:{short(D_PACK_RANDOM),short(D_PACK_GRADATION)}) {
            fixture();inputs["map"].connected=false;admitted=false;
            assert(probe(block,inactive,threeDimensions,admitted)==MS::kSuccess && admitted);
            assert(operations.empty());
        }
    }
    fixture();bool admitted=false;
    assert(scatterConnection(admitted)==MS::kSuccess && admitted);
    for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
        fixture();failAt=0;injectedStatus=code;admitted=false;
        assert(scatterConnection(admitted)==code && !admitted && callCount==1);++failures;
    }
    fixture();throwAt=0;admitted=false;
    assert(scatterConnection(admitted)==MS::kFailure && !admitted && callCount==1);++failures;
    fixture();inputs["geometry"].connected=false;admitted=false;
    assert(scatterConnection(admitted)==MS::kFailure && !admitted);
    return failures;
}
void legacyAndCheckedInput() {
    MDataBlock block;
    fixture();assert(getInputDP({"pre"},block)==&prePacked);
    for(int index=0;index<2;++index)for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
        fixture();failAt=index;injectedStatus=code;
        const DPData* borrowed=&prePacked;
        assert(getValidInputDP({"pre"},block,borrowed)==code && borrowed==nullptr && callCount==index+1);
        fixture();failAt=index;assert(getInputDP({"pre"},block)==nullptr);
    }
    for(bool wrongType:{false,true}) {
        fixture();inputs["pre"].pluginTypeMatches=!wrongType;prePacked.topologyValid=wrongType;
        const DPData* borrowed=&prePacked;
        assert(getValidInputDP({"pre"},block,borrowed)!=MS::kSuccess && borrowed==nullptr);
        reset();assert(getInputDP({"pre"},block)==nullptr);
    }
}
int admissions() {
    MDataBlock block;int failures=0;
    for(auto probe:prePackingProbes) {
        fixture();std::size_t count=99;bool admitted=false;
        assert(probe(block,count,admitted)==MS::kSuccess && admitted && count==2);
        const int calls=callCount;
        for(int index=0;index<calls;++index) {
            for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                fixture();failAt=index;injectedStatus=code;count=99;admitted=false;
                assert(probe(block,count,admitted)==code && !admitted && count==99);
                assert(callCount==index+1 && published==0);++failures;
                fixture();assert(probe(block,count,admitted)==MS::kSuccess && count==2);
            }
            fixture();throwAt=index;count=99;admitted=false;
            assert(probe(block,count,admitted)==MS::kFailure && !admitted && count==99);
            ++failures;
        }
        for(int malformed=0;malformed<4;++malformed) {
            fixture();count=99;admitted=false;
            if(malformed==0)inputs["pre"].plugin=nullptr;
            if(malformed==1)inputs["pre"].pluginTypeMatches=false;
            if(malformed==2)prePacked.topologyValid=false;
            if(malformed==3)prePacked.points[0][0]=NAN;
            assert(probe(block,count,admitted)!=MS::kSuccess && !admitted && count==99);
        }
        fixture();prePacked.points.clear();count=99;admitted=false;
        assert(probe(block,count,admitted)==MS::kSuccess && admitted && count==0);
        fixture(false);inputs["pre"].plugin=nullptr;count=99;admitted=false;
        assert(probe(block,count,admitted)==MS::kSuccess && admitted && count==0);
        for(const auto& operation:operations)
            assert(operation!="input pre" && operation!="plugin.read");
    }
    return failures;
}
int selectedRamps() {
    MDataBlock block;int failures=0;
    for(auto probe:rampProbes) {
        const short modes=probe==rampsDPointVolumeEmit?D_PACKING_3D_TYPE_COUNT:D_PACKING_TYPE_COUNT;
        for(short mode=0;mode<modes;++mode)for(bool bias:{false,true}) {
            const auto setup=[&]() {fixture();inputs["biasEnabled"].number=bias?1:0;};
            setup();MRampAttribute gradient,radius;bool admitted=false;
            assert(probe(block,mode,gradient,radius,admitted)==MS::kSuccess && admitted);
            const bool usesGradient=mode==D_PACK_GRADATION;
            const bool usesRadius=mode==D_PACK_RANDOM && bias;
            assert(gradient.attribute==(usesGradient?"gradient":"old"));
            assert(radius.attribute==(usesRadius?"radius":"old"));
            const int count=callCount;
            for(int index=0;index<count;++index) {
                for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                    setup();failAt=index;injectedStatus=code;admitted=false;
                    assert(probe(block,mode,gradient,radius,admitted)==code && !admitted);
                    assert(callCount==index+1);++failures;
                }
                setup();throwAt=index;admitted=false;
                assert(probe(block,mode,gradient,radius,admitted)==MS::kFailure && !admitted);
                ++failures;
                setup();assert(probe(block,mode,gradient,radius,admitted)==MS::kSuccess && admitted);
            }
        }
    }
    return failures;
}
int main() {
    legacyAndCheckedInput();
    const int failures=admissions();
    const int rampFailures=selectedRamps();
    const int mapFailures=mapConnections();
    std::cout<<"Three pre-packing admission paths: "<<failures
             <<" status/exception/retry cases, malformed payloads and valid empty/disconnected input passed\n";
    std::cout<<"Selected ramp modes: "<<rampFailures<<" status/exception/retry cases and unused-ramp exclusion passed\n";
    std::cout<<"Packing map/scatter connections: "<<mapFailures<<" failures and inactive-map exclusion passed\n";
}

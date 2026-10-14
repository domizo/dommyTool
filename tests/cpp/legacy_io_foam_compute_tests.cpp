#include "legacy_io_foam_doubles.h"
#include "legacy_io_foam_compute.inc"

namespace {
std::string directory;
void file(const std::string& name,const std::string& contents) {
    std::ofstream out(directory+"/"+name);assert(out.good());out<<contents;assert(out.good());
}
MStatus compute(int kind,MDataBlock& block) {
    if(kind==0)return applyVtxColorFromFileNode().compute({applyVtxColorFromFileNode::outputMesh},block);
    if(kind==1)return ReadDPointDataNode().compute({ReadDPointDataNode::outputPointArray},block);
    return foamGenerateNode().compute({foamGenerateNode::outPutMesh},block);
}
MDataBlock setup(int kind,int mode=0) {
    auto b=resetRun();
    b.path=directory+(kind==0?"/colors.txt":"/points.txt");
    if(mode==1){
        if(kind==0)meshes["input"].colors["colorSet1"]={{9,9,9}};
        else if(kind==1)b.path.clear();
        else b.controls["selfDeform"]=0;
    }
    if(mode==2){if(kind==0)b.path.clear();else if(kind==2){positions.clear();radii.clear();}}
    return b;
}
void unchanged(){assert(published==0 && cleaned==0 && output.name=="old");}
void expectFailure(int kind,MDataBlock& b){assert(compute(kind,b)!=MS::kSuccess);unchanged();}

void testCompute() {
    file("colors.txt","VTXCOLOR 3\nVC -2 3 4\nVC 5 6 7\nVC 8 9 10\n");
    file("points.txt","POINT_DATA_FROM_MESH mesh 2\nP 1 2 3\nP -4 5 6\n");
    int failures=0,exceptions=0;
    for(int kind=0;kind<3;++kind)for(int mode=0;mode<(kind==1?2:3);++mode){
        auto b=setup(kind,mode);assert(compute(kind,b)==MS::kSuccess);
        assert(published==1 && cleaned>0 && output.name!="old");
        const auto trace=operations;const int count=callCount;
        if(kind==0 && mode!=2){
            const auto& mesh=meshes.at(output.name);
            assert(mesh.current=="other" && mesh.colors.at("other")[0].r==0.5f);
            assert(mesh.colors.at("colorSet1")[0].r==-2 && mesh.colors.at("colorSet1")[2].b==10);
            assert((mesh.colorIds.at("colorSet1")==MIntArray{0,1,2}));
            assert(meshes.at("input").colors.at("other")[0].r==0.5f);
        }
        if(kind==1){assert(decoded.size()==(mode==0?2U:0U));if(mode==0)assert(decoded[1].x==-4);}
        if(kind==2 && mode!=2){
            dommy::geometry::IcosahedronTopology topology;
            assert(dommy::geometry::buildHistoricalIcosahedron(1,topology));
            const auto& mesh=meshes.at(output.name);
            assert(mesh.points.size()==topology.points.size()*2);
            assert(mesh.counts.size()==topology.triangles.size()*2);
            bool changed=false;
            for(std::size_t i=0;i<topology.points.size();++i){
                const double delta=std::fabs(mesh.points[i].x-topology.points[i].x)+
                    std::fabs(mesh.points[i].y-topology.points[i].y)+std::fabs(mesh.points[i].z-topology.points[i].z);
                if(mode==1)assert(delta<1e-6);else if(delta>1e-4)changed=true;
            }
            if(mode==0)assert(changed);
        }
        for(int at=0;at<count;++at){
            for(auto status:{MS::kFailure,MS::kInvalidParameter}){
                b=setup(kind,mode);failAt=at;injectedStatus=status;
                assert(compute(kind,b)==status && callCount==at+1);++failures;
                if(trace[at]!="data.clean")unchanged();
                assert(meshes.at("input").points.size()==3);
            }
            b=setup(kind,mode);attributeThrowAt=at;
            assert(compute(kind,b)==MS::kFailure && callCount==at+1);++exceptions;
            if(trace[at]!="data.clean")unchanged();
        }
        std::cout<<"Compute kind "<<kind<<", mode "<<mode<<": "<<count<<" checked API operations\n";
    }
    std::cout<<"Legacy IO/Foam: "<<failures<<" injected failures, "<<exceptions<<" exceptions passed\n";
}
void testInvalid() {
    for(int kind=0;kind<2;++kind){
        auto b=setup(kind);b.path=directory+"/missing";expectFailure(kind,b);
        file("bad.txt",kind==0?"VTXCOLOR 2\nVC 1 2 3\n":"POINT_DATA_FROM_MESH mesh 2\nP 1 2 3\n");
        b=setup(kind);b.path=directory+"/bad.txt";expectFailure(kind,b);
    }
    for(const char* value:{"1e100 0 0","0 -1e100 0","0 0 1e100","nan 0 0"}){
        file("bad.txt",std::string("VTXCOLOR 3\nVC ")+value+" 1 2 3 4 5 6\n");
        auto b=setup(0);b.path=directory+"/bad.txt";expectFailure(0,b);
    }
    auto b=setup(0);wrongColorName=true;expectFailure(0,b);
    for(int mode:{1,2}){b=setup(0);nullMode=mode;expectFailure(0,b);}
    b=setup(1);nullMode=3;expectFailure(1,b);
    for(int mode:{1,4}){b=setup(2);nullMode=mode;expectFailure(2,b);}
    b=setup(0);b.meshType=MFnData::kInvalid;expectFailure(0,b);
    b=setup(0);b.controls["padding"]=-1;expectFailure(0,b);
    b=setup(0);meshes["input"].ids[0]=3;expectFailure(0,b);
    b=setup(0);meshes["input"].counts[0]=4;expectFailure(0,b);
    b=setup(0);meshes["input"].reportedCorners=-2;expectFailure(0,b);
    b=setup(0);meshes["input"].reportedCorners=100000001;expectFailure(0,b);
    b=setup(2);radii.pop_back();expectFailure(2,b);
    b=setup(2);positions[0].x=NAN;expectFailure(2,b);
    b=setup(2);radii[0]=-1;expectFailure(2,b);
    b=setup(2);positions[0].x=1e100;expectFailure(2,b);
    b=setup(2);b.controls["frequency"]=0;expectFailure(2,b);
    for(int kind:{0,1,2}){
        b=setup(kind);assert(compute(kind,b)==MS::kSuccess);const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at){
            if(trace[at]=="mesh.vertices" || trace[at]=="array.append" || trace[at]=="array.length"){
                b=setup(kind);shortAt=int(at);expectFailure(kind,b);
                if(kind==0 && trace[at]=="array.append")
                    assert(std::count(operations.begin(),operations.end(),"mesh.count")==1);
            }
        }
    }
    b=resetRun();assert(applyVtxColorFromFileNode().compute({{"unknown"}},b)==MS::kUnknownParameter);
    assert(ReadDPointDataNode().compute({{"unknown"}},b)==MS::kUnknownParameter);
    assert(foamGenerateNode().compute({{"unknown"}},b)==MS::kUnknownParameter);
    assert(callCount==0);unchanged();
}
}
int main(int argc,char** argv){assert(argc==2);directory=argv[1];testCompute();testInvalid();
    std::cout<<"Legacy IO/Foam malformed data, exact arrays, null objects and named colors passed\n";}

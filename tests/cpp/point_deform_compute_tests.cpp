#include "point_deform_doubles.h"
#include "point_deform_compute.inc"

void setup() {
    resetCalls();shortAt=-1;restCount=animatedCount=-1;edgeCount=3;
    published=cleaned=replaced=copied=bulkReads=0;
    early=stuck=badIndex=badFrame=nullInput=nullMesh=badMeshType=false;connected=true;
    badSearch=0;inputRadius=0.1f;
    restPoints={{0,0,0},{1,0,0},{0,1,0}};
    animatedPoints={{2,3,4},{3,3,4},{2,4,4}};
    adjacency={{1,2},{0,2},{0,1}};
    inputData=DPData();inputData.points={{0,0,0}};
    outputData=DPData();publishedData=DPData();publishedData.metadata="previous";
}
MStatus compute() {DPPointDeform node;MDataBlock block;return node.compute(MPlug(DPPointDeform::outDP),block);}
void unchanged() {assert(published==0&&cleaned==0&&publishedData.metadata=="previous");}
void expectPoint(float x,float y,float z) {
    assert(published==1&&cleaned==1&&publishedData.points.size()==1);
    const auto& p=publishedData.points[0];
    assert(std::fabs(p[0]-x)<1e-5f&&std::fabs(p[1]-y)<1e-5f&&std::fabs(p[2]-z)<1e-5f);
    assert(publishedData.metadata=="preserved");
}
void boundaries(bool fallback) {
    setup();if(fallback)inputData.points={{0.25f,0.25f,0}};
    assert(compute());const auto trace=operations;
    for(int at=0;at<static_cast<int>(trace.size());++at){
        for(bool exception:{false,true})for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            setup();if(fallback)inputData.points={{0.25f,0.25f,0}};
            injectedStatus=status;if(exception)attributeThrowAt=at;else failAt=at;
            const MStatus result=compute();
            assert(!result);
            const bool convertsFailure=trace[at]=="data.replace"||
                trace[at]=="data.copy"||trace[at]=="data.bounds"||trace[at]=="output.clean"||
                trace[at]=="frame"||trace[at]=="cloud"||trace[at].find("search.")==0U;
            if(!exception&&!convertsFailure)assert(result==status);
            assert(callCount==at+1);
            // setClean/final block clean occur after the void/native publication
            // boundary. No atomic rollback of those host operations is claimed.
            if(trace[at]!="output.clean"&&trace[at]!="block.clean")unchanged();
            assert(inputData.metadata=="preserved"&&inputData.points.size()==1);
        }
        if(trace[at]=="rest.points"||trace[at]=="animated.points"||trace[at]=="matrix.length"){
            setup();if(fallback)inputData.points={{0.25f,0.25f,0}};
            shortAt=at;assert(!compute());unchanged();
        }
    }
    std::cout<<"Point deform "<<(fallback?"fallback":"radius")<<": "<<trace.size()<<" failure/exception sites passed\n";
}
void numerics() {
    setup();assert(compute());expectPoint(2,3,4);assert(inputData.points[0][0]==0);
    assert(copied==1&&replaced==0&&std::count(operations.begin(),operations.end(),"data.bounds")==1);
    setup();inputRadius=10;assert(compute());expectPoint(2,3,4);
    setup();animatedPoints={{0,0,0},{0,1,0},{-1,0,0}};
    inputData.points={{0.025f,0.01f,0}};assert(compute());expectPoint(-0.01f,0.025f,0);
    setup();inputData.points={{0.25f,0.25f,0}};assert(compute());expectPoint(2.25f,3.25f,4);
    setup();inputData.points={{2e15f,0,0}};assert(compute());expectPoint(2e15f,0,0);
    setup();adjacency={{},{},{}};edgeCount=0;assert(compute());expectPoint(2,3,4);
    setup();restPoints={{0,0,0},{1,0,0},{2,0,0}};animatedPoints={{2,3,4},{3,3,4},{4,3,4}};
    assert(compute());expectPoint(2,3,4);
    // Translation across the complete finite float range: the displacement
    // itself exceeds float, but the final position is exactly representable.
    setup();const float maximum=std::numeric_limits<float>::max();
    restPoints={{maximum,0,0},{maximum,1,0},{maximum,0,1}};
    animatedPoints={{-maximum,0,0},{-maximum,1,0},{-maximum,0,1}};
    inputData.points={{maximum,0,0}};
    assert(compute());expectPoint(-maximum,0,0);
    setup();inputData.points.clear();assert(compute());assert(published==1&&bulkReads==0&&replaced==0);
    setup();DPPointDeform node;MDataBlock block;assert(node.compute(MPlug(MObject{"other"}),block)==MS::kUnknownParameter);assert(callCount==0);
}
void invalid() {
    for(int mode=0;mode<20;++mode){
        setup();
        switch(mode){
        case 0:early=true;break;
        case 1:stuck=true;break;
        case 2:badIndex=true;break;
        case 3:adjacency[0][0]=-1;break;
        case 4:adjacency[0][0]=3;break;
        case 5:edgeCount=0;break;
        case 6:restPoints[0].x=NAN;break;
        case 7:animatedPoints[0].z=INFINITY;break;
        case 8:badFrame=true;break;
        case 9:nullInput=true;break;
        case 10:nullMesh=true;break;
        case 11:connected=false;break;
        case 12:badSearch=1;break;
        case 13:badSearch=2;break;
        case 14:badSearch=3;break;
        case 15:inputData.valid=false;break;
        case 16:inputData.groups["bad"]=std::make_shared<Group>();break;
        case 17:restCount=animatedCount=0;break;
        case 18:badMeshType=true;break;
        case 19:badSearch=4;break;
        }
        assert(!compute());unchanged();assert(callCount<150);
    }
    for(float radius:{0.0f,-1.0f,float(NAN),float(INFINITY)}){
        setup();inputRadius=radius;assert(!compute());unchanged();assert(bulkReads==0&&copied==0);
        setup();inputData.points.clear();inputRadius=radius;assert(!compute());unchanged();
    }
    for(int mode=0;mode<8;++mode){
        setup();
        switch(mode){
        case 0:restCount=animatedCount=10000001;break;
        case 1:restCount=animatedCount=10000000;break;
        case 2:restCount=animatedCount=-2;break;
        case 3:animatedCount=4;break;
        case 4:edgeCount=-2;break;
        case 5:inputData.countOverride=100000;restCount=animatedCount=100;break;
        case 6:inputData.metadataBytes=64U*1024U*1024U;break;
        case 7:edgeCount=50000001;break;
        }
        assert(!compute());unchanged();assert(bulkReads==0&&copied==0);
    }
    // A genuinely unrepresentable final coordinate fails without publication.
    setup();const float maximum=std::numeric_limits<float>::max();
    inputData.points={{maximum,0,0}};inputRadius=maximum;
    restPoints={{maximum/2,0,0},{maximum/2,1,0},{maximum/2,0,1}};
    animatedPoints={{maximum,0,0},{maximum,1,0},{maximum,0,1}};
    assert(!compute());unchanged();
}
int main(){boundaries(false);boundaries(true);numerics();invalid();std::cout<<"Point deform traversal, admission, finite updates and legacy cases passed\n";}

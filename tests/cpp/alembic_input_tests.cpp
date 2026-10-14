#include "alembic_input_doubles.h"
#include "alembic_input_methods.inc"

bool DCAbcWriteJob::eval(double) {
    {std::ofstream output(path.c_str(),std::ios::binary);output<<"partial archive";assert(output.good());}
    (void)ddata_export_util::getVisibilityType(MPlug(10));
    if(currentShape==0)shapeAdmissionPoint(MObject(1000));
    else if(currentShape==1)shapeAdmissionCurve(MObject(1000));
    else shapeAdmissionMesh(MObject(1000));
    return true;
}
std::string readText(const std::string& path){std::ifstream input(path.c_str(),std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
int restoreFault=0,restoreFailures=0;
unsigned expectedClosedArchives=0U;
std::vector<std::string> restoreDestinations;
MStatus fixtureViewFrame(const MTime& time){
    lifecycleEvents.push_back("time.restore");
    assert(closedArchives==expectedClosedArchives);
    for(const auto& path:restoreDestinations)assert(readText(path)=="original");
    if(restoreFailures!=0){
        if(restoreFailures>0)--restoreFailures;
        if(restoreFault==2)throw std::bad_alloc();
        if(restoreFault==3)throw std::runtime_error("time restoration fault");
        return MS::kInvalidParameter;
    }
    MTime::currentValue()=time.value();return MS::kSuccess;
}
void resetInput(int mode=0) {
    plugs.clear();nodes.clear();operations.clear();failStep=-1;failureKind=admittedShapes=0;
    temporaryPaths.clear();
    for(int key:{1,2,3,10,11,12,20,21,22})plugs[key]=PlugState();
    for(int key:{1,2,3})plugs[key].connected=true;
    if(mode>0){plugs[20].node=100;nodes[100].type=MFn::kAnimCurveTimeToUnitless;nodes[100].attributes["i"]=21;}
    if(mode==1 || mode==2)plugs[10].sources={20};
    if(mode==2)plugs[21].connected=true;
    if(mode==3){plugs[10].sources={20};nodes[100].type=MFn::kMute;nodes[100].attributes["mute"]=22;plugs[22].boolean=false;}
    if(mode==4){plugs[10].array=true;plugs[10].elements={11,12};plugs[12].sources={20};}
    if(mode==5){plugs[10].compound=true;plugs[10].children={11,12};plugs[12].sources={20};}
    if(mode==6){plugs[10].sources={20};nodes[100].type=MFn::kInvalid;}
}
bool exportArchive(const std::string& destination) try {
    const std::set<double> frames{0};AbcA::TimeSamplingPtr time(new AbcA::TimeSampling);
    AbcA::TimeSamplingPtr bounds(new AbcA::TimeSampling);
    PendingAbcWriteJob job(destination,true,frames,time,frames,time,bounds,JobArgs());
    assert(observedBoundsTime==bounds);
    if(!job.eval(0))return false;job.commit();return true;
} catch(...){return false;}

MStatus completeArchives(bool remaining) try {
    dommy::maya::ScopedCurrentTimeRestore timeRestore;
    dommy::maya::ScopedComputation computation;
    MTime::currentValue()=24.0;
    std::list<PendingAbcWriteJobPtr> jobList;
    std::vector<PendingAbcWriteJobPtr> allJobs;
    const std::set<double> frames{0};AbcA::TimeSamplingPtr time(new AbcA::TimeSampling);
    for(const auto& path:restoreDestinations){
        auto job=std::make_shared<PendingAbcWriteJob>(path,true,frames,time,frames,time,time,JobArgs());
        assert(job->eval(0));allJobs.push_back(job);
    }
    if(remaining)jobList.push_back(allJobs.back());
    return finalizeAbcJobs(jobList,allJobs,timeRestore,computation);
}catch(...){return MS::kFailure;}

void finalization(const std::string& destination){
    restoreDestinations={destination,destination+".second"};expectedClosedArchives=2U;
    for(int fault:{0,1,2,3})for(bool transient:{false,true}){
        resetInput();currentShape=0;closedArchives=0U;lifecycleEvents.clear();
        MTime::currentValue()=12.0;restoreFault=fault;restoreFailures=fault?(transient?1:-1):0;
        MComputation::beginCount()=MComputation::endCount()=0;
        for(const auto& path:restoreDestinations){std::ofstream out(path);out<<"original";}
        const MStatus status=completeArchives(false);
        assert((status!=MS::kSuccess)==(fault!=0));
        if(fault==1)assert(status==MS::kInvalidParameter);
        assert(closedArchives==2U&&lifecycleEvents.size()==(fault?4U:3U));
        assert(lifecycleEvents[0]=="archive.close"&&lifecycleEvents[1]=="archive.close"&&
               lifecycleEvents[2]=="time.restore");
        assert(MComputation::beginCount()==1&&MComputation::endCount()==1);
        assert(MTime::currentValue()==(fault&&!transient?24.0:12.0));
        for(const auto& path:restoreDestinations){
            assert(readText(path)==(fault?"original":"partial archive"));
        }
        assert(temporaryPaths.size()==2U);
        for(const auto& path:temporaryPaths)assert(!dommy::file::isRegularFile(path));
    }
    resetInput();closedArchives=0U;lifecycleEvents.clear();
    MTime::currentValue()=12.0;restoreFault=restoreFailures=0;
    for(const auto& path:restoreDestinations){std::ofstream out(path);out<<"original";}
    assert(completeArchives(true)==MS::kFailure);
    for(const auto& path:restoreDestinations){assert(readText(path)=="original");std::remove(path.c_str());}
    restoreDestinations.clear();
}

template<typename Run>void lengthRejected(Run run){bool failed=false;try{run();}catch(const std::length_error&){failed=true;}assert(failed);}
void nestedSampling(int depth){
    resetInput(3);plugs[22].boolean=true;
    for(int i=0;i<depth;++i){auto& p=plugs[1000+i];p.compound=true;
        p.children={200000+i,1000+i+1};plugs[200000+i].sources={20};}
    plugs[1000+depth]=PlugState();
}

int main(int argc,char** argv) {
    assert(argc==2);const std::string destination=argv[1];
    for(int shape=0;shape<3;++shape)for(int mode=0;mode<=6;++mode) {
        currentShape=shape;resetInput(mode);
        const int expected=mode==0?0:(mode==2 || mode==6)?1:2;
        assert(ddata_export_util::getSampledType(MPlug(10))==expected);
        if(mode==3){plugs[22].boolean=true;assert(ddata_export_util::getSampledType(MPlug(10))==0);plugs[22].boolean=false;}
        for(bool visible:{false,true}) {
            plugs[10].boolean=visible;
            assert(ddata_export_util::getVisibilityType(MPlug(10))==(expected==0?(visible?0:1):(visible?3:2)));
        }
        resetInput(mode);assert(exportArchive(destination) && admittedShapes==1);
        const auto trace=operations;
        for(std::size_t step=0;step<trace.size();++step)for(int kind:{1,2,3}) {
            {std::ofstream output(destination.c_str(),std::ios::binary);output<<"original";assert(output.good());}
            resetInput(mode);failStep=int(step);failureKind=kind;
            assert(!exportArchive(destination));
            assert(readText(destination)=="original" && !dommy::file::isRegularFile(temporaryPath));
            resetInput(mode);assert(exportArchive(destination) && admittedShapes==1);
        }
        const auto query=std::find(trace.rbegin(),trace.rend(),"plug.connected");
        assert(query!=trace.rend());
        for(bool connectionValue:{false,true}) {
            resetInput(mode);plugs[shape+1].connected=connectionValue;
            failStep=int(trace.size()-1U-std::size_t(query-trace.rbegin()));failureKind=1;
            assert(!exportArchive(destination) && admittedShapes==0);
        }
        resetInput(mode);plugs[shape+1].connected=false;
        assert(exportArchive(destination) && admittedShapes==0);
    }
    nestedSampling(256);assert(ddata_export_util::getSampledType(MPlug(1000))==0);
    nestedSampling(257);lengthRejected([&]{ddata_export_util::getSampledType(MPlug(1000));});
    nestedSampling(1024);lengthRejected([&]{ddata_export_util::getSampledType(MPlug(1000));});
    resetInput();dommy::generation::WorkBudget empty(0);
    lengthRejected([&]{ddata_export_util::getSampledType(MPlug(10),empty);});assert(operations.empty());
    resetInput();dommy::generation::WorkBudget shared(1);
    assert(ddata_export_util::getSampledType(MPlug(10),shared)==0);
    lengthRejected([&]{ddata_export_util::getSampledType(MPlug(10),shared);});
    for(int mode:{4,5}){resetInput(mode);plugs[10].countOverride=1000001;
        lengthRejected([&]{ddata_export_util::getSampledType(MPlug(10));});
        assert(std::find(operations.begin(),operations.end(),mode==4?"plug.element":"plug.child")==operations.end());}
    // A muted/static first child still permits a sampled sibling; positive
    // results keep their historical physical-order priority (1 versus 2).
    resetInput(3);plugs[22].boolean=true;plugs[10].compound=true;plugs[10].sources.clear();
    plugs[10].children={11,12};plugs[11].sources={20};plugs[30].node=101;
    nodes[101].type=MFn::kAnimCurveTimeToUnitless;nodes[101].attributes["i"]=31;plugs[31].connected=true;
    plugs[12].sources={30};assert(ddata_export_util::getSampledType(MPlug(10))==1);
    plugs[22].boolean=false;assert(ddata_export_util::getSampledType(MPlug(10))==2);
    std::swap(plugs[10].children[0],plugs[10].children[1]);assert(ddata_export_util::getSampledType(MPlug(10))==1);
    std::remove(destination.c_str());
    finalization(destination);
}

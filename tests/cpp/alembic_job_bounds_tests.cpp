#include "alembic_job_bounds_host.h"
#include "job_bounds_methods.inc"

// Construction/host/native archive are declared seams. The complete production
// setup, eval, bounds and callbacks plus command time conversion execute below.
DCAbcWriteJob::DCAbcWriteJob(const char*,bool,const std::set<double>& transforms,
    Alembic::AbcCoreAbstract::TimeSamplingPtr transformTime,const std::set<double>& shapes,
    Alembic::AbcCoreAbstract::TimeSamplingPtr shapeTime,Alembic::AbcCoreAbstract::TimeSamplingPtr boundsTime,const JobArgs& args) : mArgs(args) {
    mShapeTimeIndex=mTransTimeIndex=1;
    mBoundsTimeIndex=mBoundsSamples=0;
    mTransSamples=mShapeSamples=1;mFileName="fixture";mAsOgawa=true;
    mTransFrames=transforms;mShapeFrames=shapes;
    mTransTime=transformTime;mShapeTime=shapeTime;mBoundsTime=boundsTime;
    if(!transforms.empty()&&!shapes.empty()){
        mFirstFrame=*transforms.begin();mLastFrame=std::max(*transforms.rbegin(),*shapes.rbegin());
    }
}
DCAbcWriteJob::~DCAbcWriteJob()=default;
std::unique_ptr<DCAbcWriteJob> job() {
    return std::unique_ptr<DCAbcWriteJob>(new DCAbcWriteJob("fixture",true,{},nullptr,{},nullptr,nullptr,JobArgs()));
}
void reset() {
    calls.clear();failAt=-1;kind=0;dp.count=dc.count=dm.count=1;
    nodes.assign(5,HostNode());nodes[0].children={1,2,3,4};
    nodes[1].type=10;nodes[2].type=11;nodes[3].type=12;nodes[4].type=2;
}
void setup(DCAbcWriteJob& value,std::size_t allowance=1000,std::size_t depth=0) {
    dommy::generation::WorkBudget work(allowance);
    value.setup(0,MayaTransformWriterPtr(),kNoExportParent,depth,work);
}
template<typename Run>void rejected(Run run) {bool failed=false;try{run();}catch(const std::exception&){failed=true;}assert(failed);}
template<typename Run>void pendingRejection(const std::string& destination,Run run) {
    std::string temporary;
    rejected([&]{dommy::file::PendingOutputFile output(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=output.trustedTemporaryPath();run();});
    std::ifstream input(destination);
    assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing");
    assert(!dommy::file::isRegularFile(temporary));
}
bool same(const MBoundingBox& a,const MBoundingBox& b) {
    return a.min().x==b.min().x&&a.min().y==b.min().y&&a.min().z==b.min().z&&
           a.max().x==b.max().x&&a.max().y==b.max().y&&a.max().z==b.max().z;
}
void timeline(bool subframes,const std::set<double>& shutters,bool explicitRange=true,bool singleFrame=false){
    reset();callbackFrames.clear();
    dommy::sampling::AbcFrameRange range;range.startTime=1;range.endTime=explicitRange&&!singleFrame?2:1;
    range.shutterSamples=shutters;dommy::sampling::AbcSamplingPlan plan;
    assert(dommy::sampling::buildAbcSamplingPlan({range},explicitRange,subframes,plan));
    AbcA::TimeSamplingPtr transformTime,shapeTime,boundsTime;
    assert(createTimeSampling(plan.transformTime,ddata_export_util::spf(),transformTime));
    assert(createTimeSampling(plan.shapeTime,ddata_export_util::spf(),shapeTime));
    assert(createTimeSampling(plan.boundsTime,ddata_export_util::spf(),boundsTime));
    JobArgs args;args.melPerFrameCallback="record";
    DCAbcWriteJob value("fixture",true,plan.transformFrames,transformTime,plan.shapeFrames,shapeTime,boundsTime,args);
    std::set<double> frames=plan.transformFrames;frames.insert(plan.shapeFrames.begin(),plan.shapeFrames.end());
    MDagPath path;Alembic::Abc::OObject object;
    auto transform=std::make_shared<MayaTransformWriter>(object,path,1,args);
    value.mBoundsTransforms.push_back({transform,kNoExportParent});
    double current=0;value.trackShapeBounds([&]{return MBoundingBox(MPoint(current,0,0),MPoint(current+1,1,1));},0);
    for(double frame:frames){current=frame;assert(value.eval(frame)==(frame==*frames.rbegin()));}
    assert(value.mBoxProp.boxes.size()==frames.size()&&value.mBoundsSamples==frames.size());
    assert(value.mBoxProp.timeIndex==value.mBoundsTimeIndex);
    std::size_t index=0;
    for(double frame:frames){
        assert(value.mBoxProp.boxes[index].min.x==frame);
        if(explicitRange)assert(std::fabs(value.mRoot.state->times.at(value.mBoxProp.timeIndex).getSampleTime(index)/ddata_export_util::spf()-frame)<1e-12);
        assert(callbackFrames.at(index)==frame);++index;
    }
    assert(callbackFrames.size()==frames.size());
    if(value.mBoundsTimeIndex)
        assert(value.mRoot.state->counts.at(std::to_string(value.mBoundsTimeIndex)+".samples")==frames.size());
    const auto before=value.mBoxProp.boxes.size();value.eval(1.1);
    assert(value.mBoxProp.boxes.size()==before&&callbackFrames.size()==before);
    Alembic::Abc::boxSetFails=true;const auto count=value.mBoundsSamples;
    rejected([&]{value.perFrameCallback(3);});
    assert(value.mBoundsSamples==count&&callbackFrames.size()==before);
    Alembic::Abc::boxSetFails=false;
    value.mBoundsSamples=std::numeric_limits<unsigned>::max();
    rejected([&]{value.perFrameCallback(3);});assert(value.mBoxProp.boxes.size()==before);
}
void sharedTimeMetadata(){
    reset();auto time=std::make_shared<AbcA::TimeSampling>(1.0/24.0,0);
    DCAbcWriteJob value("fixture",true,{0,1,2},time,{0,1,2,3},time,time,JobArgs());
    for(int i=0;i<=3;++i)value.eval(i);
    assert(value.mTransSamples==3&&value.mShapeSamples==4&&value.mBoundsSamples==4);
    assert(value.mTransTimeIndex==value.mShapeTimeIndex&&value.mBoundsTimeIndex==value.mTransTimeIndex);
    assert(value.mRoot.state->counts.at(std::to_string(value.mTransTimeIndex)+".samples")==4);
}
void statistics(const std::string& destination){
    reset();std::set<double> frames;for(int i=-1;i<5000;++i)frames.insert(i);
    auto time=std::make_shared<AbcA::TimeSampling>(1.0/24.0,-1.0/24.0);
    DCAbcWriteJob value("fixture",true,frames,time,frames,time,time,JobArgs());
    assert(!value.eval(-1));MDagPath path;Alembic::Abc::OObject parent;
    auto points=std::make_shared<ShapeWriter>(path,parent,1,JobArgs());
    points->pointCount=points->curveCVs=1000000;
    value.mPointList.push_back(points);value.mCurveList.push_back(points);value.mMeshList.push_back(points);
    for(int i=0;i<5000;++i)assert(value.eval(i)==(i==4999));
    const std::uint64_t expected=UINT64_C(5000000000);
    assert(value.mStats.mDPAnimPts==expected&&value.mStats.mDCAnimCVs==expected&&value.mStats.mDMAnimVtxs==expected);
    const auto& text=value.mRoot.state->strings.at("statistics");
    assert(text.find("DPAnimPts 5000000000 ")!=std::string::npos&&
        text.find("DCAnimCVs 5000000000 ")!=std::string::npos&&
        text.find("DMAnimVtxs 5000000000 ")!=std::string::npos);
    std::uint64_t total=std::numeric_limits<std::uint64_t>::max()-1;
    AbcWriteJobStatistics::add(total,1);assert(total==std::numeric_limits<std::uint64_t>::max());
    rejected([&]{AbcWriteJobStatistics::add(total,1);});
    assert(total==std::numeric_limits<std::uint64_t>::max());
    std::string maximum;addToString(maximum,"max",total);assert(maximum=="max 18446744073709551615 ");
    for(int which=0;which<3;++which){
        value.mStats.mDPAnimPts=value.mStats.mDCAnimCVs=value.mStats.mDMAnimVtxs=0;
        auto& counter=which==0?value.mStats.mDPAnimPts:(which==1?value.mStats.mDCAnimCVs:value.mStats.mDMAnimVtxs);
        counter=std::numeric_limits<std::uint64_t>::max()-500000;
        const auto before=counter;
        pendingRejection(destination,[&]{value.eval(0);});assert(counter==before);
    }
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string destination=argv[1];{std::ofstream out(destination);out<<"existing";}
    reset();JobArgs collisionArgs;MDagPath first,second;first.path={0,1};second.path={2,1};
    collisionArgs.dagPaths.insert(first);collisionArgs.dagPaths.insert(second);
    auto time=std::make_shared<AbcA::TimeSampling>();
    DCAbcWriteJob collision("fixture",true,{0,1},time,{0,1},time,time,collisionArgs);
    pendingRejection(destination,[&]{collision.eval(0);});
    assert(!collision.mRoot.valid()&&collision.mPointList.empty()&&collision.mBoundsShapes.empty());
    reset();auto value=job();setup(*value);
    assert(value->mBoundsShapes.size()==3&&value->mBoundsTransforms.size()==1);
    assert(value->mPointList.size()==1&&value->mCurveList.size()==1&&value->mMeshList.size()==1);
    const auto bounds=value->getBoundingBox();assert(bounds.min().x==1&&bounds.max().z==6);
    const auto setupCalls=calls.size();reset();value=job();setup(*value);const auto recorded=calls;
    for(std::size_t i=0;i<recorded.size();++i)for(int fault=1;fault<=3;++fault) {
        reset();value=job();failAt=int(i);kind=fault;pendingRejection(destination,[&]{setup(*value);});
        reset();value=job();setup(*value);assert(value->mBoundsShapes.size()==3);
    }
    (void)setupCalls;
    reset();value=job();value->mArgs.useSelectionList=true;setup(*value);const auto selectedCalls=calls;
    for(std::size_t i=0;i<selectedCalls.size();++i)if(selectedCalls[i]=="selection") {
        reset();value=job();value->mArgs.useSelectionList=true;failAt=int(i);kind=1;
        rejected([&]{setup(*value);});
    }
    reset();nodes[2].selected=false;value=job();value->mArgs.useSelectionList=true;setup(*value);
    assert(value->mBoundsShapes.size()==2&&value->mCurveList.empty());
    reset();dp.count=0;value=job();setup(*value);assert(value->mBoundsShapes.size()==2);
    reset();nodes[1].connected=false;value=job();setup(*value);assert(value->mBoundsShapes.size()==2);
    reset();nodes[1].type=99;value=job();setup(*value);assert(value->mBoundsShapes.size()==2);
    reset();value=job();rejected([&]{setup(*value,1000,kMaxExportDagDepth+1);});
    reset();nodes[0].children.clear();value=job();rejected([&]{setup(*value,0);});
    reset();value=job();rejected([&]{setup(*value,2);});
    reset();nodes.assign(259,HostNode());for(unsigned i=0;i+1<nodes.size();++i)nodes[i].children={i+1};
    value=job();rejected([&]{setup(*value);});

    // The archive uses retained writer samples, not current scene geometry.
    reset();value=job();setup(*value);nodes[1].bounds={MPoint(-100,-100,-100),MPoint(100,100,100)};
    assert(same(value->getBoundingBox(),bounds));
    reset();value=job();value->mShapeTimeIndex=value->mTransTimeIndex=0;setup(*value);
    assert(value->mPointList.empty()&&value->mCurveList.empty()&&value->mMeshList.empty());
    assert(value->mBoundsShapes.size()==3&&same(value->getBoundingBox(),bounds));
    // Child matrix order, inherits=false, reflection/shear and all 8 corners.
    reset();nodes.resize(3);nodes[0].children={1};nodes[1].type=1;nodes[1].children={2};nodes[2].type=10;
    nodes[0].matrix[3][0]=10;nodes[1].matrix[3][1]=20;
    value=job();setup(*value);auto b=value->getBoundingBox();assert(b.min().x==11&&b.min().y==22);
    value->mBoundsTransforms[1].writer->inherits=false;b=value->getBoundingBox();assert(b.min().x==1&&b.min().y==22);
    value->mBoundsTransforms[1].writer->matrix[0][0]=-1;
    value->mBoundsTransforms[1].writer->matrix[1][0]=2;
    b=value->getBoundingBox();assert(b.min().x==0&&b.max().x==9&&b.min().z==3&&b.max().z==6);
    // Shape and transform samples can update independently.
    value->mBoundsShapes[0].read=[] {return MBoundingBox(MPoint(0,0,0),MPoint(1,1,1));};
    b=value->getBoundingBox();assert(b.min().x==-1&&b.max().x==2);
    value->mBoundsTransforms[1].writer->matrix[3][0]=5;
    b=value->getBoundingBox();assert(b.min().x==4&&b.max().x==7);
    reset();value=job();setup(*value);calls.clear();value->getBoundingBox();const auto n=calls.size();
    for(std::size_t i=0;i<n;++i)for(int fault=1;fault<=3;++fault){calls.clear();failAt=int(i);kind=fault;
        pendingRejection(destination,[&]{value->getBoundingBox();});failAt=-1;assert(same(value->getBoundingBox(),bounds));}
    reset();value=job();setup(*value);value->mBoundsTransforms[0].parent=0;
    rejected([&]{value->getBoundingBox();});
    reset();value=job();setup(*value);value->mBoundsShapes[0].parent=999;
    rejected([&]{value->getBoundingBox();});
    reset();value=job();setup(*value);value->mBoundsTransforms[0].writer->matrix[0][0]=std::numeric_limits<double>::infinity();
    rejected([&]{value->getBoundingBox();});
    value->mBoundsTransforms[0].writer->matrix[0][0]=1;value->mBoundsTransforms[0].writer->matrix[0][3]=1;
    rejected([&]{value->getBoundingBox();});
    reset();value=job();setup(*value);value->mBoundsShapes[0].read=[] {return MBoundingBox(MPoint(2,0,0),MPoint(1,1,1));};
    rejected([&]{value->getBoundingBox();});
    rejected([&]{requireExportRecordStorage(1000001,0,16,16);});
    rejected([&]{requireExportRecordStorage(500000,500000,128,128);});
    reset();value=job();assert(same(value->getBoundingBox(),MBoundingBox()));
    calls.clear();assert(!ddata_export_util::isIntermediate(MObject()));const auto queries=calls.size();
    for(std::size_t i=0;i<queries;++i){calls.clear();failAt=int(i);kind=1;rejected([&]{ddata_export_util::isIntermediate(MObject());});}
    reset();value=job();failAt=0;kind=1;std::string temporary;
    try {dommy::file::PendingOutputFile output(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=output.trustedTemporaryPath();setup(*value);output.commit();assert(false);}catch(const std::exception&){}
    std::ifstream input(destination);assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing");
    assert(!dommy::file::isRegularFile(temporary));
    reset();value=job();setup(*value);assert(value->mBoundsShapes.size()==3);
    timeline(false,{-0.25,0.25}); // 4 transform times, 2 shape times, 5 bounds times.
    timeline(false,{-0.25});timeline(false,{0.25});timeline(false,{0.0});
    timeline(true,{-0.25,0.25});timeline(true,{0.0},false);
    timeline(false,{-0.25},true,true); // Bounds reuse the containing shape timeline.
    statistics(destination);
    sharedTimeMetadata();
}

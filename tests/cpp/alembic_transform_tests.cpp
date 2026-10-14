#include "alembic_transform_host.h"
#include "transform_methods.inc"

void resetTransform(){calls.clear();archiveNames.clear();nodeName="node";failAt=-1;failureKind=0;spline=fbik=invalidOrder=missingPlug=ancestorFailure=inheritsOnly=badEuler=badSampleMatrix=controlsOnly=false;dagDepth=2;values.clear();}
template<class Callback>void faults(Callback callback){resetTransform();callback();const auto count=calls.size();
    for(std::size_t i=0;i<count;++i)for(int kind=1;kind<=3;++kind){resetTransform();failAt=int(i);failureKind=kind;
        bool failed=false;try{callback();}catch(const std::exception&){failed=true;}assert(failed);
        resetTransform();callback();}}
bool sameSample(const Alembic::AbcGeom::XformSample& a,const Alembic::AbcGeom::XformSample& b){
    if(a.inherits!=b.inherits||a.ops.size()!=b.ops.size())return false;
    for(std::size_t i=0;i<a.ops.size();++i)if(a.ops[i].values!=b.ops[i].values)return false;return true;}

int main(int argc,char** argv){assert(argc==2);
    for(bool joint:{false,true})for(bool world:{false,true})for(bool filtered:{false,true})for(unsigned time:{0U,1U})
        faults([&]{Alembic::Abc::OObject parent;MDagPath path;path.joint=joint;JobArgs args;args.worldSpace=world;args.filterEulerRotations=filtered;
            MayaTransformWriter writer(parent,path,time,args);});
    resetTransform();Alembic::Abc::OObject parent;MDagPath path;JobArgs args;args.filterEulerRotations=true;
    MayaTransformWriter writer(parent,path,1,args);
    for(bool joint:{false,true})for(unsigned depth:{0U,1U,2U,std::numeric_limits<unsigned>::max()}) {
        resetTransform();nodeName="outer:inner:node";MDagPath namedPath;namedPath.joint=joint;
        JobArgs namedArgs;namedArgs.stripNamespace=depth;
        const std::string expected=depth==0U?"outer:inner:node":depth==1U?"inner:node":"node";
        MayaTransformWriter namedRoot(parent,namedPath,1,namedArgs);
        MayaTransformWriter namedChild(writer,namedPath,1,namedArgs);
        assert(archiveNames.size()==2U&&archiveNames[0]==expected&&archiveNames[1]==expected);
    }
    for(bool joint:{false,true}) {
        faults([&]{nodeName="outer:inner:node";MDagPath namedPath;namedPath.joint=joint;
            JobArgs namedArgs;namedArgs.stripNamespace=1U;
            MayaTransformWriter namedRoot(parent,namedPath,1,namedArgs);});
        faults([&]{nodeName="outer:inner:node";MDagPath namedPath;namedPath.joint=joint;
            JobArgs namedArgs;namedArgs.stripNamespace=1U;
            MayaTransformWriter namedChild(writer,namedPath,1,namedArgs);});
    }
    resetTransform();
    assert(writer.getSampleMatrix()[0][0]==1);
    resetTransform();badSampleMatrix=true;bool matrixFailed=false;
    try{writer.getSampleMatrix();}catch(const std::invalid_argument&){matrixFailed=true;}assert(matrixFailed);
    resetTransform();failAt=0;failureKind=1;matrixFailed=false;
    try{writer.getSampleMatrix();}catch(const std::runtime_error&){matrixFailed=true;}assert(matrixFailed);
    resetTransform();
    for(bool joint:{false,true})for(bool filtered:{false,true})for(unsigned time:{0U,1U})
        faults([&]{MDagPath child;child.joint=joint;JobArgs childArgs;childArgs.filterEulerRotations=filtered;
            MayaTransformWriter childWriter(writer,child,time,childArgs);});
    for(bool driven:{false,true})faults([&]{spline=driven;MStatus status;MFnIkJoint node(path,&status);
        ddata_export_util::requireExportQuery(status,"bind");ddata_export_util::isDrivenByFBIK(node);ddata_export_util::isDrivenBySplineIK(node);});
    resetTransform();writer.write();const std::size_t count=calls.size();
    const auto committed=writer.mSample;const auto history=writer.mPrevRotateSolution;
    const auto frames=writer.mSchema.state->samples.size();
    for(std::size_t i=0;i<count;++i)for(int kind=1;kind<=3;++kind){
        resetTransform();values["translateX"]=20;values["rotateX"]=0.5;failAt=int(i);failureKind=kind;
        bool failed=false;try{writer.write();}catch(const std::exception&){failed=true;}assert(failed);
        assert(sameSample(writer.mSample,committed)&&writer.mPrevRotateSolution.x==history.x);
        assert(writer.mSchema.state->samples.size()==frames);
    }
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        resetTransform();values["translateX"]=bad;bool failed=false;
        try{writer.write();}catch(const std::invalid_argument&){failed=true;}assert(failed&&sameSample(writer.mSample,committed));}
    resetTransform();values["rotateX"]=std::numeric_limits<double>::max();bool failed=false;
    try{writer.write();}catch(const std::invalid_argument&){failed=true;}assert(failed);
    resetTransform();badEuler=true;failed=false;
    try{writer.write();}catch(const std::invalid_argument&){failed=true;}assert(failed&&sameSample(writer.mSample,committed));
    resetTransform();inheritsOnly=true;MDagPath transform;transform.joint=false;JobArgs inheritArgs;
    MayaTransformWriter inheritWriter(parent,transform,1,inheritArgs);
    assert(inheritWriter.mAnimChanList.empty()&&inheritWriter.isAnimated());
    values["inheritsTransform"]=0;inheritWriter.write();
    assert(!inheritWriter.mSample.getInheritsXforms()&&inheritWriter.mSchema.state->samples.size()==1);
    resetTransform();invalidOrder=true;failed=false;
    try{MayaTransformWriter invalid(parent,path,1,args);}catch(const std::invalid_argument&){failed=true;}assert(failed);
    resetTransform();missingPlug=true;failed=false;
    try{MayaTransformWriter invalid(parent,path,1,args);}catch(const std::runtime_error&){failed=true;}assert(failed);
    resetTransform();MDagPath empty;empty.depth=0;args.worldSpace=true;failed=false;
    try{MayaTransformWriter invalid(parent,empty,1,args);}catch(const std::length_error&){failed=true;}assert(failed);
    const std::string destination=argv[1];{std::ofstream out(destination);out<<"existing";}
    resetTransform();values["translateX"]=std::numeric_limits<double>::infinity();std::string temporary;
    try{dommy::file::PendingOutputFile file(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=file.trustedTemporaryPath();writer.write();file.commit();assert(false);}
    catch(const std::invalid_argument&){}
    std::ifstream input(destination);const std::string saved((std::istreambuf_iterator<char>(input)),{});
    assert(saved=="existing"&&!dommy::file::isRegularFile(temporary));
    resetTransform();writer.write();assert(writer.mSchema.state->samples.size()==frames+1);

    for(bool world:{false,true})for(bool filtered:{false,true}) {
        resetTransform();controlsOnly=true;values["rotateX"]=0.3;values["rotateY"]=0.4;
        MDagPath orderPath;orderPath.joint=false;JobArgs orderArgs;
        orderArgs.worldSpace=world;orderArgs.filterEulerRotations=filtered;
        MayaTransformWriter orderWriter(parent,orderPath,1,orderArgs);
        assert(orderWriter.isAnimated()&&orderWriter.mAnimChanList.empty());
        const auto sample=orderWriter.mSample;const auto previous=orderWriter.mPrevRotateSolution;
        const auto count=orderWriter.mSchema.state->samples.size();
        values["rotateOrder"]=5;bool rejected=false;
        try{orderWriter.write();}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&sameSample(sample,orderWriter.mSample));
        assert(orderWriter.mSchema.state->samples.size()==count&&
               orderWriter.mPrevRotateSolution.x==previous.x);
        values["rotateOrder"]=0;orderWriter.write();
        assert(orderWriter.mSchema.state->samples.size()==count+1U);
    }
    for(bool initial:{false,true}) {
        resetTransform();values["segmentScaleCompensate"]=initial;values["inverseScaleX"]=2;
        JobArgs controlArgs;MayaTransformWriter controlWriter(parent,path,1,controlArgs);
        const auto sample=controlWriter.mSample;const auto count=controlWriter.mSchema.state->samples.size();
        values["segmentScaleCompensate"]=!initial;bool rejected=false;
        try{controlWriter.write();}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&sameSample(sample,controlWriter.mSample)&&controlWriter.mSchema.state->samples.size()==count);
        values["segmentScaleCompensate"]=initial;controlWriter.write();
    }
    resetTransform();JobArgs worldArgs;worldArgs.worldSpace=true;
    MayaTransformWriter worldWriter(parent,path,1,worldArgs);
    const auto worldSample=worldWriter.mSample;values["inheritsTransform"]=0;failed=false;
    try{worldWriter.write();}catch(const std::invalid_argument&){failed=true;}
    assert(failed&&sameSample(worldWriter.mSample,worldSample));
    resetTransform();controlsOnly=true;MDagPath staticPath;staticPath.joint=false;JobArgs staticArgs;
    MayaTransformWriter staticWriter(parent,staticPath,0,staticArgs);assert(!staticWriter.isAnimated());
    resetTransform();JobArgs nullArgs;nullArgs.exportAttributeBudget.reset();failed=false;
    try{MayaTransformWriter invalid(parent,path,1,nullArgs);}catch(const std::invalid_argument&){failed=true;}
    assert(failed&&calls.empty());
    resetTransform();JobArgs fullMetadata;
    fullMetadata.exportAttributeBudget->retainInspectionStorage(64U*1024U*1024U);failed=false;
    try{MayaTransformWriter invalid(parent,path,1,fullMetadata);}catch(const std::length_error&){failed=true;}
    assert(failed&&std::find(calls.begin(),calls.end(),"plug.int")==calls.end());
    for (unsigned int time : {0U, 1U}) {
        resetTransform();JobArgs exhaustedArgs;auto& work=exhaustedArgs.exportAttributeBudget->inspectionWork();
        assert(work.consume(work.remaining()));failed=false;
        try{MayaTransformWriter invalid(parent,path,time,exhaustedArgs);}catch(const std::length_error&){failed=true;}
        assert(failed&&std::find(calls.begin(),calls.end(),"plug.int")==calls.end());
    }
    resetTransform();dagDepth=257;MDagPath deepPath;worldArgs.worldSpace=true;failed=false;
    try{MayaTransformWriter invalid(parent,deepPath,1,worldArgs);}catch(const std::length_error&){failed=true;}
    assert(failed&&std::find(calls.begin(),calls.end(),"path.pop")==calls.end());
    resetTransform();controlsOnly=true;values["rotateOrder"]=6;failed=false;
    try{MayaTransformWriter invalid(parent,staticPath,1,staticArgs);}catch(const std::invalid_argument&){failed=true;}
    assert(failed);
    resetTransform();values["rotateOrder"]=6;failed=false;
    MStatus controlStatus;MFnDependencyNode controlNode(staticPath,&controlStatus);
    try{staticWriter.trackStructuralControl(controlNode,"rotateOrder",false,true);}
    catch(const std::invalid_argument&){failed=true;}assert(failed);
    resetTransform();controlsOnly=true;values["rotateX"]=0.3;
    MayaTransformWriter atomicWriter(parent,staticPath,1,staticArgs);
    values["rotateOrder"]=5;failed=false;temporary.clear();
    try{dommy::file::PendingOutputFile owner(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=owner.trustedTemporaryPath();
        atomicWriter.write();owner.commit();}catch(const std::invalid_argument&){failed=true;}
    assert(failed&&!dommy::file::isRegularFile(temporary));
    std::ifstream preserved(destination);
    assert(std::string((std::istreambuf_iterator<char>(preserved)),{})=="existing");
}

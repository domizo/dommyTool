#include "alembic_attribute_transport.h"
#include "attribute_state_methods.inc"
#include <iterator>

using Budget=dommy::attribute::ExportAttributeBudget;
using State=dommy::attribute::ExportAttributeState;
using Domain=dommy::attribute::ExportAttributeDomain;

template<typename Run>void rejected(Run run) {
    bool failed=false;try{run();}catch(const std::exception&){failed=true;}assert(failed);
}
template<typename T>DPAttributeData::ConstPtr field(std::size_t n,T value) {
    return std::make_shared<TypedDPAttributeData<T>>(std::vector<T>(n,value),value);
}
Collection collection() {
    Collection value;value.owners={std::make_shared<DPData>(),std::make_shared<DPData>()};
    value.owners[0]->points=3;value.owners[1]->points=1;
    value.owners[0]->faces=2;value.owners[1]->faces=4;return value;
}
template<typename T>Collection populated(T value,T other) {
    auto result=collection();result.fields["primitive"]=std::make_shared<TypedDPAttributeData<T>>(std::vector<T>{value,other},value);
    result.owners[0]->fields["points"]=field<T>(3,value);result.owners[1]->fields["points"]=field<T>(1,other);
    return result;
}
void emit(DPointWriter& writer,const DPData& data,const std::list<std::string>& pointNames) {
    writer.mAttributeState.prepare(ddata_export_util::planPointExportAttributes(data,pointNames,writer.mAttributeState));
    writer.getDPointPointAttribute(data,pointNames,false);
    writer.mAttributeState.requireComplete(writer.pointAttributeList.size());
    transport("geometry.main");writer.mAttributeState.commit();
}
void emit(DCurveWriter& writer,const Collection& data,const std::list<std::string>& primitiveNames,
          const std::list<std::string>& pointNames) {
    writer.mAttributeState.prepare(ddata_export_util::planCurveExportAttributes(data,primitiveNames,pointNames,writer.mAttributeState));
    writer.getDCurvePrimAttribute(data,primitiveNames,false);writer.getDCurvePointAttribute(data,pointNames,false);
    writer.mAttributeState.requireComplete(writer.pointAttributeList.size());
    transport("geometry.main");writer.mAttributeState.commit();
}
void emit(DMeshWriter& writer,const Collection& data,const std::list<std::string>& primitiveNames,
          const std::list<std::string>& pointNames) {
    writer.mAttributeState.prepare(ddata_export_util::planMeshExportAttributes(data,primitiveNames,pointNames,writer.mAttributeState));
    writer.getDMeshPrimAttribute(data,primitiveNames,false);writer.getDMeshPointAttribute(data,pointNames,true);
    writer.mAttributeState.requireComplete(writer.pointAttributeList.size());
    transport("geometry.main");writer.mAttributeState.commit();
}
template<typename T>bool equal(T a,T b) {
    using Parts=dommy::numeric::ValueComponents<T>;
    for(unsigned axis=0;axis<Parts::count;++axis)if(Parts::get(a,axis)!=Parts::get(b,axis))return false;return true;
}
template<typename U>const std::vector<U>& values(const Alembic::Abc::Record& record,std::size_t sample=0) {
    return std::dynamic_pointer_cast<const Alembic::Abc::Values<U>>(record.frames.at(sample).payload)->data;
}
template<typename T,typename U>void allBranches(T value,T other) {
    auto budget=std::make_shared<Budget>();U a,b;assert(dommy::numeric::checkedValueCast(value,a)&&dommy::numeric::checkedValueCast(other,b));
    {
        DPData data;data.fields["points"]=field<T>(2,value);DPointWriter point(budget);
        emit(point,data,{"points"});emit(point,data,{"points"});
        const auto p=point.mSchema.records->at("points");assert(p->scope==3&&p->frames.size()==1&&values<U>(*p).size()==2&&equal(values<U>(*p)[0],a));
        auto sources=populated(value,other);DCurveWriter curve(budget);
        emit(curve,sources,{"primitive"},{"points"});emit(curve,sources,{"primitive"},{"points"});
        const auto c=curve.mSchema.records->at("primitive"),cp=curve.mSchema.records->at("points");
        assert(c->scope==1&&values<U>(*c).size()==2&&equal(values<U>(*c)[1],b));
        assert(cp->scope==3&&values<U>(*cp).size()==4&&cp->frames.size()==1&&equal(values<U>(*cp)[3],b));
        DMeshWriter mesh(budget);emit(mesh,sources,{"primitive"},{"points"});emit(mesh,sources,{"primitive"},{"points"});
        const auto m=mesh.mSchema.records->at("primitive"),mp=mesh.mSchema.records->at("points");
        assert(m->scope==1&&values<U>(*m).size()==6&&m->frames.size()==1);
        for(std::size_t i=0;i<6;++i)assert(equal(values<U>(*m)[i],i<2?a:b));
        assert(mp->scope==3&&mp->frames.size()==2&&values<U>(*mp,1).size()==4);
        assert(budget->used().fields==5);
    }
    assert(budget->used().fields==0&&budget->used().storageBytes==0);
}
void resetFaults(){calls.clear();failAt=-1;kind=0;}
template<typename Run>void pendingRejected(const std::string& destination,Run run) {
    std::string temporary;
    rejected([&]{dommy::file::PendingOutputFile output(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=output.trustedTemporaryPath();run();});
    std::ifstream input(destination);assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing");
    assert(!dommy::file::isRegularFile(temporary));
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string destination=argv[1];{std::ofstream out(destination);out<<"existing";}
    allBranches<bool,std::uint8_t>(true,false);
    allBranches<std::int32_t,std::int32_t>(7,-3);allBranches<std::uint32_t,std::uint32_t>(4000000000U,3U);
    allBranches<std::int64_t,std::int64_t>(INT64_MAX,INT64_MIN);
    allBranches<float,float>(2.5f,-1.5f);allBranches<double,double>(2.5,-1.5);
    allBranches<Imath::V2i,Imath::V2i>(Imath::V2i(2,3),Imath::V2i(-1,-2));
    allBranches<Imath::V2f,Imath::V2f>(Imath::V2f(2,3),Imath::V2f(-1,-2));
    allBranches<Imath::V2d,Imath::V2d>(Imath::V2d(2,3),Imath::V2d(-1,-2));
    allBranches<Imath::V3i,Imath::V3i>(Imath::V3i(2,3,4),Imath::V3i(-1,-2,-3));
    allBranches<Imath::V3f,Imath::V3f>(Imath::V3f(2,3,4),Imath::V3f(-1,-2,-3));
    allBranches<Imath::V3d,Imath::V3d>(Imath::V3d(2,3,4),Imath::V3d(-1,-2,-3));

    auto budget=std::make_shared<Budget>();auto source=populated(10,20);
    source.fields["same"]=source.fields["primitive"];for(auto& owner:source.owners)owner->fields["same"]=owner->fields["points"];
    for(bool mesh:{false,true}) {
        resetFaults();pendingRejected(destination,[&]{if(mesh){DMeshWriter w(budget);emit(w,source,{"same"},{"same"});}
            else{DCurveWriter w(budget);emit(w,source,{"same"},{"same"});}});assert(calls.empty());
    }
    resetFaults();pendingRejected(destination,[&]{DMeshWriter w(budget);emit(w,source,{}, {"points","points"});});assert(calls.empty());
    DPData pointData;pointData.fields["points"]=field<int>(2,10);DPointWriter staticPoint(budget);emit(staticPoint,pointData,{"points"});
    pointData.points=3;pointData.fields["points"]=field<int>(3,10);
    pendingRejected(destination,[&]{emit(staticPoint,pointData,{"points"});});assert(staticPoint.mAttributeState.samples()==1);
    source=populated(10,20);DCurveWriter staticCurve(budget);emit(staticCurve,source,{}, {"points"});
    source.owners[0]->points=2;source.owners[1]->points=2;
    for(auto& owner:source.owners)owner->fields["points"]=field<int>(2,10);
    pendingRejected(destination,[&]{emit(staticCurve,source,{}, {"points"});});assert(staticCurve.mAttributeState.samples()==1);
    source=populated(10,20);DCurveWriter staticPrimitive(budget);emit(staticPrimitive,source,{"primitive"},{});
    source.owners.push_back(std::make_shared<DPData>());source.fields["primitive"]=field<int>(3,10);
    pendingRejected(destination,[&]{emit(staticPrimitive,source,{"primitive"},{});});
    source=populated(10,20);DMeshWriter staticFaces(budget);emit(staticFaces,source,{"primitive"},{});
    source.owners[0]->faces=1;source.owners[1]->faces=5;
    pendingRejected(destination,[&]{emit(staticFaces,source,{"primitive"},{});});
    source=populated(10,20);DMeshWriter dynamic(budget);emit(dynamic,source,{}, {"points"});
    for(auto& owner:source.owners)owner->fields.clear();
    pendingRejected(destination,[&]{emit(dynamic,source,{}, {"points"});});assert(dynamic.mAttributeState.samples()==1);
    source=populated(30,40);emit(dynamic,source,{}, {"points"});
    assert(dynamic.mAttributeState.samples()==2&&values<int>(*dynamic.mSchema.records->at("points"),1)[0]==30);
    pendingRejected(destination,[&]{emit(dynamic,source,{},{});});
    for(auto& owner:source.owners)owner->fields["points"]=field<float>(owner->points,1.0f);
    pendingRejected(destination,[&]{emit(dynamic,source,{}, {"points"});});
    source=populated(10,20);DMeshWriter late(budget);emit(late,source,{}, {"later"});
    for(auto& owner:source.owners)owner->fields["later"]=owner->fields["points"];
    pendingRejected(destination,[&]{emit(late,source,{}, {"later"});});assert(late.pointAttributeList.empty());
    DPData unsupported;unsupported.fields["text"]=field<std::string>(2,"text");DPointWriter omitted(budget);
    emit(omitted,unsupported,{"text","absent"});assert(omitted.pointAttributeList.empty());
    source=populated(10,20);DMeshWriter damaged(budget);emit(damaged,source,{}, {"points"});
    auto record=damaged.mSchema.records->at("points");record->frames.push_back(record->frames.front());
    pendingRejected(destination,[&]{emit(damaged,source,{}, {"points"});});
    DAbcPointAttrList missing;int datum=10;pendingRejected(destination,[&]{DSetAbcAttributeValues("absent",missing,Alembic::AbcGeom::Int32ArraySample(&datum,1),1);});
    DPointWriter unexpected(budget);auto existing=std::make_shared<Alembic::Abc::Record>();(*unexpected.mSchema.records)["points"]=existing;
    pointData.points=2;pointData.fields["points"]=field<int>(2,10);
    pendingRejected(destination,[&]{emit(unexpected,pointData,{"points"});});
    DPointWriter mismatched(budget);mismatched.mAttributeState.prepare(
        ddata_export_util::planPointExportAttributes(pointData,{"points"},mismatched.mAttributeState));
    auto parent=mismatched.mSchema.getArbGeomParams();Alembic::AbcGeom::OInt32GeomParam::Sample badScope;
    int pair[2]={1,2};badScope.setVals(Alembic::AbcGeom::Int32ArraySample(pair,2));badScope.setScope(Alembic::AbcGeom::kUniformScope);
    pendingRejected(destination,[&]{publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
        mismatched.mAttributeState,parent,mismatched.pointAttributeList,"points",badScope,
        Alembic::AbcGeom::kVertexScope,DP_ATTR_INT,1,false);});
    badScope.setScope(static_cast<Alembic::AbcGeom::GeometryScope>(127));
    pendingRejected(destination,[&]{publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
        mismatched.mAttributeState,parent,mismatched.pointAttributeList,"points",badScope,
        static_cast<Alembic::AbcGeom::GeometryScope>(127),DP_ATTR_INT,1,false);});
    assert(mismatched.pointAttributeList.empty());

    // Every observed typed archive API seam: first frame and later samples.
    for(bool initial:{true,false}) {
        resetFaults();auto shared=std::make_shared<Budget>();DMeshWriter normal(shared);source=populated(10,20);
        if(!initial)emit(normal,source,{"primitive"},{"points"});calls.clear();emit(normal,source,{"primitive"},{"points"});
        const auto count=calls.size();
        for(std::size_t i=0;i<count;++i)for(int fault:{1,2}) {
            resetFaults();auto clean=std::make_shared<Budget>();
            {DMeshWriter writer(clean);if(!initial)emit(writer,source,{"primitive"},{"points"});
                calls.clear();failAt=int(i);kind=fault;
                pendingRejected(destination,[&]{emit(writer,source,{"primitive"},{"points"});});
                assert(writer.mAttributeState.samples()==(initial?0U:1U));}
            assert(clean->used().fields==0);resetFaults();DMeshWriter fresh(clean);emit(fresh,source,{"primitive"},{"points"});
        }
    }
    resetFaults();State incomplete(budget);auto frame=incomplete.beginFrame();frame.add("unwritten",Domain::Point,1,true);incomplete.prepare(std::move(frame));
    rejected([&]{incomplete.commit();});assert(!incomplete.initialized());
    State identity(budget);auto first=identity.beginFrame();first.add("existing",Domain::Point,1,true);
    identity.prepare(std::move(first));identity.markWritten("existing");identity.commit();
    auto added=identity.beginFrame();added.add("existing",Domain::Point,1,true);added.add("added",Domain::Point,1,true);
    rejected([&]{identity.prepare(std::move(added));});
    auto changed=identity.beginFrame();changed.add("existing",Domain::Point,2,true);
    rejected([&]{identity.prepare(std::move(changed));});
    dommy::attribute::ExportAttributeCost excessive;excessive.fields=1000001;
    rejected([&]{budget->require(excessive);});excessive={};excessive.nameBytes=100000001;
    rejected([&]{budget->require(excessive);});excessive={};excessive.storageBytes=std::numeric_limits<std::size_t>::max();
    rejected([&]{budget->require(excessive);});
    // Aggregated metadata across writers: the ninth million-entry layout
    // must be rejected before allocating it. Previous reservations survive.
    auto aggregate=std::make_shared<Budget>();std::vector<std::unique_ptr<State>> holders;
    for(unsigned i=0;i<8;++i) {
        auto state=std::unique_ptr<State>(new State(aggregate));auto f=state->beginFrame();f.add("layout",Domain::Primitive,1,false);
        f.setLayout(Domain::Primitive,1000000,[](std::size_t){return 1U;});state->prepare(std::move(f));
        state->markWritten("layout");state->commit();holders.push_back(std::move(state));
    }
    State ninth(aggregate);auto overflow=ninth.beginFrame();overflow.add("layout",Domain::Primitive,1,false);
    rejected([&]{overflow.setLayout(Domain::Primitive,1000000,[](std::size_t){return 1U;});});
    assert(aggregate->used().fields==8);holders.clear();assert(aggregate->used().fields==0);
}

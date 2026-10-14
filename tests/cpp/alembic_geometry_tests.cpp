#include "alembic_geometry_host.h"
#include "geometry_methods.inc"

void resetGeometry(){calls.clear();archiveNames.clear();failAt=-1;kind=0;missingPlug=shortArrays=false;stringLength=0;
    numbers.clear();numbers["aConstantWidth"]=1.0;numbers["aIsMultiEndKnot"]=1.0;
    strings.clear();strings["aVelocityAttributeName"]="velocity";strings["aWidthAttributeName"]="width";
    velocityPresent=widthPresent=false;widthSize=-1;widthValue=1;knotValue=0.5;
    points=DPData();curves=DCurveData();meshes=DMeshData();
    curves.parts.push_back(std::make_shared<GeometryPart>());
    meshes.parts.push_back(std::make_shared<GeometryPart>());meshes.parts.push_back(std::make_shared<GeometryPart>());
}
template<typename Callback>void constructorFaults(Callback callback){resetGeometry();callback();const auto n=calls.size();
    for(std::size_t i=0;i<n;++i)for(int fault=1;fault<=3;++fault){resetGeometry();failAt=int(i);kind=fault;
        bool failed=false;try{callback();}catch(const std::exception&){failed=true;}assert(failed);
        resetGeometry();callback();}}
std::size_t geometryCount(const DPointWriter& w){return w.mPointCount;}
std::size_t geometryCount(const DCurveWriter& w){return w.mCVCount;}
std::size_t geometryCount(const DMeshWriter& w){return w.mPtCount;}
template<typename Writer>void frameFaults(Writer& writer){
    resetGeometry();calls.clear();writer.write();const auto n=calls.size();
    const auto frames=writer.mSchema.state->frames.size();const auto old=writer.mSchema.state->frames.back().positions;
    const auto previousCount=geometryCount(writer);
    const auto previousBounds=writer.mSampleBounds;
    for(std::size_t i=0;i<n;++i)for(int fault=1;fault<=3;++fault){resetGeometry();
        points.positions[0][0]=20;curves.parts[0]->positions[0][0]=20;meshes.parts[0]->positions[0][0]=20;
        failAt=int(i);kind=fault;bool failed=false;
        try{writer.write();}catch(const std::exception&){failed=true;}
        assert(failed&&writer.mSchema.state->frames.size()==frames);
        assert(geometryCount(writer)==previousCount);
        assert(writer.mSampleBounds.min().x==previousBounds.min().x&&
               writer.mSampleBounds.max().z==previousBounds.max().z);
        assert(writer.mSchema.state->frames.back().positions[0].values[0]==old[0].values[0]);
    }
    // The command abandons the entire archive after failure. Child schemas
    // may already contain samples, so recovery means a fresh writer/archive,
    // not claiming that an old native backend can undo partial writes.
    resetGeometry();MDagPath path;Alembic::Abc::OObject parent;JobArgs args;
    Writer retried(path,parent,1,args);assert(retried.mSchema.state->frames.size()==1);
}
template<typename Callback>void requireGeometryFailure(Callback callback){bool failed=false;try{callback();}catch(const std::exception&){failed=true;}assert(failed);}
template<typename Writer>void rejectBuiltinFrame(Writer& writer){
    calls.clear();const auto frames=writer.mSchema.state->frames.size();const auto count=geometryCount(writer);
    const auto bounds=writer.mSampleBounds;
    requireGeometryFailure([&]{writer.write();});
    assert(writer.mSchema.state->frames.size()==frames&&geometryCount(writer)==count);
    assert(writer.mSampleBounds.min().x==bounds.min().x&&writer.mSampleBounds.max().z==bounds.max().z);
    for(const auto& call:calls)assert(call!="archive.set"&&call!="attributes.point"&&call!="attributes.primitive");
}
int main(int argc,char** argv){assert(argc==2);MDagPath path;Alembic::Abc::OObject parent;JobArgs args;
    for(unsigned time:{0U,1U}){
        constructorFaults([&]{DPointWriter w(path,parent,time,args);assert(w.getNumPoints()==2);});
        constructorFaults([&]{DCurveWriter w(path,parent,time,args);assert(w.getNumCVs()==3);});
        constructorFaults([&]{DMeshWriter w(path,parent,time,args);assert(w.getNumPoints()==6);});
        constructorFaults([&]{MayaLocatorWriter w(path,parent,time,args);});
    }
    for(unsigned depth:{0U,1U,2U,std::numeric_limits<unsigned>::max()}) {
        JobArgs namedArgs;namedArgs.stripNamespace=depth;
        const std::string expected=depth==0U?"outer:inner:shape":depth==1U?"inner:shape":"shape";
        resetGeometry();strings["node.name"]="outer:inner:shape";
        DPointWriter namedPoint(path,parent,1,namedArgs);assert(archiveNames.front()==expected);
        resetGeometry();strings["node.name"]="outer:inner:shape";
        DCurveWriter namedCurve(path,parent,1,namedArgs);assert(archiveNames.front()==expected);
        resetGeometry();strings["node.name"]="outer:inner:shape";
        DMeshWriter namedMesh(path,parent,1,namedArgs);
        assert(archiveNames.size()==3U&&archiveNames[0]==expected&&
               archiveNames[1]==expected+"_0"&&archiveNames[2]==expected+"_1");
        resetGeometry();strings["node.name"]="outer:inner:shape";
        MayaLocatorWriter namedLocator(path,parent,1,namedArgs);assert(archiveNames.front()==expected);
    }
    JobArgs namespaceArgs;namespaceArgs.stripNamespace=1U;
    constructorFaults([&]{strings["node.name"]="outer:inner:shape";DPointWriter w(path,parent,1,namespaceArgs);});
    constructorFaults([&]{strings["node.name"]="outer:inner:shape";DCurveWriter w(path,parent,1,namespaceArgs);});
    constructorFaults([&]{strings["node.name"]="outer:inner:shape";DMeshWriter w(path,parent,1,namespaceArgs);});
    constructorFaults([&]{strings["node.name"]="outer:inner:shape";MayaLocatorWriter w(path,parent,1,namespaceArgs);});
    constructorFaults([&]{velocityPresent=widthPresent=true;numbers["aPointWidthType"]=1;
        DPointWriter w(path,parent,1,args);assert(w.mSchema.state->frames.back().velocities.size()==2U);});
    constructorFaults([&]{curves.uniform=true;numbers["aCurveWidthType"]=1;
        DCurveWriter w(path,parent,1,args);assert(w.mSchema.state->frames.back().widths.size()==1U);});
    constructorFaults([&]{curves.vertex=true;numbers["aCurveWidthType"]=1;
        DCurveWriter w(path,parent,1,args);assert(w.mSchema.state->frames.back().widths.size()==3U);});
    resetGeometry();DMeshWriter mesh(path,parent,1,args);
    assert(mesh.mSampleBounds.min().x==1&&mesh.mSampleBounds.max().z==9);
    assert(archiveNames.size()==3&&archiveNames[1]=="shape_0"&&archiveNames[2]=="shape_1");
    frameFaults(mesh);const auto frames=mesh.mSchema.state->frames.size();
    resetGeometry();const auto schemas=mesh.mTransSchemaList;mesh.mTransSchemaList.pop_back();
    const auto firstSchemaFrames=mesh.mTransSchemaList.front().state->frames.size();
    requireGeometryFailure([&]{mesh.write();});
    assert(mesh.mTransSchemaList.front().state->frames.size()==firstSchemaFrames);
    mesh.mTransSchemaList=schemas;
    for(std::size_t count:{0U,1U,3U}){resetGeometry();meshes.parts.resize(count,std::make_shared<GeometryPart>());
        const auto first=mesh.mTransSchemaList[0].state->frames.size();const auto second=mesh.mTransSchemaList[1].state->frames.size();
        requireGeometryFailure([&]{mesh.write();});assert(mesh.mSchema.state->frames.size()==frames);
        assert(mesh.mTransSchemaList[0].state->frames.size()==first&&mesh.mTransSchemaList[1].state->frames.size()==second);
    }
    resetGeometry();DPointWriter point(path,parent,1,args);frameFaults(point);
    resetGeometry();DCurveWriter curve(path,parent,1,args);frameFaults(curve);
    resetGeometry();MayaLocatorWriter locator(path,parent,1,args);calls.clear();locator.write();const auto n=calls.size();
    const auto locatorFrames=locator.mSp.state->frames.size();
    for(std::size_t i=0;i<n;++i)for(int fault=1;fault<=3;++fault){resetGeometry();numbers["localPositionX"]=20;failAt=int(i);kind=fault;
        requireGeometryFailure([&]{locator.write();});assert(locator.mSp.state->frames.size()==locatorFrames);}
    resetGeometry();locator.write();assert(locator.mSp.state->frames.size()==locatorFrames+1);
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        resetGeometry();numbers["localPositionX"]=bad;requireGeometryFailure([&]{locator.write();});
        resetGeometry();numbers["aConstantWidth"]=bad;requireGeometryFailure([&]{point.write();});
        resetGeometry();numbers["aConstantWidth"]=bad;requireGeometryFailure([&]{curve.write();});
        resetGeometry();knotValue=bad;requireGeometryFailure([&]{curve.write();});
    }
    resetGeometry();knotValue=std::numeric_limits<double>::max();requireGeometryFailure([&]{curve.write();});
    resetGeometry();numbers["aConstantWidth"]=-1;requireGeometryFailure([&]{point.write();});
    resetGeometry();numbers["aConstantWidth"]=-1;requireGeometryFailure([&]{curve.write();});
    resetGeometry();numbers["aPointWidthType"]=2;requireGeometryFailure([&]{point.write();});
    resetGeometry();numbers["aCurveWidthType"]=2;requireGeometryFailure([&]{curve.write();});
    resetGeometry();shortArrays=true;requireGeometryFailure([&]{mesh.write();});
    resetGeometry();shortArrays=true;requireGeometryFailure([&]{curve.write();});
    resetGeometry();points.valid=false;requireGeometryFailure([&]{point.write();});
    resetGeometry();curves.parts[0]->valid=false;requireGeometryFailure([&]{curve.write();});
    resetGeometry();meshes.parts[0]->valid=false;requireGeometryFailure([&]{mesh.write();});
    resetGeometry();points.positions.clear();requireGeometryFailure([&]{point.write();});
    resetGeometry();curves.parts.clear();requireGeometryFailure([&]{curve.write();});
    resetGeometry();points.reportedPoints=3000000;requireGeometryFailure([&]{DPointWriter invalid(path,parent,1,args);});assert(archiveNames.empty());
    resetGeometry();curves.parts[0]->reportedPoints=3000000;requireGeometryFailure([&]{DCurveWriter invalid(path,parent,1,args);});assert(archiveNames.empty());
    resetGeometry();meshes.parts[0]->reportedPoints=3000000;requireGeometryFailure([&]{DMeshWriter invalid(path,parent,1,args);});assert(archiveNames.empty());
    resetGeometry();missingPlug=true;requireGeometryFailure([&]{MayaLocatorWriter invalid(path,parent,1,args);});
    resetGeometry();numbers["aPointWidthType"]=1;widthPresent=true;DPointWriter vertexPoint(path,parent,1,args);
    vertexPoint.write();assert(vertexPoint.mSchema.state->frames.back().widths.size()==2);
    assert(vertexPoint.mSchema.state->frames.back().widthScope==Alembic::AbcGeom::kVertexScope);
    for(const auto& c:calls)assert(c!="plug.aConstantWidth");
    points.positions.push_back(DPoint{{10,11,12}});widthValue=2.0f;vertexPoint.write();
    assert(vertexPoint.mSchema.state->frames.back().widths.size()==3U);
    assert(vertexPoint.mSchema.state->frames.back().widths[2]==2.0f);
    points.positions.resize(2U);vertexPoint.write();assert(vertexPoint.mSchema.state->frames.back().widths.size()==2U);
    resetGeometry();point.write();for(const auto& c:calls)assert(c!="plug.aWidthAttributeName");
    numbers["aConstantWidth"]=2.0;point.write();assert(point.mSchema.state->frames.back().widths[0]==2.0f);
    resetGeometry();numbers["aPointWidthType"]=1;widthPresent=true;rejectBuiltinFrame(point);
    resetGeometry();numbers["aPointWidthType"]=1;widthPresent=true;widthSize=1;requireGeometryFailure([&]{vertexPoint.write();});
    resetGeometry();numbers["aPointWidthType"]=1;rejectBuiltinFrame(vertexPoint);
    resetGeometry();rejectBuiltinFrame(vertexPoint);
    resetGeometry();velocityPresent=true;DPointWriter velocityPoint(path,parent,1,args);
    assert(velocityPoint.mSchema.state->frames.back().velocities.size()==2U);
    assert(velocityPoint.mSchema.state->frames.back().velocities[1][2]==6.0f);
    widthValue=7.0f;velocityPoint.write();
    assert(velocityPoint.mSchema.state->frames.back().velocities[0][0]==1.0f);
    resetGeometry();velocityPresent=true;widthSize=1;requireGeometryFailure([&]{velocityPoint.write();});
    resetGeometry();rejectBuiltinFrame(velocityPoint);
    resetGeometry();velocityPresent=true;rejectBuiltinFrame(point);
    resetGeometry();velocityPresent=true;points.positions.push_back(DPoint{{10,11,12}});velocityPoint.write();
    assert(velocityPoint.mSchema.state->frames.back().velocities.size()==3U);
    assert(velocityPoint.mSchema.state->frames.back().velocities[2][2]==12.0f);
    points.positions.resize(2U);velocityPoint.write();assert(velocityPoint.mSchema.state->frames.back().velocities.size()==2U);
    resetGeometry();numbers["aPointWidthType"]=1;DPointWriter absentPoint(path,parent,1,args);absentPoint.write();
    assert(absentPoint.mSchema.state->widthScope<0&&absentPoint.mSchema.state->frames.back().widths.empty());
    widthPresent=true;rejectBuiltinFrame(absentPoint);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.uniform=true;DCurveWriter uniformCurve(path,parent,1,args);
    uniformCurve.write();assert(uniformCurve.mSchema.state->frames.back().widths.size()==1);
    assert(uniformCurve.mSchema.state->frames.back().widthScope==Alembic::AbcGeom::kUniformScope);
    curves.parts.push_back(std::make_shared<GeometryPart>());auto widthAttribute=std::make_shared<DPAttributeData>();
    widthAttribute->size=2U;curves.attribute=widthAttribute;uniformCurve.write();
    assert(uniformCurve.mSchema.state->frames.back().widths.size()==2U);
    curves.parts.resize(1U);widthAttribute->size=1U;uniformCurve.write();
    assert(uniformCurve.mSchema.state->frames.back().widths.size()==1U);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.vertex=true;DCurveWriter vertexCurve(path,parent,1,args);
    vertexCurve.write();assert(vertexCurve.mSchema.state->frames.back().widths.size()==3);
    curves.parts[0]->positions.push_back(DPoint{{10,11,12}});vertexCurve.write();
    assert(vertexCurve.mSchema.state->frames.back().widths.size()==4U);
    curves.parts[0]->positions.resize(3U);vertexCurve.write();
    assert(vertexCurve.mSchema.state->frames.back().widths.size()==3U);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.vertex=true;rejectBuiltinFrame(uniformCurve);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.uniform=true;rejectBuiltinFrame(vertexCurve);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.vertex=true;rejectBuiltinFrame(curve);
    resetGeometry();numbers["aCurveWidthType"]=1;rejectBuiltinFrame(vertexCurve);
    resetGeometry();numbers["aCurveWidthType"]=1;curves.vertex=true;widthSize=1;requireGeometryFailure([&]{vertexCurve.write();});
    resetGeometry();numbers["aCurveWidthType"]=1;curves.uniform=true;widthSize=0;requireGeometryFailure([&]{uniformCurve.write();});
    resetGeometry();numbers["aCurveWidthType"]=1;DCurveWriter absentCurve(path,parent,1,args);absentCurve.write();
    assert(absentCurve.mSchema.state->widthScope<0&&absentCurve.mSchema.state->frames.back().widths.empty());
    curves.vertex=true;rejectBuiltinFrame(absentCurve);
    for(bool firstOpen:{false,true}){
        resetGeometry();curves.parts[0]->open=firstOpen;curves.parts.push_back(std::make_shared<GeometryPart>());
        curves.parts[1]->open=!firstOpen;
        requireGeometryFailure([&]{DCurveWriter mixed(path,parent,1,args);});assert(archiveNames.empty());
        resetGeometry();curves.parts[0]->open=firstOpen;curves.parts.push_back(std::make_shared<GeometryPart>());
        curves.parts[1]->open=firstOpen;DCurveWriter homogeneous(path,parent,1,args);
        assert(homogeneous.mSchema.state->frames.back().wrap==
            (firstOpen?Alembic::AbcGeom::kNonPeriodic:Alembic::AbcGeom::kPeriodic));
        curves.parts[1]->open=!firstOpen;rejectBuiltinFrame(homogeneous);
        curves.parts[0]->open=!firstOpen;homogeneous.write();
        assert(homogeneous.mSchema.state->frames.back().wrap==
            (firstOpen?Alembic::AbcGeom::kPeriodic:Alembic::AbcGeom::kNonPeriodic));
    }
    resetGeometry();curves.parts[0]->degree=2;curves.parts.push_back(std::make_shared<GeometryPart>());
    DCurveWriter variedOrders(path,parent,1,args);const auto& variedSample=variedOrders.mSchema.state->frames.back();
    assert(variedSample.type==Alembic::AbcGeom::kVariableOrder&&variedSample.orders.size()==2U);
    assert(variedSample.orders[0]==3U&&variedSample.orders[1]==2U&&variedSample.knots.size()==10U);
    ddata_export_util::BuiltinSampleState builtin;
    assert(!builtin.initialized());
    requireGeometryFailure([&]{builtin.require(true,Alembic::AbcGeom::kUnknownScope,false);});
    builtin.require(false,Alembic::AbcGeom::kUnknownScope,false);assert(!builtin.initialized());
    builtin.commit(false,Alembic::AbcGeom::kUnknownScope,false);assert(builtin.initialized());
    requireGeometryFailure([&]{builtin.require(true,Alembic::AbcGeom::kConstantScope,false);});
    for(auto scope:{Alembic::AbcGeom::kVaryingScope,Alembic::AbcGeom::kFacevaryingScope})
        requireGeometryFailure([&]{ddata_export_util::BuiltinSampleState state;state.require(true,scope,false);});
    resetGeometry();curves.uniform=true;curves.attribute.reset();strings["aExportCurveAttribute"]="selected";
    requireGeometryFailure([&]{curve.write();});
    resetGeometry();meshes.uniform=true;meshes.attribute.reset();strings["aExportMeshAttribute"]="selected";
    requireGeometryFailure([&]{mesh.write();});
    resetGeometry();meshes.uniform=true;auto primitive=std::make_shared<DPAttributeData>();primitive->size=2;meshes.attribute=primitive;
    strings["aExportMeshAttribute"]="selected";DMeshWriter staticMesh(path,parent,1,args);
    meshes.parts[0]->faces=0;meshes.parts[1]->faces=2;calls.clear();
    requireGeometryFailure([&]{staticMesh.write();});
    for(const auto& call:calls)assert(call!="archive.set");
    meshes.uniform=false;calls.clear();requireGeometryFailure([&]{staticMesh.write();});
    for(const auto& call:calls)assert(call!="archive.set");
    meshes.uniform=true;meshes.parts[0]->faces=1;meshes.parts[1]->faces=1;staticMesh.write();
    resetGeometry();meshes.uniform=true;primitive->type=DP_ATTR_STRING;meshes.attribute=primitive;
    strings["aExportMeshAttribute"]="selected";DMeshWriter unsupportedMesh(path,parent,1,args);
    assert(unsupportedMesh.pointAttributeList.empty());
    resetGeometry();meshes.uniform=meshes.vertex=true;primitive->type=DP_ATTR_INT;meshes.attribute=primitive;
    strings["aExportMeshAttribute"]=strings["aExportPointAttribute"]="same";
    requireGeometryFailure([&]{DMeshWriter collision(path,parent,1,args);});
    for(const auto& call:calls)assert(call!="archive.set");
    resetGeometry();meshes.vertex=true;strings["aExportPointAttribute"]="selected";
    DMeshWriter dynamicMesh(path,parent,1,args);const auto dynamicFrames=dynamicMesh.mSchema.state->frames.size();
    meshes.vertex=false;requireGeometryFailure([&]{dynamicMesh.write();});
    assert(dynamicMesh.mSchema.state->frames.size()==dynamicFrames);
    meshes.vertex=true;meshes.pointType=DP_ATTR_INT;requireGeometryFailure([&]{dynamicMesh.write();});
    meshes.pointType=DP_ATTR_FLOAT;strings["aExportPointAttribute"]="later";
    requireGeometryFailure([&]{dynamicMesh.write();});
    strings["aExportPointAttribute"]="selected";dynamicMesh.write();
    assert(dynamicMesh.mAttributeState.samples()==2);
    resetGeometry();auto pointField=std::make_shared<DPAttributeData>();pointField->size=2;
    points.attributes["selected"]=pointField;strings["aExportPointAttribute"]="selected";
    DPointWriter staticPoint(path,parent,1,args);points.positions.push_back(DPoint{{1,2,3}});pointField->size=3;
    requireGeometryFailure([&]{staticPoint.write();});assert(staticPoint.mAttributeState.samples()==1);
    resetGeometry();curves.vertex=true;curves.parts.push_back(std::make_shared<GeometryPart>());
    strings["aExportPointAttribute"]="selected";DCurveWriter staticCurves(path,parent,1,args);
    curves.parts[0]->positions.resize(2);curves.parts[1]->positions.resize(4);
    requireGeometryFailure([&]{staticCurves.write();});assert(staticCurves.mAttributeState.samples()==1);
    resetGeometry();dommy::maya::NameListBudget budget;strings["names"]="  one\ttwo\nthree \r\nfour ";
    const auto names=ddata_export_util::readExportNameList(MPlug(MString("names")),3,budget);
    assert(names.size()==4&&names.front()=="one"&&names.back()=="four"&&budget.workItems==12);
    const auto previous=budget;
    stringLength=dommy::generation::kMaxGeneratedStringByteCount+1;
    requireGeometryFailure([&]{ddata_export_util::readExportNameList(MPlug(MString("names")),3,budget);});
    assert(budget.bytes==previous.bytes&&budget.names==previous.names);
    resetGeometry();strings["names"]="one two";budget.names=dommy::generation::kMaxGeneratedStringCount;
    requireGeometryFailure([&]{ddata_export_util::readExportNameList(MPlug(MString("names")),1,budget);});
    const std::string destination=argv[1];{std::ofstream out(destination);out<<"existing";}
    for(unsigned reason=0U;reason<4U;++reason){
        resetGeometry();
        if(reason==0U)numbers["aConstantWidth"]=std::numeric_limits<double>::quiet_NaN();
        if(reason==1U){numbers["aPointWidthType"]=1;widthPresent=true;}
        if(reason==3U){curves.parts.push_back(std::make_shared<GeometryPart>());curves.parts[1]->open=false;}
        std::string temporary;try{dommy::file::PendingOutputFile file(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=file.trustedTemporaryPath();
            if(reason==2U)velocityPoint.write();else if(reason==3U)curve.write();else point.write();
            file.commit();assert(false);}catch(const std::exception&){}
        std::ifstream input(destination);assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing"&&!dommy::file::isRegularFile(temporary));
    }
    resetGeometry();point.write();curve.write();mesh.write();
}

#include "alembic_sample_doubles.h"
#include "sample_methods.inc"
#include <iterator>

void resetSample(){failAt=-1;failKind=reads=0;arrayCount=2U;multi=true;
    usedAsColor=false;
    nodeAttributeCount=1U;uniqueAttributeNames=false;attributeReadable=true;Alembic::nativePropertyConstructions=0;
    Alembic::failNativeProperty=false;
    numericKind=MFnNumericData::k3Double;typedKind=MFnData::kNumeric;
    stringValues={MString("a"),MString("b")};}

AbcA::DataType numericSchema(int kind){
    // Independent fixture expectations, not the production schema mapper.
    using namespace Alembic::Util;
    const PlainOldDataType pods[]={kBooleanPOD,kBooleanPOD,kInt8POD,kInt8POD,kInt16POD,
        kInt32POD,kFloat32POD,kFloat64POD,kInt16POD,kInt16POD,kInt32POD,kInt32POD,
        kFloat32POD,kFloat32POD,kFloat64POD,kFloat64POD,kFloat64POD};
    const unsigned extents[]={0,1,1,1,1,1,1,1,2,3,2,3,2,3,2,3,4};
    return {pods[kind],extents[kind]};
}
AbcA::DataType typedSchema(int kind){
    using namespace Alembic::Util;
    if(kind==MFnData::kString||kind==MFnData::kStringArray)return {kStringPOD,1};
    if(kind==MFnData::kIntArray)return {kInt32POD,1};
    if(kind==MFnData::kMatrix)return {kFloat64POD,16};
    if(kind==MFnData::kDoubleArray)return {kFloat64POD,1};
    return {kFloat64POD,3};
}
template<typename Run>void schemaRejected(Run run){
    bool failed=false;try{run();}catch(const std::runtime_error&){failed=true;}assert(failed);
}

template<typename Callback>void faults(Callback callback){
    resetSample();assert(callback());const int count=reads;
    for(int index=0;index<count;++index)for(int kind=1;kind<=3;++kind){
        resetSample();failAt=index;failKind=kind;bool accepted=false;
        try{accepted=callback();}catch(const std::exception&){}
        assert(!accepted);resetSample();assert(callback());
    }
}

int main(int argc,char** argv){
    assert(argc==2);MPlug plug;plug.null=false;
    for(int kind=MFnNumericData::kBoolean;kind<=MFnNumericData::k4Double;++kind){
        if(kind==MFnNumericData::kInvalid)continue;
        faults([&]{numericKind=static_cast<MFnNumericData::Type>(kind);Abc::OArrayProperty prop;prop.type=numericSchema(kind);
            const bool ok=MFnNumericDataToSample(numericKind,plug,prop);
            assert(ok||prop.state->sets==0);return ok;});
        if(kind!=MFnNumericData::k4Double)faults([&]{numericKind=static_cast<MFnNumericData::Type>(kind);
            Abc::OScalarProperty prop;prop.state->type=numericSchema(kind);const bool ok=MFnNumericDataToSample(numericKind,plug,prop);
            assert(ok||prop.state->sets==0);return ok;});
    }
    for(int kind=MFnData::kNumeric;kind<=MFnData::kVectorArray;++kind){
        faults([&]{typedKind=static_cast<MFnData::Type>(kind);Abc::OArrayProperty prop;
            prop.type=typedSchema(kind);const bool ok=MFnTypedDataToSample(typedKind,plug,prop);
            assert(ok||prop.state->sets==0);return ok;});
    }
    for(bool user:{false,true})faults([&]{
        MObject attribute(MFn::kTypedAttribute);Abc::OCompoundProperty parent;
        std::vector<PlugAndObjScalar> scalars;std::vector<PlugAndObjArray> arrays;
        const auto budget=std::make_shared<dommy::attribute::ExportAttributeBudget>();
        if(user)createUserPropertyFromMFnAttr(attribute,plug,parent,0,AbcGeom::kUnknownScope,MString(),scalars,arrays,budget);
        else createGeomPropertyFromMFnAttr(attribute,plug,parent,0,AbcGeom::kUnknownScope,MString(),arrays,budget);
        assert(budget->used().fields==1U);
        return scalars.size()+arrays.size()==1U;
    });
    for(int sampled:{0,1})faults([&]{
        getSampledKind=sampled;Abc::OCompoundProperty geometry,user;
        Abc::OObject object;MFnDependencyNode node;JobArgs args;args.attribs.insert("sample");
        MayaAttributeWriter complete(geometry,user,object,node,0,args);
        return true;
    });
    resetSample();arrayCount=0;Abc::OArrayProperty empty;empty.type=numericSchema(MFnNumericData::kDouble);
    assert(MFnNumericDataToSample(MFnNumericData::kDouble,plug,empty)&&empty.state->count==0);
    const std::size_t count=dommy::mesh::kMaxMeshOperationBytes/(4U*sizeof(double));
    requireAttributeStorage(count,sizeof(double));bool failed=false;
    try{requireAttributeStorage(count+1U,sizeof(double));}catch(const std::length_error&){failed=true;}
    assert(failed);
    resetSample();arrayCount=unsigned(dommy::generation::kMaxGeneratedPointCount+1U);
    Abc::OArrayProperty bounded;bounded.type=numericSchema(MFnNumericData::kDouble);failed=false;
    try{MFnNumericDataToSample(MFnNumericData::kDouble,plug,bounded);}catch(const std::length_error&){failed=true;}
    assert(failed&&bounded.state->sets==0);
    resetSample();stringValues={MString("selected"),MString("未選択")};
    const auto strings=exportStrings(plug);
    assert(strings==std::vector<std::string>({"selected","未選択"}));
    arrayCount=unsigned(dommy::generation::kMaxGeneratedStringCount+1U);failed=false;
    try{exportStrings(plug);}catch(const std::length_error&){failed=true;}
    assert(failed);
    const std::size_t stringBytes=dommy::mesh::kMaxMeshOperationBytes/4U-sizeof(std::string);
    requireAttributeStorage(1U,sizeof(std::string),stringBytes);failed=false;
    try{requireAttributeStorage(1U,sizeof(std::string),stringBytes+1U);}catch(const std::length_error&){failed=true;}
    assert(failed);
    for(bool scalar:{false,true})faults([&]{
        MayaAttributeWriter frame;PlugAndObjScalar s;PlugAndObjArray a;
        s.plug=plug;a.plug=plug;s.obj=a.obj=MObject(MFn::kNumericAttribute);
        if(scalar)frame.mPlugObjScalarVec.push_back(s);else frame.mPlugObjArrayVec.push_back(a);
        frame.write();return true;
    });
    resetSample();multi=false;Abc::OScalarProperty text;text.state->type=typedSchema(MFnData::kString);
    assert(MFnTypedDataToSample(MFnData::kString,plug,text)&&text.state->text=="a");
    resetSample();MayaAttributeWriter writer;writer.mAnimVisibility.plug.null=true;
    PlugAndObjScalar sample;sample.plug=plug;sample.obj=MObject(MFn::kNumericAttribute);writer.mPlugObjScalarVec.push_back(sample);
    failAt=0;failed=false;try{writer.write();}catch(const std::runtime_error&){failed=true;}
    assert(failed&&writer.mPlugObjScalarVec[0].prop.state->sets==0);
    // Exercise failure through the actual atomic file transaction, with the
    // archive producer remaining a double. The preexisting destination survives.
    const std::string destination=argv[1];{std::ofstream out(destination);out<<"existing";}
    resetSample();failAt=0;std::string temporary;
    try{dommy::file::PendingOutputFile output(destination,dommy::file::PendingOutputFile::Access::TrustedPath);temporary=output.trustedTemporaryPath();std::ofstream staged(temporary);staged<<"partial";
        staged.close();writer.write();output.commit();assert(false);}catch(const std::runtime_error&){}
    std::ifstream input(destination);const std::string saved((std::istreambuf_iterator<char>(input)),{});
    assert(saved=="existing");
    assert(!temporary.empty()&&!dommy::file::isRegularFile(temporary));
    resetSample();writer.write();assert(writer.mPlugObjScalarVec[0].prop.state->sets==1);

    // A fixed generic typed-numeric attribute can change its datum subtype.
    // Both transports actually consume the schema's bytes at set(), so removal
    // of the production guard exposes the original heap/stack over-read to ASan.
    for(bool scalar:{false,true})for(int changed:{MFnNumericData::k2Double,MFnNumericData::k3Float}){
        resetSample();MayaAttributeWriter frame;PlugAndObjScalar s;PlugAndObjArray a;
        s.plug=a.plug=plug;s.obj=a.obj=MObject(MFn::kTypedAttribute);
        if(scalar)frame.mPlugObjScalarVec.push_back(s);else frame.mPlugObjArrayVec.push_back(a);
        frame.write();numericKind=static_cast<MFnNumericData::Type>(changed);
        schemaRejected([&]{frame.write();});
        assert((scalar?s.prop.state->sets:a.prop.state->sets)==1U);
        numericKind=MFnNumericData::k3Double;frame.write();
        assert((scalar?s.prop.state->sets:a.prop.state->sets)==2U);
    }
    resetSample();arrayCount=0;Abc::OArrayProperty missing;
    schemaRejected([&]{MFnNumericDataToSample(MFnNumericData::k2Double,plug,missing);});
    assert(missing.state->sets==0U&&reads==0);
    resetSample();Abc::OArrayProperty wrongBytes;wrongBytes.type.byteOverride=32;
    schemaRejected([&]{MFnNumericDataToSample(MFnNumericData::k3Double,plug,wrongBytes);});
    assert(wrongBytes.state->sets==0U&&reads==0);
    resetSample();Abc::OArrayProperty wrongExtent;wrongExtent.type.byteOverride=16;
    schemaRejected([&]{MFnNumericDataToSample(MFnNumericData::k2Double,plug,wrongExtent);});
    assert(wrongExtent.state->sets==0U&&reads==0);
    // Same byte size does not imply the same POD or component interpretation.
    for(bool scalar:{false,true}){
        resetSample();Abc::OScalarProperty s;Abc::OArrayProperty a;
        const AbcA::DataType wrong(Alembic::Util::kInt32POD,3);
        s.state->type=wrong;a.type=wrong;
        schemaRejected([&]{if(scalar)MFnNumericDataToSample(MFnNumericData::k3Float,plug,s);
            else MFnNumericDataToSample(MFnNumericData::k3Float,plug,a);});
        assert(s.state->sets==0&&a.state->sets==0&&reads==0);
    }
    for(int kind=MFnData::kString;kind<=MFnData::kVectorArray;++kind){
        resetSample();Abc::OArrayProperty a;a.type={Alembic::Util::kInt8POD,3};
        schemaRejected([&]{MFnTypedDataToSample(static_cast<MFnData::Type>(kind),plug,a);});
        assert(a.state->sets==0);
    }
    for(int extent:{2,3,4}){
        resetSample();Abc::OArrayProperty a;a.type={Alembic::Util::kFloat64POD,unsigned(extent)};
        assert(MFnTypedDataToSample(MFnData::kPointArray,plug,a));
        assert(a.state->bytes.size()==2U*unsigned(extent)*sizeof(double));
        if(extent<4)assert(MFnTypedDataToSample(MFnData::kVectorArray,plug,a));
    }
    for(int kind:{MFnData::kString,MFnData::kMatrix}){
        resetSample();Abc::OScalarProperty s;s.state->type=typedSchema(kind);
        assert(MFnTypedDataToSample(static_cast<MFnData::Type>(kind),plug,s));
        s.state->type={Alembic::Util::kInt8POD,1};
        schemaRejected([&]{MFnTypedDataToSample(static_cast<MFnData::Type>(kind),plug,s);});
        assert(s.state->sets==1);
    }
    // Creation retains typed metadata (color/vector and user tuple paths).
    for(int kind=MFnNumericData::kBoolean;kind<=MFnNumericData::k4Double;++kind)for(bool user:{false,true}){
        resetSample();numericKind=static_cast<MFnNumericData::Type>(kind);
        MObject attribute(MFn::kTypedAttribute);Abc::OCompoundProperty parent;
        std::vector<PlugAndObjScalar> scalars;std::vector<PlugAndObjArray> arrays;
        const auto budget=std::make_shared<dommy::attribute::ExportAttributeBudget>();
        if(user)createUserPropertyFromMFnAttr(attribute,plug,parent,0,AbcGeom::kUnknownScope,MString(),scalars,arrays,budget);
        else createGeomPropertyFromMFnAttr(attribute,plug,parent,0,AbcGeom::kUnknownScope,MString(),arrays,budget);
        assert(budget->used().fields==1U);
        assert(scalars.size()+arrays.size()==1U);
        if(!scalars.empty())assert(attributeToScalarPropertyPair(attribute,plug,scalars.front().prop));
        if(!arrays.empty())assert(attributeToArrayPropertyPair(attribute,plug,arrays.front().prop));
    }
    resetSample();numericKind=MFnNumericData::k3Float;usedAsColor=true;
    MObject colorAttribute(MFn::kNumericAttribute);Abc::OCompoundProperty parent;
    std::vector<PlugAndObjArray> colors;
    createGeomPropertyFromMFnAttr(colorAttribute,plug,parent,0,AbcGeom::kVertexScope,MString(),colors,std::make_shared<dommy::attribute::ExportAttributeBudget>());
    assert(colors.size()==1U&&attributeToArrayPropertyPair(colorAttribute,plug,colors[0].prop));
    assert(colors[0].prop.state->bytes.size()==2U*3U*sizeof(float));
    for(std::size_t i=0;i<6;++i){float value=0;std::memcpy(&value,colors[0].prop.state->bytes.data()+i*sizeof(float),sizeof(float));
        assert(value==float(i%3+1));}
    resetSample();Abc::OArrayProperty vectors;arrayCount=3;
    assert(MFnNumericDataToSample(MFnNumericData::k3Double,plug,vectors));
    assert(vectors.state->bytes.size()==9U*sizeof(double));
    for(std::size_t i=0;i<9;++i){double value=0;std::memcpy(&value,vectors.state->bytes.data()+i*sizeof(double),sizeof(double));
        assert(value==double(i%3+1));}
    // Native metadata survives temporary/static C++ writer destruction.
    resetSample();nodeAttributeCount=4;uniqueAttributeNames=true;JobArgs sharedArgs;sharedArgs.prefixFilters={"sample"};
    std::vector<std::unique_ptr<MayaAttributeWriter>> writers;Abc::OCompoundProperty geometry,user;Abc::OObject object;MFnDependencyNode node;
    for(int i=0;i<8;++i)writers.emplace_back(new MayaAttributeWriter(geometry,user,object,node,1,sharedArgs));
    assert(sharedArgs.exportAttributeBudget->used().fields==32U&&sharedArgs.exportAttributeBudget->used().nameBytes==224U);
    const auto retained=sharedArgs.exportAttributeBudget->used().storageBytes;
    writers.clear();assert(sharedArgs.exportAttributeBudget->used().fields==32U&&sharedArgs.exportAttributeBudget->used().storageBytes==retained);
    getSampledKind=0;{MayaAttributeWriter statics(geometry,user,object,node,1,sharedArgs);}
    assert(sharedArgs.exportAttributeBudget->used().fields==36U);getSampledKind=1;
    {dommy::attribute::ExportAttributeState state(sharedArgs.exportAttributeBudget);auto frame=state.beginFrame();
        frame.add("procedural",dommy::attribute::ExportAttributeDomain::Point,0,true);state.prepare(std::move(frame));
        state.markWritten("procedural");state.commit();assert(sharedArgs.exportAttributeBudget->used().fields==37U);}
    assert(sharedArgs.exportAttributeBudget->used().fields==36U);
    resetSample();JobArgs saturated;saturated.prefixFilters={"sample"};
    const std::size_t fixed=2U*sizeof(std::string)+6U*sizeof(void*)+2U*4U;
    saturated.exportAttributeBudget->retainNativeProperty("seed",dommy::mesh::kMaxMeshOperationBytes/4U-fixed);
    assert(saturated.exportAttributeBudget->used().storageBytes==dommy::mesh::kMaxMeshOperationBytes/4U);
    bool blocked=false;try{MayaAttributeWriter denied(geometry,user,object,node,1,saturated);}catch(const std::length_error&){blocked=true;}
    assert(blocked&&Alembic::nativePropertyConstructions==0U&&saturated.exportAttributeBudget->used().fields==1U);
    resetSample();JobArgs failedArgs;failedArgs.prefixFilters={"sample"};Alembic::failNativeProperty=true;
    schemaRejected([&]{MayaAttributeWriter failed(geometry,user,object,node,1,failedArgs);});
    assert(failedArgs.exportAttributeBudget->used().fields==1U);
    resetSample();JobArgs fresh;fresh.prefixFilters={"sample"};MayaAttributeWriter retry(geometry,user,object,node,1,fresh);
    assert(fresh.exportAttributeBudget->used().fields==1U);
    assert(fresh.exportAttributeBudget->inspectionWork().used()==15U);
    resetSample();JobArgs exhausted;exhausted.prefixFilters={"sample"};
    assert(exhausted.exportAttributeBudget->inspectionWork().consume(dommy::generation::kMaxWorkItemCount));blocked=false;
    try{MayaAttributeWriter denied(geometry,user,object,node,1,exhausted);}catch(const std::length_error&){blocked=true;}
    assert(blocked&&Alembic::nativePropertyConstructions==0U);
    resetSample();attributeReadable=false;JobArgs unreadable;
    assert(unreadable.exportAttributeBudget->inspectionWork().consume(dommy::generation::kMaxWorkItemCount));blocked=false;
    try{MayaAttributeWriter denied(geometry,user,object,node,1,unreadable);}catch(const std::length_error&){blocked=true;}
    assert(blocked);
    resetSample();JobArgs filtering;filtering.prefixFilters={std::string(8U,'x')};
    assert(filtering.exportAttributeBudget->inspectionWork().consume(dommy::generation::kMaxWorkItemCount-7U));blocked=false;
    try{bool userField=false;MayaAttributeWriter::matchFilterOrAttribs(plug,filtering,userField);}catch(const std::length_error&){blocked=true;}
    assert(blocked);
    resetSample();JobArgs missingArgs;missingArgs.exportAttributeBudget.reset();
    bool invalid=false;try{MayaAttributeWriter denied(geometry,user,object,node,1,missingArgs);}catch(const std::invalid_argument&){invalid=true;}
    assert(invalid&&reads==0);
    resetSample();MObject unsupported(MFn::kTypedAttribute);numericKind=MFnNumericData::kInvalid;
    std::vector<PlugAndObjArray> omitted;const auto omitBudget=std::make_shared<dommy::attribute::ExportAttributeBudget>();
    createGeomPropertyFromMFnAttr(unsupported,plug,geometry,0,AbcGeom::kUnknownScope,MString(),omitted,omitBudget);
    assert(omitted.empty()&&omitBudget->used().fields==0U&&Alembic::nativePropertyConstructions==0U);
}

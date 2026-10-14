#include "arnold_translation_doubles.h"
#include "translation_methods.inc"
void reset(){calls.clear();writes.clear();connections.clear();sources.clear();failAt=-1;kind=errors=reports=0;
    admitted=shaderAccepted=replacementAccepted=false;exportShader=true;acceptedReplacements=0;
    copyMode=DP_INST_COPY_ASS;rowCount=2;tableIsArray=uuidValid=true;targetNames="meshA meshB";exportFailure.clear();
    replacementBudgetBytes=replacementBudgetNames=replacementBudgetWork=0U;
    renderEnabled=motionEnabled=true;for(const char* name:{"inDP","inDC","inDM","inMesh","VDBInput"})connections[name]=true;
    for(const char* name:{"PointShader","CurveShader","MeshShader","DisplacementShader","VdbShader","Shader","ReplaceShader"}){
        sources[name]="material";connections["material"]=true;}}
void failureWrites(int failure){
    if(failure==1)assert(writes.empty());
    else assert(writes==std::vector<std::string>{"visibility"});
    // The real exception guard retains its existing fail-closed visibility
    // policy; no geometry/material publication is allowed here.
}
template<typename Node>void admission(void(Node::*method)(AtNode*),bool renderControl,bool motion,int copy=-1){
    const auto setup=[copy](){reset();if(copy>=0)copyMode=short(copy);};
    Node node;AtNode output;setup();(node.*method)(&output);assert(admitted);
    const auto trace=calls;
    // This fixture stops before typed point acquisition. Copy-mode and Mesh
    // admission now follow a verified nonempty point payload; the lifecycle
    // fixture executes those complete production branches for both modes.
    if(copy>=0)for(const auto& call:trace)assert(call.find("inMesh")==std::string::npos);
    for(std::size_t i=0;i<trace.size();++i)
      if(trace[i].find("connected.")==0||trace[i].find("owner.")==0||trace[i].find("read.")==0){
        for(bool value:{false,true})for(int failure:{1,2,3}){
            setup();if(trace[i].find("connected.")==0)connections[trace[i].substr(10)]=value;
            failAt=int(i);kind=failure;
            (node.*method)(&output);assert(!admitted);failureWrites(failure);
            setup();(node.*method)(&output);assert(admitted);
        }
    }
    for(std::size_t i=0;i<trace.size();++i)if(trace[i].find("connected.")==0){
        setup();connections[trace[i].substr(10)]=false;(node.*method)(&output);
        assert(!admitted&&errors==0&&reports==0);
    }
    if(renderControl){setup();renderEnabled=false;(node.*method)(&output);assert(!admitted);
        for(const auto& call:calls)assert(call.find("connected.")!=0);}
    if(motion){setup();motionEnabled=false;(node.*method)(&output);assert(!admitted&&writes.empty());
        for(const auto& call:calls)assert(call.find("connected.")!=0);}
}
template<typename Node>void shader(bool resetsMissing=true){
    Node node;AtNode output;reset();node.shaderAdmission(&output);assert(shaderAccepted);
    const auto trace=calls;
    for(std::size_t i=0;i<trace.size();++i)for(int failure:{1,2,3}){
        if(failure==1&&trace[i].find("source.")!=0)continue;
        reset();failAt=int(i);kind=failure;node.shaderAdmission(&output);assert(!shaderAccepted);failureWrites(failure);
        reset();node.shaderAdmission(&output);assert(shaderAccepted);
    }
    reset();exportShader=false;node.shaderAdmission(&output);assert(!shaderAccepted&&writes.empty());
    reset();sources.clear();node.shaderAdmission(&output);assert(shaderAccepted);
    assert(resetsMissing ? !writes.empty() : writes.empty());
    for(const auto& call:calls)assert(call.find("connected.")!=0);
}
template<typename Node>void shaderTransitions(const char* plug,bool displacement=false){
    Node translator;AtNode output;reset();translator.shaderAdmission(&output);assert(shaderAccepted);
    assert(output.bindings.count("shader")==1U);
    if(displacement)assert(output.bindings.count("disp_map")==1U);
    const auto original=output.bindings;
    // A successful absent source clears old state on this same node.
    reset();sources[plug].clear();if(displacement)sources["DisplacementShader"].clear();
    translator.shaderAdmission(&output);assert(shaderAccepted&&output.bindings.empty());
    reset();translator.shaderAdmission(&output);assert(shaderAccepted&&output.bindings==original);
    const auto trace=calls;
    for(std::size_t i=0;i<trace.size();++i)if(trace[i].find("source.")==0){
        for(int failure:{1,2,3}){
            reset();sources.clear();failAt=int(i);kind=failure;
            translator.shaderAdmission(&output);
            assert(!shaderAccepted&&output.bindings==original);
            failureWrites(failure);
        }
    }
    reset();sources.clear();translator.shaderAdmission(&output);assert(shaderAccepted);
    const auto disconnectTrace=calls;
    for(std::size_t i=0;i<disconnectTrace.size();++i)if(disconnectTrace[i].find("reset.")==0){
        for(int failure:{2,3}){
            reset();translator.shaderAdmission(&output);assert(shaderAccepted);
            reset();sources.clear();failAt=int(i);kind=failure;
            translator.shaderAdmission(&output);assert(!shaderAccepted&&output.visibility==0);
            reset();translator.shaderAdmission(&output);assert(shaderAccepted&&output.bindings==original);
        }
    }
}
int main(){
    MatteCollection matte;
    for(const DPIdList& mask:{DPIdList{true,false,true},DPIdList{false,true}}){
        auto mesh=std::make_shared<MatteMesh>();auto group=std::make_shared<DPGroupData>();
        group->ids=mask;mesh->faces=mask.size();mesh->group=group;matte.meshes.push_back(mesh);
    }
    std::vector<float> values(5U,0.0f);
    assert(faceMatteStage(&matte,values));
    assert(values==std::vector<float>({1.0f,0.0f,1.0f,0.0f,1.0f}));
    matte.meshes[1]->exists=false;values.assign(5U,0.0f);
    assert(faceMatteStage(&matte,values)&&values==std::vector<float>({1,0,1,0,0}));
    matte.meshes[1]->exists=true;matte.meshes[1]->faces=3U;
    values.assign(6U,0.0f);assert(!faceMatteStage(&matte,values));
    matte.meshes[1]->faces=2U;matte.meshes[1]->group.reset();
    assert(!faceMatteStage(&matte,values));
    values.assign(1U,0.0f);assert(!faceMatteStage(&matte,values));
    reset();dommy::arnold::PlugReader reader([](const char* name){return MPlug(name);},"test");
    MPlug upstream("stale");assert(!reader.source(MPlug(),"missing",upstream)&&upstream.isNull());
    admission(&DPointTranslatorAi::Update,true,false);admission(&DPointTranslatorAi::ExportMotion,false,true);
    admission(&DCurveTranslatorAi::Update,true,false);admission(&DCurveTranslatorAi::ExportMotion,true,true);
    admission(&DMeshTranslatorAi::Update,true,false);admission(&DMeshTranslatorAi::ExportMotion,true,true);
    admission(&DVDBTranslatorAi::Update,false,false);
    admission(&DPInstancerTranslatorAi::Update,true,false,DP_INST_COPY_ASS);
    admission(&DPInstancerTranslatorAi::Update,true,false,DP_INST_COPY_MESH);
    shader<DPointTranslatorAi>();shader<DCurveTranslatorAi>();shader<DMeshTranslatorAi>();
    shader<DVDBTranslatorAi>();shader<DPInstancerTranslatorAi>(false);
    shaderTransitions<DPointTranslatorAi>("PointShader");
    shaderTransitions<DCurveTranslatorAi>("CurveShader");
    shaderTransitions<DMeshTranslatorAi>("MeshShader",true);
    shaderTransitions<DVDBTranslatorAi>("VdbShader");
    DMeshTranslatorAi reusedMesh;AtNode reusedOutput;
    reset();reusedMesh.shaderAdmission(&reusedOutput);assert(shaderAccepted);
    const auto bothBindings=reusedOutput.bindings;
    for(const char* missing:{"MeshShader","DisplacementShader"}) {
        reset();sources[missing].clear();reusedMesh.shaderAdmission(&reusedOutput);
        assert(shaderAccepted&&reusedOutput.bindings.size()==1U);
        assert(reusedOutput.bindings.count(std::string(missing)=="MeshShader"?"disp_map":"shader")==1U);
        reset();reusedMesh.shaderAdmission(&reusedOutput);assert(reusedOutput.bindings==bothBindings);
        reset();sources[missing].clear();exportShader=false;reusedMesh.shaderAdmission(&reusedOutput);
        assert(!shaderAccepted&&reusedOutput.bindings==bothBindings&&writes.empty());
    }
    DPInstancerTranslatorAi invalidOwner;AtNode ownerOutput;reset();uuidValid=false;
    invalidOwner.Update(&ownerOutput);assert(!admitted&&writes.empty());
    DMeshTranslatorAi mesh;AtNode output;
    reset();sources["DisplacementShader"]="displacement";exportFailure="material";
    mesh.shaderAdmission(&output);assert(!shaderAccepted&&writes.empty());
    reset();sources["DisplacementShader"]="displacement";exportFailure="displacement";
    mesh.shaderAdmission(&output);assert(!shaderAccepted&&writes.empty());
    DPInstancerTranslatorAi inst;reset();inst.replacementAdmission();assert(replacementAccepted&&acceptedReplacements==2);
    const auto trace=calls;
    for(std::size_t i=0;i<trace.size();++i)for(int failure:{1,2,3}){
        if(failure==1&&(trace[i].find("export.")==0||trace[i].find("find.")==0))continue;
        reset();failAt=int(i);kind=failure;inst.replacementAdmission();
        assert(!replacementAccepted&&acceptedReplacements==0);
        reset();inst.replacementAdmission();assert(replacementAccepted&&acceptedReplacements==2);
    }
    reset();exportShader=false;inst.replacementAdmission();assert(!replacementAccepted&&acceptedReplacements==0);
    reset();tableIsArray=false;inst.replacementAdmission();assert(!replacementAccepted&&acceptedReplacements==0);
    reset();rowCount=0;inst.replacementAdmission();assert(replacementAccepted&&acceptedReplacements==0);
    for(const char* names:{""," ","   "}){
        reset();targetNames=names;exportShader=false;inst.replacementAdmission();
        assert(replacementAccepted&&acceptedReplacements==0&&errors==0);
        for(const auto& call:calls)assert(call.find("source.")!=0&&call.find("export.")!=0);
    }
    reset();inst.replacementAdmission();const auto firstElement=std::find(calls.begin(),calls.end(),"element.ReplaceShaderList");
    assert(firstElement!=calls.end());const int firstElementCall=static_cast<int>(firstElement-calls.begin());
    for(int condition=0;condition<4;++condition){reset();
        // Stop the first row if a mutation removes the row cap: never allocate
        // millions of fake rows just to prove the missing early admission.
        if(condition==0){rowCount=dommy::generation::kMaxGeneratedStringCount+1U;failAt=firstElementCall;kind=1;}
        if(condition==1)replacementBudgetBytes=dommy::generation::kMaxGeneratedStringByteCount;
        if(condition==2)replacementBudgetNames=dommy::generation::kMaxGeneratedStringCount;
        if(condition==3)replacementBudgetWork=dommy::generation::kMaxWorkItemCount;
        inst.replacementAdmission();assert(!replacementAccepted&&acceptedReplacements==0&&errors>0);
        for(const auto& call:calls){assert(call.find("export.")!=0);
            if(condition==0||condition==3)assert(call.find("element.")!=0);}
    }
    reset();rowCount=dommy::generation::kMaxGeneratedStringCount;failAt=firstElementCall;kind=1;
    inst.replacementAdmission();assert(!replacementAccepted&&calls.back()=="element.ReplaceShaderList");
    reset();sources["ReplaceShader"]="";inst.replacementAdmission();assert(replacementAccepted&&acceptedReplacements==0);
    reset();copyMode=DP_INST_COPY_MESH;inst.replacementAdmission();assert(replacementAccepted&&calls.empty());
}

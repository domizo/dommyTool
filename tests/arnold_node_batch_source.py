"""Actual renderer collection/creation/commit bodies, not a native renderer.

Lazy point/matrix inputs let node-fanout admission be checked without allocating
millions of nodes. API failures are injected before side effects.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps

ROOT=Path(__file__).resolve().parent.parent
LIGHT='src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp'
INST='src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp'
RESOURCES='src/integrations/arnold/DArnoldResources.h'


def bodies(overrides=None):
    overrides=overrides or {}
    def source(path):return overrides.get(path,(ROOT/path).read_text())
    light=source(LIGHT);inst=source(INST);resources=source(RESOURCES)
    update=extract(light,'void DPLightTranslatorAi::Update(')
    assert update.index('if (!pendingNodes_.retire())') < update.index('collectGeneratedNodes(')
    assert update.index('checkedNodeGenerationLayout(') < update.index('DScalarDataList intensityArray(')
    assert 'pendingNodes_.adopt(std::move(preparedMeshOwner));' in inst
    assert inst.index('pendingNodes_.adopt(std::move(preparedMeshOwner));') < inst.index('AtNode* const inst = pendingNodes_.create("ginstance");')
    owners=extract(resources,'class NodeOwner')+';\n'+extract(resources,'struct NodeIteratorDeleter')+';\n'
    owners+='typedef std::unique_ptr<AtNodeIterator,NodeIteratorDeleter> NodeIteratorPtr;\n'
    for name in ('NodeBatch','NodeBatchScope'):
        if 'class '+name+' {' in resources:owners+=extract(resources,'class '+name)+';\n'
    helpers=[]
    if 'void appendUniqueNode(' in light:helpers.append(extract(light,'void appendUniqueNode('))
    helpers.extend(extract(light,s) for s in ('bool collectGeneratedNodes(', 'bool destroyGeneratedNodes('))
    create=inst[inst.index('#pragma mark create inst data'):]
    mesh=extract(create,'if (copyType == DP_INST_COPY_MESH) {')
    ass=extract(create,'else if (copyType == DP_INST_COPY_ASS) {')
    commit=light[light.index('if (!destroyGeneratedNodes(previousNodes))'):light.index('\n\t\t\t\t\n        }//check point count')]
    names=source('src/core/common/utility/DMayaNameList.h')
    helpers += ['namespace dommy {namespace maya {',extract(names,'struct NameListBudget')+';','}}']
    shader_plan='bool buildReplacementShaderPlan(' in inst
    if shader_plan:helpers.append(extract(inst,'bool buildReplacementShaderPlan('))
    stages='#define DOMMY_SHADER_PLAN '+str(int(shader_plan))+'\n'+'\n'.join(helpers)+'''
void stage(int copyType,std::size_t count,std::size_t referenceCount=2U){
    dommy::arnold::NodeBatch pendingNodes_;
    dommy::arnold::NodeOwner preparedMeshOwner(copyType==DP_INST_COPY_MESH?make("polymesh"):nullptr);
    Points points{count};const Points* dPoints=&points;
    const std::string nodeNamePrefix="generated_owner";MUuid nodeId;
    const AtString visParam("visibility"),sidednessParam("sidedness"),invNormalParam("invert_normals"),
       aiOpaqueParam("opaque"),aiMatteParam("matte"),aiSelfShadowsParam("self_shadows"),aiReceiveShadowsParam("receive_shadows"),shaderParam("shader");
    const bool sidedness=false,invertNormals=false,opaque=false,matte=false,selfShadows=false,receiveShadows=false,isOverrideShader=overrideShader;
    AtNode* aiSgNode=commonShader;NameArray exAttrNameArray;LazyMatrices matrices;LazyMatrices* matrixArray=&matrices;
    std::vector<AtNode*> refNodeList;
    if(copyType==DP_INST_COPY_ASS){for(std::size_t i=0;i<referenceCount;++i){auto* ref=make("reference");ref->name=referenceNames.empty()?"reference_"+std::to_string(i):referenceNames.at(i);refNodeList.push_back(ref);}}
    const std::vector<ReplaceShaderData>& repShaderDataList=replacementRows;
    dommy::maya::NameListBudget nameListBudget;
'''+'''
    const std::size_t instCount=count;
    dommy::arnold::NodeGenerationLayout generationLayout;
'''+extract(inst,'const auto preflightInstanceGeneration =')+';\n'+extract(inst,'if (!preflightInstanceGeneration()) {')+(
    inst[inst.index('std::vector<AtNode*> replacementShaders;'):inst.index('if (copyType == DP_INST_COPY_ASS)\n                    pendingNodes_.reserve(')]
    if shader_plan else '')+'''
    pendingNodes_.reserve(generationLayout.nodeCount);
    dommy::arnold::NodeBatchScope generationScope(pendingNodes_);
    AtNode* preparedMesh=preparedMeshOwner.get();pendingNodes_.adopt(std::move(preparedMeshOwner));
'''+mesh+ass+'''
    generationScope.commit();
}
void commitStage(){
    const std::vector<AtNode*> previousNodes;
    dommy::arnold::NodeBatch pendingNodes_;pendingNodes_.reserve(4U);
    dommy::arnold::NodeBatchScope generationScope(pendingNodes_);
    AtNode* texNode=pendingNodes_.create("image");AtNode* goboNode=pendingNodes_.create("gobo");
    goboNode->link=texNode;
    std::vector<AtNode*> lightNodes{pendingNodes_.create("light"),pendingNodes_.create("light")};
    for(auto* n:lightNodes)n->link=goboNode;
    const std::vector<std::string> lightNodeNames{"final_light_0","final_light_1"};
    const std::string lightTextureNodeName="final_texture",goboNodeName="final_gobo";
'''+commit+'\n}\n'
    return owners,stages


def run(overrides=None,capture=False,phase='all',sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-node-batch-') as directory:
        temp=Path(directory);owners,stages=bodies(overrides)
        (temp/'node_batch_resources.inc').write_text(owners)
        (temp/'node_batch_stages.inc').write_text(stages)
        (temp/'ai_ray.h').write_text('#pragma once\n#include <cstdint>\n#define AI_RAY_ALL uint8_t(-1)\n')
        ray_path='src/integrations/arnold/DArnoldRayFlags.h'
        if ray_path in (overrides or {}):
            target=temp/ray_path[len('src/'):]
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(overrides[ray_path])
        budget_path='src/integrations/arnold/DArnoldArrayBudget.h'
        if budget_path in (overrides or {}):
            target=temp/'integrations/arnold/DArnoldArrayBudget.h'
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(overrides[budget_path])
        binary=temp/'test'
        command=shlex.split(os.environ.get('CXX','clang++'))+[
            '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unknown-pragmas',
            '-I'+str(temp),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core'),
            str(ROOT/'tests/cpp/arnold_node_batch_tests.cpp'),'-o',str(binary)]
        if sdk:
            command=command[:-2]+['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include'),'-fsyntax-only']
            subprocess.run(command,check=True,capture_output=capture,text=True)
            subprocess.run(command[:-1]+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
                '-analyzer-werror','-o',os.devnull],check=True,capture_output=capture,text=True)
            print('Complete selected node-creation/commit and ownership/budget bodies: Maya SDK syntax/analysis passed; renderer/framework/input storage are seams')
            return
        command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
        subprocess.run(command,check=True,capture_output=capture,text=True)
        cases=(('collect',),('collectfail',),('oldretire',),('budget',),('fanout',),('meshorder',),('reset',),('retry',),('reservation',),('commitall',),('shaderplan',)) if phase=='all' else ((phase,),)
        results=[]
        for case in cases:
            result=subprocess.run([str(binary),*case],capture_output=capture,text=True,timeout=30,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),
                preexec_fn=disable_core_dumps)
            results.append(result)
            if result.returncode!=0:return result
        return results[-1]


def mutations():
    shader_plan_mutations()
    light=(ROOT/LIGHT).read_text();resources=(ROOT/RESOURCES).read_text()
    budget_path='src/integrations/arnold/DArnoldArrayBudget.h'
    budget=(ROOT/budget_path).read_text()
    retirement=extract(light,'bool destroyGeneratedNodes(')
    for label,old,new in (
        ('old Light dependency order reversed','return a.first < b.first;', 'return a.first > b.first;'),
        ('old Light failed consumer retires dependencies',
         'return false;\n        }\n    }\n    return true;',
         '(void)0;\n        }\n    }\n    return true;'),
        ('unknown old Light role admitted','if (priority < 0)', 'if (false)'),
        ('null old Light silently ignored','if (!node) return false;', 'if (!node) continue;'),
        ('duplicate old Light retired twice','if (!seen.insert(node).second) continue;', 'seen.insert(node);'),
        ('old Light deletion during planning','ordered.emplace_back(priority, node);',
         'ordered.emplace_back(priority, node); (void)AiNodeDestroy(node);')):
        assert old in retirement,label
        result=run({LIGHT:light.replace(retirement,retirement.replace(old,new,1),1)},
                   capture=True,phase='oldretire')
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected node-batch mutation:',label,flush=True)
    resource_mutations(light,resources,budget_path,budget)


def resource_mutations(light,resources,budget_path,budget):
    # This header is included rather than retyped in the executable fixture.
    variants=[
        ('foreign light owner admitted',{LIGHT:light.replace('ownerId == tag.c_str()','true',1)},'collect'),
        ('wrong light tag schema admitted',{LIGHT:light.replace('if (!parameter || AiUserParamGetType(parameter) != AI_TYPE_STRING ||\n            AiUserParamGetCategory(parameter) != AI_USERDEF_CONSTANT) continue;', '(void)parameter;',1)},'collect'),
        ('failed reset replaces current node',{RESOURCES:resources.replace('if (!retire()) return false;','retire();',1)},'reset'),
        ('forward dependency cleanup',{RESOURCES:resources.replace('nodes_.rbegin(); it != nodes_.rend()', 'nodes_.begin(); it != nodes_.end()',1)},'meshorder'),
        ('failed child cleanup retires dependencies',{RESOURCES:resources.replace('if (!it->retire()) return false;', '(void)it->retire();',1)},'retry'),
        ('early light handoff',{LIGHT:light.replace('            generationScope.commit();','',1).replace('\t\t\tif (texNode) {','            generationScope.commit();\n\t\t\tif (texNode) {',1)},'commitall'),
        ('no scope rollback',{RESOURCES:resources.replace('if (!committed_) nodes_.retire();','(void)committed_;',1)},'retry'),
        ('node total limit raised',{budget_path:budget.replace('dommy::generation::kMaxGeneratedTransformCount;', 'std::numeric_limits<std::size_t>::max();',1)},'budget'),
        ('combined attribute work omitted',{budget_path:budget.replace('!work.consumeProduct(pointCount, attributeCount)', 'false',1)},'budget'),
        ('replacement fanout work omitted',{budget_path:budget.replace('!work.consumeProduct(candidate.instanceCount, replacementTargetCount)', 'false',1)},'budget'),
    ]
    for label,overrides,phase in variants:
        assert all(value!=(ROOT/path).read_text() for path,value in overrides.items()),label
        result=run(overrides,capture=True,phase=phase)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),'Undetected node-batch mutation: '+label
        print('Detected node-batch mutation:',label,flush=True)


def shader_plan_mutations():
    source=(ROOT/INST).read_text()
    helper=extract(source,'bool buildReplacementShaderPlan(')
    variants=[]
    for label,old,new in (
        ('first matching row priority lost', 'if (matched) {', 'if (matched && false) {'),
        ('reference bytes not accounted','candidate.bytes += bytes;', '(void)bytes;'),
        ('comparison byte work not accounted','if (!work.consume(bytes))', 'if (false)'),
        ('name scan work not accounted','bytes >= remainingBytes || !work.consume(1U)', 'bytes >= remainingBytes'),
        ('row visit work not accounted','for (const ReplaceShaderData& row : rows) {\n            if (!work.consume(1U))',
         'for (const ReplaceShaderData& row : rows) {\n            if (false)'),
        ('plan output cleared before admission','std::vector<AtNode*> staged;', 'result.clear(); std::vector<AtNode*> staged;'),
        ('plan budget committed before complete query','staged.push_back(selected);', 'budget = candidate; staged.push_back(selected);')):
        assert old in helper,label
        variants.append((label,source.replace(helper,helper.replace(old,new,1),1)))
    for label,old,new in (
        ('plan admission failure ignored','!buildReplacementShaderPlan(\n                        refNodeList, repShaderDataList, nameListBudget, replacementShaders)',
         '(buildReplacementShaderPlan(refNodeList, repShaderDataList, nameListBudget, replacementShaders), false)'),
        ('reference plan misaligned','static_cast<std::size_t>(nIt - refNodeList.begin())', '0U'),
        ('name queries repeated per instance','// The aligned plan is resolved once per reference.',
         '(void)AiNodeGetName(*nIt); // Regressed repeated query.')):
        assert old in source,label
        variants.append((label,source.replace(old,new,1)))
    for label,changed in variants:
        result=run({INST:changed},capture=True,phase='shaderplan')
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected ASS replacement-plan mutation:',label,flush=True)

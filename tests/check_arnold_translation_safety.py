#!/usr/bin/env python3
"""Production translator admission/helper boundaries, not an MtoA runtime."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_path_reservation_exception_safety import disable_core_dumps
from arnold_translation_payload_source import run as run_payload, mutations as payload_mutations
from arnold_optional_translation_source import run as run_optional, mutations as optional_mutations
from light_renderer_source import run as run_light, mutations as light_mutations
from arnold_parameter_source import run as run_parameters, mutations as parameter_mutations
from arnold_control_attribute_source import run as run_controls, mutations as control_mutations
from arnold_name_list_source import run as run_names, mutations as name_mutations, name_helper
from arnold_motion_curve_source import run as run_motion_curve, mutations as motion_curve_mutations
from arnold_motion_curve_source import mesh_write_mutations
from arnold_callback_lifecycle_source import run as run_lifecycle
from arnold_callback_lifecycle_source import mutations as lifecycle_mutations
from arnold_node_batch_source import run as run_node_batch
from arnold_node_batch_source import mutations as node_batch_mutations

ROOT=Path(__file__).resolve().parent.parent
PATHS={
 'DPoint':'src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp',
 'DCurve':'src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp',
 'DMesh':'src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp',
 'DVDB':'src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp',
 'DPInstancer':'src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp'}
HELPER='src/integrations/arnold/DArnoldMayaPlug.h'

def check_array_writes_and_notifications():
    count=0
    for path in sorted((ROOT/'src').rglob('*.cpp')):
        clean=strip_comments_and_literals(path.read_text())
        for call in re.finditer(r'\bAiArraySet(\w+)\s*\(',clean):
            if call.group(1)=='Key':continue # Separate checked motion boundary.
            assert re.search(r'dommy::arnold::requireArrayWrite\(\s*$',clean[:call.start()]),path
            count+=1
    assert count==111,count
    pairs=(
        (PATHS['DPoint'],'src/modules/point/DPoint/Display/DPointDisplay.cpp'),
        (PATHS['DCurve'],'src/modules/curve/DCurve/Display/DCurveDisplay.cpp'),
        (PATHS['DMesh'],'src/modules/mesh/DMesh/Display/DMeshDisplay.cpp'),
        (PATHS['DPInstancer'],'src/modules/point/DPoint/Duplicator/DPInstancer.cpp'),
        ('src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp','src/modules/point/DPoint/render/light/DPLightGen.cpp'),
        (PATHS['DVDB'],'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp'))
    for translator,node in pairs:
        callback=extract((ROOT/translator).read_text(),'void '+Path(translator).stem+'::NodeChanged(')
        assert re.search(r'partialName\(\s*false,\s*false,\s*false,\s*false,\s*false,\s*true,\s*&status\s*\)',callback)
        names=set(re.findall(r'pName\s*==\s*"([^"]+)"',callback))
        source=(ROOT/node).read_text();clean=strip_comments_and_literals(source)
        fields={m.group(1) for m in re.finditer(r'\bcreate(?:Point|Color)?\(\s*"([^"]+)"\s*,\s*"[^"]+"',source)
                if clean[m.start():m.start()+len('create')]=='create'}
        # These two legacy Mesh placeholders have declarations but no schema
        # registration or Update reader. Do not add attributes or change saved
        # scenes just to satisfy a notification-name check.
        placeholders={'DispMap','SubdivDicingCamera'} if translator==PATHS['DMesh'] else set()
        assert names-fields==placeholders,(translator,sorted(names-fields))
    print('111 element-write results and six translator long-name/schema comparisons checked',flush=True)

def bodies(overrides=None):
    overrides=overrides or {}
    helper=overrides.get(HELPER,(ROOT/HELPER).read_text())
    guard=(ROOT/'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    names_path='src/core/common/utility/DMayaNameList.h'
    names=overrides.get(names_path,(ROOT/names_path).read_text())
    code=[extract((ROOT/'src/core/common/string/DStringOp.cpp').read_text(), 'std::vector<std::string> splitStringWithChar('),
          name_helper(names, nonempty=True),'namespace dommy {namespace arnold {',extract(helper,'class PlugReader')+';',
          extract(guard,'inline void hideIncompleteUpdate('),
          extract(guard,'class UpdateVisibilityGuard')+';',
          extract(guard,'inline void reportTranslatorException('),'}}',
          guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):]]
    for name,path in PATHS.items():
        source=overrides.get(path,(ROOT/path).read_text())
        methods=('Update','ExportMotion') if name in ('DPoint','DCurve','DMesh') else ('Update',)
        for method in methods:
            callback=extract(source,'void '+name+'TranslatorAi::'+method+'(')
            prefix=callback[:callback.index('dommy::maya::ScopedPlugDataHandle')]
            clean=strip_comments_and_literals(prefix)
            observed=''.join('(void)'+var+';' for var in ('motionstep','numMotionStep','copyType')
                if re.search(r'\b'+var+r'\s*=',clean))
            completion='updateVisibility.complete();' if 'UpdateVisibilityGuard' in prefix else ''
            if completion:assert completion in callback
            code += [prefix+observed+'admitted=true;'+completion+'\n'+'}'*(clean.count('{')-clean.count('}')),
                     'DOMMY_ARNOLD_TRANSLATOR_CATCH("'+name+'::'+method+'",node)']
        starts={'DPoint':'MPlug PointShaderPlug =','DCurve':'MPlug CurveShaderPlug =',
                'DMesh':'MPlug MeshShaderPlug =','DVDB':'MPlug VdbShaderPlug =','DPInstancer':'MPlug shaderPlug ='}
        start=source.index(starts[name])
        end=source.index('// Resolve the entire replacement table',start) if name=='DPInstancer' else source.index('ProcessRenderFlags(node)',start)
        stage=source[start:end]
        code += ['void '+name+'TranslatorAi::shaderAdmission(AtNode* node) try {',
                 'const bool VERBOSE=false;dommy::arnold::PlugReader plugs([this](const char* n){return FindMayaPlug(n);},"shader");',
                 stage, '(void)aiSgNode;' if name=='DPInstancer' else '',
                 'shaderAccepted=true;}','DOMMY_ARNOLD_TRANSLATOR_CATCH("shader",node)']
        if name=='DPInstancer':
            refresh=extract(source,'bool DPInstancerTranslatorAi::refreshReferenceFile(')
            assert 'if (!fileObj.setRawFullName(filename.c_str()))' in refresh
            assert refresh.index('filename = fileObj.resolvedFullName().asChar();') < refresh.index('AiASSLoad(')
            update=extract(source,'void DPInstancerTranslatorAi::Update(')
            assert update.index('if (!assFile.setRawFullName(assFilename.c_str()))') < update.index('if (!deleteInstancedNodes(')
            start=source.index('std::vector<ReplaceShaderData> repShaderDataList;')
            end=source.index('INST_DEBUG("repShaderDataList count:',start)
            stage=source[start:end]
            code += ['void DPInstancerTranslatorAi::replacementAdmission() try {',
                     'const short copyType=copyMode;dommy::arnold::PlugReader plugs([this](const char* n){return FindMayaPlug(n);},"replacement");',
                     'dommy::maya::NameListBudget nameListBudget;nameListBudget.bytes=replacementBudgetBytes;nameListBudget.names=replacementBudgetNames;nameListBudget.workItems=replacementBudgetWork;',
                     stage,'acceptedReplacements=repShaderDataList.size();replacementAccepted=true;}',
                     'DOMMY_ARNOLD_TRANSLATOR_CATCH("replacement",nullptr)']
            callback=extract(source,'void DPInstancerTranslatorAi::Update(')
            assert callback.index('INST_DEBUG("repShaderDataList count:') < callback.index('if (!deleteInstancedNodes(')
        if name=='DMesh':
            # Execute the complete production face-group branch, including
            # offsets and mask admission; renderer publication is outside it.
            stage=extract(source,'if (dMeshes->isExistCompGrp(D_COMP_FACE, grpListNameArray[i].asChar()))')
            code += ['bool faceMatteStage(const MatteCollection* dMeshes, std::vector<float>& output) {',
                     'const std::vector<MString> grpListNameArray{MString("selected")};',
                     'const std::size_t i=0U;std::vector<float>* colIds=&output;bool isAllExist=true;',
                     stage,'return isAllExist;}']
    return '\n'.join(code)

def run(overrides=None,capture=False,sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-translation-') as directory:
        temp=Path(directory);(temp/'translation_methods.inc').write_text(bodies(overrides))
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-variable',
               '-Wno-unused-parameter','-Wno-unknown-pragmas','-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "arnold_translation_doubles.h"\n#include "translation_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Arnold translator admission/helpers: Maya SDK syntax/analysis passed; renderer/payload boundaries declared')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        binary=temp/'test';subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/arnold_translation_tests.cpp'),'-o',str(binary)],
            check=True,capture_output=capture,text=True)
        options={'preexec_fn':disable_core_dumps} if os.name=='posix' else {}
        return subprocess.run([str(binary)],capture_output=capture,text=True,timeout=30,cwd=temp,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)

def mutations():
    helper=(ROOT/HELPER).read_text()
    variants=[]
    for name in ('connection','source'):
        method=extract(helper,'bool '+name+'(')
        candidate=method.replace('if (!status) {','if (false) {',1)
        assert candidate!=method
        variants.append((name+' status ignored',{HELPER:helper.replace(method,candidate,1)}))
    for name,path in PATHS.items():
        source=(ROOT/path).read_text()
        checks=('if (!shPlug.isNull() && !shaderNode)', 'if (!dshPlug.isNull() && !displacementNode)') if name=='DMesh' else ('if (!aiSgNode)',)
        for check in checks:
            assert check in source
            variants.append((name+' shader export admitted '+check,{path:source.replace(check,'if (false)',1)}))
    path=PATHS['DPInstancer'];source=(ROOT/path).read_text()
    start=source.index('std::vector<ReplaceShaderData> repShaderDataList;')
    end=source.index('INST_DEBUG("repShaderDataList count:',start)
    table=source[start:end]
    checks=list(re.finditer(r'if \(!stat(?: \|\| !isArray)?\)',table))
    assert len(checks)==5
    for index,check in enumerate(checks):
        candidate=table[:check.start()]+'if (false)'+table[check.end():]
        variants.append(('replacement query '+str(index)+' ignored',{path:source.replace(table,candidate,1)}))
    for label,old,new in (
        ('target read ignored','if (readPlugValue(targets, names) != MS::kSuccess)','if ((static_cast<void>(readPlugValue(targets, names)), false))'),
        ('replacement export ignored','if (!replacementNode)','if (false)'),
        ('wrong target child','row.child(DPInstancer::aTargetNodeNames, &stat)','row.child(DPInstancer::aReplaceShader, &stat)'),
        ('wrong shader child','row.child(DPInstancer::aReplaceShader, &stat)','row.child(DPInstancer::aTargetNodeNames, &stat)')):
        assert old in table
        variants.append((label,{path:source.replace(table,table.replace(old,new,1),1)}))
    for check in ('if (!ownerStatus)', 'if (!ownerStatus || !nodeId.valid())'):
        assert check in source
        variants.append(('instancer owner '+check,{path:source.replace(check,'if (false)',1)}))
    for label,overrides in variants:
        result=run(overrides,capture=True)
        assert result.returncode!=0,'Undetected mutation: '+label
        print('Detected Arnold admission mutation:',label,flush=True)
    print('Arnold runtime mutations detected:',len(variants))

def reset_mutations():
    count=0
    for name,path in PATHS.items():
        if name=='DPInstancer':continue
        source=(ROOT/path).read_text()
        for parameter in ('shader','disp_map') if name=='DMesh' else ('shader',):
            old='AiNodeResetParameter(node, "'+parameter+'");'
            assert source.count(old)==1
            result=run({path:source.replace(old,'(void)node;',1)},capture=True)
            assert result.returncode!=0 and 'Assertion' in result.stderr
            count+=1;print('Detected missing binding reset:',name,parameter,flush=True)
    print('Arnold binding reset runtime mutations detected:',count)

def replacement_mutations():
    path=PATHS['DPInstancer'];source=(ROOT/path).read_text()
    variants=(
        ('blank target row exports shader','if (selections.empty()) continue;', 'if (false) continue;'),
        ('replacement row cap ignored','rowCount > dommy::generation::kMaxGeneratedStringCount','false'),
        ('replacement row work ignored','!dommy::generation::checkedSum(\n                            nameListBudget.workItems, rowCount,\n                            dommy::generation::kMaxWorkItemCount, nameListBudget.workItems)','false'),
        ('target parser status ignored','if (dommy::maya::splitCheckedNonemptyNameList(\n                                names, 1U, nameListBudget, selections) != MS::kSuccess)',
         'if ((static_cast<void>(dommy::maya::splitCheckedNonemptyNameList(names, 1U, nameListBudget, selections)), false))'),
        ('target parser replaced by legacy splitter','dommy::maya::splitCheckedNonemptyNameList(\n                                names, 1U, nameListBudget, selections)',
         '(selections = splitStringWithChar(names.asChar(), " "), MS::kSuccess)'))
    for label,old,new in variants:
        assert old in source,label
        result=run({path:source.replace(old,new,1)},capture=True)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected replacement input mutation:',label,flush=True)
    print('Replacement input runtime mutations detected:',len(variants),flush=True)

if __name__=='__main__':
    check_array_writes_and_notifications()
    if '--sdk' in sys.argv:
        sdk=sys.argv[sys.argv.index('--sdk')+1]
        run(sdk=sdk);run_payload(sdk=sdk);run_optional(sdk=sdk);run_light(sdk=sdk);run_parameters(sdk=sdk);run_controls(sdk=sdk);run_names(sdk=sdk);run_motion_curve(sdk=sdk);run_lifecycle(sdk=sdk);run_node_batch(sdk=sdk)
    else:
        result=run_node_batch();assert result.returncode==0
        node_batch_mutations()
        result=run_lifecycle();assert result.returncode==0
        lifecycle_mutations()
        result=run();assert result.returncode==0
        replacement_mutations()
        result=run_motion_curve();assert result.returncode==0
        print('Checked motion admission, complete Curve payload and nine merged Mesh field types/notification passed')
        motion_curve_mutations()
        mesh_write_mutations()
        result=run_names();assert result.returncode==0
        print('Eleven renderer name admissions and reused VDB velocity disconnect/failure/retry passed')
        name_mutations()
        reset_mutations()
        result=run_controls();assert result.returncode==0
        print('Twenty renderer control branches: real typed readers, numeric families, wrong kinds/counts and failed copies passed')
        print('Eight translator admission prefixes and real shader/table stages: status/exception failures, publication, inactive modes and retry passed')
        result=run_payload();assert result.returncode==0
        print('Three geometry translators: checked scalar/vector staging, wide curve directions and six input/failure scopes passed')
        payload_mutations()
        result=run_optional();assert result.returncode==0
        print('Optional instancer mesh/attributes/static keys, light numeric staging and VDB failed-update visibility passed')
        optional_mutations()
        result=run_light();assert result.returncode==0
        print('Light mode selection, returned transform failures and shader-link failures/retry passed')
        light_mutations()
        result=run_parameters();assert result.returncode==0
        print('Arnold parameter declaration/reuse, collisions and 70 production callsite statements passed')
        parameter_mutations()
        if '--mutations' in sys.argv:
            mutations();control_mutations()
        if '--matte-mutations' in sys.argv:
            path=PATHS['DMesh'];source=(ROOT/path).read_text()
            for label,old,new in (
                ('unselected faces rejected','(*colIds)[meshOffset + face] = ids[face] ? 1.0f : 0.0f;',
                 'if (!ids[face]) {isAllExist=false;break;} (*colIds)[meshOffset + face] = 1.0f;'),
                ('mask length ignored','!group || group->getGroupIds().size() != faces', '!group'),
                ('partition offset lost','(*colIds)[meshOffset + face]', '(*colIds)[face]')):
                assert old in source
                result=run({path:source.replace(old,new,1)},capture=True)
                assert result.returncode!=0,'Undetected Group Matte mutation: '+label
                print('Detected Group Matte mutation:',label,flush=True)

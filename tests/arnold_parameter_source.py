"""User declaration/ownership callsites and their common parameter boundary."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
from check_path_reservation_exception_safety import disable_core_dumps
from check_geometry_state_transactions import extract
ROOT=Path(__file__).resolve().parent.parent
HELPER='src/integrations/arnold/DArnoldUserParameters.h'
PATHS=(
    'src/core/DDataUtility/IO/DPWriteToAi.cpp',
    'src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp',
    'src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp',
    'src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp',
    'src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp',
    'src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp',
    'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp',
    'src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp')


def methods(overrides):
    cases=[]; declarations=[]; names=[]; custom=[]
    for path in PATHS:
        source=overrides.get(path,(ROOT/path).read_text())
        assert '#include "integrations/arnold/DArnoldUserParameters.h"' in source,path
        assert not re.search(r'\bAiNodeDeclare\s*\(',source),path
        matches=list(re.finditer(r'(?:AiNodeDeclare|dommy::arnold::require(?:UserParameter|InstanceUserParameter|OwnershipTag|VelocityGridsParameter))\([^;]+\);',source))
        assert matches,path
        for match in matches:
            statement=match.group(0)
            string_args=re.findall(r'"([^"]*)"',statement)
            tag=next((s for s in string_args if s in ('dInstMeshTag','dInstObjTag')),None)
            if 'kLightOwnerTag' in statement:tag='dlightTag'
            velocity='velocity_grids' in statement or 'requireVelocityGridsParameter' in statement
            declarations.append('constant STRING' if tag else 'constant ARRAY STRING' if velocity else string_args[-1])
            names.append(tag or 'velocity_grids' if velocity else tag or 'custom')
            custom.append(not tag and not velocity)
            cases.append('case '+str(len(cases))+': '+statement+' break;')
    assert len(cases)==70,len(cases)
    instancer=overrides.get(PATHS[5],(ROOT/PATHS[5]).read_text())
    declaration=instancer.index('requireOwnershipTag(preparedMesh,')
    retirement=instancer.index('if (!deleteInstancedNodes(')
    assert declaration<retirement
    assert 'AiNodeSetStr(preparedMeshOwner.get()' not in instancer[:retirement]
    return '\n'.join((
        'const char* stageDeclarations[]={'+','.join('"'+v+'"' for v in declarations)+'};',
        'const char* stageNames[]={'+','.join('"'+v+'"' for v in names)+'};',
        'const bool stageCustom[]={'+','.join('true' if v else 'false' for v in custom)+'};',
        'void parameterStage(unsigned id,AtNode* output,const std::string& field="custom") {',
        'AtNode* node=output;AtNode* nodeCurves=output;AtNode* baseMesh=output;AtNode* preparedMesh=output;AtNode* inst=output;',
        'const std::string attrName=field,grpMatteName=field;const char* kLightOwnerTag="dlightTag";',
        'switch(id){','\n'.join(cases),'default:assert(false);}',
        '++valueWrites;}'))


def prepare(temp,overrides=None,api='arnold_export_api.h'):
    overrides=overrides or {}
    for name in ('ai.h','ai_array.h','ai_nodes.h','ai_universe.h','ai_params.h','ai_node_entry.h','ai_msg.h'):
        (temp/name).write_text('#include "'+api+'"\n')
    if (ROOT/HELPER).exists():
        target=temp/HELPER[len('src/'):];target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(overrides.get(HELPER,(ROOT/HELPER).read_text()))


def run(overrides=None,capture=False,sdk=None):
    overrides=overrides or {}
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-parameters-') as directory:
        temp=Path(directory);prepare(temp,overrides)
        (temp/'parameter_methods.inc').write_text(methods(overrides))
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-variable',
            '-Wno-unused-parameter','-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src'),
            '-DDOMMY_PARAMETER_HELPER='+str(int((ROOT/HELPER).exists()))]
        unit=ROOT/'tests/cpp/arnold_parameter_tests.cpp'
        if sdk:
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Arnold parameter helper/70 callsite statements: C++11 syntax/analysis passed; renderer metadata is an explicit seam, not a native Arnold SDK check')
            return
        flags+=['-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(unit),'-o',str(binary)],check=True,capture_output=capture,text=True)
        options={'preexec_fn':disable_core_dumps} if os.name=='posix' else {}
        return subprocess.run([str(binary)],cwd=temp,capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)


def mutations():
    source=(ROOT/HELPER).read_text()
    schema=extract(source,'inline bool hasOwnershipTagSchema(')
    for label,old,new in (
        ('unknown ownership name read','parameter_detail::isOwnershipTag(name)','(name && *name)'),
        ('null ownership node queried','return node &&','return')):
        assert old in schema
        result=run({HELPER:source.replace(schema,schema.replace(old,new,1),1)},capture=True)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected ownership read runtime mutation:',label,flush=True)
    for label,old,new in (
        ('declaration result','if (!AiNodeDeclare(node, name, declaration))','if (!AiNodeDeclare(node, name, declaration) && false)'),
        ('existing scalar type','AiUserParamGetType(parameter) == expected.type','true'),
        ('existing category','AiUserParamGetCategory(parameter) == expected.category','true'),
        ('existing array element type','AiUserParamGetArrayType(parameter) == expected.arrayType','true'),
        ('built-in collision','if (AiNodeEntryLookUpParameter(entry, name))','if (AiNodeEntryLookUpParameter(entry, name) && false)'),
        ('reserved owner names','if (ownership != isOwnershipTag(name))','if (false)'),
        ('declaration postcondition','if (!matches(AiNodeLookUpUserParameter(node, name), expected))','if (false && !matches(AiNodeLookUpUserParameter(node, name), expected))'),
        ('instance name preflight','parameter_detail::validateName(AiNodeEntryLookUp("ginstance"), name, false);','(void)name;'),
        ('velocity builtin type','if (AiParamGetType(builtin) != AI_TYPE_ARRAY)','if (false)'),
        ('velocity array element type','if (value && AiArrayGetType(value) != AI_TYPE_STRING)','if (value && false)')):
        assert old in source,label
        result=run({HELPER:source.replace(old,new,1)},capture=True)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected parameter runtime mutation:',label,flush=True)

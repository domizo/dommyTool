"""Execute complete MtoA entry/deletion/import bodies with explicit host seams.

This checks control flow and cached pointer retirement, not Arnold ASS parsing,
native ownership, or native Maya event delivery.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_path_reservation_exception_safety import disable_core_dumps
from arnold_name_list_source import name_helper

ROOT = Path(__file__).resolve().parent.parent
PATHS = {
    'DPoint': 'src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp',
    'DCurve': 'src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp',
    'DMesh': 'src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp',
    'DPInstancer': 'src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp',
    'DPLight': 'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp',
    'DVDB': 'src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp',
}


def bodies(overrides=None):
    overrides = overrides or {}
    def source(path):
        return overrides.get(path, (ROOT / path).read_text())
    guard = source('src/integrations/arnold/DArnoldEntryPointGuard.h')
    resources = source('src/integrations/arnold/DArnoldResources.h')
    helper = source('src/integrations/arnold/DArnoldMayaPlug.h')
    assert 'std::vector<AtNode*> ownedReferenceNodes;' in source(
        'src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.h')
    code = ['namespace dommy { namespace arnold {',
            extract(guard, 'inline void hideIncompleteUpdate('),
            extract(guard, 'inline void reportTranslatorException('),
            extract(helper, 'class PlugReader') + ';',
            extract(resources, 'struct NodeIteratorDeleter') + ';',
            'typedef std::unique_ptr<AtNodeIterator,NodeIteratorDeleter> NodeIteratorPtr;',
            '}}', guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):],
            extract(source('src/core/common/string/DStringOp.cpp'), 'bool isMatchPrefixString(')]
    declarations, tests = [], []
    for name, path in PATHS.items():
        text = source(path)
        cls = name + 'TranslatorAi'
        methods = [('AtNode*', 'CreateArnoldNodes'), ('void', 'Export'), ('void', 'NodeChanged')]
        if name in ('DPoint', 'DCurve', 'DMesh', 'DPInstancer'):
            methods.append(('bool', 'RequiresMotionData'))
        declaration = ['struct ' + cls + ': BaseTranslator {']
        for result, method in methods:
            body = extract(text, result + ' ' + cls + '::' + method + '(')
            # The extractor intentionally does not include macro-expanded catches.
            end = text.index(body) + len(body)
            macro = re.match(r'\s*DOMMY_ARNOLD_TRANSLATOR_CATCH\([^\n]+\)', text[end:])
            if macro:
                body += '\n' + macro.group().strip()
            code.append(body)
            args = {'Export': 'AtNode*', 'NodeChanged': 'MObject&,MPlug&'}.get(method, '')
            declaration.append(result + ' ' + method + '(' + args + ');')
        if name == 'DPInstancer':
            declaration += ['bool deleteInstancedNodes(bool); bool refreshReferenceFile();',
                            'void inputUpdate(AtNode*);']
            for method in ('deleteInstancedNodes', 'refreshReferenceFile'):
                code.append(extract(text, 'bool ' + cls + '::' + method + '('))
            if 'bool ' + cls + '::retireGeneratedNodes(' in text:
                declaration.append('bool retireGeneratedNodes();')
                code.append(extract(text, 'bool ' + cls + '::retireGeneratedNodes('))
            declaration.append('void assAdmission(std::size_t,bool);')
            update = extract(text, 'void ' + cls + '::Update(')
            after_retirement = update[update.index('if (!deleteInstancedNodes('):]
            refresh = extract(after_retirement, 'if (copyType == DP_INST_COPY_ASS && shouldRefreshAss) {')
            reserve_start = update.index('if (copyType == DP_INST_COPY_ASS)\n                    pendingNodes_.reserve(')
            reserve = update[reserve_start:update.index(';', reserve_start)+1]
            hide_start = update.index('for (std::vector<AtNode*>::const_iterator it =')
            code += [extract(text, 'struct ReplaceShaderData') + ';',
                     'void '+cls+'::assAdmission(std::size_t instCount,bool failReserve) try {',
                     'const int copyType=DP_INST_COPY_ASS;const AtString visParam("visibility");',
                     'const std::vector<ReplaceShaderData> repShaderDataList;const NameCount exAttrNameArray;',
                     'dommy::arnold::NodeGenerationLayout generationLayout;',
                     extract(update, 'const auto preflightInstanceGeneration =') + ';',
                     refresh, extract(update, 'if (!preflightInstanceGeneration()) {'),
                     'if(failReserve)allocationFaultAt=allocationCalls;', reserve,
                     extract(update[hide_start:], 'for ('), '}',
                     'DOMMY_ARNOLD_TRANSLATOR_CATCH("ASS admission fixture", &output)']
            # Execute every real input/normal-empty branch. Only admitted,
            # nonempty geometry generation is replaced by a sentinel; its
            # separate generation/commit fixture still executes the real body.
            callback = extract(text, 'void ' + cls + '::Update(')
            prefix = callback[:callback.index('#pragma mark Export arnold')]
            prefix = prefix.replace(cls + '::Update(', cls + '::inputUpdate(', 1)
            clean = strip_comments_and_literals(prefix)
            code += [prefix + '(void)numMotionStep;(void)instCount;payloadReached=true;\n' +
                     '}' * (clean.count('{') - clean.count('}')),
                     'DOMMY_ARNOLD_TRANSLATOR_CATCH("Instancer input fixture",node)']
        declaration.append('};')
        declarations += declaration
        names = re.findall(r'pName\s*==\s*"([^"]+)"', extract(text, 'void ' + cls + '::NodeChanged('))
        tests.append('testCallbacks<' + cls + '>("' + names[0] + '");')
        if any(method == 'RequiresMotionData' for _, method in methods):
            tests.append('testMotion<' + cls + '>(' + ('false' if name == 'DPInstancer' else 'true') + ');')
    sidedness = []
    for name in ('DMesh', 'DPInstancer'):
        text = source(PATHS[name])
        sidedness += re.findall(r'AiNodeSetByte\((?:node|inst), sidednessParam,.*?\);', text, re.S)
    assert len(sidedness) == 3
    for index, statement in enumerate(sidedness):
        code.append('void sidednessStage' + str(index) + '(bool sidedness) {' +
                    'AtNode* node=&output;AtNode* inst=&output;' +
                    'const AtString sidednessParam("sidedness");' + statement + '}')
    return '\n'.join(declarations + code), '\n'.join(tests)


def run(overrides=None, capture=False, phase='all', sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-lifecycle-') as directory:
        temp = Path(directory)
        code, tests = bodies(overrides)
        resources_path = 'src/integrations/arnold/DArnoldResources.h'
        resources = (overrides or {}).get(resources_path, (ROOT/resources_path).read_text())
        (temp / 'lifecycle_resources.inc').write_text('\n'.join(
            extract(resources, 'class ' + name) + ';' for name in ('NodeOwner', 'NodeBatch')))
        parameter_path = 'src/integrations/arnold/DArnoldUserParameters.h'
        parameters = (overrides or {}).get(parameter_path, (ROOT/parameter_path).read_text())
        schema = ''
        if 'inline bool hasOwnershipTagSchema(' in parameters:
            schema = '\n'.join(('namespace dommy{namespace arnold{namespace parameter_detail{',
                extract(parameters, 'struct Spec')+';', extract(parameters, 'inline bool isOwnershipTag('),
                extract(parameters, 'inline bool matches('), '}',
                extract(parameters, 'inline bool hasOwnershipTagSchema('), '}}'))
        (temp / 'lifecycle_schema.inc').write_text(schema)
        names_path = 'src/core/common/utility/DMayaNameList.h'
        names = (overrides or {}).get(names_path, (ROOT/names_path).read_text())
        enabled = 'inline MStatus splitCheckedNonemptyNameList(' in names
        (temp / 'lifecycle_names.inc').write_text('\n'.join((
            '#define DOMMY_REPLACEMENT_NAMES_HELPER '+str(int(enabled)),
            name_helper(names, nonempty=True) if enabled else '')))
        ray_path = 'src/integrations/arnold/DArnoldRayFlags.h'
        # This explicit renderer constant seam is not an Arnold SDK check.
        (temp / 'ai_ray.h').write_text('#pragma once\n#include <cstdint>\n#define AI_RAY_ALL uint8_t(-1)\n')
        (temp / 'lifecycle_ray_flags.inc').write_text(
            (overrides or {}).get(ray_path, (ROOT/ray_path).read_text()) if (ROOT/ray_path).exists() else '')
        (temp / 'lifecycle_methods.inc').write_text(code)
        (temp / 'lifecycle_cases.inc').write_text(tests)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
            '-Wno-unknown-pragmas', '-I' + str(temp), '-I' + str(ROOT/'src'), '-I' + str(ROOT/'src/core'),
            str(ROOT / 'tests/cpp/arnold_callback_lifecycle_tests.cpp'),
            '-o', str(binary)]
        if sdk:
            command = command[:-2] + ['-DDOMMY_SDK_CHECK=1', '-isystem',
                str(Path(sdk)/'include'), '-Wno-unknown-pragmas', '-fsyntax-only']
            subprocess.run(command, check=True, capture_output=capture, text=True)
            subprocess.run(command[:-1]+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull], check=True,
                capture_output=capture, text=True)
            print('Complete callback/lifecycle bodies: Maya SDK syntax/analysis passed; MtoA/Arnold are explicit seams')
            return
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        subprocess.run(command, check=True, capture_output=capture, text=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary), phase], capture_output=capture, text=True,
            timeout=30, cwd=temp, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                ('0' if sys.platform == 'darwin' else '1')), **options)


def mutations():
    inactive_settings_mutations()
    followup_mutations()
    input_mutations()
    path=PATHS['DPInstancer'];source=(ROOT/path).read_text()
    variants=[]
    retirement=extract(source,'bool DPInstancerTranslatorAi::deleteInstancedNodes(')
    for label,old,new in (
        ('old consumer failure retires dependencies',
         'return false;\n        }\n    }\n    if (deleteReferenceNodes',
         '(void)0;\n        }\n    }\n    if (deleteReferenceNodes'),
        ('old instancer dependency order reversed','return a.first < b.first;',
         'return a.first > b.first;')):
        assert old in retirement,label
        result=run({path:source.replace(retirement,retirement.replace(old,new,1),1)},
                   capture=True,phase='dependencies')
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected callback/lifecycle mutation:',label,flush=True)
    for label,old,new in (
        ('prefix collision admitted','if (name && isMatchPrefixString(name, prefixStr))',
         'if (false && name && isMatchPrefixString(name, prefixStr))'),
        ('pre-existing nodes adopted','previousNodes.count(candidate)', 'false'),
        ('early cache invalidation','MStatus status;\n    const MFnDependencyNode nodeFn(GetMayaObject(), &status);',
         'refNodeList.clear(); MStatus status;\n    const MFnDependencyNode nodeFn(GetMayaObject(), &status);'),
        ('reference deletion by prefix','referenceNodes.count(candidate)',
         'isMatchPrefixString(AiNodeGetName(candidate), referenceNodePrefix)'),
        ('destroyed shape cache retained','refNodeList.erase(std::remove(refNodeList.begin(), refNodeList.end(), value),\n                              refNodeList.end());', '(void)value;'),
        ('destroyed owner cache retained','ownedReferenceNodes.erase(\n                std::remove(ownedReferenceNodes.begin(), ownedReferenceNodes.end(), value),\n                ownedReferenceNodes.end());', '(void)value;'),
        ('failed ASS import not tracked','const int success = AiASSLoad(\n        filename.c_str(), (AI_NODE_SHAPE | AI_NODE_SHADER));',
         'const int success = AiASSLoad(\n        filename.c_str(), (AI_NODE_SHAPE | AI_NODE_SHADER)); if(success == -1) return false;')):
        assert old in source,label
        variants.append((label,{path:source.replace(old,new,1)}))
    for name,path in PATHS.items():
        source=(ROOT/path).read_text();body=extract(source,'void '+name+'TranslatorAi::NodeChanged(')
        assert 'if (!status)' in body
        changed=body.replace('if (!status)','if (false)')
        variants.append((name+' notification status ignored',{path:source.replace(body,changed,1)}))
    for label,overrides in variants:
        result=run(overrides,capture=True)
        assert result.returncode!=0,'Undetected lifecycle mutation: '+label
        print('Detected callback/lifecycle mutation:',label,flush=True)


def inactive_settings_mutations():
    path=PATHS['DPInstancer'];source=(ROOT/path).read_text()
    update=extract(source,'void DPInstancerTranslatorAi::Update(')
    for label,old,new in (
        ('unknown render state used','if (!plugs.read("IsRender", isRender))',
         'if ((plugs.read("IsRender", isRender), false))'),
        ('disabled short circuit omitted','if (!isRender)', 'if (false)'),
        ('disconnected points short circuit omitted','if (!pointConnected)', 'if (false)'),
        ('empty points short circuit omitted','if (instCount == 0U)', 'if (false)'),
        ('disconnected Mesh short circuit omitted','if (!meshConnected)', 'if (false)'),
        ('unused copy mode read early','bool isRender = false;',
         'short earlyMode = 0; if (!plugs.read("CopyObjectType", earlyMode)) return; bool isRender = false;'),
        ('unused render flag read early','bool isRender = false;',
         'bool earlyOpaque = false; if (!plugs.read("aiOpaque", earlyOpaque)) return; bool isRender = false;'),
        ('unused matrix exported early','bool isRender = false;', 'ExportMatrix(node); bool isRender = false;'),
        ('unused motion queried early','bool isRender = false;', '(void)GetNumMotionSteps(); bool isRender = false;'),
        ('unused radius set early','bool isRender = false;', 'AiNodeSetFlt(node, "radius", 0.001); bool isRender = false;')):
        assert old in update,label
        changed=source.replace(update,update.replace(old,new,1),1)
        result=run({path:changed},capture=True,phase='inactivesettings')
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected inactive-settings lifecycle mutation:',label,flush=True)


def input_mutations():
    path = PATHS['DPInstancer']
    source = (ROOT/path).read_text()
    update = extract(source, 'void DPInstancerTranslatorAi::Update(')
    variants = []
    for label, old, new in (
        ('ASS queries unused mesh', 'if (copyType == DP_INST_COPY_MESH) {\n            inmeshPlug =', 'if (true) {\n            inmeshPlug ='),
        ('ASS acquires unused mesh', 'if (copyType == DP_INST_COPY_MESH) {\n            if (inMeshHndOwner.read', 'if (true) {\n            if (inMeshHndOwner.read'),
        ('inactive instances retained', 'const bool retired = retireGeneratedNodes();',
         'const bool retired = node == nullptr ? retireGeneratedNodes() : true;'),
        ('missing point plug treated as disconnected', 'if (dpPlug.isNull()) {', 'if (false) {'),
        ('missing mesh plug treated as disconnected', 'if (inmeshPlug.isNull()) {', 'if (false) {'),
        ('point query failure treated as disconnected', 'if (!plugs.connection(dpPlug, "inDP", pointConnected)) return;',
         '(void)plugs.connection(dpPlug, "inDP", pointConnected);'),
        ('mesh query failure treated as disconnected', 'if (!plugs.connection(inmeshPlug, "inMesh", meshConnected)) return;',
         '(void)plugs.connection(inmeshPlug, "inMesh", meshConnected);'),
        ('invalid points admitted', 'if (!dPoints) {', 'if (false) {'),
        ('invalid mesh admitted', 'if (inMeshHnd.type() != MFnData::kMesh) {', 'if (false) {')):
        assert old in update, label
        variants.append((label, {path:source.replace(update, update.replace(old, new, 1), 1)}, 'input'))
    empty = extract(update, 'if (instCount == 0U) {')
    old = 'retireInactive();'
    assert old in empty
    changed = update.replace(empty, empty.replace(old, '(void)0;', 1), 1)
    variants.append(('empty instances retained', {path:source.replace(update, changed, 1)}, 'input'))
    retirement = extract(source, 'bool DPInstancerTranslatorAi::retireGeneratedNodes(')
    for label, old, new in (
        ('inactive references retired', 'return deleteInstancedNodes(false);', 'return deleteInstancedNodes(true);'),
        ('pending retirement skipped', 'if (!pendingNodes_.retire()) {', 'if (false) {'),
        ('pending retirement failure ignored', 'if (!pendingNodes_.retire()) {', 'if ((pendingNodes_.retire(), false)) {'),
        ('tagged pointers collected before pending retirement', 'if (!pendingNodes_.retire()) {',
         'if (!deleteInstancedNodes(false)) return false;\n    if (!pendingNodes_.retire()) {')):
        assert old in retirement, label
        variants.append((label, {path:source.replace(retirement, retirement.replace(old, new, 1), 1)}, 'inactive'))
    for name in ('DMesh', 'DPInstancer'):
        path = PATHS[name];text = (ROOT/path).read_text()
        calls = list(re.finditer(r'AiNodeSetByte\((?:node|inst), sidednessParam,.*?\);', text, re.S))
        assert len(calls) == (1 if name == 'DMesh' else 2)
        for index, call in enumerate(calls):
            statement = call.group()
            changed = statement.replace('dommy::arnold::sidednessMask(sidedness)', 'sidedness', 1)
            assert changed != statement
            variants.append((name+' sidedness Boolean '+str(index),
                             {path:text[:call.start()]+changed+text[call.end():]}, 'sidedness'))
    ray_path = 'src/integrations/arnold/DArnoldRayFlags.h'
    text = (ROOT/ray_path).read_text()
    assert 'static_cast<std::uint8_t>(AI_RAY_ALL)' in text
    variants.append(('shared sidedness uses camera-only bit',
                     {ray_path:text.replace('static_cast<std::uint8_t>(AI_RAY_ALL)', 'std::uint8_t(1)', 1)}, 'sidedness'))
    for label, overrides, phase in variants:
        result = run(overrides, capture=True, phase=phase)
        assert result.returncode != 0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr), label
        print('Detected Instancer input/ray-mask mutation:', label, flush=True)
    print('Instancer input/ray-mask runtime mutations detected:', len(variants), flush=True)


def followup_mutations():
    path=PATHS['DPInstancer'];source=(ROOT/path).read_text()
    variants=[]
    for label,old,new,phase in (
        ('new ASS bases remain visible','AiNodeSetByte(candidate, visParam, 0);',
         '(void)candidate;','assvisibility'),
        ('retained ASS bases remain visible','AiNodeSetByte(reference, visParam, 0);',
         '(void)reference;','assvisibility'),
        ('instance tag schema ignored','if (dommy::arnold::hasOwnershipTagSchema(candidate, "dInstObjTag"))',
         'if (true)','schema'),
        ('base tag schema ignored','dommy::arnold::hasOwnershipTagSchema(candidate, "dInstMeshTag")',
         'true','schema')):
        assert old in source,label
        variants.append((label,{path:source.replace(old,new,1)},phase))
    parameter_path='src/integrations/arnold/DArnoldUserParameters.h'
    parameters=(ROOT/parameter_path).read_text();schema=extract(parameters,'inline bool hasOwnershipTagSchema(')
    for label,old,new in (
        ('ownership type is numeric','AI_TYPE_STRING','AI_TYPE_INT'),
        ('ownership category is varying','AI_USERDEF_CONSTANT','AI_USERDEF_VARYING')):
        assert old in schema
        variants.append((label,{parameter_path:parameters.replace(schema,schema.replace(old,new,1),1)},'schema'))
    names_path='src/core/common/utility/DMayaNameList.h'
    names=(ROOT/names_path).read_text()
    for label,old,new in (
        ('empty name tokens retained','if (end != start) {', 'if (true) {'),
        ('name bytes not accounted','!generation::checkedSum(candidate.bytes, bytes,\n            generation::kMaxGeneratedStringByteCount, candidate.bytes)', 'false'),
        ('name count not accounted','!generation::checkedSum(candidate.names, count,\n            generation::kMaxGeneratedStringCount, candidate.names)', 'false'),
        ('parse work undercounted','checkedProduct(bytes, 3U,','checkedProduct(bytes, 2U,'),
        ('repeated names undercounted','checkedProduct(count, repetitions,','checkedProduct(count, 1U,'),
        ('budget committed before allocation','std::vector<std::string> staged;',
         'budget = scan.admitted; std::vector<std::string> staged;'),
        ('output cleared before admission','NameListScan scan;', 'names.clear(); NameListScan scan;'),
        ('Maya length metadata ignored','const std::size_t encodedBytes = value.length();',
         'const std::size_t encodedBytes = std::char_traits<char>::length(value.asChar());')):
        assert old in names,label
        variants.append((label,{names_path:names.replace(old,new,1)},'names'))
    for label,overrides,phase in variants:
        result=run(overrides,capture=True,phase=phase)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected ASS/name/schema lifecycle mutation:',label,flush=True)
    print('ASS/name/schema lifecycle runtime mutations detected:',len(variants),flush=True)

#!/usr/bin/env python3
"""Actual translator conversion expressions and input prefixes, not MtoA.

Post-input renderer work is an explicit failure seam. Source placement checks
keep the visibility guard alive through the real callback's last operation.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
PATHS = {
    'DPoint': 'src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp',
    'DCurve': 'src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp',
    'DMesh': 'src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp',
}
GUARD = 'src/integrations/arnold/DArnoldEntryPointGuard.h'


def sources(overrides):
    return {name: overrides.get(path, (ROOT/path).read_text())
            for name, path in PATHS.items()}


def numeric_cases(texts):
    resources=(ROOT/'src/integrations/arnold/DArnoldResources.h').read_text()
    functions=['namespace dommy {namespace arnold {',extract(resources,'inline void requireArrayWrite('),'}}']
    checks = []
    for name, text in texts.items():
        for tag, dimensions in (('DOUBLE', 0), ('VEC2D', 2), ('VEC3D', 3)):
            stage = extract(text, 'else if (type == DP_ATTR_'+tag+')')
            label = name+'_'+tag
            if dimensions == 0:
                statement = re.search(r'dommy::arnold::requireArrayWrite\(AiArraySetFlt\(.*?\);', stage, re.S).group(0)
                functions += ['float '+label+'(double value) {',
                              'const std::vector<double> tmp{value};',
                              'const std::size_t i=0U,meshoffset=0U;std::size_t offset=0U;Array outArray;',
                              statement, 'return submitted;}']
                checks += ['scalarCase('+label+');']
            else:
                variable = 'pt2' if dimensions == 2 else 'vec'
                assignments = re.findall(r'\b'+variable+r'\.[xyz]\s*=.*?;', stage)
                assert len(assignments) == dimensions, label
                result = 'AtVector2' if dimensions == 2 else 'AtVector'
                value_type = 'Imath::V'+str(dimensions)+'d'
                functions += [result+' '+label+'('+value_type+' value) {',
                              'const std::vector<'+value_type+'> tmp{value};',
                              'const std::size_t i=0U,meshoffset=0U;',
                              result+' '+variable+';', '\n'.join(assignments),
                              'return '+variable+';}']
                checks += ['vector'+str(dimensions)+'Case('+label+');']
        # Execute the staging-array conversions, including repeated primitive
        # and component paths, retaining the declared source scalar type.
        clean = strip_comments_and_literals(text)
        pattern = (r'\b(\w+)\s*=\s*(?:std::vector<(float|Imath::V[23]f)>\('
                   r'(\w+)\.begin\(\),\s*\3\.end\(\)\)|'
                   r'DVec3DataList\((\w+)\.begin\(\),\s*\4\.end\(\)\)|'
                   r'dommy::arnold::checkedFloatValues\((\w+)\)|'
                   r'dommy::arnold::checkedFloatVectors<(Imath::V[23]f)>\((\w+)\))\s*;')
        matches = list(re.finditer(pattern, clean))
        # Mesh varying fields now use the shared checked export builder; its
        # nine typed paths are executed by the complete Mesh attribute fixture.
        assert len(matches) == {'DPoint': 0, 'DCurve': 9, 'DMesh': 0}[name], name
        for index, match in enumerate(matches):
            destination = match.group(2) or match.group(6) or ('Imath::V3f' if match.group(4) else 'float')
            source_name = match.group(3) or match.group(4) or match.group(5) or match.group(7)
            declarations = list(re.finditer(r'std::vector<([^>]+)>\s*[&]?\s*'+source_name+r'\b', clean[:match.start()]))
            assert declarations, name+': missing source type for '+source_name
            source_type = declarations[-1].group(1)
            label = name+'_array_'+str(index)
            # Take the original statement, not the comment-stripped copy.
            statement = text[match.start():match.end()]
            functions += ['std::vector<'+destination+'> '+label+'(const std::vector<'+source_type+'>& '+source_name+') {',
                          'std::vector<'+destination+'> '+match.group(1)+';',
                          statement, 'return '+match.group(1)+';}']
            check = 'scalarArrayCase' if destination == 'float' else ('integerVectorArrayCase' if source_type.endswith('i') else 'vectorArrayCase')
            checks += [check+'('+label+');']
    curve = texts['DCurve']
    start = curve.index('} else if (modeType==2 && orientType==2 )')
    first = curve.index('DVec3DataList srcList;', start) if 'std::vector<Imath::V3d> orientationValues;' not in curve[start:] else curve.index('std::vector<Imath::V3d> orientationValues;', start)
    end = curve.index('if (isCubicMode)', first)
    functions += ['DVec3DataList pointOrientation(const std::vector<Imath::V3d>& values,const MMatrix& matrix,bool identity) {',
                  'auto curve=std::make_shared<OrientationCurve>();curve->values=values;curve->matrix=matrix;curve->identity=identity;',
                  'std::vector<std::shared_ptr<OrientationCurve>> curves{curve};auto cIt=curves.begin();',
                  'const std::string orientAttrName="orientation";bool isNotType=false;',
                  'for (;;) {', curve[first:end], 'return srcList;}return {}; }']
    checks += ['orientationCases();']
    return '\n'.join(functions), '\n'.join(checks)


def payload_cases(texts, guard):
    helper = (ROOT/'src/integrations/arnold/DArnoldMayaPlug.h').read_text()
    code = ['namespace dommy {namespace arnold {', extract(helper, 'class PlugReader')+';']
    if 'class UpdateVisibilityGuard' in guard:
        code += [extract(guard, 'inline void hideIncompleteUpdate('),
                 extract(guard, 'class UpdateVisibilityGuard')+';']
    code += [extract(guard, 'inline void reportTranslatorException('), '}}',
             guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):]]
    checks = []
    for name, text in texts.items():
        data_name, count_name = {'DPoint': ('dPoints', 'numberOfPoints'),
                                 'DCurve': ('dCurves', 'numberOfCurves'),
                                 'DMesh': ('dMeshes', 'numberOfMesh')}[name]
        for method in ('Update', 'ExportMotion'):
            callback = extract(text, 'void '+name+'TranslatorAi::'+method+'(')
            end = callback.index('if ('+data_name+' && '+data_name+'->'+count_name+'())')
            prefix = callback[:end]
            clean = strip_comments_and_literals(prefix)
            completion = ''
            if 'UpdateVisibilityGuard' in callback:
                active = strip_comments_and_literals(callback)
                assert active.count('UpdateVisibilityGuard updateVisibility(node);') == 1
                assert active.count('updateVisibility.complete();') == 1
                assert active.index('UpdateVisibilityGuard updateVisibility(node);') < active.index('ScopedPlugDataHandle')
                # Completion must be the final statement of the connected
                # branch, after shader/light linking or motion key submission.
                tail = active[active.index('updateVisibility.complete();')+len('updateVisibility.complete();'):]
                assert not tail.replace('}', '').strip(), name+'::'+method
                assert active.index('updateVisibility.complete();') > active.index('ProcessRenderFlags' if method == 'Update' else 'setCheckedVectorMotionKey')
                completion = 'updateVisibility.complete();'
            prefix = prefix.replace('::'+method+'(', '::payload'+method+'(', 1)
            code += [prefix,
                     'if ('+data_name+' && '+data_name+'->'+count_name+'()) {',
                     'if (api("renderer.stage") != MS::kSuccess) {AiMsgError("renderer failure");return;}admitted=true;}',
                     completion, '}'*(clean.count('{')-clean.count('}')),
                     'DOMMY_ARNOLD_TRANSLATOR_CATCH("payload",node)']
            checks += ['payloadCase(&'+name+'TranslatorAi::payload'+method+',"in'+{'DPoint':'DP','DCurve':'DC','DMesh':'DM'}[name]+'",'+
                       ('false' if name == 'DPoint' and method == 'ExportMotion' else 'true')+','+
                       ('true' if method == 'ExportMotion' else 'false')+');']
    return '\n'.join(code), '\n'.join(checks)


def run(overrides=None, capture=False, sdk=None):
    overrides = overrides or {}
    texts = sources(overrides)
    numeric, number_checks = numeric_cases(texts)
    payload, payload_checks = payload_cases(texts, overrides.get(GUARD, (ROOT/GUARD).read_text()))
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-payload-') as directory:
        temp = Path(directory)
        (temp/'translation_numeric.inc').write_text(numeric)
        (temp/'translation_payload.inc').write_text(payload)
        (temp/'translation_payload_checks.inc').write_text(number_checks+'\n'+payload_checks)
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unknown-pragmas',
                 '-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core')]
        if sdk:
            # Numeric adapters/transforms use real Maya matrices; renderer,
            # procedural attributes and callback payloads remain declarations.
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            flags += ['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            subprocess.run(compiler+flags+['-fsyntax-only',str(ROOT/'tests/cpp/arnold_translation_payload_tests.cpp')],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,
                str(ROOT/'tests/cpp/arnold_translation_payload_tests.cpp')],check=True)
            print('Translator numeric stages: Maya SDK syntax/analysis passed; renderer/payload seams are not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/arnold_translation_payload_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary)],capture_output=capture,text=True,timeout=30,cwd=temp,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')),**options)


def mutations():
    variants = (
        ('returned failure remains visible', GUARD,
         'if (!complete_) hideIncompleteUpdate(node_);', 'if (false) hideIncompleteUpdate(node_);'),
        ('point Double custom attribute unchecked', PATHS['DPoint'],
         'dommy::arnold::checkedFloat(tmp[i])', 'static_cast<float>(tmp[i])'),
        ('curve Double width unchecked', PATHS['DCurve'],
         'wList = dommy::arnold::checkedFloatValues(tmp);', 'wList = std::vector<float>(tmp.begin(), tmp.end());'),
        ('mesh Double vector component unchecked', PATHS['DMesh'],
         'vec.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);', 'vec.y = tmp[meshoffset][1];'),
        ('final curve direction unchecked', PATHS['DCurve'],
         'dommy::arnold::checkedFloatVectors<Imath::V3f>(orientationValues)',
         'std::vector<Imath::V3f>(orientationValues.begin(), orientationValues.end())'),
        ('invalid point payload treated as empty', PATHS['DPoint'],
         'if (!dPoints) {', 'if (false) {'),
    )
    for label, path, old, new in variants:
        text = (ROOT/path).read_text()
        assert old in text, 'Mutation target missing: '+label
        result = run({path: text.replace(old, new)}, capture=True)
        assert result.returncode != 0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr), 'Undetected mutation: '+label
        print('Detected translator payload mutation:', label, flush=True)

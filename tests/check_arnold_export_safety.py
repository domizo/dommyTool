#!/usr/bin/env python3
"""Execute complete legacy writer callbacks with explicit Maya/Arnold doubles."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps
from light_renderer_source import transform_methods
from arnold_parameter_source import prepare as prepare_parameters
from arnold_name_list_source import name_helper

ROOT = Path(__file__).resolve().parent.parent
LIGHT = 'src/modules/point/DPoint/render/light/DPLightGen.cpp'
POINT = 'src/core/DDataUtility/IO/DPWriteToAi.cpp'
CURVE = 'src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp'
WRITE = 'src/integrations/arnold/DArnoldAssWrite.h'
NUMERIC = 'src/integrations/arnold/DArnoldNumeric.h'


def source(path, overrides):
    return overrides.get(path, (ROOT/path).read_text())


def sources(overrides, sdk=False):
    declarations, methods, definitions = [], [], []
    attribute_id = 100
    light = source(LIGHT, overrides)
    for path, name, signatures in (
            (LIGHT, 'DPLightGen', (
                'MStatus DPLightGen::exportArnoldLight(',
                'MStatus DPLightGen::compute(',
                'MBoundingBox DPLightGen::boundingBox()')),
            (POINT, 'DPWriteToAi', (
                'MStatus DPWriteToAi::exportDPointToArnold(',
                'MStatus DPWriteToAi::compute(')),
            (CURVE, 'DWriteDCurveToAi', (
                'MStatus DWriteDCurveToAi::compute(',))):
        text = source(path, overrides)
        attributes = re.findall(r'MObject\s+'+name+r'::(\w+);', text)
        assert attributes and len(attributes) == len(set(attributes))
        declarations += ['struct '+name+' {',
                         '\n'.join('static MObject '+value+';' for value in attributes),
                         'MStatus compute(const MPlug&,MDataBlock&);',
                         'MString name() const {return "'+name+'";}',
                         'MObject thisMObject() const {return MObject();}']
        if name == 'DPLightGen':
            declarations += ['MStatus exportArnoldLight(MDataBlock&,const DPData*,const std::string&);',
                             'MBoundingBox boundingBox() const;']
        if name == 'DPWriteToAi':
            declarations += ['MStatus exportDPointToArnold(MDataBlock&,const DPData*,const std::string&);']
        declarations += ['};']
        if not sdk:
            for value in attributes:
                definitions += ['MObject '+name+'::'+value+'('+str(attribute_id)+');']
                attribute_id += 1
            if name != 'DPLightGen':
                compute = text.split('MStatus '+name+'::initialize')[0]
                inputs = sorted((set(re.findall(r'data\.inputValue\(\s*(\w+)', compute)) |
                                 set(re.findall(r'readInputHandle\(\s*(\w+)', compute))) & set(attributes))
                definitions += ['const std::vector<MObject> '+name+'InputAttributes = {'+
                                ','.join(name+'::'+value for value in inputs)+'};']
        for signature in signatures:
            callback = extract(text, signature)
            methods += [callback]
            if 'MStatus '+name+'::compute(' in signature:
                assert re.match(r'\s*DOMMY_COMPUTE_CATCH\(',
                                text[text.index(callback)+len(callback):])
                methods += ['DOMMY_COMPUTE_CATCH("'+name+'")']
    declarations += ['struct DPLightGenOverride {MBoundingBox boundingBox(const MDagPath&,const MDagPath&) const;};']
    if not sdk:
        inputs = sorted(set(re.findall(r'data\.inputValue\(\s*(a\w+)',
            extract(light, 'MStatus DPLightGen::exportArnoldLight('))) | {'inDP','aExportMode','aFilePath'})
        definitions += ['const std::vector<MObject> lightInputAttributes = {'+
                        ','.join('DPLightGen::'+value for value in inputs)+'};']
    methods.insert(0, extract(light, 'MBoundingBox lightFallbackBounds()'))
    methods.insert(0, extract((ROOT / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                              'MStatus getValidInputDP('))
    methods.insert(0, extract((ROOT / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                              'MStatus getValidInputDC('))
    methods += [extract(light, 'MBoundingBox DPLightGenOverride::boundingBox(')]
    return '\n'.join(declarations+definitions), name_helper()+'\n'+'\n'.join(methods)


def run(overrides=None, capture=False, sdk=None):
    overrides = overrides or {}
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-export-') as directory:
        temp = Path(directory)
        prepare_parameters(temp,overrides)
        (temp/'light_transform_methods.inc').write_text(transform_methods(overrides, bool(sdk)))
        for name in ('ai.h', 'ai_array.h', 'ai_nodes.h', 'ai_universe.h', 'ai_msg.h'):
            (temp/name).write_text('#include "arnold_export_api.h"\n')
        # Overridden production headers are compiled, not reimplemented in tests.
        for path in (WRITE, NUMERIC):
            target = temp/path[len('src/'):]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(source(path, overrides))
        classes, methods = sources(overrides, sdk=bool(sdk))
        (temp/'arnold_export_classes.inc').write_text(classes)
        guard = (ROOT/'src/core/common/utility/DComputeGuard.h').read_text()
        if not sdk:
            methods = guard[guard.index('#define DOMMY_COMPUTE_CATCH'):guard.index('#endif')]+'\n'+methods
        (temp/'arnold_export_methods.inc').write_text(methods)
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unknown-pragmas',
                 '-I'+str(temp), '-I'+str(ROOT/'tests/cpp'), '-I'+str(ROOT/'src'),
                 '-I'+str(ROOT/'src/core'), '-I'+str(ROOT/'src/plugin'),
                 '-I'+str(ROOT/'src/modules/curve/DCurve/IO')]
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            unit = temp/'sdk.cpp'
            unit.write_text('#include "arnold_export_host.h"\n'
                '#include "light_transform_methods.inc"\n'
                '#include "integrations/arnold/DArnoldNumeric.h"\n'
                '#include "integrations/arnold/DArnoldUserParameters.h"\n'
                '#include "integrations/arnold/DArnoldAssWrite.h"\n'
                '#include "integrations/arnold/DArnoldResources.h"\n'
                '#include "integrations/arnold/DArnoldArrayBudget.h"\n'
                '#include "integrations/arnold/DArnoldSession.h"\n'
                '#include "common/utility/DStatusOperation.h"\n'
                '#include "common/file/DExportPath.h"\n'
                '#include "DCurveArnoldLayout.h"\n'
                '#include "arnold_export_classes.inc"\n'
                '#include "arnold_export_methods.inc"\n')
            flags += ['-DDOMMY_SDK_CHECK=1', '-DDOMMY_WITH_DG_FILE_WRITERS=1',
                      '-isystem', str(Path(sdk)/'include')]
            subprocess.run(compiler+flags+['-fsyntax-only', str(unit)], check=True)
            subprocess.run(compiler+flags+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(unit)], check=True)
            print('Three Arnold writer callbacks and light bounds: real Maya SDK syntax/analysis passed; Arnold 5 and procedural data boundaries are explicit declarations, not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'), '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        results = []
        for enabled in (1,0):
            binary = temp/('test-'+str(enabled))
            subprocess.run(compiler+flags+['-DDOMMY_WITH_DG_FILE_WRITERS='+str(enabled),
                str(ROOT/'tests/cpp/arnold_export_tests.cpp'), '-o', str(binary)],
                check=True, capture_output=capture, text=True)
            options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
            result = subprocess.run([str(binary)], env=dict(os.environ,
                ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')),
                capture_output=capture, text=True, cwd=str(temp), **options)
            results.append(result)
            if result.returncode != 0:
                return result
        return results[-1]


def main():
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label, path, old, new in (
                ('light path status',LIGHT,'DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));','(void)fileObj.setRawFullName(filename.c_str());'),
                ('point path status',POINT,'DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));','(void)fileObj.setRawFullName(filename.c_str());'),
                ('curve path status',CURVE,'DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));','(void)fileObj.setRawFullName(filename.c_str());'),
                ('write return classification', WRITE, 'result != AI_SUCCESS', 'result == AI_SUCCESS'),
                ('light export status propagation', LIGHT,
                 'DOMMY_RETURN_IF_FAILURE(\n\t\t\t\t\t\t\t\texportArnoldLight',
                 'CHECK_MSTATUS(\n\t\t\t\t\t\t\t\texportArnoldLight'),
                ('point export status propagation', POINT,
                 'DOMMY_RETURN_IF_FAILURE(\n\t\t\t\t\t\texportDPointToArnold',
                 'CHECK_MSTATUS(\n\t\t\t\t\t\texportDPointToArnold'),
                ('curve write status propagation', CURVE,
                 'DOMMY_RETURN_IF_FAILURE(dommy::arnold::writeAssFile(\n',
                 'CHECK_MSTATUS(dommy::arnold::writeAssFile(\n'),
                ('float admission', NUMERIC, 'if (!numeric::checkedFloatCast(value, result))', 'if (false)'),
                ('tiny repeat guard', NUMERIC, 'if (std::fabs(repeat) < smallestRepeat)', 'if (false)'),
                ('light bounds exception containment', LIGHT,
                 '"DPLightGen", "boundingBox", std::current_exception());',
                 '"DPLightGen", "boundingBox", std::current_exception()); throw;'),
                ('override bounds exception containment', LIGHT,
                 '"DPLightGenOverride", "boundingBox", std::current_exception());',
                 '"DPLightGenOverride", "boundingBox", std::current_exception()); throw;')):
            text = source(path, {})
            assert old in text, 'Mutation target missing: '+label
            result = run({path: text.replace(old,new,1)}, capture=True)
            assert result.returncode != 0, 'Undetected mutation: '+label
            print('Detected mutation: '+label, flush=True)
    else:
        result = run()
        assert result.returncode == 0, 'Arnold export regression failed'
        print('Three writer status paths, checked float staging, ownership cleanup and both light bounds callbacks: OK')
        for path,suffix,name in ((POINT,'DP','DPWriteToAi'),(CURVE,'DC','DWriteDCurveToAi')):
            text=source(path,{})
            variable='inputDPData' if suffix=='DP' else 'inputDCData'
            old='DOMMY_RETURN_IF_FAILURE(getValidInput'+suffix+'(in'+suffix+', data, '+variable+'));'
            assert old in text
            changed=text.replace(old,'if(!getValidInput'+suffix+'(in'+suffix+', data, '+variable+')) return MS::kFailure;',1)
            result=run({path:changed},capture=True)
            assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr)
            print('Detected required Arnold input status mutation:',name,flush=True)
            pattern=r'("'+name+r'::Failed to create a new DPData object\."\);\s*return status;)' if suffix=='DP' else r'("Failed to create a new DCurveData object"\);\s*return stat;)'
            match=re.search(pattern,text);assert match,name
            old=match.group(0)
            changed=text.replace(old,re.sub(r'return (status|stat);','return MS::kFailure;',old),1)
            result=run({path:changed},capture=True)
            assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr)
            print('Detected Arnold output-factory status mutation:',name,flush=True)
        text=source(LIGHT,{})
        for parameter,target in (('slidemap','goboNode'),('color','light')):
            old='if (!AiNodeLink(texNode, "'+parameter+'", '+target+')) {'
            new='if (!AiNodeLink(texNode, "'+parameter+'", '+target+') && false) {'
            assert old in text
            result=run({LIGHT:text.replace(old,new,1)},capture=True)
            assert result.returncode != 0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr)
            print('Detected required Light export link-status mutation:',parameter,flush=True)


if __name__ == '__main__':
    main()

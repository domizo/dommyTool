#!/usr/bin/env python3
"""Exercise the complete light preparation callback with explicit host doubles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from light_renderer_source import transform_methods, TRANSFORM
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
LIGHT = 'src/modules/point/DPoint/render/light/DPLightGen.cpp'
GUARD = 'src/core/common/utility/DViewportCallbackGuard.h'


def bodies(overrides=None, sdk=False):
    source = (overrides or {}).get(LIGHT, (ROOT/LIGHT).read_text())
    callback = extract(source, 'MUserData* DPLightGenOverride::prepareForDraw(')
    assert re.match(r'\s*DOMMY_VIEWPORT_POINTER_CATCH\(', source[source.index(callback)+len(callback):])
    attributes = sorted(set(re.findall(r'DPLightGen::(\w+)', callback)))
    code = [extract((ROOT/'src/modules/point/DPoint/render/light/DPLightGen.h').read_text(),
                    'class DPLightGenData final')+';',
            'struct DPLightGen {'+''.join('static MObject '+a+';' for a in attributes)+'};',
            'struct DPLightGenOverride {MUserData* prepareForDraw(const MDagPath&,const MDagPath&,const MHWRender::MFrameContext&,MUserData*);};']
    if not sdk:
        guard = (overrides or {}).get(GUARD, (ROOT/GUARD).read_text())
        code.insert(0, guard[guard.index('#define DOMMY_VIEWPORT_VOID_CATCH'):])
        code += ['MObject DPLightGen::'+a+'('+str(i+100)+');' for i,a in enumerate(attributes)]
        code += ['const std::vector<MObject> lightPreparationAttributes = {'+
                 ','.join('DPLightGen::'+a for a in attributes if a != 'inDP')+'};']
    code += [extract(source, 'bool validLightDrawData('), callback,
             'DOMMY_VIEWPORT_POINTER_CATCH("DPLightGenOverride", "prepareForDraw")']
    return '\n'.join(code)


def run(overrides=None, capture=False, sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-light-prepare-') as directory:
        temp = Path(directory)
        (temp/'light_transform_methods.inc').write_text(transform_methods(overrides, bool(sdk)))
        (temp/'light_preparation_methods.inc').write_text(bodies(overrides, bool(sdk)))
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-Wno-unused-parameter','-Wno-unused-variable','-I'+str(temp),
                 '-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'src')]
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            flags += ['-DDOMMY_SDK_CHECK=1','-Wno-deprecated-declarations',
                      '-isystem',str(Path(sdk)/'include')]
            unit = temp/'sdk.cpp'
            unit.write_text('#include "light_preparation_host.h"\n#include "light_preparation_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Complete light prepareForDraw: Maya SDK syntax/analysis passed; procedural data/readers/math runtime not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/light_preparation_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        options = {'preexec_fn':disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary)],env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')),
            capture_output=capture,text=True,cwd=temp,**options)


def main():
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label,path,old,new in (
            ('used position admission',TRANSFORM,'!maya::isFiniteVector(position)', 'false'),
            ('zero direction guard',TRANSFORM,'directionLength <= 1.0e-12', 'false'),
            ('preparation scale status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(base.addScale(scale, MSpace::kWorld));', '(void)base.addScale(scale, MSpace::kWorld);'),
            ('cache publication status',LIGHT,'if (!dommy::maya::copyCheckedArray(data->matrixArray, cachedData->matrixArray) ||',
             'if (false ||'),
            ('cache ownership release',LIGHT,'        newData.release();','        (void)newData.get();'),
            ('preparation exception boundary',GUARD,'        return NULL; \\\n', '        throw; \\\n')):
            source = (ROOT/path).read_text()
            start = source.index('MUserData* DPLightGenOverride::prepareForDraw(') if path == LIGHT else 0
            assert old in source[start:], 'Mutation target missing: '+label
            modified = source[:start]+source[start:].replace(old,new,1)
            result = run({path:modified},capture=True)
            assert result.returncode != 0, 'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
    else:
        result = run()
        assert result.returncode == 0
        print('Light preparation read/transform/copy status, fresh/reused ownership, numeric rejection and recovery: OK')


if __name__ == '__main__':
    main()

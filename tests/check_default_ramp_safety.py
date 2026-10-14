#!/usr/bin/env python3
"""Freeze legacy ramp defaults and inject failures into complete host callbacks."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

NAMES = ('DPointPlaneEmit', 'DPointCurveEmit', 'DStretchData', 'DBSMirrorMesh',
         'DPolyWire', 'DCollisionDeformer', 'DMotionDeformer', 'DPRemap',
         'DCurveFromCurve', 'DCAhoGen', 'DCUVMap', 'DCClumping', 'DCWeightMap',
         'DCurveField', 'LeafGen', 'DHEyeGen', 'DOpenVDBRemap')


def sources(root):
    found = {}
    for path in (root / 'src').rglob('*.cpp'):
        if path.stem in NAMES:
            assert path.stem not in found
            found[path.stem] = path
    assert set(found) == set(NAMES)
    return found


def bodies(root, overrides=None):
    result = []
    helper = root / 'src/core/common/utility/DMayaDefaultRamp.h'
    if helper.exists():
        helper_source = (overrides or {}).get(str(helper.relative_to(root)), helper.read_text())
        result.append(extract(helper_source, 'namespace dommy {'))
    for name, path in sources(root).items():
        source = (overrides or {}).get(str(path.relative_to(root)), path.read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + '{void postConstructor();'
                      'MObject thisMObject() const {return {"' + name + '"};}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'void ' + name + '::postConstructor('))
    result.append('const std::vector<std::pair<std::string, void(*)()>> nodes = {' +
                  ','.join('{"'+n+'", []{'+n+'().postConstructor();}}' for n in NAMES) + '};')
    return '\n\n'.join(result).replace('MCurveAttribute', 'TestCurveAttribute').replace(
        'MRampAttribute', 'TestRampAttribute')


def run(root, source, dump=False, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-default-ramp-') as directory:
        temp = Path(directory)
        (temp / 'default_ramp.inc').write_text(source)
        binary = temp / 'tests'
        subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unknown-pragmas',
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-I'+str(temp), '-I'+str(root/'tests/stubs'), '-I'+str(root/'src'),
            '-I'+str(root/'src/core'), str(root/'tests/cpp/default_ramp_tests.cpp'),
            '-o', str(binary)], check=True)
        return subprocess.run([str(binary), '--dump' if dump else str(
            root/'tests/fixtures/default_ramp_contract.txt')], check=True, text=True,
            capture_output=capture, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                                            ('0' if sys.platform == 'darwin' else '1')))


def check_sdk(root, sdk):
    # Compile every actual callback against real Maya declarations, including
    # optional modules whose complete translation units need other SDKs.
    result = ['#include "common/utility/DMayaDefaultRamp.h"']
    for name, path in sources(root).items():
        source = path.read_text()
        attrs = re.findall(r'^MObject\s+'+name+r'::(\w+);', source, re.M)
        result.append('struct '+name+' {void postConstructor();'
                      'MObject thisMObject() const {return MObject::kNullObj;}' +
                      ''.join('static MObject '+a+';' for a in attrs)+'};')
        result.extend('MObject '+name+'::'+a+';' for a in attrs)
        result.append(extract(source, 'void '+name+'::postConstructor('))
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-isystem', str(Path(sdk)/'include'), '-I'+str(root/'src/core'), '-x', 'c++', '-']
    source = '\n'.join(result)
    subprocess.run(command+['-fsyntax-only'], input=source, text=True, check=True)
    subprocess.run(command+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                           '-Xanalyzer', '-analyzer-werror', '-o', os.devnull],
                   input=source, text=True, check=True)
    print('Default ramp callbacks: real Maya SDK syntax/analyzer checks passed')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        check_sdk(root, sys.argv[sys.argv.index('--sdk')+1])
    else:
        run(root, bodies(root), dump='--dump' in sys.argv)

#!/usr/bin/env python3
"""Run complete MeshCopy/Replicator computes and three initializer contracts.

Maya boundaries use failure-injectable doubles, not a Maya runtime/emulator.
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

FOLDER = 'src/modules/curve/DCurve/Utility/'


def bodies(root, overrides=None):
    overrides = overrides or {}
    result = []
    for file, marker in (
        ('src/modules/curve/DCurve/DCurveUtil.h', 'class DCurveOutputBatch final'),
        ('src/modules/curve/DCurve/DCurveUtil.cpp', 'MStatus getValidInputDC('),
        ('src/core/common/object/DNodeOperation.cpp', 'MStatus DGetMayaCurveFromPlug('),
    ):
        result.append(extract((root / file).read_text(), marker) + ';')
    noise = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    result.append('template<typename T>\n' + extract(noise, 'void DGenerateRandomValue('))
    for name in ('DCMeshCopy', 'DCReplicator', 'DCNormal'):
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        if name == 'DCNormal':
            result.append(extract(source, 'enum {') + ';')
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}'
                      'MString name() const {return "' + name + '";}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DCNormal':
            continue  # Its compute/normal arithmetic is covered in existing suites.
        if name == 'DCMeshCopy':
            result.append(source[source.index('namespace {'):source.index('MStatus DCMeshCopy::compute(')])
            assert 'std::vector<MMatrix>' not in source
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def run(root, source, header_overrides=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-mesh-curve-copy-') as directory:
        temp = Path(directory)
        (temp / 'mesh_curve_copy.inc').write_text(source)
        for path, content in (header_overrides or {}).items():
            header = temp / path
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text(content)
        binary = temp / 'mesh_curve_copy_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp),
            '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/mesh_curve_copy_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary), str(root / 'tests/fixtures/mesh_curve_copy_initialization_contract.txt')],
                              check=True, env=environment, capture_output=capture, text=True)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root))

#!/usr/bin/env python3
"""Run complete motion-path/divide bodies with failure-injectable host boundaries."""
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

FOLDER = 'src/modules/curve/DCurve/Utility/'
NAMES = ('DCMotionPath', 'DCDivide')


def bodies(root, overrides=None):
    overrides = overrides or {}
    result = [extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                      'void AddAttributeList('),
              extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                      'class DCurveOutputBatch final') + ';',
              extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC(')]
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        if name == 'DCMotionPath':
            result.append(extract(source, 'namespace {'))
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}'
                      'MString name() const {return "' + name + '";}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    # Exact DCRest dispatch/mandatory-reader prefix, not its native rest geometry.
    path = 'src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp'
    rest = overrides.get(path, (root / path).read_text())
    prefix = extract(rest, 'MStatus DCRest::compute(').split('    MStatus status;')[0]
    result.append('struct DCRest {static MObject inDC,outDC; MStatus compute(const MPlug&,MDataBlock&);};'
                  'MObject DCRest::inDC{"inDC"},DCRest::outDC{"outDC"};\n' +
                  prefix + 'return MS::kSuccess; } DOMMY_COMPUTE_CATCH("DCRest input boundary")')
    return '\n\n'.join(result)


def run(root, source, capture=False, suite='curve_path', arguments=None, extra_flags=(),
        header_overrides=None):
    with tempfile.TemporaryDirectory(prefix='dommy-curve-path-') as directory:
        temp = Path(directory)
        (temp / (suite + '.inc')).write_text(source)
        # Mutations shadow production headers only inside this disposable build.
        for name, content in (header_overrides or {}).items():
            path = Path(name)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError('Header override must stay inside the temporary build')
            header = temp / path
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text(content)
        binary = temp / (suite + '_tests')
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp),
            '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'),
            str(root / ('tests/cpp/' + suite + '_tests.cpp')), '-o', str(binary)]
        command.extend(extra_flags)
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        if arguments is None:
            arguments = [str(root / ('tests/fixtures/' + suite + '_initialization_contract.txt'))]
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary)] + arguments,
                              check=True, env=environment, capture_output=capture,
                              text=True, cwd=str(temp), **options)


def publication_mutations(root):
    source = bodies(root)
    cases = []
    for left, right in (('Normal', 'Tangent'), ('Normal', 'Parameter'), ('Tangent', 'Parameter')):
        old = 'write' + left + ' && write' + right + ' && ' + left.lower() + 'Attribute == ' + right.lower() + 'Attribute'
        assert old in source
        cases.append((left + '/' + right + ' collision admitted', source.replace(old, 'false', 1)))
    for field in ('normal', 'tangent', 'parameter'):
        old = ('if (!dommy::attribute::commitStagedAttribute(*outputPoints, ' + field +
               'Attribute, candidate,\n                                                    curveCount)) return MS::kFailure;')
        assert old in source
        cases.append((field + ' write failure ignored', source.replace(old,
            '(void)dommy::attribute::commitStagedAttribute(*outputPoints, ' + field + 'Attribute, candidate, curveCount);', 1)))
    old = 'DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));'
    assert old in source
    cases.append(('Rest mandatory status erased', source.replace(old,
        'if (!getValidInputDC(inDC, data, inputDCData)) return MS::kFailure;', 1)))
    for label, changed in cases:
        try:
            run(root, changed, capture=True, extra_flags=('-DDOMMY_TYPED_PATH_ATTRIBUTES',))
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_path_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected curve-path mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1],
                  tuple(FOLDER + name + '.cpp' for name in NAMES) +
                  ('src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp',), 'Curve path/rest')
    else:
        run(root, bodies(root), extra_flags=('-DDOMMY_TYPED_PATH_ATTRIBUTES',))
        publication_mutations(root)

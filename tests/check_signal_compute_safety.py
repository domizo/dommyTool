#!/usr/bin/env python3
"""Complete Signal node bodies, real Perlin, numeric boundaries and API faults."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations

ROOT = Path(__file__).resolve().parent.parent
NODES = {name: 'src/modules/point/DPoint/animation/' + name + '.cpp'
         for name in ('DPSignal', 'DPSignal3D')}


def methods(overrides=None):
    overrides = overrides or {}
    math_source = (ROOT / 'src/core/math/dommyMath.cpp').read_text()
    angle_constant = next(line for line in math_source.splitlines()
                          if line.strip().startswith('const double kDegreesToRadians ='))
    result = ['namespace dommy {namespace maya {', extract(
        (ROOT / 'src/core/common/utility/DMayaRampSampling.h').read_text(),
        'inline bool sampleFiniteRamp('), '}}',
        'namespace dommy {namespace math {',
        angle_constant, extract(math_source, 'double degreesToRadians('), '}}']
    for name, path in NODES.items():
        source = overrides.get(path, (ROOT / path).read_text())
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};} MStatus compute(')
        result.extend(attrs)
        result.extend([extract(source, 'MStatus ' + name + '::compute('),
                       'DOMMY_COMPUTE_CATCH("' + name + '")'])
    return '\n'.join(result)


def run(overrides=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-signal-compute-') as directory:
        temp = Path(directory)
        (temp / 'signal_compute_methods.inc').write_text(methods(overrides))
        for path, content in (overrides or {}).items():
            if path.endswith('.h'):
                destination = temp / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(content)
        for contraction in ('off', 'on'):
            binary = temp / ('signal_compute_' + contraction)
            command = shlex.split(os.environ.get('CXX', 'clang++')) + [
                '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
                '-Wno-unused-parameter',
                '-ffp-contract=' + contraction,
                '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
                '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(temp / 'src/core'),
                '-I' + str(ROOT / 'tests/stubs'), '-I' + str(ROOT / 'src'),
                '-I' + str(ROOT / 'src/core'), '-I' + str(ROOT / 'src/core/common/utility'),
                '-isystem', str(ROOT / 'third_party/libnoise/source'),
                str(ROOT / 'tests/cpp/signal_compute_tests.cpp')]
            command.extend(str(ROOT / 'third_party/libnoise/source' / (module + '.cpp'))
                           for module in ('noisegen', 'modulebase', 'perlin'))
            subprocess.run(command + ['-o', str(binary)], check=True)
            environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                               ('0' if sys.platform == 'darwin' else '1'))
            subprocess.run([str(binary)], env=environment, check=True, capture_output=capture, text=True)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_motion_duplicator_safety import sdk_check
        for name, path in NODES.items():
            sdk_check(ROOT, sys.argv[2], path, name)
    else:
        run()

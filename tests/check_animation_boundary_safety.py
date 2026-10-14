#!/usr/bin/env python3
"""Execute animation-node boundaries and seed consumers without Maya.

Complete DPAnimMatrix compute and three initializers use host doubles; seed
consumer blocks use the actual random helper and vendored Perlin implementation.
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
from check_motion_rotation_safety import payload_methods

SOURCES = {name: 'src/modules/point/DPoint/animation/' + name + '.cpp'
           for name in ('DPAnimMatrix', 'DPSignal', 'DPSignal3D')}
CURVE = 'src/modules/curve/DCurve/Generate/DCurveFromCurve.cpp'


def generated_methods(root, overrides=None):
    overrides = overrides or {}
    bodies = [payload_methods(root), 'namespace dommy { namespace maya {\n' + extract(
        (root / 'src/core/common/utility/DTimeSampling.h').read_text(),
        'inline bool computeSampleTime(') + '\n} }']
    for name, path in SOURCES.items():
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        bodies.append('struct ' + name + ': AnimationNode {\nstatic MStatus initialize();\n'
                      'MStatus compute(const MPlug&, MDataBlock&);\n' +
                      ''.join('static MObject ' + a + ';\n' for a in attrs) + '};')
        bodies.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DPAnimMatrix':
            bodies.append(extract(source, 'MStatus DPAnimMatrix::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("DPAnimMatrix")')
    return '\n\n'.join(bodies)


def generated_seeds(root, overrides=None):
    overrides = overrides or {}
    signal = overrides.get(SOURCES['DPSignal3D'], (root / SOURCES['DPSignal3D']).read_text())
    curve = overrides.get(CURVE, (root / CURVE).read_text())
    start = signal.index('noise::module::Perlin noiseModules[3];')
    loop = extract(signal[start:], 'for (int axis = 0; axis < 3; ++axis)')
    radius = extract(curve, 'if (useRandomRadius && randomRadius != 0.0)')
    return '''
MStatus signalNoiseSeeds(int seed, std::array<int, 3>& seeds, std::array<double, 3>& output) {
    noise::module::Perlin noiseModules[3];
    std::array<double, 3> noiseValue = {}, amplitude = {{0.5, 2.0, -3.0}};
    const int octave = 6;
    const double frequency = 2.0, lacunarity = 2.0, persistence = 0.5, calculatedTime = 13.25;
''' + loop + '''
    for (unsigned i = 0; i < 3; ++i) seeds[i] = noiseModules[i].GetSeed();
    for (unsigned i = 0; i < 3; ++i) noiseValue[i] *= amplitude[i];
    output = noiseValue;
    return MS::kSuccess;
}
MStatus curveRadiusSeeds(int seed, bool useRandomRadius, double randomRadius, std::vector<double>& output) {
    const std::vector<int> sourceObjects(5);
    std::vector<double> randomRadii(sourceObjects.size(), 0.0);
''' + radius + '''
    output.swap(randomRadii);
    return MS::kSuccess;
}
'''


def run(root, methods, arguments, capture=False, seeds=None):
    with tempfile.TemporaryDirectory(prefix='dommy-animation-boundaries-') as directory:
        temp = Path(directory)
        (temp / 'animation_boundary_methods.inc').write_text(methods)
        (temp / 'animation_seed_methods.inc').write_text(seeds or generated_seeds(root))
        results = []
        for name in ('animation_boundary', 'animation_seed'):
            binary = temp / name
            command = shlex.split(os.environ.get('CXX', 'clang++')) + [
                '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
                '-Wno-unused-parameter', '-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
                '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
                '-isystem', str(root / 'third_party/libnoise/source'),
                str(root / ('tests/cpp/' + name + '_tests.cpp'))]
            if name == 'animation_seed':
                command.extend(str(root / 'third_party/libnoise/source' / (module + '.cpp'))
                               for module in ('noisegen', 'modulebase', 'perlin'))
            subprocess.run(command + ['-o', str(binary)], check=True)
            environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                               ('0' if sys.platform == 'darwin' else '1'))
            args = arguments if name == 'animation_boundary' else (
                ['--normal-only'] if arguments in (['--schema'], ['--trace']) else [])
            results.append(subprocess.run([str(binary)] + args, check=True, env=environment,
                                          capture_output=capture, text=True))
        return results


def main():
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], [SOURCES['DPAnimMatrix']], 'Matrix animation')
        return
    def execute(methods, capture=False):
        run(root, methods, [str(root / 'tests/fixtures/animation_boundary_initialization_contract.txt'),
                            str(root / 'tests/fixtures/animation_boundary_result_contract.txt')], capture)
        run(root, methods.replace('maximum = 256U * 1024U * 1024U',
                                  'maximum = 16384U'), ['--budget'], capture)
    source = generated_methods(root)
    execute(source)
    if '--mutations' in sys.argv:
        for label, old, new in (
            ('matrix aggregate admission omitted', 'dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);', ''),
            ('matrix connection error ignored', 'DOMMY_RETURN_IF_FAILURE(status);\n        if (matrixConnected)', 'if (matrixConnected)'),
            ('timing connection error ignored', 'if (!inputStatus)\n                    return inputStatus;\n                if (connected)', 'if (connected)'),
        ):
            assert old in source, label
            try:
                execute(source.replace(old, new, 1), True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'animation_boundary', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()

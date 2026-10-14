#!/usr/bin/env python3
"""Test production field initialization and radial-emitter noise without Maya.

Attribute doubles verify status propagation and the declared schema, not Maya's
registration rollback, DG behavior, or implicit attribute-property inheritance.
"""

import os
import argparse
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    field_root = root / 'src/modules/particles/particle/field'
    bodies = []
    for name in ('DCurlNoiseField', 'DCurveField'):
        source = (field_root / (name + '.cpp')).read_text()
        constants = re.findall(
            r'const (?:double|int) k(?:MinimumCurlStep|MaximumPositionNoiseFrequency)\s*=\s*[^;]+;',
            source)
        definitions = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        if not definitions:
            raise SystemExit(name + ': missing attribute definitions')
        declaration = ('struct ' + name + ' : FieldBase {\n' +
                       '\n'.join('    static MObject ' + member + ';' for member in definitions) +
                       '\n    static MStatus initialize();\n};\n')
        bodies.extend(constants)
        bodies.append(declaration)
        bodies.extend('MObject ' + name + '::' + member + ';' for member in definitions)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))

    emitter = (root / 'src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp').read_text()
    bodies.extend(extract(emitter, signature) for signature in (
        'bool isFiniteVector(', 'bool isSafePerlinCoordinate(', 'bool checkedNoiseCoordinate('))
    # Exercise the actual gate, work budget, coordinate checks, and displacement
    # block; retain Maya geometry acquisition/publication as a runtime gate.
    start = emitter.index('    const bool noiseEnabled =')
    end = emitter.index('    MVectorArray outputPositions =', start)
    displacement = extract(
        emitter[emitter.index('        if (noiseEnabled)', end):], 'if (noiseEnabled)')
    bodies.append('MStatus evaluateRadialNoise(MVector& target, const MVector& directionNoiseValue, '
                  'const MVector& noiseScaleValue, const MVector& noiseOffsetValue, '
                  'int noiseOctaveCount, std::size_t totalSampleCount) {\n' +
                  emitter[start:end] + '\nnoise::PerlinOctave perlinNoise(noiseOctaveCount, 1U);\n' +
                  displacement + '\nreturn MS::kSuccess;\n}')
    with tempfile.TemporaryDirectory(prefix='dommy-particle-boundary-') as directory:
        temporary = Path(directory)
        production = '\n\n'.join(bodies)
        include = temporary / 'particle_boundary_methods.inc'
        include.write_text(production)
        binary = temporary / 'particle_boundary_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/particle_boundary_tests.cpp'),
            str(root / 'src/core/noise/PerlinNoise.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/field_initialization_contract.txt')],
                       check=True, env=environment)
        if args.mutations:
            for label, old, new in (
                ('curl compound parent dependency omitted',
                 'noiseFrequency, noiseFrequencyX,', 'noiseFrequencyX,'),
                ('curl compound child dependency omitted',
                 'phaseOffset, phaseOffsetX, phaseOffsetY, phaseOffsetZ,',
                 'phaseOffset, phaseOffsetX, phaseOffsetY,'),
                ('curl dependency status ignored',
                 'DOMMY_RETURN_IF_FAILURE(attributeAffects(affectingAttributes[index], mOutputForce));',
                 '(void)attributeAffects(affectingAttributes[index], mOutputForce);'),
            ):
                assert old in production, label
                include.write_text(production.replace(old, new, 1))
                subprocess.run(command, check=True, capture_output=True)
                result = subprocess.run(
                    [str(binary), str(root / 'tests/fixtures/field_initialization_contract.txt')],
                    env=environment, capture_output=True)
                assert result.returncode != 0, 'Undetected mutation: ' + label
                print('Detected mutation:', label)


if __name__ == '__main__':
    main()

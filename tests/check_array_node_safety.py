#!/usr/bin/env python3
"""Run three production array-node methods with failure-injectable host doubles.

The doubles observe transform component arguments and sampled contexts; they do
not implement Maya transform decomposition, dependency evaluation or ownership.
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

SOURCES = {
    'DPVectorArrayGen': 'src/modules/point/DPoint/generate/data/DPVectorArrayGen.cpp',
    'DPMatrix': 'src/modules/point/DPoint/animation/DPMatrix.cpp',
    'DPEditDouble': 'src/modules/point/DPoint/Modify/DPEditDouble.cpp',
}


def generated_methods(root, sources=None):
    if sources is None:
        sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    expected = {
        'DPVectorArrayGen': ['generateType', 'aValueType', 'value', 'valueSize', 'isNormalize',
                            'seed', 'minRange', 'maxRange', 'aGlobalOffset', 'aInOffsetArray',
                            'aInStretchArray', 'outDataArray'],
        'DPMatrix': ['inMatrix', 'dataSize', 'seed', 'translateNoise', 'rotateNoise',
                     'scaleNoise', 'outMatrixArray'],
        'DPEditDouble': ['inputDataArray', 'isUseExpression', 'expression', 'exValue',
                         'editDataArray', 'dataIndex', 'dataValue', 'outDataArray'],
    }
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    timing = (root / 'src/core/common/utility/DTimeSampling.h').read_text()
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    bodies = ['namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(') for name in ('isFiniteVector', 'isFiniteMatrix'))
        + '\n' + extract(timing, 'inline bool computeSampleTime(') + '\n} }',
        'namespace dommy { namespace math {\n' +
        re.search(r'const double kDegreesToRadians\s*=\s*[^;]+;', math).group(0) + '\n' +
        extract(math, 'double degreesToRadians(') + '\n} }']
    vector = sources['DPVectorArrayGen']
    bodies.extend(extract(vector, signature) for signature in
                  ('bool finiteVector(', 'MVector normalizedOrZero('))
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attribute + ';' for attribute in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(bodies)


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-array-nodes-') as directory:
        temporary = Path(directory)
        (temporary / 'array_node_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'array_node_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/array_node_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/array_node_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()

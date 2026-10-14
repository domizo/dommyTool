#!/usr/bin/env python3
"""Execute the Arc and Remake compute/initializer bodies with host API doubles.

This checks control flow, generated buffers and declaration contracts, not
Maya's geometry kernel, DG evaluation, data ownership or registration rollback.
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


def generated_methods(root, sources=None):
    node_root = root / 'src/modules/curve/Curve/Node'
    if sources is None:
        sources = {name: (node_root / (name + '.cpp')).read_text()
                   for name in ('DArcCurveNode', 'remakeCurveNode')}
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    constant = re.search(r'const double kDegreesToRadians = [^;]+;', math).group(0)
    bodies = ['namespace dommy { namespace maya {\n' +
              extract(validation, 'inline bool isFinitePoint(') + '\n} }',
              'namespace dommy { namespace math {\n' + constant + '\n' +
              extract(math, 'double degreesToRadians(') + '\n} }']
    expected = {
        'DArcCurveNode': ['divide', 'radius', 'width', 'startAngle', 'endAngle',
                          'offsetAngle', 'outputMesh'],
        'remakeCurveNode': ['inputCurve', 'sampling', 'curveLength', 'offset', 'outputCurve'],
    }
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
    with tempfile.TemporaryDirectory(prefix='dommy-arc-remake-') as directory:
        temporary = Path(directory)
        (temporary / 'arc_remake_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'arc_remake_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/arc_remake_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/arc_remake_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()

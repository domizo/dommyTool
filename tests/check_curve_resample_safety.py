#!/usr/bin/env python3
"""Execute curve node boundaries and shared resampling with host API doubles.

Tests production control flow and arithmetic, not NURBS geometry, Maya data
ownership, registration rollback or dependency graph evaluation.
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


def generated_methods(root, sources=None, utility=None):
    node_root = root / 'src/modules/curve/Curve/Node'
    if sources is None:
        sources = {name: (node_root / (name + '.cpp')).read_text()
                   for name in ('DResampleCurve', 'DBSCurve')}
    if utility is None:
        utility = (node_root / 'DCurveResampleUtils.h').read_text()
    assert '#include "common/utility/DMayaPointBlend.h"' in utility
    numeric = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    point_math = (root / 'src/core/common/utility/DMayaPointBlend.h').read_text()
    bodies = ['namespace dommy { namespace maya {\n' +
              extract(numeric, 'template <typename DataBlock, typename Attribute>') + '\n} }',
              extract(point_math, 'namespace dommy {'),
              utility[utility.index('namespace dommy {'):]]
    expected = {
        'DResampleCurve': ['inCurve', 'aSampling', 'outCurve'],
        'DBSCurve': ['inBaseCurve', 'inTargetCurve', 'aBlend', 'aSampling', 'outCurve'],
    }
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attr + ';' for attr in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(bodies)


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-curve-resample-') as directory:
        temporary = Path(directory)
        (temporary / 'curve_resample_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'curve_resample_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/curve_resample_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/curve_resample_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()

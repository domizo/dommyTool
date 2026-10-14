#!/usr/bin/env python3
"""Execute the production delay node with failure-injectable Maya API doubles.

Checks control flow, explicit attribute declarations and time forwarding, not
Maya DG evaluation, registration rollback or actual array publication semantics.
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


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / 'src/modules/deformers/transform/DTDelayTransformMatrix.cpp').read_text()
    numeric = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    historical = (root / 'src/core/common/utility/DMayaHistoricalSampling.h').read_text()
    attributes = re.findall(r'^MObject\s+DTDelayTransformMatrix::(\w+);', source, re.M)
    if attributes != ['transformMatrix', 'delay', 'time', 'outputTransformMatrix']:
        raise SystemExit('Delay node attributes changed')
    shared = [extract(numeric, 'template <typename DataBlock, typename Attribute>')]
    shared.extend(extract(historical, signature) for signature in (
        'inline bool validSampleOffsets(', 'inline bool historicalSampleTime(',
        'inline MStatus sampleMatrixPlugSeries('))
    bodies = ['namespace dommy { namespace maya {\n' + '\n'.join(shared) + '\n} }']
    bodies.extend('MObject DTDelayTransformMatrix::' + name + ';' for name in attributes)
    bodies.append(extract(source, 'MStatus DTDelayTransformMatrix::initialize('))
    bodies.append(extract(source, 'MStatus DTDelayTransformMatrix::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DTDelayTransformMatrix")')
    with tempfile.TemporaryDirectory(prefix='dommy-delay-transform-') as directory:
        temporary = Path(directory)
        (temporary / 'delay_transform_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'delay_transform_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/delay_transform_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/delay_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()

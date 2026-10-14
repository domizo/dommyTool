#!/usr/bin/env python3
"""Run the production Curve field calculation/compute with injected host failures."""

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
    source = (root / 'src/modules/particles/particle/field/DCurveField.cpp').read_text()
    compute = extract(source, 'MStatus DCurveField::compute(')
    calculation = extract(source, 'MStatus DCurveField::calculateForces(')
    for forbidden in (r'inputValue\([^;]*\)\.(?:as\w+|type|data)\(',
                      r'\b(?:outputForce|forceArray)\.append\(', r'\bdouble\s+parameter\s*;',
                      r'\bapply(?:NoMaxDist|MaxDist)\('):
        if re.search(forbidden, compute + calculation):
            raise SystemExit('Unsafe Curve field boundary: ' + forbidden)
    if calculation.count('curve.closestPoint(') != 1 or calculation.count('curve.tangent(') != 1:
        raise SystemExit('Expected one shared Curve field query path')
    numeric = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    arrays = (root / 'src/core/common/utility/DMayaCheckedArray.h').read_text()
    shared = ('namespace dommy { namespace maya {\n' +
              extract(numeric, 'template <typename DataBlock, typename Attribute>') + '\n} }\n')
    shared += arrays[arrays.index('namespace dommy {'):arrays.index('// Adapter for legacy')] + '\n} }\n'
    helpers = source[source.index('namespace {'):source.index('MObject DCurveField::inputTargetCurve;')]
    compute = compute.replace(') try {', ') {', 1)
    with tempfile.TemporaryDirectory(prefix='dommy-curve-field-') as directory:
        temporary = Path(directory)
        (temporary / 'curve_field_methods.inc').write_text(shared + helpers + calculation + '\n' + compute)
        binary = temporary / 'curve_field_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/curve_field_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)


if __name__ == '__main__':
    main()

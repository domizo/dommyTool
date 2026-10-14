#!/usr/bin/env python3
"""Run production geometry commit bodies with failure-injectable host arrays.

This verifies ownership/control flow, not Maya array or geometry behavior.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals


def extract(source, signature):
    start = source.index(signature)
    code = strip_comments_and_literals(source)
    opening = code.index('{', start)
    while True:
        depth, end = 1, opening + 1
        while depth and end < len(code):
            depth += (code[end] == '{') - (code[end] == '}')
            end += 1
        if depth:
            raise ValueError('Unbalanced function: ' + signature)
        catch = re.match(r'\s*catch\s*\([^)]*\)\s*\{', code[end:])
        if not catch:
            return source[start:end]
        opening = end + catch.end() - 1


def main():
    root = Path(__file__).resolve().parent.parent
    bodies = []
    for relative, signatures in (
        ('src/core/Data/DCurveData.cpp', (
            'DCurveInternalData::DCurveInternalData(DCurveInternalData const &src)',
            'void DCurveInternalData::copy(const DCurveInternalData &other)',
            'DCurveInternalData& DCurveInternalData::operator=',
            'void DCurveInternalData::swapState(',
            'bool DCurveInternalData::replaceKnots(',
            'bool DCurveInternalData::replacePositions(\n\tconst DPointList& points)')),
        ('src/modules/mesh/DMesh/Data/DMeshData.cpp', (
            'DMeshInternalData::DMeshInternalData(DMeshInternalData const &src)',
            'void DMeshInternalData::copy(const DMeshInternalData &other)',
            'DMeshInternalData& DMeshInternalData::operator=',
            'void DMeshInternalData::swapState(',
            'bool DMeshInternalData::setUvData(',
            'void DMeshInternalData::uvInitialize()',
            'void DMeshInternalData::clearUVs()')),
    ):
        source = (root / relative).read_text()
        bodies.extend(extract(source, signature) for signature in signatures)
    with tempfile.TemporaryDirectory(prefix='dommy-geometry-state-') as directory:
        temporary = Path(directory)
        (temporary / 'geometry_state_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'geometry_state_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), '-I' + str(root / 'src/modules/point'),
            str(root / 'tests/cpp/geometry_state_transactions.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)


if __name__ == '__main__':
    main()

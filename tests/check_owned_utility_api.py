#!/usr/bin/env python3
"""Compile and verify the owned utility compatibility facade without Maya."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
VECTOR = ROOT / 'src/core/Utility/3d/Math/DVec.h'


def run(source=None, mutation=False, override=False):
    with tempfile.TemporaryDirectory(prefix='dommy-owned-utility-') as directory:
        temp = Path(directory)
        if source is not None:
            (temp / 'DVec.h').write_text(source)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/Utility'),
            '-I' + str(ROOT / 'src/core/Utility/3d/Math'),
            '-I' + str(ROOT / 'tests/stubs'),
            str(ROOT / 'tests/cpp/owned_utility_api_tests.cpp'), '-o', str(binary)]
        if override:
            command += ['-DVEC_RX_FEQ_EPS=1e-5']
        subprocess.run(command, check=True, capture_output=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary)], cwd=str(temp), timeout=20,
            text=True, capture_output=True, **options,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                     ('0' if sys.platform == 'darwin' else '1')))
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('A compatibility mutation was not rejected: ' + result.stderr)
        elif result.returncode:
            raise AssertionError(result.stderr)


def main():
    run()
    run(override=True)
    source = VECTOR.read_text()
    cases = (
        ('Vec4 division policy changed',
         'for (std::size_t axis = 0; axis != 4; ++axis) data[axis] /= divisor;',
         'detail::reciprocalDivide(data, divisor);'),
        ('aliased scalar copied before update',
         'for (std::size_t axis = 0; axis != N; ++axis) destination[axis] *= factor;',
         'const DReal snapshot = factor; for (std::size_t axis = 0; axis != N; ++axis) destination[axis] *= snapshot;'),
        ('dot product signed zero changed',
         'DReal sum = left[0] * right[0];', 'DReal sum = 0.0f + left[0] * right[0];'),
    )
    for label, before, after in cases:
        if source.count(before) != 1:
            raise AssertionError('Ambiguous mutation: ' + label)
        run(source.replace(before, after), mutation=True)
    print('Owned utility API: storage, overloads, aliasing, IEEE policies, tolerance override and negative controls passed')


if __name__ == '__main__':
    main()

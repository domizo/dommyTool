#!/usr/bin/env python3
"""Verify independent typed-array policies with nonzero native-default sentinels."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
ATTRIBUTE = 'DDataUtility/Data/DPAttributeData.h'
LOOKUP = 'common/utility/DAttributeLookup.h'


def run(overrides=None, mutation=False):
    with tempfile.TemporaryDirectory(prefix='dommy-owned-attribute-') as directory:
        temp = Path(directory)
        for name, source in (overrides or {}).items():
            path = temp / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-function', '-DDOMMY_ATTRIBUTE_API_ONLY',
            '-DDOMMY_POISON_IMATH_DEFAULT',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), '-I' + str(ROOT / 'tests/stubs'),
            '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/modules/point'),
            str(ROOT / 'tests/cpp/attribute_access_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True, capture_output=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary)], cwd=str(temp), timeout=20,
            capture_output=True, text=True, **options,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                     ('0' if sys.platform == 'darwin' else '1')))
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('Compatibility mutation was not rejected: ' + result.stderr)
        elif result.returncode:
            raise AssertionError(result.stdout + result.stderr)


def main():
    run()
    sources = {name: (ROOT / 'src/core' / name).read_text() for name in (ATTRIBUTE, LOOKUP)}
    for name, before, after in (
        (ATTRIBUTE, 'return Imath::V3f(0, 0, 0);', 'return Imath::V3f();'),
        (ATTRIBUTE, 'return DP_ATTR_UINT32;', 'return DP_ATTR_INT;'),
        (ATTRIBUTE, 'const std::vector<T> snapshot(rows);', 'const std::vector<T> snapshot;'),
        (LOOKUP, 'std::dynamic_pointer_cast<Target>(value)', 'std::static_pointer_cast<Target>(value)'),
    ):
        if sources[name].count(before) != 1:
            raise AssertionError('Ambiguous mutation: ' + before)
        run({name: sources[name].replace(before, after)}, mutation=True)
    print('Owned attribute API: 15 types, tags/defaults, arrays/aliases, typed lookup and negative controls passed')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Real-file reservation allocation failures without relying on C++ copy elision."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'src/core/common/file/DAtomicFile.h'


def disable_core_dumps():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(source=None, mutation=False):
    with tempfile.TemporaryDirectory(prefix='dommy-reservation-exceptions-') as directory:
        temporary = Path(directory)
        shadow = temporary / 'shadow'
        if source is not None:
            header = shadow / 'common/file/DAtomicFile.h'
            header.parent.mkdir(parents=True)
            header.write_text(source)
        scratch = temporary / 'scratch'
        scratch.mkdir(mode=0o700)
        binary = temporary / 'reservation_exception_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fno-elide-constructors', '-fno-omit-frame-pointer',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-I' + str(shadow), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/common/file'),
            str(ROOT / 'tests/cpp/path_reservation_exception_tests.cpp'), '-o', str(binary)]
        try:
            subprocess.run(command, check=True, capture_output=True)
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stderr.decode(errors='replace'))
            raise
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary), str(scratch)], env=environment,
                                cwd=str(scratch), text=True, capture_output=True,
                                timeout=20, **options)
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('Mutation must be detected by a runtime assertion:\n' + result.stderr)
        else:
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            if list(scratch.iterdir()):
                raise AssertionError('Reservation fixture left an owned scratch entry behind')
            print(result.stdout.strip(), flush=True)


def main():
    run()
    source = HEADER.read_text()
    cases = (
        ('cleanup omitted', 'if (owned_) std::remove(path_.c_str());',
         'if (false && owned_) std::remove(path_.c_str());'),
        ('ownership released before return-value allocation',
         'std::string result(path);\n            cleanup.release();',
         'cleanup.release();\n            std::string result(path);'),
        ('successful ownership transfer omitted', 'cleanup.release();', '(void)cleanup;'),
    )
    for label, before, after in cases:
        if source.count(before) != 1:
            raise AssertionError('Ambiguous mutation boundary: ' + label)
        run(source.replace(before, after, 1), mutation=True)
        print('Detected mutation: ' + label, flush=True)


if __name__ == '__main__':
    main()

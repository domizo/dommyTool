#!/usr/bin/env python3
"""Production sequence/temporary-name allocation failures and runtime mutations."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
SEQUENCE = ROOT / 'src/core/common/file/DSequencePath.h'
ATOMIC = ROOT / 'src/core/common/file/DAtomicFile.h'


def run(header=None, source=None, mutation=False):
    with tempfile.TemporaryDirectory(prefix='dommy-sequence-failure-') as directory:
        temporary = Path(directory)
        shadow = temporary / 'shadow'
        if header is not None:
            target = shadow / 'common/file' / header.name
            target.parent.mkdir(parents=True)
            target.write_text(source)
        binary = temporary / 'sequence_failure_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fno-omit-frame-pointer', '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-I' + str(shadow),
            '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/common/file'), '-I' + str(ROOT / 'tests/stubs'),
            str(ROOT / 'tests/cpp/sequence_failure_tests.cpp'), '-o', str(binary)]
        try:
            subprocess.run(command, check=True, capture_output=True)
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stderr.decode(errors='replace'))
            raise
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary)], env=environment, cwd=str(temporary),
                                text=True, capture_output=True, timeout=20, **options)
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('Mutation must be detected by a runtime assertion:\n' + result.stderr)
        else:
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            print(result.stdout.strip(), flush=True)


def main():
    run()
    cases = (
        ('legacy stream failure ignored', SEQUENCE,
         'if (!value) return false;\n            candidate += value.str();',
         'candidate += value.str();'),
        ('temporary stream failure ignored', ATOMIC,
         'if (!path) throw std::runtime_error("Temporary output path formatting failed");',
         '(void)path;'),
        ('padding output published too early', SEQUENCE,
         'std::string candidate(value);', 'result = value;\n    std::string candidate(value);'),
        ('legacy output cleared before validation', SEQUENCE,
         'std::string candidate;\n    candidate.reserve(source.size() + 16U);',
         'result.clear();\n    std::string candidate;\n    candidate.reserve(source.size() + 16U);'),
        ('hash output cleared before validation', SEQUENCE,
         'std::string candidate;\n    candidate.reserve(source.size() + unpadded.size());',
         'result.clear();\n    std::string candidate;\n    candidate.reserve(source.size() + unpadded.size());'),
    )
    for label, header, before, after in cases:
        source = header.read_text()
        if source.count(before) != 1:
            raise AssertionError('Ambiguous mutation boundary: ' + label)
        run(header, source.replace(before, after, 1), mutation=True)
        print('Detected mutation: ' + label, flush=True)


if __name__ == '__main__':
    main()

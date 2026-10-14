#!/usr/bin/env python3
"""Run the production wire-array adapter with status/short-copy API seams."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
HEADER = 'src/core/common/utility/DMayaWireTopology.h'


def check(header=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-wire-arrays-') as directory:
        temp = Path(directory)
        (temp / 'maya').mkdir()
        # The fixture supplies the Maya array interfaces, not native storage.
        for name in ('MIntArray', 'MFloatArray'):
            (temp / ('maya/' + name + '.h')).write_text('#pragma once\n')
        (temp / 'DMayaWireTopology.h').write_text(header or (ROOT / HEADER).read_text())
        binary = temp / 'wire_array_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-I' + str(temp), '-I' + str(ROOT / 'tests/stubs'),
            '-I' + str(ROOT / 'src/core/common/utility'), '-I' + str(ROOT / 'src/core'),
            str(ROOT / 'tests/cpp/wire_array_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary)], env=env, check=True, text=True, capture_output=capture)


def sdk_check(sdk):
    code = '#include "common/utility/DMayaWireTopology.h"\n'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(ROOT / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Complete wire-array adapter and checked templates: native Maya SDK syntax/analysis passed')


def mutations():
    check()
    header = (ROOT / HEADER).read_text()
    for label, old, new in (
        ('staging allocation failure ignored',
         'DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.faceCounts.size(),',
         '(void)(buildCheckedArray(plan.faceCounts.size(),'),
        ('output copy failure ignored',
         'DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedFaceCounts, faceCounts));',
         '(void)copyCheckedArray(preparedFaceCounts, faceCounts);'),
        ('V coordinate source replaced with U',
         'value = plan.vCoordinates[index];', 'value = plan.uCoordinates[index];'),
        ('final UV copy failure ignored',
         'return copyCheckedArray(preparedUvIndices, uvIndices);',
         '(void)copyCheckedArray(preparedUvIndices, uvIndices); return MS::kSuccess;'),
    ):
        assert old in header, label
        try:
            check(header.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'wire_array_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation:', label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()

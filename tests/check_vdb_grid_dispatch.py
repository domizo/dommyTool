#!/usr/bin/env python3
"""Compile and exercise the owned dispatcher without Maya or an OpenVDB SDK."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/modules/volume/VDB/Common/DVdbGridDispatch.h').read_text()
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-dispatch-') as directory:
        temp = Path(directory)
        for name in ('openvdb/openvdb.h', 'openvdb/tools/PointIndexGrid.h',
                     'openvdb/points/PointDataGrid.h'):
            path = temp / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#pragma once\n')
        header = temp / 'vdb_dispatch_under_test.h'
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), str(ROOT / 'tests/cpp/vdb_grid_dispatch_tests.cpp'),
            '-o', str(binary)]
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        header.write_text(source)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, env=environment, timeout=20)
        # Removing RTTI validation must not turn a forged type label into a
        # usable typed pointer, even if metadata reports a supported kind.
        broken = source.replace('dynamic_cast<const Grid*>(value)',
                                'static_cast<const Grid*>(value)', 1)
        assert broken != source
        header.write_text(broken)
        subprocess.run(command, check=True)
        result = subprocess.run([str(binary)], env=environment, timeout=20,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode == 0:
            raise AssertionError('Removing runtime type validation was not detected')
    print('Owned VDB dispatcher: 10 types, 7 families, 6 views, null/forged types and compile-time scope passed')


if __name__ == '__main__':
    main()

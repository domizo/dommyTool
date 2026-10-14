#!/usr/bin/env python3
"""Execute the complete VDB writer compute with failure-injectable API/IO seams.

No native OpenVDB serialization or file writes are performed by this test.
The production time/path guards and complete NodeInfo publisher run unchanged.
"""
from pathlib import Path
import re
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
WRITER = 'src/modules/volume/VDB/IO/DWriteOpenVDB.cpp'
UTIL = 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp'


def bodies(overrides=None):
    overrides = overrides or {}
    writer = overrides.get(WRITER, (ROOT / WRITER).read_text())
    util = overrides.get(UTIL, (ROOT / UTIL).read_text())
    signature = re.search(r'^(?:void|MStatus) updateNodeInfo\(', util, re.M).group(0)
    return '\n'.join([extract(util, signature)] + declarations('DWriteOpenVDB', writer, False) + [
        extract(writer, 'MStatus DWriteOpenVDB::compute('),
        'DOMMY_VDB_COMPUTE_CATCH("DWriteOpenVDB")'])


def check(overrides=None, capture=False):
    return run(ROOT, bodies(overrides), suite='vdb_writer_status', arguments=[], capture=capture)


def sdk_boundary(sdk):
    code = '#include "vdb_writer_status_sdk.h"\n' + re.sub(r'\{"[^\"]+"\}', '{}', bodies())
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src/core'),
        '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests/cpp'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Complete VDB writer and NodeInfo publisher: native Maya API syntax/analysis passed; OpenVDB declarations only')


def mutations():
    cases = (
        ('NodeInfo accepts failed text stream', UTIL,
         'if (!stream) return MS::kFailure;', ''),
        ('read before input acquisition check', WRITER,
         'DOMMY_RETURN_IF_FAILURE(stat);\n        const bool writeFile = writeFileHandle.asBool();',
         'const bool writeFile = writeFileHandle.asBool();\n        DOMMY_RETURN_IF_FAILURE(stat);'),
        ('compression acquisition unchecked', WRITER,
         'DOMMY_RETURN_IF_FAILURE(stat);\n                const short compressionType',
         'const short compressionType'),
        ('NodeInfo output acquisition unchecked', UTIL,
         'if (!status) return status;\n    strHandle.set(str);', 'strHandle.set(str);'),
        ('NodeInfo clean status discarded', UTIL,
         'return data.setClean(strAttr);', 'data.setClean(strAttr); return MS::kSuccess;'),
        ('disabled writer drops NodeInfo failure', WRITER,
         'DOMMY_RETURN_IF_FAILURE(updateNodeInfo(disabledInfo, data, aNodeInfo));',
         'updateNodeInfo(disabledInfo, data, aNodeInfo);'),
        ('enabled writer drops NodeInfo failure', WRITER,
         'DOMMY_RETURN_IF_FAILURE(updateNodeInfo(infoStr, data, aNodeInfo));',
         'updateNodeInfo(infoStr, data, aNodeInfo);'),
    )
    for label, path, old, new in cases:
        source = (ROOT / path).read_text()
        assert old in source, label
        try:
            check({path: source.replace(old, new)}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'vdb_writer_status_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        sdk_boundary(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()

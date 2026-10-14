#!/usr/bin/env python3
"""Execute complete VDB field compute with explicit host/grid/sampling seams."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/volume/VDB/Dynamics/DOpenVDBField.cpp'


def bodies(source=None, include_arrays=True, include_helpers=True):
    source = source or (ROOT / SOURCE).read_text()
    arrays = (ROOT / 'src/core/common/utility/DMayaCheckedArray.h').read_text()
    arrays = arrays[arrays.index('namespace dommy {'):arrays.index('// Adapter for legacy')] + '\n} }\n'
    field_data = (ROOT / 'src/core/common/utility/DMayaFieldData.h').read_text()
    return '\n'.join((arrays if include_arrays else '',
        extract(field_data, 'namespace dommy {') if include_helpers else '', extract(source, 'namespace {'),
        extract(source, 'MStatus DOpenVDBField::applyNoMaxDist('),
        extract(source, 'MStatus DOpenVDBField::compute('),
        'DOMMY_VDB_COMPUTE_CATCH("DOpenVDBField")'))


def check(source=None, capture=False):
    return run(ROOT, bodies(source), suite='vdb_field', arguments=[], capture=capture)


def sdk_boundary(sdk):
    code = ('#include "vdb_field_sdk.h"\n' +
            extract((ROOT / SOURCE).read_text(), 'MStatus DOpenVDBField::initialize(') + '\n' +
            bodies(include_arrays=False, include_helpers=False))
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src/core'),
        '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests/cpp'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Complete VDB field initializer/compute/calculation and real class header: Maya SDK syntax/analysis passed; OpenVDB declarations only')


def mutations():
    check()  # Establish a healthy baseline before interpreting mutation failures.
    source = (ROOT / SOURCE).read_text()
    cases = (
        ('magnitude acquisition ignored',
         'DOMMY_RETURN_IF_FAILURE(status);\n    const double magValue', 'const double magValue'),
        ('VDB acquisition ignored',
         'DOMMY_RETURN_IF_FAILURE(status);\n    if (inputVdbHandle.data()', 'if (inputVdbHandle.data()'),
        ('selection acquisition ignored',
         'DOMMY_RETURN_IF_FAILURE(status);\n    const std::string selectionStr', 'const std::string selectionStr'),
        ('payload validity ignored', 'if (!inputVdb || !inputVdb->hasValidData())', 'if (false)'),
        ('zero array allocation ignored',
         'return setZeroForces(receptorSize, outputForce);',
         'setZeroForces(receptorSize, outputForce); return MS::kSuccess;'),
        ('transformed coordinate guard omitted',
         'if (!validBoxSamplingPoint(indexPoint)) return MS::kInvalidParameter;', ''),
        ('positive stencil neighbor guard omitted',
         'base == std::numeric_limits<std::int32_t>::max()', 'false'),
    )
    for label, old, new in cases:
        assert old in source, label
        try:
            check(source.replace(old, new), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'vdb_field_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_boundary(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()

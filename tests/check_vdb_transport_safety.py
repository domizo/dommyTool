#!/usr/bin/env python3
"""Actual Advect/Morph orchestration with explicit Maya/OpenVDB boundary doubles."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

FOLDER = 'src/modules/volume/VDB/Edit/'
NAMES = ('DOpenVDBAdvect', 'DOpenVDBMorphLevelSet')

from vdb_dispatch_source import grid_dispatch_source

def initializers(root):
    code = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += declarations(name, source, True)
        code += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(code)

def compute_bodies(root):
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.h').read_text()
    plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
    value = (root / (FOLDER + 'DOpenVDBValuePolicy.h')).read_text()
    code = ['namespace dommy { namespace vdb {',
            value[value.index('template <typename T> struct ValueComponents'):value.index('template <typename T, bool Integral')],
            extract(value, 'struct ValuePreflight') + ';', '}}',
            extract((root / (FOLDER + 'DOpenVDBFilterPolicy.h')).read_text(), 'namespace dommy {'),
            extract((root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace maya {',
            extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    code += [extract(util, signature) for signature in (
        'bool matchesGridName(', 'bool containsGrid(', 'bool\ngetSelectedGrids(', 'bool getSelectedGrids(')]
    code += [grid_dispatch_source(root, point_index_in_points=True)]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += declarations(name, source, False)
        code += [extract(source, 'namespace {'), extract(source, 'MStatus ' + name + '::compute('),
                 'DOMMY_VDB_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(code)

def sdk_check(root, sdk):
    code = ['#include <maya/MPxNode.h>', '#include <maya/MFnNumericAttribute.h>',
            '#include <maya/MFnTypedAttribute.h>', '#include <maya/MFnEnumAttribute.h>',
            '#include <maya/MFnStringData.h>', '#include <maya/MTypeId.h>',
            '#include <maya/MPxData.h>', '#include <cmath>',
            '#include "common/utility/DComputeGuard.h"',
            '#include "common/utility/DStatusOperation.h"',
            '#include "common/utility/DNumericValidation.h"',
            '#include "common/utility/DMayaPluginData.h"',
            '#include "vdb_transport_enums.h"',
            'struct DOpenVDBData : MPxData {static const MTypeId id;};',
            'const MTypeId DOpenVDBData::id(123);', 'namespace dommy {namespace vdb {']
    policy = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    code += ['const int kMaxTransportSteps = 1024;', extract(policy, 'struct TransportSettings') + ';',
             extract(policy, 'inline bool validTransportSettings('), '}}']
    for name, reader in zip(NAMES, ('readAdvectionSettings', 'readMorphSettings')):
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        code += declared + [extract(source, 'MStatus ' + name + '::initialize('),
            'using dommy::vdb::TransportSettings;', extract(source, 'MStatus ' + reader + '(')]
    # Also validate the optional-mask MDataHandle API with the real SDK.
    code += ['bool optionalMask(MDataHandle handle) {return !handle.data().isNull();}']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-Wno-unused-variable', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core'), '-I' + str(root / 'tests/cpp')]
    for name in NAMES:
        subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + name + '.h')),
                                 '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-transport-sdk-') as directory:
        path = Path(directory) / 'transport.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('Transport public headers, complete initializers and settings readers: SDK syntax/analysis passed')
    print('Native OpenVDB kernels/full compute translation units are NOT covered by the SDK check')

def guards(root):
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        assert 'CHECK_MSTATUS(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
        assert 'publishOpenVDBOutput(' in source
        assert 'namespace {' not in (root / (FOLDER + name + '.h')).read_text()

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('Double Level Set output omitted', False, body.replace('output.insert(copy);', 'if (!source->isType<openvdb::DoubleGrid>()) output.insert(copy);')),
        ('failed input consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('invalid enum accepted', False, body.replace('s.temporal <= 2', 's.temporal <= 100')),
        ('unbounded substeps', False, body.replace('s.substeps <= kMaxTransportSteps', 'true')),
        ('invalid payload accepted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('distance budget removed', False, body.replace('distance > kMaxTransportDistance', 'false')),
        ('coordinate budget removed', False, body.replace('!filterCoordinate(index[axis], 4)', 'false')),
        ('mask interval unchecked', False, body.replace('!dommy::vdb::validTransportMask(settings, magnitude)', 'false')),
        ('postcondition omitted', False, body.replace('!dommy::vdb::transportValues(*result, work, magnitude)', 'false')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_transport_init' if initialization else 'vdb_transport_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    guards(root)
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='vdb_transport_init')
        run(root, compute_bodies(root), suite='vdb_transport_compute', arguments=[])

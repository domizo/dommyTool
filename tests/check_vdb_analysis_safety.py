#!/usr/bin/env python3
"""Analysis actual-body boundary regressions; no native sparse-kernel emulation."""
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
from check_vdb_fill_safety import compute_bodies as fill_bodies

FOLDER = 'src/modules/volume/VDB/Edit/'
NAME = 'DOpenVDBAnalysis'


def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])


def compute_bodies(root):
    common = fill_bodies(root).split('struct DOpenVDBFill {')[0]
    spatial = extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {')
    transport = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    magnitude = transport[transport.index('template <typename T> struct TransportMagnitude'):
                          transport.index('template <typename GridT>\nbool transportValues')]
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    declared = declarations(NAME, source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    reader = extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(), 'MStatus getValidInputVDB(')
    return '\n'.join([common, spatial, reader, 'namespace dommy {namespace vdb {', magnitude, '}}'] +
                     declared + [extract(source, 'namespace {'),
                     extract(source, 'MStatus ' + NAME + '::compute('),
                     'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")'])


def sdk_check(root, sdk):
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnTypedAttribute', 'MFnEnumAttribute', 'MFnStringData', 'MTypeId', 'MPxData')]
    code = includes + ['#include <string>', '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"', '#include "common/utility/DMayaPluginData.h"',
        'struct DOpenVDBData:MPxData{static const MTypeId id;bool hasValidData()const;};',
        'const MTypeId DOpenVDBData::id(123);']
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'enum OpId') + ';', extract(source, 'struct AnalysisSettings') + ';',
        extract(source, 'MStatus readAnalysisSettings('),
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(), 'MStatus getValidInputVDB('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-analysis-sdk-') as directory:
        path = Path(directory) / 'analysis.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Analysis public header, full initializer and readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    length = extract(body, 'template <typename T> T analysisMagnitude(')
    unsafe_length = re.sub(r'const long double length =.*?;',
        'const long double length = std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);',
        length, count=1, flags=re.S)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('result && result->hasValidData()', 'result')),
        ('mask connection ignored', False, body.replace('if (connected) {', 'if (connected && false) {')),
        ('invalid naming mode accepted', False, body.replace('settings.renaming < 0 || settings.renaming > 2', 'false')),
        ('integer scalar claims support', False, body.replace('input != Type::kFloat && input != Type::kDouble', 'input != Type::kFloat && input != Type::kDouble && input != Type::kInt32')),
        ('source finite values unchecked', False, body.replace('!values.valid || !dommy::vdb::validTransportTransform(grid)', '!dommy::vdb::validTransportTransform(grid)')),
        ('derivative range unchecked', False, body.replace('dommy::vdb::TransportMagnitude<Value>::get(grid.background()) > limit', 'false').replace('dommy::vdb::TransportMagnitude<Value>::get(*it) > limit', 'false')),
        ('squared vector length overflows', False, body.replace(length, unsafe_length)),
        ('normalization returns zero', False, body.replace('dommy::math::normalized3OrZero(value[0], value[1], value[2], x, y, z, T(1.0e-7));', '(void)value; x=y=z=T(0);')),
        ('coordinate headroom omitted', False, body.replace('kGridCoordinatePadding = 4096', 'kGridCoordinatePadding = 0')),
        ('relative mask scale unchecked', False, body.replace('length < 1.0 / 64 || length > 64', 'false')),
        ('tile expansion uncharged', False, body.replace('if (settings.operation < kMagnitude && !inspect.bounds.empty())', 'if (false && !inspect.bounds.empty())')),
        ('source memory uncharged', False, body.replace('if (!budget.addBytes(1, grid.memUsage()))', 'if (false)')),
        ('later grid preflight skipped', False, body.replace('i < grids.size()', 'i < std::min(grids.size(), std::size_t(1))', 1)),
        ('result aliases input', False, body.replace('result.get() == grids[i].get() ||', '')),
        ('result finite values unchecked', False, body.replace(' || !values.valid', '')),
        ('historical suffix changed', False, body.replace('"_gradient"', '"_grad"')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True, suite='vdb_analysis_init' if initialization else 'vdb_analysis_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    assert 'CHECK_MSTATUS(' not in source
    assert not re.search(r'inputValue\([^;]+\)\.as', source)
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        mutations(root)
    elif '--init-only' in sys.argv:
        run(root, initializers(root), suite='vdb_analysis_init')
    else:
        run(root, initializers(root), suite='vdb_analysis_init')
        run(root, compute_bodies(root), suite='vdb_analysis_compute', arguments=[])

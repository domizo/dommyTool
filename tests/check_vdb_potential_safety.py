#!/usr/bin/env python3
"""Potential Flow production bodies with explicit host/native boundaries."""
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
NAME = 'DVDBPotentialFlow'

def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])

def compute_bodies(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    transport = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    magnitude = transport[transport.index('template <typename T> struct TransportMagnitude'):
                          transport.index('template <typename GridT>\nbool transportValues')]
    code = [fill_bodies(root).split('struct DOpenVDBFill {')[0],
            extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace vdb {', magnitude, '}}',
            extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(),
                    'MStatus getValidInputVDB('),
            extract((root / (FOLDER + 'DOpenVDBPotentialFlowCompat.h')).read_text(), 'namespace dommy {')]
    declared = declarations(NAME, source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    code += declared + [extract(source, 'namespace {'),
        extract(source, 'MStatus ' + NAME + '::compute('), 'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")']
    return '\n'.join(code)

def sdk_check(root, sdk):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    code = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MTypeId', 'MPxData', 'MDataHandle', 'MDataBlock')]
    code += ['#include <string>', '#include <cmath>', '#include <algorithm>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"',
        '#include "common/utility/DNumericValidation.h"',
        'struct DOpenVDBData:MPxData{static const MTypeId id;bool hasValidData()const;};',
        'const MTypeId DOpenVDBData::id(123);',
        'const int DEFAULT_MAX_ITERATIONS=10000, kMaxPotentialDilation=256;',
        'constexpr double DEFAULT_MAX_ERROR=1e-20;',
        'namespace openvdb{namespace math{template<typename T>struct Vec3{'
        'T v[3];explicit Vec3(T n):v{n,n,n}{} Vec3(T a,T b,T c):v{a,b,c}{}'
        'T operator[](int i)const{return v[i];}};'
        'template<typename T>struct Delta;'
        'template<>struct Delta<float>{static float value(){return 1e-5f;}};}'
        'using Vec3f=math::Vec3<float>;}']
    value = (root / (FOLDER + 'DOpenVDBValuePolicy.h')).read_text()
    transport = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    code += ['namespace dommy{namespace vdb{',
        value[value.index('template <typename T> struct ValueComponents'):
              value.index('template <typename T, bool Integral')],
        transport[transport.index('template <typename T> struct TransportMagnitude'):
                  transport.index('template <typename GridT>\nbool transportValues')], '}}']
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'struct PotentialSettings') + ';',
        extract(source, 'MStatus readPotentialSettings('),
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(),
                'MStatus getValidInputVDB('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-potential-sdk-') as directory:
        path = Path(directory) / 'potential.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Potential Flow header, initializer and settings reader: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(',
                                                            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('result && result->hasValidData()', 'result')),
        ('dilation cast unchecked', False, body.replace(
            '!dommy::numeric::checkedNumericCast(std::ceil(value / voxelWidth), settings.dilation)',
            '(settings.dilation = static_cast<int>(std::ceil(value / voxelWidth)), false)')),
        ('dilation ceiling removed', False, body.replace('settings.dilation > kMaxPotentialDilation', 'false')),
        ('iteration ceiling removed', False, body.replace('settings.iterations > DEFAULT_MAX_ITERATIONS', 'false')),
        ('invalid mask mode accepted', False, body.replace('settings.maskType < 0 || settings.maskType > 1', 'false')),
        ('nonfinite source admitted', False, body.replace('if (!check.valid ||', 'if (false ||')),
        ('numeric envelope removed', False, body.replace('if (magnitude > limit)', 'if (false)')),
        ('source memory uncharged', False, body.replace('if (!budget.addBytes(1, solid->memUsage()))', 'if (false)')),
        ('domain budget ignored', False, body.replace('if (!estimate.valid)', 'if (false)')),
        ('relative map unchecked', False, body.replace(
            '!dommy::vdb::validRelativeGridMap(from.transform(), to.transform())', 'false')),
        ('potential output unchecked', False, body.replace('!finitePotentialGrid<ScalarGrid>(scalar)', 'false')),
        ('flow output unchecked', False, body.replace('!finitePotentialGrid<VelocityGrid>(vector)', 'false')),
        ('nonfinite solver state ignored', False, body.replace(
            '!std::isfinite(state.absoluteError) || state.absoluteError < 0', 'false')),
        ('boundary voxel width wrong', False, body.replace('voxelWidth_(voxelWidth)', 'voxelWidth_(voxelWidth * 2)')),
        ('boundary arithmetic unchecked', False, body.replace(
            'if (!numeric::checkedNumericCast(value, source))', 'if ((source = double(value), false))')),
        ('wrong Dirichlet sign', False, body.replace('static_cast<long double>(diagonal) - 1',
                                                    'static_cast<long double>(diagonal) + 1')),
        ('non-staggered solver', False, body.replace('rhs, boundary, state, interrupter, true',
                                                    'rhs, boundary, state, interrupter, false')),
        ('disabled background consumed', False, body.replace('if (useBackground.asBool())', 'if (true)')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True, suite='vdb_potential_init' if initialization else 'vdb_potential_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
        sys.exit(0)
    if '--mutations' in sys.argv:
        mutations(root)
        sys.exit(0)
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    assert 'CHECK_MSTATUS(' not in source
    assert not re.search(r'inputValue\([^;]+\)\.as', source)
    assert 'tools::computeScalarPotential(' not in source
    run(root, initializers(root), suite='vdb_potential_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv and '--init-only' not in sys.argv:
        run(root, compute_bodies(root), suite='vdb_potential_compute', arguments=[])

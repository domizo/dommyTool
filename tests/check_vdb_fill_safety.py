#!/usr/bin/env python3
"""Actual Fill bodies with recording host/tree boundaries (not native OpenVDB)."""
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
NAME = 'DOpenVDBFill'


from vdb_dispatch_source import grid_dispatch_source

def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])


def compute_bodies(root):
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.h').read_text()
    plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
    policy = (root / (FOLDER + 'DOpenVDBValuePolicy.h')).read_text()
    code = ['namespace dommy { namespace vdb {',
            policy[policy.index('template <typename T> struct ValueComponents'):
                   policy.index('template <typename T, bool Integral')],
            extract(policy, 'struct ValuePreflight') + ';',
            extract(policy, 'struct PositiveMaskBackground') + ';',
            extract((root / (FOLDER + 'DOpenVDBFilterPolicy.h')).read_text(),
                    'inline bool filterCoordinate('),
            extract((root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text(),
                    'template <typename GridT> bool validTransportTransform('), '}}',
            extract((root / (FOLDER + 'DOpenVDBTreeBudget.h')).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace maya {',
            extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    code += [extract(util, sig) for sig in ('bool matchesGridName(', 'bool containsGrid(',
             'bool\ngetSelectedGrids(', 'bool getSelectedGrids(')]
    code += [grid_dispatch_source(root, point_index_in_points=True)]
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    declared = declarations(NAME, source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    code += declared + [extract(source, 'namespace {'),
                        extract(source, 'MStatus ' + NAME + '::compute('),
                        'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")']
    return '\n'.join(code)


def sdk_check(root, sdk):
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MFnMeshData', 'MBoundingBox', 'MTypeId', 'MPxData')]
    code = includes + ['#include <cmath>', '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        'struct DOpenVDBData {static const MTypeId id;};',
        'const MTypeId DOpenVDBData::id(123);',
        # Only the vector value container is doubled; every Maya API below is real.
        'namespace openvdb {struct Vec3R {double v[3]; Vec3R(){} '
        'Vec3R(double a,double b,double c):v{a,b,c}{} '
        'double operator[](int i)const{return v[i];}};}',
        'MStatus getMeshInformation(const MObject&, MDataBlock&, MBoundingBox&, MObject&);']
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'struct FillSettings') + ';',
        extract(source, 'bool validFillBounds('), extract(source, 'MStatus readFillSettings('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'),
        '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-fill-sdk-') as directory:
        path = Path(directory) / 'fill.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Fill public header, full initializer and input readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer failure ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('invalid mode accepted', False, body.replace('settings.bounds < 0 || settings.bounds > 2', 'false')),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('integer cast unchecked', False, body.replace('return dommy::numeric::checkedNumericCast(value[0], result);', 'result = static_cast<T>(value[0]); return true;')),
        ('negative bool becomes false', False, body.replace('result = value[0] != 0;', 'result = value[0] > 0;')),
        ('geometry status ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(getMeshInformation(Node::aInMesh, data, bounds, mesh));', '(void)getMeshInformation(Node::aInMesh, data, bounds, mesh);')),
        ('coordinate padding omitted', False, body.replace('filterCoordinate(first, RootChild::DIM)', 'filterCoordinate(first, 0)').replace('filterCoordinate(last, RootChild::DIM)', 'filterCoordinate(last, 0)')),
        ('fill expansion budget omitted', False, body.replace('if (!FillNodeBudget<RootChild>::add(plan.bounds, budget))', 'if (false)')),
        ('copy memory not charged', False, body.replace('!grid || !budget.addBytes(1U, grid->memUsage())', '!grid')),
        ('late preflight failure ignored', False, body.replace('plans[i].supported && !operation.valid', 'false')),
        ('fill result unchecked', False, body.replace('valid = result.valid;', 'valid = true;')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_fill_init' if initialization else 'vdb_fill_compute',
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
        run(root, initializers(root), suite='vdb_fill_init')
    else:
        run(root, initializers(root), suite='vdb_fill_init')
        run(root, compute_bodies(root), suite='vdb_fill_compute', arguments=[])

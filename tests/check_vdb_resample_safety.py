#!/usr/bin/env python3
"""Resample production bodies with explicit Maya/OpenVDB boundaries."""
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
NAME = 'DOpenVDBResample'

def declare(source, initialization):
    result = declarations(NAME, source, initialization)
    result[0] = result[0].replace('{', '{enum {MODE_PARMS,MODE_REF_GRID,MODE_VOXEL_SIZE,MODE_VOXEL_SCALE};', 1)
    return result

def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declare(source, True) + [extract(source, 'MStatus ' + NAME + '::initialize(')])

def compute_bodies(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    value = (root / (FOLDER + 'DOpenVDBValuePolicy.h')).read_text()
    transport = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.h').read_text()
    code = [fill_bodies(root).split('struct DOpenVDBFill {')[0],
        extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {'),
        'namespace dommy { namespace vdb {',
        value[value.index('template <typename T, bool Integral'):value.index('// Charge stored values')],
        transport[transport.index('template <typename T> struct TransportMagnitude'):
                  transport.index('template <typename GridT>\nbool transportValues')],
        'const int kMaxFilterWidth = 64;',
        extract((root / (FOLDER + 'DOpenVDBFilterPolicy.h')).read_text(),
                'template <typename GridT> bool validLevelSetScale('), '}}',
        'namespace dommy { namespace maya {',
        extract((root / 'src/core/common/utility/DMayaValueValidation.h').read_text(),
                'inline bool isFiniteMatrix('), '}}',
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(),
                'MStatus getValidInputVDB('),
        extract(util, 'template<typename Sampler>\nclass GridTransformOp') + ';',
        extract(util, 'template<typename Sampler>\nclass GridResampleToMatchOp') + ';']
    declared = declare(source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    code += declared + [extract(source, 'namespace {'),
        extract(source, 'MStatus ' + NAME + '::compute('), 'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")']
    return '\n'.join(code)

def sdk_check(root, sdk):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    code = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MFnMatrixAttribute', 'MTypeId', 'MDataHandle', 'MDataBlock',
        'MTransformationMatrix', 'MPlug')]
    code += ['#include <string>', '#include <cmath>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaValueValidation.h"',
        'struct DOpenVDBData{static const MTypeId id;};const MTypeId DOpenVDBData::id(123);']
    declared = declare(source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'struct ResampleSettings') + ';',
        extract(source, 'struct ResampleTransform') + ';',
        extract(source, 'MStatus readResampleSettings('), extract(source, 'MStatus readResampleTransform('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-resample-sdk-') as directory:
        path = Path(directory) / 'resample.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Resample header, initializer and settings/matrix readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(',
                                                            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('result && result->hasValidData()', 'result')),
        ('explicit mip scale unchecked', False, body.replace('std::fabs(result.scale[axis]) < 1.0 / 64', 'false')),
        ('voxel scale unchecked', False, body.replace('settings.scale < 1.0f / 64 || settings.scale > 64', 'false')),
        ('source original dispatched', False, body.replace('type, *copy, operation', 'type, *source, operation')),
        ('source preflight ignored', False, body.replace('!inspect.valid ||', 'false ||')),
        ('magnitude envelope removed', False, body.replace(' > limit) return;', ' > limit && false) return;')),
        ('source memory uncharged', False, body.replace('!budget.addBytes(2, source.memUsage())', 'false')),
        ('relative scale unchecked', False, body.replace(
            '!dommy::vdb::validRelativeGridMap(from, to) || !dommy::vdb::validRelativeGridMap(to, from)', 'false')),
        ('translation envelope removed', False, body.replace(
            'std::fabs(origin[axis]) > 1.0e6 || std::fabs(inverse[axis]) > 1.0e6', 'false')),
        ('resource estimate ignored', False, body.replace('!cost.valid', 'false')),
        ('level-set band unchecked', False, body.replace('!band.valid', 'false')),
        ('half-width underflow accepted', False, body.replace('halfWidth > 0;', 'true;')),
        ('result finite validation removed', False, body.replace(
            '!finiteResampleResult(outGrid, type, outputWork)', 'false')),
        ('integer pruning range unchecked', False, body.replace('if (!range.add(*it)) return;',
                                                                 'if (!range.add(*it) && false) return;')),
        ('disabled tolerance consumed', False, body.replace('if (settings.prune) {', 'if (true) {', 1)),
        ('vector world-space gate removed', False, body.replace('outGrid->isInWorldSpace() &&', 'true &&')),
        ('bool sampler override removed', False, body.replace(
            'type == DVdbGridType::kBool ? 0 : settings.order', 'settings.order')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True, suite='vdb_resample_init' if initialization else 'vdb_resample_compute',
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
    run(root, initializers(root), suite='vdb_resample_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv and '--init-only' not in sys.argv:
        run(root, compute_bodies(root), suite='vdb_resample_compute', arguments=[])

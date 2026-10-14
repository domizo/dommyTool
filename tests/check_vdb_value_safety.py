#!/usr/bin/env python3
"""Prune/Remap numeric policy and host-boundary regressions."""
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
NAMES = ('DOpenVDBPrune', 'DOpenVDBRemap')
POLICY = FOLDER + 'DOpenVDBValuePolicy.h'


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
    code = [extract((root / POLICY).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace maya {',
            extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    code += [extract(util, signature) for signature in (
        'bool matchesGridName(', 'bool containsGrid(', 'bool\ngetSelectedGrids(', 'bool getSelectedGrids(')]
    code += [grid_dispatch_source(root)]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += [extract(source, 'namespace {')]
        declared = declarations(name, source, False)
        declared[0] = declared[0].replace('MStatus compute(', 'MObject thisMObject()const{return {"node"};} MStatus compute(')
        code += declared + [extract(source, 'MStatus ' + name + '::compute('),
            'DOMMY_VDB_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(code)


def guards(root):
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        assert 'CHECK_MSTATUS(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
        for dependency in ('DComputeGuard.h', 'DMayaPluginData.h', 'DStatusOperation.h'):
            assert '#include "common/utility/' + dependency + '"' in source
        assert 'publishOpenVDBOutput(' in source


def sdk_check(root, sdk):
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MRampAttribute', 'MTypeId', 'MPxData')]
    code = includes + ['#include <cmath>', '#include <vector>', '#include <stdexcept>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"',
        'struct DOpenVDBData : MPxData {static const MTypeId id; bool hasValidData() const;};',
        'const MTypeId DOpenVDBData::id(123);', 'namespace dommy {namespace vdb {']
    policy = (root / POLICY).read_text()
    code += [extract(policy, 'inline bool validPruneSettings('),
             extract(policy, 'inline bool validRemapSettings('), '}}']
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode').replace(
            'static MStatus initialize();', 'static MStatus initialize(); MStatus readInputs(MDataBlock&);')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        body = extract(source, 'MStatus ' + name + '::compute(')
        readers = body[body.index('MStatus stat;'):body.index('GridCPtrVec selected;')]
        if name == 'DOpenVDBRemap':
            readers += '\nstatic_cast<void>(deactivate);\n'
        code += declared + [extract(source, 'MStatus ' + name + '::initialize('),
            'MStatus ' + name + '::readInputs(MDataBlock& data) try {' + readers +
            '\nreturn MS::kSuccess;} DOMMY_COMPUTE_CATCH("value input readers")']
    remap = (root / (FOLDER + NAMES[1] + '.cpp')).read_text()
    code += [extract(remap, 'class RampLookup') + ';']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-Wno-unused-variable', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    for name in NAMES:
        subprocess.run(command + ['-x', 'c++', '-include',
            str(root / (FOLDER + name + '.h')), '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-value-sdk-') as directory:
        path = Path(directory) / 'value.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB value edits: complete initializers, input acquisitions, ramp lookup and public headers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation units are NOT covered by this SDK check')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer failure ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('invalid input handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('invalid payload accepted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('prune tolerance guard omitted', False, body.replace(' && tolerance <= 1', '')),
        ('prune integer difference unchecked', False, body.replace('high <= std::numeric_limits<T>::max() + low', 'true')),
        ('unsafe level-set background allowed', False, body.replace('ValueComponents<typename GridT::ValueType>::negatable(', 'ValueComponents<typename GridT::ValueType>::finite(')),
        ('preserve classified after float narrowing', False, body.replace('compareRemapBound(source, inMin)', 'compareRemapBound(static_cast<float>(source), inMin)').replace('compareRemapBound(source, inMax)', 'compareRemapBound(static_cast<float>(source), inMax)')),
        ('ramp position narrowed to float', False, body.replace('const long double position, const bool extrapolate', 'const float position, const bool extrapolate')),
        ('ramp sample difference narrowed', False, body.replace('static_cast<long double>(mSamples[lower + 1])', 'mSamples[lower + 1]')),
        ('unsupported grid dropped', False, body.replace('outputVdb->insert(*source);', '')),
        ('prune result check omitted', False, body.replace(' || !result.valid', '')),
        ('component work undercounted', False, body.replace('Components::count * passes', 'passes')),
        ('background comparison subtracts signed values', False, body.replace('std::is_integral<T>::value ? a == b', 'std::is_integral<T>::value ? (a-b)==0')),
        ('background not remapped', False, body.replace('setBackground(op.map(grid.background()),', 'setBackground(grid.background(),')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_value_init' if initialization else 'vdb_value_compute',
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
    elif '--emit' in sys.argv:
        run(root, initializers(root), suite='vdb_value_init', arguments=['--emit'])
    else:
        run(root, initializers(root), suite='vdb_value_init')
        run(root, compute_bodies(root), suite='vdb_value_compute', arguments=[])

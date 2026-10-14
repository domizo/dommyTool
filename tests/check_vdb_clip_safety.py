#!/usr/bin/env python3
"""Clip preflight and actual compute/initialization boundary regressions."""
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
NAME = 'DOpenVDBClip'


def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])


def compute_bodies(root):
    # Shared numeric, tree-budget, selection and dispatch production bodies.
    common = fill_bodies(root).split('struct DOpenVDBFill {')[0]
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    header = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.h').read_text()
    spatial = extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {')
    reader = extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(), 'MStatus getValidInputVDB(')
    return '\n'.join([common, spatial, reader, extract(header, 'struct BBoxClipOp') + ';'] +
                     declarations(NAME, source, False) + [extract(source, 'namespace {'),
                     extract(source, 'MStatus ' + NAME + '::compute('),
                     'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")'])


def sdk_check(root, sdk):
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnGenericAttribute', 'MFnStringData', 'MTypeId', 'MPxData')]
    code = includes + ['#include <cmath>', '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"',
        'struct DOpenVDBData : MPxData {static const MTypeId id; bool hasValidData()const;};',
        'const MTypeId DOpenVDBData::id(123);']
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'struct ClipSettings') + ';',
        extract(source, 'MStatus readClipSettings('),
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(), 'MStatus getValidInputVDB('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'),
        '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-clip-sdk-') as directory:
        path = Path(directory) / 'clip.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Clip public header, full initializer and readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')


def budget_link_check(root):
    # Compile the shared production namespace in two translation units: a
    # single-unit extraction would miss an accidental non-inline definition.
    source = '\n'.join([
        '#include <array>', '#include "common/utility/DGenerationLimits.h"',
        '#include "common/utility/DNumericValidation.h"',
        '#include "modules/volume/VDB/Common/DOpenVDBLimits.h"',
        'namespace openvdb { struct CoordBBox {'
        'std::array<int,3> min()const{return {{0,0,0}};}'
        'std::array<int,3> max()const{return {{0,0,0}};} };}',
        'namespace dommy {namespace vdb {template<typename T> struct ValueComponents;}}',
        extract((root / (FOLDER + 'DOpenVDBTreeBudget.h')).read_text(), 'namespace dommy {')])
    flags = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-tree-budget-link-') as directory:
        a, b = Path(directory) / 'a.cpp', Path(directory) / 'b.cpp'
        function = 'std::size_t n=0;return dommy::vdb::treeBlockCount(openvdb::CoordBBox(),8,n)&&n==1;'
        a.write_text(source + '\nint first(){' + function + '}')
        b.write_text(source + '\nint first();int main(){std::size_t n=0;'
                     'return !(first()==1 && dommy::vdb::treeBlockCount('
                     'openvdb::CoordBBox(),8,n) && n==1);}')
        binary = Path(directory) / 'link'
        subprocess.run(flags + [str(a), str(b), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('Shared tree budget: two-translation-unit link passed')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('result && result->hasValidData()', 'result')),
        ('invalid clip mode accepted', False, body.replace('settings.mode < 0 || settings.mode > 3', 'false')),
        ('coordinate headroom omitted', False, body.replace('kGridCoordinatePadding = 4096', 'kGridCoordinatePadding = 0')),
        ('upper endpoint rounds up', False, body.replace('std::floor(box.max()[axis])', 'std::ceil(box.max()[axis])')),
        ('source memory not charged', False, body.replace('if (!budget.addBytes(1, grids[i]->memUsage()))', 'if (false)')),
        ('region budget omitted', False, body.replace('valid = dommy::vdb::TreeNodeBudget<RootChild>::add(region, budget);', 'valid = sizeof(RootChild) != 0;')),
        ('relative scale unchecked', False, body.replace('length < 1.0 / 64 || length > 64', 'false')),
        ('invalid source preflight ignored', False, body.replace('plans[i].supported && !inspect.valid', 'false')),
        ('native null silently accepted', False, body.replace('if (!result || result.get()', 'if (!result) { outputVdb->insert(*grid); continue; }\n if (result.get()')),
        ('result values unchecked', False, body.replace(' || !values.valid', '')),
        ('historical suffix changed', False, body.replace('"_cliped"', '"_clipped"')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_clip_init' if initialization else 'vdb_clip_compute',
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
        run(root, initializers(root), suite='vdb_clip_init')
    else:
        run(root, initializers(root), suite='vdb_clip_init')
        run(root, compute_bodies(root), suite='vdb_clip_compute', arguments=[])
        budget_link_check(root)

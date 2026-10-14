#!/usr/bin/env python3
"""VDB filter policy and complete host-boundary control-flow regression tests."""
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
NAMES = ('DOpenVDBFilter', 'DOpenVDBFilterLevelSet')
POLICY = FOLDER + 'DOpenVDBFilterPolicy.h'


def initializer(root):
    header = (root / (FOLDER + NAMES[1] + '.h')).read_text()
    level = (root / (FOLDER + NAMES[1] + '.cpp')).read_text()
    # Original header helpers are used only for the pre-edit schema capture.
    helpers = extract(header, 'namespace {') if 'namespace {' in header else extract(level, 'namespace {')
    if 'namespace {' not in header:
        helpers = helpers[:helpers.index('template <typename GridT>')] + '\n}'
    code = [helpers]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += declarations(name, source, True) + [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(code)


def compute_bodies(root):
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
    code = [extract((root / POLICY).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace maya {',
            extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    code += [extract(util, signature) for signature in ('void getGrids(',
        'bool matchesGridName(', 'bool containsGrid(', 'bool\ngetSelectedGrids(', 'bool getSelectedGrids(')]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += [extract(source, 'namespace {')] + declarations(name, source, False)
        code += [extract(source, 'MStatus ' + name + '::compute('),
                 'DOMMY_VDB_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(code)


def sdk_check(root, sdk):
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MTypeId', 'MPxData')]
    includes += ['#include <memory>', '#include <vector>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"', '#include "' + POLICY[4:] + '"']
    # Real Maya input-handle/attribute APIs; native grid functions are declared
    # boundaries. This deliberately does not claim full OpenVDB compilation.
    stubs = '''
namespace openvdb {struct FloatGrid {std::uint64_t activeVoxelCount() const;};}
struct Grid : openvdb::FloatGrid {template<typename T> bool isType() const;};
using GridCPtrVec = std::vector<std::shared_ptr<const Grid>>;
struct DOpenVDBData : MPxData {static const MTypeId id; bool hasValidData() const;};
const MTypeId DOpenVDBData::id(123);
bool getSelectedGrids(GridCPtrVec&, const std::string&, const DOpenVDBData&);
void getGrids(GridCPtrVec&, const DOpenVDBData&, const std::string&);
namespace dommy {namespace vdb {bool finiteFilterValues(const openvdb::FloatGrid&);}}
'''
    level = (root / (FOLDER + NAMES[1] + '.cpp')).read_text()
    menus = extract(level, 'namespace {').split('template <typename GridT>')[0] + '\n}'
    code = includes + [stubs, menus]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode').replace(
            'static MStatus initialize();', 'static MStatus initialize(); MStatus readInputs(MDataBlock&);')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        body = extract(source, 'MStatus ' + name + '::compute(')
        start = body.index('MStatus stat;')
        end = body.index('dommy::vdb::FilterPlan plan;' if name==NAMES[0] else 'std::size_t work = 0;')
        code += declared + [extract(source, 'MStatus ' + name + '::initialize('),
            'MStatus ' + name + '::readInputs(MDataBlock& data) try {' + body[start:end] +
            '\nreturn MS::kSuccess;} DOMMY_COMPUTE_CATCH("filter input readers")']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-Wno-unused-but-set-variable', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    for name in NAMES:
        subprocess.run(command + ['-x', 'c++', '-include',
            str(root / (FOLDER + name + '.h')), '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-filter-sdk-') as directory:
        path = Path(directory) / 'filter.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB filters: complete initializers, input acquisitions, public headers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation units are NOT covered by this SDK check')


def guards(root):
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        assert 'CHECK_MSTATUS(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
        assert 'publishOpenVDBOutput(' in source
        for dependency in ('DComputeGuard.h', 'DMayaPluginData.h', 'DStatusOperation.h'):
            assert '#include "common/utility/' + dependency + '"' in source
    header = (root / (FOLDER + NAMES[1] + '.h')).read_text()
    assert 'FilterParms' not in header and 'openvdb/' not in header


def mutations(root):
    init, body = initializer(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('input status ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('payload check omitted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('invalid filter output published', False, body.replace('return dommy::vdb::finiteFilterValues(output);', 'return true;')),
        ('invalid level-set output published', False, body.replace('if (!dommy::vdb::finiteFilterValues(output) ||', 'if (false ||').replace('return dommy::vdb::finiteFilterValues(output) &&', 'return true &&')),
        ('selected fog dropped', False, body.replace('outputVdb->insert(*source);', '', 2)),
        ('aggregate work reset', False, body.replace('grid->activeVoxelCount(), plan, work', '(work=0, grid->activeVoxelCount()), plan, work')),
        ('level-set aggregate work reset', False, body.replace('grid->activeVoxelCount(), plans[i], work', '(work=0, grid->activeVoxelCount()), plans[i], work')),
        ('last mask changed to first', False, body.replace('if (candidate->isType<openvdb::FloatGrid>())', 'if (!mask && candidate->isType<openvdb::FloatGrid>())')),
        ('mask silently disabled', False, body.replace('const bool masked = enabledHandle.asBool();', 'const bool masked = (enabledHandle.asBool(), false);')),
        ('unsafe world width cast', False, body.replace('!numeric::checkedNumericCast(width, candidate)', '(candidate=static_cast<int>(width), false)')),
        ('offset steps unbounded', False, body.replace('count > kMaxFilterOffsetSteps', 'false')),
        ('coordinate padding removed', False, body.replace('double(std::numeric_limits<int>::max()) - padding', 'double(std::numeric_limits<int>::max())')),
        ('mask transform unchecked', False, body.replace('(!mask || dommy::vdb::validFilterMaskDomain(source, *mask, plan))', '(static_cast<void>(mask), true)')),
        ('Open direction reversed', False, re.sub(r'filter.offset\(plan.distance, mask\);\s*filter.offset\(-plan.distance, mask\);', 'filter.offset(-plan.distance, mask); filter.offset(plan.distance, mask);', body)),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_filter_init' if initialization else 'vdb_filter_compute',
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
        run(root, initializer(root), suite='vdb_filter_init', arguments=['--emit'])
    else:
        run(root, initializer(root), suite='vdb_filter_init')
        run(root, compute_bodies(root), suite='vdb_filter_compute', arguments=[])

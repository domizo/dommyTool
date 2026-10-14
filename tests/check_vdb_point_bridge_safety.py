#!/usr/bin/env python3
"""DPoint/OpenVDB bridges: production bodies with explicit host/API doubles."""
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

NODES = {
    'DPAttributeVDB': 'src/core/DDataUtility/Attribute/Edit/DPAttributeVDB.cpp',
    'DPClosestPointVDB': 'src/modules/point/DPoint/Utility/DPClosestPointVDB.cpp',
}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def compute_bodies(root):
    result = [extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                      'void AddAttributeList('),
              extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'MStatus getValidInputDP('),
              extract((root / 'src/core/common/utility/DPointPayloadBudget.h').read_text(),
                      'namespace dommy {')]
    for name, path in NODES.items():
        source = (root / path).read_text()
        if name == 'DPAttributeVDB':
            result.append(extract(source, 'namespace {'))
        declared = declarations(name, source, False)
        declared[0] = declared[0].replace(' {', ' {MString name() const {return "bridge";}', 1)
        result.extend(declared)
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_VDB_COMPUTE_CATCH("' + name + '")')
    return '\n'.join(result)


def guards(root):
    for path in NODES.values():
        source = (root / path).read_text()
        assert 'deepCopyGrid(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
        assert source.index('preflightPointPayload(') < source.index('createPluginData(')
        assert 'return MS::kUnknownParameter' in source
        assert 'DOMMY_VDB_COMPUTE_CATCH' in source
    cache = (root / 'src/modules/point/DPoint/Utility/DPCache.cpp').read_text()
    assert 'using CacheWorkspace = dommy::point::PointPayloadBudget;' in cache
    assert re.search(r'dommy::point::preflightPointPayload\(\s*input,', cache)


def sdk_check(root, sdk):
    headers = ('MPxNode', 'MFnNumericAttribute', 'MFnGenericAttribute', 'MFnTypedAttribute',
               'MFnStringData', 'MTypeId', 'MObject', 'MDataBlock', 'MDataHandle')
    result = ['#include <maya/' + h + '.h>' for h in headers]
    result += ['#include "common/utility/DComputeGuard.h"',
               '#include "common/utility/DStatusOperation.h"',
               '#include "common/utility/DPointPayloadBudget.h"',
               'struct DPData {static const MTypeId id;};const MTypeId DPData::id(1);',
               'struct DOpenVDBData {static const MTypeId id;};const MTypeId DOpenVDBData::id(2);']
    readers = 0
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
        result += [declared[0]] + [re.sub(r'\{"[^"]*"\}', '', line) for line in declared[1:]]
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        pattern = (r'const MDataHandle (\w+) = data\.inputValue\((\w+), &status\);\s*'
                   r'DOMMY_RETURN_IF_FAILURE\(status\);\s*const [^;]+\1\.as\w+\(\)(?:\.asChar\(\))?;')
        for match in re.finditer(pattern, source):
            body = match.group().replace('inputValue(' + match.group(2),
                                       'inputValue(' + name + '::' + match.group(2))
            target = re.findall(r'const [\w:]+ (\w+) =', body)[-1]
            result.append('MStatus read' + str(readers) + '(MDataBlock& data) try {MStatus status;\n' +
                          body + '\n(void)' + target +
                          ';return MS::kSuccess;} DOMMY_COMPUTE_CATCH("bridge")')
            readers += 1
    assert readers == 5, readers
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-variable',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core'),
        '-I' + str(root / 'src/modules/point/DPoint'), '-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-bridge-sdk-') as directory:
        path = Path(directory) / 'bridge.cpp'
        path.write_text('\n'.join(result))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB point bridges: two complete initializers, five input readers and shared budget header passed SDK syntax/analysis')
    print('Limited Imath declarations; complete OpenVDB translation units and native queries are not covered')
    sdk_output_check(root, sdk)


def sdk_output_check(root, sdk):
    # Use actual project owners/attributes and Maya declarations for the changed
    # writers. OpenVDB accessors/transforms are declaration-only API boundaries;
    # this does not compile full OpenVDB units or implement native query behavior.
    result = ['''
#include "DPData.h"
#include "DPointDataAccess.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DNumericValidation.h"
#include <memory>
#include <stdexcept>
namespace openvdb {
using Vec3s = Imath::V3f;
using Vec3d = Imath::V3d;
using Vec3R = Imath::V3d;
struct Coord { Coord(int, int, int); };
struct Transform {
    Vec3d worldToIndex(const Vec3s&) const;
    Vec3d worldToIndex(const Vec3d&) const;
};
template <typename Value> struct Grid {
    struct ConstAccessor { Value getValue(const Coord&) const; };
    const std::string& getName() const;
    ConstAccessor getConstAccessor() const;
    const Transform& transform() const;
};
}
''']
    result.append(extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                          'MStatus getValidInputDP('))
    source = (root / NODES['DPAttributeVDB']).read_text()
    result.append(extract(source, 'namespace {'))
    # Explicitly instantiate each production attribute/coordinate combination.
    for index, (attribute, vector, value, convert) in enumerate((
        ('DA_FLOAT', 'Vec3s', 'float', 'return value;'),
        ('DA_DOUBLE', 'Vec3s', 'double', 'return value;'),
        ('DA_INT', 'Vec3s', 'int32_t', 'return value;'),
        ('DA_INT64', 'Vec3s', 'int64_t', 'return value;'),
        ('DA_VEC3F', 'Vec3s', 'openvdb::Vec3s', 'return DA_VEC3F(value[0], value[1], value[2]);'),
        ('DA_VEC3D', 'Vec3d', 'openvdb::Vec3d', 'return DA_VEC3D(value[0], value[1], value[2]);'),
    )):
        result.append('void checkSample' + str(index) + '(DPData& output, const DPointList& points, '
                      'const openvdb::Grid<' + value + '>& grid) {\n'
                      'sampleVdbAttribute<' + attribute + ', openvdb::' + vector + '>(\n'
                      'output, grid, points, [](' + value + ' value) {' + convert + '});\n}')
    closest = (root / NODES['DPClosestPointVDB']).read_text()
    block = extract(closest, 'if (addAttribute) {')
    result.append('MStatus checkClosestOutput(DPData* output, bool addAttribute, '
                  'const std::string& attributeName, std::size_t count, '
                  'const std::vector<openvdb::Vec3R>& points) {\n' + block + '\nreturn MS::kSuccess;\n}')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + directory for directory, _, _ in os.walk(root / 'src')]
    command += ['-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-output-sdk-') as directory:
        path = Path(directory) / 'output.cpp'
        path.write_text('\n'.join(result))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB outputs: six private sample-helper types, closest writer and checked input passed SDK syntax/analysis')
    print('Real project attributes/owners; OpenVDB declarations only, no native query/ABI coverage')


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('empty surface searched', 'compute', compute.replace('query->indexTree().activeVoxelCount() == 0U', 'false')),
        ('null query dereferenced', 'compute', compute.replace('!query || ', '')),
        ('failed query ignored', 'compute', compute.replace('!query->searchAndReplace(points, distances)',
                                                          '(query->searchAndReplace(points, distances), false)')),
        ('short points accepted', 'compute', compute.replace('points.size() != count', 'false')),
        ('invalid distance accepted', 'compute', compute.replace('!std::isfinite(distances[index])', 'false')),
        ('invalid point accepted', 'compute', compute.replace('!dommy::numeric::canRepresentAsFloat(points[index][axis])', 'false')),
        ('integer overflow accepted', 'compute', compute.replace(
            '!dommy::numeric::checkedNumericCast(value[axis], components[axis])',
            '(components[axis] = static_cast<int>(value[axis]), false)')),
        ('negative coordinate rounded down', 'compute', compute.replace(
            'checkedNumericCast(value[axis], components[axis])',
            'checkedNumericCast(std::floor(value[axis]), components[axis])')),
        ('sample nonfinite accepted', 'compute', compute.replace('!dommy::attribute::isValidValue(value)', 'false')),
        ('input status ignored', 'compute', compute.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('output creation failure ignored', 'compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DPData::id, output));',
            '(void)dommy::maya::createPluginData(pluginData, DPData::id, output);', 1)),
        ('aggregate budget reset', 'compute', compute.replace('checkedSum(bytes_, bytes,', 'checkedSum(0U, bytes,')),
        ('aggregate budget removed', 'compute', compute.replace('!dommy::attribute::validArrayConversionBudget(1U, total)', 'false')),
        ('initializer status ignored', 'init', init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escaped', 'init', init.replace('return dommy::maya::reportEntryPointException(',
                                                              'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, kind, body in cases:
        assert body != (init if kind == 'init' else compute), label
        try:
            run(root, body, capture=True, suite='vdb_point_bridge_' + kind,
                arguments=None if kind == 'init' else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def publication_mutations(root):
    source = compute_bodies(root)
    closest = ('if (!dommy::attribute::commitStagedAttribute(\n'
               '                                *output, attributeName, candidate, count)) return MS::kFailure;')
    sample = ('if (!dommy::attribute::commitStagedAttribute(output, attributeName, candidate, points.size()))\n'
              '        throw std::runtime_error("DPAttributeVDB attribute publication failed");')
    for label, old, new in (
        ('closest existing type conflict ignored', closest, 'output->addAttrWithValues(attributeName, values);'),
        ('sample existing type conflict ignored', sample, 'output.addAttrWithValues(attributeName, values);'),
        ('closest write failure ignored', closest,
         '(void)dommy::attribute::commitStagedAttribute(*output, attributeName, candidate, count);'),
        ('sample write failure ignored', sample,
         '(void)dommy::attribute::commitStagedAttribute(output, attributeName, candidate, points.size());'),
    ):
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), suite='vdb_point_bridge_compute', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'vdb_point_bridge_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected VDB publication mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected VDB publication mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='vdb_point_bridge_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        guards(root)
        mutations(root)
    else:
        guards(root)
        run(root, initializers(root), suite='vdb_point_bridge_init')
        run(root, compute_bodies(root), suite='vdb_point_bridge_compute', arguments=[])
        publication_mutations(root)

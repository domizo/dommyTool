#!/usr/bin/env python3
"""Scatter/sphere packing production bodies with explicit host/native boundaries."""
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

NODES = {
    'DOpenVDBScatter': 'src/modules/volume/VDB/Generate/DOpenVDBScatter.cpp',
    'DOpenVDBToSpheres': 'src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp',
}

def initializers(root):
    code = ['const int kMaxMeshDetail = 5;']
    for name, path in NODES.items():
        source = (root / path).read_text()
        code += declarations(name, source, True) + [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(code)

def compute_bodies(root):
    folder = 'src/modules/volume/VDB/Edit/'
    code = [fill_bodies(root).split('struct DOpenVDBFill {')[0],
        extract((root / (folder + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {'),
        extract((root / (folder + 'DOpenVDBPointGenerationPolicy.h')).read_text(), 'namespace dommy {'),
        'namespace dommy { namespace vdb { const int kMaxFilterWidth = 64;',
        extract((root / (folder + 'DOpenVDBFilterPolicy.h')).read_text(),
                'template <typename GridT> bool validLevelSetScale('), '}}',
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(),
                'MStatus getValidInputVDB('),
        extract((root / 'src/modules/volume/VDB/Common/VDBPointUtil.h').read_text(),
                'class PointAccessor') + ';',
        extract((root / (folder + 'DOpenVDBScatterPolicy.h')).read_text(), 'namespace dommy {')]
    for name, path in NODES.items():
        source = (root / path).read_text()
        code += declarations(name, source, False) + [extract(source, 'namespace {'),
            extract(source, 'MStatus ' + name + '::compute('), 'DOMMY_VDB_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(code)

def sdk_check(root, sdk):
    code = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MTypeId', 'MDataHandle', 'MDataBlock', 'MPlug', 'MPointArray',
        'MIntArray')]
    code += ['#include <cmath>', '#include <cstdint>', '#include <vector>',
        '#include "common/utility/DComputeGuard.h"', '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DGenerationLimits.h"', '#include "common/utility/DNumericValidation.h"', r'''
// Declarations only at the non-Maya boundary; no native OpenVDB compile claim.
namespace openvdb {
using Index64 = std::uint64_t;
struct Vec3R {double x() const; double y() const; double z() const;};
struct Vec4s {float x() const; float y() const; float z() const; float operator[](int) const;};
}
using DPoint=MPoint;
using DPointList=std::vector<DPoint>;
struct DOpenVDBData {static const MTypeId id;};
struct DPData {static const MTypeId id;};
struct meshPrimData {MPointArray m_pts; MIntArray m_faces, m_connects;};
namespace dommy {namespace vdb {
struct TreeEditBudget {bool addBytes(std::size_t,std::size_t);bool addWork(std::size_t,std::size_t);};
}}
const int kMaxMeshDetail=5;
''']
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        code += declared + [extract(source, 'MStatus ' + name + '::initialize(')]
        if name.endswith('Scatter'):
            policy = (root / 'src/modules/volume/VDB/Edit/DOpenVDBScatterPolicy.h').read_text()
            code += [extract(policy, 'struct ScatterSettings') + ';',
                     extract(source, 'MStatus readScatterSettings('),
                     extract(policy, 'bool addScatterCount(')]
        else:
            code += [extract(source, prefix) for prefix in ('MStatus readFloatInput(',
                'MStatus readIntInput(', 'MStatus readBoolInput(', 'bool isFiniteSphere(',
                'MStatus publishMeshData(', 'bool reserveSphereMesh(')]
    code += [extract((root / 'src/modules/volume/VDB/Common/VDBPointUtil.h').read_text(),
                     'class PointAccessor') + ';']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    for path in NODES.values():
        subprocess.run(command + ['-x', 'c++', '-include', str((root / path).with_suffix('.h')),
                                  '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-distribution-sdk-') as directory:
        path = Path(directory) / 'distribution.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Scatter/ToSpheres headers, initializers, readers and mesh validation: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation units are NOT covered')

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('mode unchecked', False, body.replace('settings.mode < 0 || settings.mode > 2', 'false')),
        ('negative seed admitted', False, body.replace('settings.seed < 0', 'false')),
        ('spread unchecked', False, body.replace('settings.spread < 0 || settings.spread > 1', 'false')),
        ('signed native conversion unchecked', False, body.replace(
            '(signedNative && !dommy::numeric::checkedNumericCast(value, nativeCount))',
            '(signedNative && !dommy::numeric::checkedNumericCast(value, nativeCount) && false)')),
        ('point budget ignored', False, body.replace('!budget.addWork(plan.count, 16 + dommy::vdb::pointSortPasses(plan.count))', 'false')),
        ('nonfinite grid accepted', False, body.replace('if (!values.valid) return false;', 'if (!values.valid && false) return false;')),
        ('source memory uncharged', False, body.replace('!budget.addBytes(1, grid.memUsage())', 'false', 1)),
        ('world coordinate unchecked', False, body.replace('std::fabs(world[axis]) > 1e12', 'false')),
        ('mask transform unchecked', False, body.replace('interior.mask->transform() != plan.grid->transform()', 'false')),
        ('sphere candidate work uncharged', False, body.replace(
            '!budget.addWork(candidates, spheres + 128 + dommy::vdb::pointSortPasses(candidates))', 'false')),
        ('base mesh budget ignored', False, body.replace(
            '!reserveSphereMesh(sphereMeshData, maximumSelectedSphereCount, budget)',
            '(!reserveSphereMesh(sphereMeshData, maximumSelectedSphereCount, budget) && false)')),
        ('base mesh read failure ignored', False, body.replace(
            'DOMMY_RETURN_IF_FAILURE(getPrimDataFromMesh(baseMesh, sphereMeshData));',
            '(void)getPrimDataFromMesh(baseMesh, sphereMeshData);')),
        ('sphere magnitude unchecked', False, body.replace('std::fabs(static_cast<long double>(*it)) > 1e12', 'false')),
        ('sphere result unchecked', False, body.replace('!isFiniteSphere(sphere)', '(!isFiniteSphere(sphere) && false)')),
        ('sphere count unchecked', False, body.replace('spheres.size() > sphereCount', 'false')),
        ('transformed cardinality unchecked', False, body.replace(
            'transformedSphere.m_connects.length() != sphereMeshData.m_connects.length()', 'false')),
        ('transform copy failure ignored', False, body.replace(
            'DOMMY_RETURN_IF_FAILURE(sphereMeshData.duplicateTransform(transform, transformedSphere));',
            '(void)sphereMeshData.duplicateTransform(transform, transformedSphere);')),
        ('radius units ignored', False, body.replace('minWorld / dx, minimum', 'minWorld, minimum')),
        ('mesh data null admitted', False, body.replace('if (outputMeshData.isNull()) return MS::kFailure;', '')),
        ('generated mesh null admitted', False, body.replace('if (createdMesh.isNull()) return MS::kFailure;', '')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True, suite='vdb_distribution_init' if initialization else 'vdb_distribution_compute',
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
    for path in NODES.values():
        source = (root / path).read_text()
        assert 'CHECK_MSTATUS(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
    run(root, initializers(root), suite='vdb_distribution_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv and '--init-only' not in sys.argv:
        run(root, compute_bodies(root), suite='vdb_distribution_compute', arguments=[])

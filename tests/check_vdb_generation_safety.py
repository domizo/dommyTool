#!/usr/bin/env python3
"""Sphere/fracture production bodies with explicit host/native boundaries."""
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
NAMES = ('DOpenVDBPlatonic', 'DOpenVDBFracture')

def initializers(root):
    code = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        code += declarations(name, source, True) + [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(code)

def compute_bodies(root):
    code = [fill_bodies(root).split('struct DOpenVDBFill {')[0],
        extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {'),
        'namespace dommy { namespace vdb { const int kMaxFilterWidth = 64;',
        extract((root / (FOLDER + 'DOpenVDBFilterPolicy.h')).read_text(),
                'template <typename GridT> bool validLevelSetScale('), '}}',
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(),
                'MStatus getValidInputVDB(')]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, False)
        declared[0] = declared[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};} MStatus compute(')
        code += declared + [extract(source, 'namespace {'),
            extract(source, 'MStatus ' + name + '::compute('), 'DOMMY_VDB_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(code)

def sdk_check(root, sdk):
    code = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MPxData', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute', 'MFnGenericAttribute',
        'MFnStringData', 'MTypeId', 'MDataHandle', 'MDataBlock', 'MPlug', 'MFnMesh',
        'MBoundingBox', 'MPointArray')]
    code += ['#include <string>', '#include <vector>', '#include <cmath>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DGenerationLimits.h"',
        '#include "common/utility/DNumericValidation.h"',
        '#include "common/utility/DMayaPluginData.h"', '#include "math/DBounds.h"', r'''
// Declarations only at the OpenVDB/DP boundary: this is a Maya SDK API check.
namespace openvdb {
template<typename T> struct Vec3 {
    explicit Vec3(T = 0); Vec3(T,T,T);
    T& operator[](int); const T& operator[](int) const;
};
using Vec3f = Vec3<float>; using Vec3s = Vec3f; using Vec3d = Vec3<double>;
struct Vec4I {}; struct BBoxd {}; struct CoordBBox {};
namespace math {struct Quats {}; struct Transform {Vec3d worldToIndex(const Vec3d&) const;};}
enum GridClass {GRID_LEVEL_SET};
struct GridBase {
    static std::string gridClassToString(GridClass);
    static std::string gridClassToMenuName(GridClass);
};
}
struct DOpenVDBData {static const MTypeId id;};
struct DPData:MPxData {
    static const MTypeId id;
    bool hasValidPointTopology() const;
    std::size_t numberOfPoints() const;
    const openvdb::Vec3d& positionAt(std::size_t) const;
};
namespace dommy {namespace vdb {openvdb::BBoxd gridBounds(const dommy::math::Bounds3d&);}}
MStatus getMeshInformation(const MObject&, MDataBlock&, MBoundingBox&, MObject&);
MStatus DOpenVDBMakePrimList(const MObject&, std::vector<openvdb::Vec4I>&);
bool fractureBox(const openvdb::BBoxd&, openvdb::CoordBBox&);
''']
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        code += declared + [extract(source, 'MStatus ' + name + '::initialize(')]
        if name.endswith('Platonic'):
            code += [extract(source, 'struct SphereSettings') + ';',
                     extract(source, 'MStatus readSphereSettings(')]
        else:
            code += ['const std::size_t kMaxFractureInstanceCount = 10000;',
                     extract(source, 'struct FractureSettings') + ';',
                     extract(source, 'struct FractureMesh') + ';',
                     extract(source, 'MStatus readFractureSettings('),
                     extract(source, 'MStatus readFractureMesh(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    for name in NAMES:
        subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + name + '.h')),
                                  '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-generation-sdk-') as directory:
        path = Path(directory) / 'generation.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Sphere/fracture headers, initializers and settings/mesh readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation units are NOT covered')

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(',
                                                            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('native half-width precondition removed', False, body.replace('settings.width <= 1', 'settings.width < 1')),
        ('sphere coordinate check ignored', False, body.replace('std::fabs(low) > 1e6f || std::fabs(high) > 1e6f', 'false')),
        ('sphere workspace uncharged', False, body.replace(
            'dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(checked, budget, 2, 32, 32);',
            '(dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(checked, budget, 2, 32, 32) || true);')),
        ('empty fog enters native conversion', False, body.replace('if (grid->empty()) {', 'if (grid->empty() && false) {', 1)),
        ('invalid sphere result accepted', False, body.replace('!finiteSphere(*grid, work)', '(!finiteSphere(*grid, work) && false)')),
        ('wrong point data type accepted', False, body.replace('handle.typeId() != expectedTypeId', 'handle.typeId() != expectedTypeId && false')),
        ('point topology unchecked', False, body.replace('!points->hasValidPointTopology()', 'false')),
        ('negative seed accepted', False, body.replace('settings.seed < 0', 'false')),
        ('disabled seed read', False, body.replace('if (settings.random) {\n        const auto seed', 'if (true) {\n        const auto seed')),
        ('mesh counts unchecked', False, body.replace('vertices == 0 || polygons == 0', 'false')),
        ('mesh cardinality unchecked', False, body.replace('vertexArray.length() != vertices', 'false')),
        ('source nonfinite values accepted', False, body.replace('if (!values.valid) return false;', 'if (!values.valid && false) return false;')),
        ('level-set value magnitude unchecked', False, body.replace('std::fabs(static_cast<long double>(*it)) > limit',
                                                                 'std::fabs(static_cast<long double>(*it)) > limit && false')),
        ('source memory uncharged', False, body.replace('!budget.addBytes(2, grid->memUsage())', 'false')),
        ('piece alias accepted', False, body.replace('!seen.insert(grid.get()).second', '(!seen.insert(grid.get()).second && false)')),
        ('piece result validation ignored', False, body.replace(
            '!checkFracturePieces<GridT>(residuals, tool.fragments(), sources, *first, available, budget)',
            '(!checkFracturePieces<GridT>(residuals, tool.fragments(), sources, *first, available, budget) && false)')),
        ('segmentation estimate disabled', False, body.replace('active = std::max(std::size_t(1), active);', 'active = 1;')),
        ('cutter null check removed', False, body.replace('!cutter || cutter->activeVoxelCount() == 0', 'cutter->activeVoxelCount() == 0')),
        ('overlap disabled', False, body.replace('rotation.empty() ? nullptr : &rotation, settings.overlap',
                                                'rotation.empty() ? nullptr : &rotation, false')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True, suite='vdb_generation_init' if initialization else 'vdb_generation_compute',
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
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        assert 'CHECK_MSTATUS(' not in source
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
    run(root, initializers(root), suite='vdb_generation_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv and '--init-only' not in sys.argv:
        run(root, compute_bodies(root), suite='vdb_generation_compute', arguments=[])

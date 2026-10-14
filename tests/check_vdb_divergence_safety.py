#!/usr/bin/env python3
"""Remove Divergence boundaries and projection arithmetic, without native Poisson emulation."""
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
NAME = 'DOpenVDBRemoveDivergence'


def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join(declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])


def compute_bodies(root):
    common = fill_bodies(root).split('struct DOpenVDBFill {')[0]
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    transport = (root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text()
    magnitude = transport[transport.index('template <typename T> struct TransportMagnitude'):
                          transport.index('template <typename GridT>\nbool transportValues')]
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DVdbGridDispatch.h').read_text()
    code = [common, extract((root / (FOLDER + 'DOpenVDBSpatialPolicy.h')).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace vdb {', magnitude, '}}',
            extract(util, 'MStatus getValidInputVDB(')]
    code += declarations(NAME, source, False)
    code += [extract(source, 'namespace {')]
    code += [extract(source, signature) + ';' for signature in (
        'struct SolverParms', 'struct DivergenceSettings')]
    code += [re.search(r'const int kMaxDivergenceIterations[^;]+;', source).group(0),
             extract(source, 'MStatus readDivergenceSettings('),
             extract(source, 'struct DivergenceInspect') + ';',
             extract(source, 'template<typename GridT>\n    bool divergenceTreeBudget('),
             extract(source, 'struct DivergenceSolveBudget') + ';',
             extract(source, 'bool finiteDivergenceGrid('),
             extract(source, 'struct DivergencePlan') + ';',
             extract(source, 'bool divergenceColliderBox(')]
    code += [extract(source, signature) + ';' for signature in (
        'struct LevelSetMaskOp', 'struct ColliderMaskOp',
        'template<typename TreeType> struct PressureLeafPair')]
    code += [extract(source, 'template<typename TreeType>\n    std::vector<PressureLeafPair<TreeType>> pressureLeafPairs('),
             extract(source, 'template<typename TreeType>\n    struct PressureProjectionOp') + ';',
             'template<typename GridT> bool processGrid(SolverParms&);',
             extract(source, 'MStatus ' + NAME + '::compute('),
             'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")']
    return '\n'.join(code)


def sdk_check(root, sdk):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    code = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnStringData', 'MTypeId', 'MPxData', 'MDataHandle', 'MDataBlock')]
    code += ['#include <string>', '#include <cmath>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"',
        'struct DOpenVDBData:MPxData{static const MTypeId id;bool hasValidData()const;};',
        'const MTypeId DOpenVDBData::id(123);',
        'const int DEFAULT_MAX_ITERATIONS=10000, kMaxDivergenceIterations=10000;',
        'namespace openvdb{namespace math{template<typename T>struct Delta;'
        'template<>struct Delta<float>{static float value(){return 1e-5f;}};}}']
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'struct DivergenceSettings') + ';',
        extract(source, 'MStatus readDivergenceSettings('),
        extract((root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text(), 'MStatus getValidInputVDB('),
        extract(source, 'MStatus ' + NAME + '::initialize(')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                              '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-divergence-sdk-') as directory:
        path = Path(directory) / 'divergence.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Remove Divergence header, initializer and readers: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation unit are NOT covered')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid payload accepted', False, body.replace('result && result->hasValidData()', 'result')),
        ('iteration ceiling removed', False, body.replace('settings.iterations > kMaxDivergenceIterations', 'false')),
        ('nonfinite input admitted', False, body.replace('if (!values.valid ||', 'if (false ||')),
        ('numeric envelope removed', False, body.replace('magnitude > std::sqrt(std::sqrt(maximum)) / 1024', 'false')),
        ('affine map unchecked', False, body.replace('!dommy::vdb::validDerivativeGridMap(grid.transform())', 'false')),
        ('source memory uncharged', False, body.replace('if (!budget.addBytes(1, grid.memUsage()))', 'if (false)')),
        ('solver work uncharged', False, body.replace('budget.addWork(voxels, 64U * (std::size_t(iterations) + 4U))', 'true')),
        ('later grid not preflighted', False, body.replace('i < grids.size()', 'i < std::min(grids.size(), std::size_t(1))', 1)),
        ('invalid result published', False, body.replace('!finiteDivergenceGrid(parms.velocityGrid)', 'false')),
        ('invalid pressure published', False, body.replace('!finiteDivergenceGrid(parms.pressureGrid)', 'false')),
        ('disabled collider read', False, body.replace('if (settings.useCollider) {', 'if (true) {')),
        ('inactive leaves paired', False, body.replace('if (leaf->isInactive()) continue;', '')),
        ('leaf origins ignored', False, body.replace(' || pressure->origin() != leaf->origin()', '')),
        ('leaf matched by wrong coordinate', False, body.replace('gradient.probeConstLeaf(leaf->origin())', 'gradient.probeConstLeaf(openvdb::Coord(0))')),
        ('projection overflow ignored', False, body.replace('if (!dommy::numeric::checkedNumericCast(value, projected[axis]))', 'if ((projected[axis] = static_cast<ElementType>(value), false))')),
    ]
    for label, initialization, changed in cases:
        assert changed != (init if initialization else body), label
        try:
            run(root, changed, capture=True,
                suite='vdb_divergence_init' if initialization else 'vdb_divergence_compute',
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
    assert 'mGradientOfPressureNodes[n]' not in source
    assert 'pressureLeafPairs(velocityGrid.tree(), gradientOfPressure->tree())' in source
    assert 'finiteDivergenceGrid(divGrid)' in source
    assert 'finiteDivergenceGrid(gradientOfPressure)' in source
    run(root, initializers(root), suite='vdb_divergence_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv and '--init-only' not in sys.argv:
        run(root, compute_bodies(root), suite='vdb_divergence_compute', arguments=[])

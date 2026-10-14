#!/usr/bin/env python3
"""Legacy native-mesh API boundaries, search/storage limits and frozen schemas."""
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
from check_mesh_color_safety import color_output_body

NAMES = ('DBSMirrorMesh', 'DBSFlipMesh', 'DBSUpDownMesh', 'DPolySymmetry', 'DPolyResetVtx')
FOLDER = 'src/modules/mesh/mesh/'


def initializers(root):
    result = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)


def compute(root, sources=None, point_math=None):
    helper = (root / 'src/core/common/utility/DMayaMeshInput.h').read_text()
    traversal = (root / 'src/core/common/utility/DMayaMeshTraversal.h').read_text()
    pair = extract(helper, 'inline MStatus getMatchingMeshPair(')
    search = extract(helper, 'inline MStatus getMirroredPointMatches(')
    assert pair.index('validMeshPairStorage(') < pair.index('firstMesh.getPoints(')
    assert search.index('validPointSearchStorage(') < search.index('searchPoints.reserve(')
    result = [extract((root / ('src/core/common/utility/' + name)).read_text(), 'namespace dommy {')
              for name in ('DMeshInputValidation.h', 'DSymmetryPointMatcher.h')]
    if point_math is None:
        point_math = (root / 'src/core/common/utility/DMayaPointBlend.h').read_text()
    result.append(extract(point_math, 'namespace dommy {'))
    result += ['namespace dommy {namespace maya {',
              extract((root / 'src/core/common/utility/DMayaValueValidation.h').read_text(),
                      'inline bool isFinitePoint('),
              extract(helper, 'struct MeshPairInput {') + ';']
    for signature in ('inline MStatus getMeshObject(', 'inline MStatus getConnectedMeshObject(',
                      'inline bool areFinitePoints(', 'inline MStatus getPerVertexColors(',
                      'inline MStatus getMatchingMeshPair(', 'inline MStatus getMirroredPointMatches('):
        result.append(extract(helper, signature))
    result += [extract(traversal, 'template <typename Iterator, typename Visitor>'), '}}']
    result += [color_output_body(root)]
    for name in NAMES + ('DSkinInfo',):
        source = (sources or {}).get(name)
        if source is None:
            source = (root / (FOLDER + name + '.cpp')).read_text()
        if name in ('DBSMirrorMesh', 'DBSUpDownMesh', 'DBSFlipMesh', 'DPolySymmetry'):
            assert '#include "common/utility/DMayaPointBlend.h"' in source, name
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};}'
            'MString name()const{return "' + name + '";} MStatus compute(')
        result += attrs + [extract(source, 'MStatus ' + name + '::compute('),
                           'DOMMY_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(result)


def mutations(root):
    initial, body = initializers(root), compute(root)
    cases = [
        ('initializer status', 'legacy_mesh_init', initial, 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;', None),
        ('initializer setter', 'legacy_mesh_init', initial, 'DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));',
         '(void)nAttr.setDefault( 1.0 );', None),
        ('initializer exception', 'legacy_mesh_init', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', None),
    ]
    for label, old, new in (
        ('compute status', 'CHECK_MSTATUS_AND_RETURN_IT(stat);', '(void)stat;'),
        ('input before status', 'CHECK_MSTATUS_AND_RETURN_IT(status);\n    const double tolerance',
         'const double tolerance'),
        ('failed ramp query', 'if (!rampStatus) return rampStatus;', ''),
        ('failed connected query', 'if (!status) return status;\n    if (!connected)', 'if (!connected)'),
        ('premature publication', 'MDataHandle rightOutHandle = data.outputValue',
         'DOMMY_RETURN_IF_FAILURE(leftOutHandle.set(newLeftMeshData));\n        MDataHandle rightOutHandle = data.outputValue'),
        ('iterator status', 'if (!status) return status;\n        if (done)', 'if (done)'),
        ('incomplete iteration', 'visited == expectedCount ? MS::kSuccess : MS::kFailure', 'MS::kSuccess'),
        ('mesh pair admission', '!mesh::validMeshPairStorage(candidate.pointCount, retainedPoints)', 'false'),
        ('point scratch charge', 'checkedProduct(points, 256U, maximum, bytes)', 'checkedProduct(points, 128U, maximum, bytes)'),
        ('retained destination charge', 'checkedProduct(retainedPoints, 32U, maximum, retainedBytes)',
         'checkedProduct(retainedPoints, 0U, maximum, retainedBytes)'),
        ('search allowance', 'spatial::RadiusSearchBudget budget(maximumWork);',
         '(void)maximumWork; spatial::RadiusSearchBudget budget;'),
        ('query visit charge', 'if (nodeIndex == kNoPointMatch) return;\n        budget_.consume(1U);',
         'if (nodeIndex == kNoPointMatch) return;'),
        ('equal-distance tie', 'node.pointIndex < bestIndex', 'node.pointIndex > bestIndex'),
        ('inclusive radius boundary', 'if (splitDistance <= bestDistance)', 'if (splitDistance < bestDistance)'),
    ):
        cases.append((label, 'legacy_mesh_compute', body, old, new, []))
    for label, suite, source, old, new, arguments in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new), capture=True, suite=suite, arguments=arguments)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    (label == 'initializer exception' and 'uncaught exception' in error.stderr)), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

    point_math = (root / 'src/core/common/utility/DMayaPointBlend.h').read_text()
    blend_cases = []
    for name in ('DBSMirrorMesh', 'DBSUpDownMesh', 'DBSFlipMesh', 'DPolySymmetry'):
        source = (root / (FOLDER + name + '.cpp')).read_text()
        pattern = (r'MPoint (\w+);\n\s*if \(!dommy::maya::checkedPointBlend\('
                   r'([^,\n]+), ([^,\n]+), ([^,\n]+), \1,\n\s*'
                   r'dommy::maya::PointBlendRounding::LegacyDisplacement\)\) \{\n\s*'
                   r'return MS::kInvalidParameter;\n\s*\}')
        def restore(match):
            output, first, second, weight = match.groups()
            return ('MPoint ' + output + ' = ' + first + ' + (' + second + '-' + first + ')*' + weight +
                    ';\nif (!dommy::maya::isFinitePoint(' + output + ')) return MS::kInvalidParameter;')
        altered, count = re.subn(pattern, restore, source)
        assert count == (2 if name in ('DBSMirrorMesh', 'DBSUpDownMesh') else 1), name
        blend_cases.append((name + ' old displacement restored', compute(root, {name: altered})))
    for label, old, new in (
        ('point full-weight endpoint lost',
         'if (weight == 1.0) { *components[axis] = second; continue; }', ''),
        ('point zero-weight endpoint lost',
         'if (weight == 0.0) { *components[axis] = first; continue; }', ''),
        ('homogeneous division omitted', 'sources[axis] / source.w', 'sources[axis]'),
        ('reciprocal homogeneous conversion', 'sources[axis] / source.w', 'sources[axis] * (1.0/source.w)'),
        ('zero homogeneous weight accepted', 'source.w == 0.0 || target.w == 0.0', 'false'),
        ('finite legacy rounding replaced', 'if (std::isfinite(legacy)) {', 'if (false) {'),
        ('finite fallback disabled',
         'if (!attribute::checkedLinearBlend(first, second, weight, *components[axis])) return false;',
         'return false;'),
        ('point extrapolation clamped', 'double weight, MPoint& output,', 'double weight, MPoint& output,'),
        ('point output published before final-axis validation',
         'const double first = sources[axis] / source.w;',
         'output = candidate; const double first = sources[axis] / source.w;'),
    ):
        assert old in point_math, label
        altered = point_math.replace(old, new)
        if label == 'point extrapolation clamped':
            altered = altered.replace('    const double sources[3]',
                '    weight = std::max(0.0, std::min(1.0, weight));\n    const double sources[3]')
        elif label == 'zero homogeneous weight accepted':
            altered = altered.replace('sources[axis] / source.w',
                '(source.w == 0.0 ? sources[axis] : sources[axis] / source.w)').replace(
                'targets[axis] / target.w',
                '(target.w == 0.0 ? targets[axis] : targets[axis] / target.w)')
        assert altered != point_math, label
        blend_cases.append((label, compute(root, point_math=altered)))
    for label, altered in blend_cases:
        try:
            run(root, altered, capture=True, suite='legacy_mesh_compute', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'legacy_mesh_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    from check_group_transfer_safety import sdk_check as check_units
    check_units(root, sdk, {name: FOLDER + name + '.cpp' for name in NAMES + ('DSkinInfo',)},
                'Legacy mesh', boundary_source='''
#include "common/utility/DMayaPointBlend.h"
bool compilePointBlend(const MPoint& source, const MPoint& target, double weight, MPoint& output) {
    return dommy::maya::checkedPointBlend(source, target, weight, output) &&
        dommy::maya::checkedPointBlend(source, target, weight, output,
            dommy::maya::PointBlendRounding::LegacyDisplacement);
}
''')
    # The MPoint constructor/fields/assignment used here are inline in the SDK;
    # no Maya library, mesh execution, or dependency-graph runtime is involved.
    with tempfile.TemporaryDirectory(prefix='dommy-sdk-point-blend-') as directory:
        binary = Path(directory) / 'sdk_maya_point_blend_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++14', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-isystem', str(Path(sdk) / 'include'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/sdk_maya_point_blend_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializers(root), capture=True, suite='legacy_mesh_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='legacy_mesh_init')
        run(root, compute(root), suite='legacy_mesh_compute', arguments=[])

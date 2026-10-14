#!/usr/bin/env python3
"""Mesh color-node boundary tests; no Maya runtime is required."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check

NODES = {
    'DPolyThickness': 'src/modules/mesh/mesh/DPolyThickness.cpp',
    'DStretchData': 'src/core/UtilityNode/DStretchData/DStretchData.cpp',
}


def color_output_body(root):
    folder = root / 'src/core/common/utility'
    return '\n'.join([
        '#include "common/utility/DMeshMergePlan.h"',
        'namespace dommy { namespace maya {',
        extract((folder / 'DMayaValueValidation.h').read_text(), 'inline bool isFiniteColor('),
        extract((folder / 'DMayaMeshColorOutput.h').read_text(), 'inline MStatus setMeshDataVertexColors('),
        '}}',
    ])


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)


def compute_bodies(root):
    folder = root / 'src/core/common/utility'
    validation = (folder / 'DMayaValueValidation.h').read_text()
    input_source = (folder / 'DMayaMeshInput.h').read_text()
    result = [color_output_body(root), 'namespace dommy { namespace maya {']
    for name in ('isFinitePoint', 'isFiniteVector', 'isFloatRepresentablePoint', 'isFloatRepresentableVector'):
        result += [extract(validation, 'inline bool ' + name + '(')]
    for signature in ('inline MStatus getMeshObject(', 'inline MStatus getConnectedMeshObject(',
                      'inline bool areFinitePoints('):
        result += [extract(input_source, signature)]
    result += [extract((folder / 'DMayaMeshTraversal.h').read_text(),
                       'template <typename Iterator, typename Visitor>'), '}}']
    for name, path in NODES.items():
        source = (root / path).read_text()
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};}'
            'MString name()const{return "' + name + '";} MStatus compute(')
        result += attrs + [extract(source, 'namespace {'),
                           extract(source, 'MStatus ' + name + '::compute('),
                           'DOMMY_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(result)


def run_compute(root, source, capture=False):
    return run(root, source, suite='mesh_color_compute', arguments=[], capture=capture,
               extra_flags=['-I' + str(root / 'src/core/Utility'),
                            '-I' + str(root / 'src/core/Utility/3d/Math')])


def mutations(root):
    initial, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status', initial, 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;', True),
        ('initializer exception', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', True),
        ('incomplete traversal', body, 'visited == expectedCount ? MS::kSuccess : MS::kFailure',
         'MS::kSuccess', False),
        ('named assignment failure', body, 'return mesh.assignColors(colorIds, &name);',
         '(void)mesh.assignColors(colorIds, &name); return MS::kSuccess;', False),
        ('normal normalization failure', body, 'DOMMY_RETURN_IF_FAILURE(normal.normalize());',
         '(void)normal.normalize();', False),
        ('null mesh copy', body, 'if (copiedMesh.isNull()) return MS::kFailure;', '', False),
        ('grid equal short arrays', body,
         'relaxedPoints.length() != vertexCount || currentPoints.length() != vertexCount',
         'relaxedPoints.length() != currentPoints.length()', False),
        ('grid input status', body,
         'DOMMY_RETURN_IF_FAILURE(sourceMesh.getPoints(currentPoints, MSpace::kObject));',
         '(void)sourceMesh.getPoints(currentPoints, MSpace::kObject);', False),
        ('output setter failure', body, 'DOMMY_RETURN_IF_FAILURE(output.set(outputObject));',
         '(void)output.set(outputObject);', False),
        ('grid color convention', body, 'MColor(red, green, 0.0f, 1.0f)',
         'MColor(green, red, 0.0f, 1.0f)', False),
    ]
    for label, source, old, new, init in cases:
        assert old in source, label
        try:
            if init:
                run(root, source.replace(old, new), suite='mesh_color_init', capture=True)
            else:
                run_compute(root, source.replace(old, new), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializers(root), capture=True,
                     suite='mesh_color_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        for name, path in NODES.items():
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], path, name)
    else:
        run(root, initializers(root), suite='mesh_color_init')
        run_compute(root, compute_bodies(root))

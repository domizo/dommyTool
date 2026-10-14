#!/usr/bin/env python3
"""Selected component reads and exact face means, using complete production bodies."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NAMES = ('DPolyFaceToMat', 'DPolyEdgeToMat')
FOLDER = 'src/modules/mesh/mesh/'


def initializers(root):
    result = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def bodies(root):
    utility = root / 'src/core/common/utility'
    validation = (utility / 'DMayaValueValidation.h').read_text()
    result = [extract((utility / 'DMayaPointBlend.h').read_text(), 'namespace dommy {'),
              'namespace dommy {namespace maya {']
    for name in ('isFiniteMatrix', 'isFinitePoint', 'isFiniteQuaternion',
                 'isFiniteVector', 'isFloatRepresentableVector', 'normalizedVector'):
        result.append(extract(validation, 'inline bool ' + name + '('))
    result.append('}}')
    result.append(extract((utility / 'DPolyComponentFrame.h').read_text(), 'namespace dommy {'))
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        assert 'getPoints(' not in source
        result.append(extract(source, 'namespace {'))
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def mutations(root):
    body = bodies(root)
    initialization = initializers(root)
    for label, old, new in (
        ('face initializer setter ignored', 'DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));',
         '(void)genericAttribute.setStorable(false);'),
        ('edge initializer default ignored', 'DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0));',
         '(void)numericAttribute.setDefault(0);'),
        ('initializer exception escapes', 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
    ):
        assert old in initialization, label
        try:
            run(root, initialization.replace(old, new), capture=True, suite='poly_component_frame_init')
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'poly_component_frame_init_tests', 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    (label == 'initializer exception escapes' and 'uncaught exception' in error.stderr)), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    cases = (
        ('face mean cancellation',
         'if (!sums[0].mean(divisor, center.x) || !sums[1].mean(divisor, center.y) ||\n'
         '        !sums[2].mean(divisor, center.z)) return MS::kFailure;',
         'for (unsigned i=0;i<points.length();++i) center += (points[i]-center)/double(i+1);'),
        ('face returned cardinality', 'if (vertexIds.length() != vertexCount) return MS::kFailure;', ''),
        ('point storage cardinality', 'if (points.length() != vertexCount) return MS::kFailure;', ''),
        ('connected returned count', 'returnedFaceCount != rawConnectedFaceCount', 'false'),
        ('connected array cardinality', 'faceIds.length() != connectedFaceCount', 'false'),
        ('selected query failure', 'if (!status) return status;\n    return checkedPointBlend',
         'return checkedPointBlend'),
        ('whole mesh copy restored', 'const int meshVertexCount = mesh.numVertices(&status);',
         'MPointArray allPoints; if (!mesh.getPoints(allPoints, MSpace::kObject)) return MS::kFailure;\n'
         'const int meshVertexCount = mesh.numVertices(&status);'),
        ('face work/storage preflight', '!dommy::mesh::validComponentFrameStorage(\n'
         '            vertexCount, sizeof(MPoint) + sizeof(int), 4U)', 'false'),
    )
    for label, old, new in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), capture=True, suite='poly_component_frame', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'poly_component_frame_tests', 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    'ERROR: AddressSanitizer' in error.stderr), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    for name in NAMES:
        source = str(root / (FOLDER + name + '.cpp'))
        for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
            subprocess.run(command + flags + [source], check=True)
    print('Poly frames: two complete SDK units passed syntax/analysis; no Maya runtime/link')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run(root, initializers(root), suite='poly_component_frame_init', arguments=['--emit'])
    else:
        run(root, initializers(root), suite='poly_component_frame_init')
        run(root, bodies(root), suite='poly_component_frame', arguments=[])

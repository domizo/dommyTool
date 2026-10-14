#!/usr/bin/env python3
"""Complete remesh/debris/slicer bodies with real independent geometry math."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NAMES = ('DMRemesh', 'DMDebrisGen', 'DMSlicer', 'DMVectorDeform')
FOLDER = 'src/modules/mesh/DMesh/'


def bodies(root):
    traversal = (root / 'src/core/common/utility/DMayaMeshTraversal.h').read_text()
    result = [extract(traversal, 'namespace dommy {'),
              extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                      'class DCurveOutputBatch final') + ';']
    for name in NAMES:
        source = (root / (FOLDER + ('Deform/' if name == 'DMVectorDeform' else 'Utility/') + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if name in ('DMRemesh', 'DMDebrisGen'):
            result.append(extract(source, 'namespace {'))
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&,MDataBlock&);' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name != 'DMVectorDeform':
            result.append(extract(source, 'MStatus ' + name + '::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    # DMSlicer uses narrow curve-data/util headers, not the unrelated SeExpr
    # umbrella. All four complete units are checked without that dependency.
    for name in NAMES:
        path = root / (FOLDER + ('Deform/' if name == 'DMVectorDeform' else 'Utility/') + name + '.cpp')
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('Remesh/slice: %d complete SDK units passed syntax/analysis (limited Imath test double)' % len(NAMES))


def face_storage(root, source=None, header_overrides=None):
    header = (root / 'src/core/common/utility/DMeshMergePlan.h').read_text()
    reduced = header.replace('256U * 1024U * 1024U', '8192U')
    assert reduced != header
    overrides = {'common/utility/DMeshMergePlan.h': reduced}
    overrides.update(header_overrides or {})
    run(root, source or bodies(root), suite='mesh_remesh', arguments=['--face-storage'],
        header_overrides=overrides)


def mutations(root):
    source = bodies(root)
    remesher = (root / 'src/core/geometry/DMeshRemesher.h').read_text()
    collapse = extract(remesher, 'inline bool collapseShortEdges(').replace(
        'collapseShortEdges(', 'collapseShortEdgesMutated(')
    collapse = re.sub(r'if \(mapped.first == mapped.second[\s\S]*?continue;', '', collapse)
    isotropic = extract(remesher, 'inline bool isotropicRemesh(').replace(
        'isotropicRemesh(', 'isotropicRemeshMutated(').replace(
            'collapseShortEdges(', 'collapseShortEdgesMutated(')
    collapsed_faces = ('namespace dommy {namespace geometry {namespace remesh_detail {' +
                       collapse + '}\n' + isotropic + '}}\n' +
                       source.replace('isotropicRemesh(', 'isotropicRemeshMutated('))
    flip = extract(remesher, 'inline bool flipEdges(').replace('flipEdges(', 'flipEdgesMutated(')
    guard = 'if (edges.find(EdgeKey(a, b)) != edges.end()) continue;'
    assert guard in flip
    flip = flip.replace(guard, '')
    flipped_isotropic = extract(remesher, 'inline bool isotropicRemesh(').replace(
        'isotropicRemesh(', 'isotropicRemeshFlipped(').replace('flipEdges(', 'flipEdgesMutated(')
    existing_diagonal = ('namespace dommy {namespace geometry {namespace remesh_detail {' +
                         flip + '}\n' + flipped_isotropic + '}}\n' +
                         source.replace('isotropicRemesh(', 'isotropicRemeshFlipped('))
    # Reintroduce the old unchecked append behavior only in temporary compiled
    # copies; the actual production helper stays unchanged on disk.
    helper = extract((root / 'src/core/common/utility/DMayaTriangleMesh.h').read_text(),
                     'namespace dommy {').replace('appendTriangleMeshChecked', 'appendTriangleMeshMutated')
    for expression in ['points.append(Point(point.x, point.y, point.z))', 'faces.append(3)'] + [
            'connections.append(static_cast<int>(offset + triangle.' + field + '))'
            for field in ('first', 'second', 'third')]:
        assert '!' + expression in helper
        helper = helper.replace('!' + expression, '(' + expression + ', false)')
    helper = re.sub(r'return points.length\(\) == nextPoints[\s\S]*?nextConnections;', 'return true;', helper)
    cases = [
        ('remesh tolerance depends on absolute coordinates',
         'namespace dommy { namespace geometry {\n' +
         extract(remesher, 'inline bool isotropicRemesh(')
         .replace('isotropicRemesh(', 'isotropicRemeshAbsolute(')
         .replace('scaledTolerance(scale, 1.0e-12)',
                  'scaledTolerance(coordinateScale(input.points), 1.0e-12)') +
         '\n}}\n' + source.replace('isotropicRemesh(', 'isotropicRemeshAbsolute(')),
        ('existing replacement diagonal accepted', existing_diagonal),
        ('collapsed faces are retained', collapsed_faces),
        ('remesh accepts incomplete native arrays', re.sub(
            r'if \(!dommy::maya::validMeshTriangleArrays\(mayaPoints,[\s\S]*?triangleCount\)\)',
            'if (false)', source)),
        ('remesh loses transform', source.replace('candidate->setTransformMatrix(sourceTransform);', '', 1)),
        ('debris loses transform', ''.join(source.rsplit('candidate->setTransformMatrix(sourceTransform);', 1))),
        ('remesh returns to per-primitive insertion', source.replace(
            'generatedMeshes.push_back(candidate);',
            'if (!outputMeshes->insert(candidate)) return MS::kFailure;', 1)),
        ('debris returns to per-primitive insertion',
         'if (!outputMeshes->insert(candidate)) return MS::kFailure;'.join(source.rsplit(
             'generatedMeshes.push_back(candidate);', 1))),
        ('remesh ignores batch insertion failure', source.replace(
            'if (!outputMeshes->insert(generatedMeshes)) return MS::kFailure;',
            '(void)outputMeshes->insert(generatedMeshes);', 1)),
        ('debris ignores batch insertion failure',
         '(void)outputMeshes->insert(generatedMeshes);'.join(source.rsplit(
             'if (!outputMeshes->insert(generatedMeshes)) return MS::kFailure;', 1))),
        ('mesh append ignores failures', helper + '\n' + source.replace(
            'appendTriangleMeshChecked', 'appendTriangleMeshMutated')),
        ('slicer ignores aggregate work', source.replace('cutCount, budget)', '1U, budget)')),
        ('initializer ignores failed create', source.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('slicer ignores CV allocation', source.replace('DOMMY_RETURN_IF_FAILURE(cvs->setLength(static_cast<unsigned int>(pointCount)));',
                                                       'cvs->setLength(static_cast<unsigned int>(pointCount));').replace(
                                                           'if (cvs->length() != pointCount) return MS::kFailure;', '')),
        ('slicer accepts invalid degree enum', source.replace(
            'if (rawDegree != 0 && rawDegree != 1)', 'if (false && rawDegree != 0 && rawDegree != 1)')),
        ('slicer accepts invalid open enum', source.replace(
            'if (rawIsOpen != 0 && rawIsOpen != 1)', 'if (false && rawIsOpen != 0 && rawIsOpen != 1)')),
        ('remesh source storage admitted after native staging', source.replace(
            '!dommy::mesh::validMeshOperationStorage(storagePlan)) return MS::kInvalidParameter;',
            'false) return MS::kInvalidParameter;', 1)),
        ('debris source storage admitted after output staging',
         'false) return MS::kInvalidParameter;'.join(source.rsplit(
             '!dommy::mesh::validMeshOperationStorage(storagePlan)) return MS::kInvalidParameter;', 1))),
        ('source boundary positions flattened', re.sub(
            r'            if \(i < triangulation.sourceIndices.size\(\)\) \{[\s\S]*?\n            \}',
            '', source)),
    ]
    for label, modified in cases:
        assert modified != source, label
        try:
            run(root, modified, capture=True, suite='mesh_remesh')
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    append_header = (root / 'src/core/common/utility/DMayaTriangleMesh.h').read_text()
    modified_append = append_header.replace('!mesh::validMeshOperationStorage(storage, extraStorageBytes)',
                                           '(static_cast<void>(extraStorageBytes), false)')
    assert modified_append != append_header
    try:
        run(root, source, capture=True, suite='mesh_remesh',
            header_overrides={'common/utility/DMayaTriangleMesh.h': modified_append})
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name.endswith('_tests'), 'Append-byte mutation must fail at runtime'
        print('Detected mutation: append memory admission omitted')
    else:
        raise AssertionError('Undetected mutation: append memory admission omitted')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) > 1 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) > 1 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run(root, bodies(root), suite='mesh_remesh')
        face_storage(root)

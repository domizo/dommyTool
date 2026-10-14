#!/usr/bin/env python3
"""Execute both Bullet mesh readers with real shared validation and bounded host doubles."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

COLLISION = 'src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp'
SOFT_BODY = 'src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp'
TRAVERSAL = 'src/core/common/utility/DMayaMeshTraversal.h'


def bodies(root, include_traversal=True):
    collision = (root / COLLISION).read_text()
    soft = (root / SOFT_BODY).read_text()
    result = [extract((root / TRAVERSAL).read_text(), 'namespace dommy {')] if include_traversal else []
    return '\n'.join(result + [extract(collision, 'struct MeshCollisionData') + ';',
                              extract(collision, 'bool readMeshCollisionData('),
                              extract(soft, 'MStatus DBSoftBodyNode::createHelperMesh(')])


def execute(root, source, capture=False):
    return run(root, source, suite='bullet_mesh_input', arguments=[], capture=capture)


def mutations(root, source):
    cases = []
    for signature, mesh in [('bool readMeshCollisionData(', 'mesh'),
                            ('MStatus DBSoftBodyNode::createHelperMesh(', 'mayaMesh')]:
        body = extract(source, signature)
        old = mesh + '.numVertices(&status)'
        cases.append((mesh + ' ignored scalar query failure', body,
                      body.replace(old, mesh + '.numVertices(&status); status = MS::kSuccess', 1)))
        # Request a native bulk copy before the scalar limit is considered.
        early = ('MFloatPointArray premature; ' + mesh +
                 '.getPoints(premature, MSpace::kWorld);\n    ')
        marker = 'const int raw' + ('VertexCount' if mesh == 'mesh' else 'PointCount')
        cases.append((mesh + ' copy before preflight', body, body.replace(marker, early + marker, 1)))
        call = ('!dommy::maya::validMeshTriangleArrays(\n            ' +
                ('mayaPoints, triangleCounts, triangleVertices,\n            vertexCount' if mesh == 'mesh'
                 else 'ptArray, tCounts, tVerts, pointCount') + ', faceCount, triangleCount)')
        assert call in body
        cases.append((mesh + ' removed triangle cardinality check', body, body.replace(call, 'false', 1)))
    cases += [
        ('normal pool indexed as vertices', source, source.replace(
            'mesh.getVertexNormals(false, mayaNormals, MSpace::kWorld)',
            'mesh.getNormals(mayaNormals, MSpace::kWorld)', 1)),
        ('failed normal read consumed', source, source.replace(
            'status && mayaNormals.length() == vertexCount', 'mayaNormals.length() == vertexCount', 1)),
        ('short normal read consumed', source, source.replace(
            'status && mayaNormals.length() == vertexCount', 'bool(status)', 1)),
        ('shared triangle preflight sum unbounded', source, source.replace(
            'generation::kMaxGeneratedFaceCount, triangles)',
            'generation::kMaxWorkItemCount, triangles)', 1)),
    ]
    for label, old, new in cases:
        assert old in source and old != new, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'bullet_mesh_input_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Bullet mesh input mutations: all {} detected at runtime'.format(len(cases)))


def sdk_check(root, sdk):
    includes = ['MDagPath', 'MFnMesh', 'MFloatPointArray', 'MFloatVectorArray', 'MIntArray']
    source = '\n'.join('#include <maya/' + name + '.h>' for name in includes) + '''
#include "common/utility/DMayaMeshTraversal.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include "modules/dynamics/DBullet/mvl/vec.h"
using vec3f=mvl::vec<float,3>;
struct DBSoftBodyNode {
    static MStatus createHelperMesh(MFnMesh&,std::vector<int>&,std::vector<float>&,MSpace::Space);
};
''' + bodies(root, include_traversal=False)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-mesh-sdk-') as directory:
        path = Path(directory) / 'mesh.cpp'
        path.write_text(source)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Both complete mesh readers and shared validation: Maya SDK syntax/static analysis passed')
    print('Full Bullet node translation units, linking and native simulation are NOT covered by this check')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    else:
        source = bodies(root)
        execute(root, source)
        if '--mutations' in sys.argv:
            mutations(root, source)

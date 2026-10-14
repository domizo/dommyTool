#!/usr/bin/env python3
"""Exercise real sphere/primitive helpers without Maya or OpenVDB."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

SOURCE = 'src/core/common/object/DPolygonUtil.cpp'
HEADER = 'src/core/common/object/DPolygonUtil.h'


def bodies(root, sdk=False):
    source = (root / SOURCE).read_text()
    header = (root / HEADER).read_text()
    assert 'meshPrimData(MObject' not in header
    result = [extract(header, 'class ' + name) + ';'
              for name in ('TriangleIndices', 'DMeshSphere', 'meshPrimData')]
    result.append(extract(source, 'namespace {'))
    if not sdk:
        result.append('namespace dommy { namespace maya {')
        for path, signatures in (
            ('DMayaMeshImport.h', ('inline MStatus preflightMeshImport(',)),
            ('DMayaValueValidation.h', ('inline bool isFinitePoint(',
                                      'inline bool isFloatRepresentablePoint(',
                                      'inline bool isFiniteMatrix(',
                                      'inline bool isFiniteVector(',
                                      'inline bool isFloatRepresentableVector('))):
            content = (root / 'src/core/common/utility' / path).read_text()
            result.extend(extract(content, signature) for signature in signatures)
        result.append('}}')
    for signature in ('DMeshSphere::DMeshSphere(', 'int DMeshSphere::addVertex(',
                      'int DMeshSphere::getMiddlePoint(', 'MStatus getPrimDataFromMesh(',
                      'MStatus meshPrimData::duplicateTransform(',
                      'MStatus meshPrimData::setNormalPerVtxPerFace(',
                      'MStatus meshPrimData::updatePoints('):
        result.append(extract(source, signature))
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    headers = ('MFnMesh', 'MFnMeshData', 'MFloatPointArray', 'MPointArray',
               'MIntArray', 'MVectorArray', 'MTransformationMatrix')
    prelude = '\n'.join('#include <maya/' + h + '.h>' for h in headers)
    prelude += '\n#include <map>\n#include <list>\n#include <vector>\n#include <cstdint>\n'
    prelude += '\n'.join('#include "common/utility/' + h + '.h"' for h in
                         ('DMayaCheckedArray', 'DMayaMeshImport', 'DMayaValueValidation'))
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-polygon-sdk-') as directory:
        path = Path(directory) / 'polygon.cpp'
        path.write_text(prelude + '\n' + bodies(root, True))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Sphere/import/transform/normal/update production declarations and bodies: real Maya SDK syntax/analysis passed')
    print('No native link, Maya execution or OpenVDB kernel validation')


def mutations(root):
    source = bodies(root)
    variants = [
        ('sphere vertex append unchecked', 'dommy::maya::CheckedArray<MFloatPointArray> baseVertexArray;',
         'MFloatPointArray baseVertexArray;'),
        ('sphere face append unchecked', 'dommy::maya::CheckedArray<MIntArray> basePolyCountArray;',
         'MIntArray basePolyCountArray;'),
        ('sphere connection append unchecked', 'dommy::maya::CheckedArray<MIntArray> basePolyConnectArray;',
         'MIntArray basePolyConnectArray;'),
        ('native create failure ignored', 'if (!status || created.isNull()) return;', ''),
        ('native data null accepted', 'if (!status || candidate.isNull()) return;', 'if (!status) return;'),
        ('historical subdivision changed', 'i < frequency;', 'i < frequency - 1;'),
        ('import budget omitted', '!mesh::validMeshOperationStorage(candidate)', 'false'),
        ('point cardinality unchecked', 'points.length() != plan.points ||', ''),
        ('face cardinality unchecked', 'faces.length() != plan.faces ||', ''),
        ('point copy failure ignored', 'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(points, primData.m_pts));',
         '(void)dommy::maya::copyCheckedArray(points, primData.m_pts);'),
        ('connection copy failure ignored', 'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(connections, primData.m_connects));',
         '(void)dommy::maya::copyCheckedArray(connections, primData.m_connects);'),
        ('source space changed', 'srcMesh.getPoints(points, MSpace::kObject)', 'srcMesh.getPoints(points, MSpace::kWorld)'),
        ('transform source copy failure ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(m_pts, newPos));',
         '(void)dommy::maya::copyCheckedArray(m_pts, newPos);'),
        ('transform destination copy failure ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(newPos, other.m_pts));',
         '(void)dommy::maya::copyCheckedArray(newPos, other.m_pts);'),
        ('transform topology copy failure ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(m_connects, other.m_connects));',
         '(void)dommy::maya::copyCheckedArray(m_connects, other.m_connects);'),
        ('invalid transform reaches copies', '!dommy::maya::isFiniteMatrix(matrix)', 'false'),
        ('transformed point unchecked',
         'if (!dommy::maya::isFloatRepresentablePoint(newPos[i]) ||\n'
         '            !dommy::numeric::canRepresentAsFloat(newPos[i].w))', 'if (false)'),
        ('normal cardinality unchecked', 'count != normalfvtxIds.length()', 'false'),
        ('nonfinite normal accepted', '!dommy::maya::isFloatRepresentableVector(normalArray[i])', 'false'),
        ('normal ID range unchecked', 'normalfaceIds[i] >= faces || normalfvtxIds[i] >= points', 'false'),
        ('normal array copied by value', 'MVectorArray& normalArray', 'MVectorArray normalArray'),
    ]
    for label, old, new in variants:
        assert old in source, label
        changed = source.replace(old, new, 1)
        if label == 'normal array copied by value':
            # Keep the real declaration and definition aligned. Pointer identity
            # at the native boundary must detect the extra copy at runtime.
            assert source.count(old) == 2
            changed = source.replace(old, new)
        if 'append unchecked' in label:
            # Remove only that member's budget initializer as well, so a
            # detected mutation must be runtime failure, not compile failure.
            member = {'vertex': 'baseVertexArray', 'face': 'basePolyCountArray',
                      'connection': 'basePolyConnectArray'}[label.split()[1]]
            import re
            changed = re.sub(member + r'\(dommy::generation::\w+\)', member + '()', changed, count=1)
            # The final cardinality checks are intentionally redundant with
            # checked append. Remove both defenses to model the old failure.
            if member == 'baseVertexArray':
                changed = changed.replace(
                    'if (baseVertexArray.length() != index + 1U)', 'if (false)', 1)
            elif member == 'basePolyCountArray':
                changed = changed.replace('basePolyCountArray.length() != facelist.size()', 'false', 1)
            else:
                changed = changed.replace('basePolyConnectArray.length() != expectedConnections', 'false', 1)
        try:
            run(root, changed, capture=True, suite='polygon_generation',
                arguments=[str(root / 'tests/fixtures/sphere_geometry_contract.txt')])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, bodies(root), suite='polygon_generation',
            arguments=[str(root / 'tests/fixtures/sphere_geometry_contract.txt')])

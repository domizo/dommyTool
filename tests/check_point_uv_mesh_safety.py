#!/usr/bin/env python3
"""Frozen UV initializer and ordered-triangle staging with injectable boundaries."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

UV = 'src/modules/point/DPoint/nurbs/DPUVGen.cpp'
MESH = 'src/modules/point/DPoint/mesh/DPToMesh.cpp'


def initializer(root):
    source = (root / UV).read_text()
    return '\n'.join(declarations('DPUVGen', source, True) +
                     [extract(source, 'MStatus DPUVGen::initialize(')])


def compute(root, overrides=None):
    overrides = overrides or {}
    def read(path):
        return overrides.get(path, (root / path).read_text())
    source = read(MESH)
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    reader = extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                     'MStatus getValidInputDP(')
    owner = extract(read('src/core/common/utility/DMayaMeshDataOutput.h'), 'namespace dommy {')
    common = read('src/modules/mesh/DMesh/common/DMeshUtil.cpp')
    geometry = [extract(common, 'bool canCreateMayaMesh('),
                extract(common, 'MStatus generateMMeshFromDmesh('),
                extract(common, 'MStatus generateMMeshFromDmeshWithUV(')]
    group = read('src/modules/mesh/DMesh/Data/DMGroup.cpp')
    cursor = 0
    for name in ('groupFaceMesh', 'groupEdgeMesh'):
        start = group.index('                    MFnMesh meshFn;', cursor)
        end = group.index('                    const int raw', start)
        geometry.append('MStatus ' + name + '() { MStatus stat; auto mIt = groupMeshes.begin();\n' +
                        group[start:end] + '\nreturn MS::kSuccess; }')
        cursor = end
    return '\n'.join([reader, owner] + geometry + ['namespace dommy { namespace maya {',
        extract(values, 'inline bool isFinitePoint('), '}}'] +
        declarations('DPToMesh', source, False) +
        [extract(source, 'MStatus DPToMesh::compute('),
         'DOMMY_COMPUTE_CATCH("DPToMesh")'])


def sdk_check(root, sdk):
    source = (root / MESH).read_text()
    body = '#include "point_uv_mesh_sdk_boundaries.h"\n'
    body += extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                    'MStatus getValidInputDP(') + '\n'
    body += extract(source, 'MStatus DPToMesh::compute(')
    body += '\nDOMMY_COMPUTE_CATCH("DPToMesh")\n'
    body += extract(source, 'MStatus DPToMesh::initialize(') + '\n'
    body += extract((root / UV).read_text(), 'MStatus DPUVGen::initialize(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-unused-parameter', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src/core'), '-I' + str(root / 'src/modules/point/DPoint'),
        '-I' + str(root / 'src/core/Debug'), '-I' + str(root / 'tests/cpp'),
        '-I' + str(root), '-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-uv-mesh-sdk-') as directory:
        path = Path(directory) / 'uv_mesh.cpp'
        path.write_text(body)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('UV initializer and complete DPToMesh methods: SDK syntax/analysis passed')
    print('Real DPData/Maya headers and actual checked reader; curve/mesh IDs declared, Imath uses test stub')
    print('No complete legacy translation-unit build, native linking or Maya execution')


def mutations(root):
    initial, body = initializer(root), compute(root)
    cases = [
        ('initializer create status', True, 'DOMMY_RETURN_IF_FAILURE(status);', ''),
        ('default string status', True,
         'const MObject defaultName = stringData.create("uv", &status);\n'
         '    DOMMY_RETURN_IF_FAILURE(status);',
         'const MObject defaultName = stringData.create("uv", &status);'),
        ('initializer setter status', True,
         'DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));',
         '(void)generic.setReadable(false);'),
        ('affects status', True, 'DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));',
         '(void)attributeAffects(inDD, outDD);'),
        ('initializer exception', True, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
        ('input status before read', False,
         'DOMMY_RETURN_IF_FAILURE(status);\n    const short generationType',
         'const short generationType'),
        ('workspace admission omitted', False,
         '!dommy::mesh::validMeshOperationStorage(plan)', 'false'),
        ('export cardinality unchecked', False,
         'if (points.length() != inputPointCount) return MS::kFailure;', ''),
        ('nonfinite point admitted', False, '!dommy::maya::isFinitePoint(points[index])', 'false'),
        ('trim failure ignored', False,
         'DOMMY_RETURN_IF_FAILURE(points.setLength(usablePointCount));',
         '(void)points.setLength(usablePointCount);'),
        ('face array allocation failure ignored', False,
         'DOMMY_RETURN_IF_FAILURE(faceCounts.setLength(faceCount));',
         '(void)faceCounts.setLength(faceCount);'),
        ('wrong triangle arity', False, 'faceCounts[index] = 3;', 'faceCounts[index] = 4;'),
        ('wrong vertex indices', False, 'connections[index] = static_cast<int>(index);',
         'connections[index] = 0;'),
        ('publication status ignored', False, 'DOMMY_RETURN_IF_FAILURE(output.set(meshData));',
         '(void)output.set(meshData);'),
        ('clean status ignored', False, 'return data.setClean(plug);',
         '(void)data.setClean(plug); return MS::kSuccess;'),
    ]
    variants = []
    for label, is_init, old, new in cases:
        source = initial if is_init else body
        assert old in source, label
        variants.append((label, is_init, source.replace(old, new, 1)))
    export = '    MPointArray points = input->getMPosition();\n'
    assert export in body
    variants.append(('bulk export before admission', False, body.replace(export, '', 1)
        .replace('    const std::size_t inputPointCount', export + '    const std::size_t inputPointCount', 1)))
    for label, is_init, source in variants:
        try:
            run(root, source, capture=True,
                suite='point_uv_mesh_init' if is_init else 'point_uv_mesh_compute',
                arguments=None if is_init else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def output_mutations(root):
    cases = (
        ('mesh null owner admitted', 'src/core/common/utility/DMayaMeshDataOutput.h',
         'if (candidate.isNull()) return MS::kFailure;', ''),
        ('point geometry null result ignored', MESH,
         'if (geometry.isNull()) return MS::kFailure;', ''),
        ('group geometry null result ignored', 'src/modules/mesh/DMesh/Data/DMGroup.cpp',
         'if (geometry.isNull()) return MS::kFailure;', ''),
        ('common conversion geometry null result ignored', 'src/modules/mesh/DMesh/common/DMeshUtil.cpp',
         'if (status && geometry.isNull()) status = MS::kFailure;', ''),
    )
    for label, path, old, new in cases:
        source = (root / path).read_text()
        assert old in source, label
        try:
            run(root, compute(root, {path: source.replace(old, new)}), capture=True,
                suite='point_uv_mesh_compute', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_uv_mesh_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr
            print('Detected mesh output mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
        output_mutations(root)
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--emit' in sys.argv:
        run(root, initializer(root), suite='point_uv_mesh_init', arguments=['--emit'])
    else:
        run(root, initializer(root), suite='point_uv_mesh_init')
        run(root, compute(root), suite='point_uv_mesh_compute', arguments=[])
        output_mutations(root)

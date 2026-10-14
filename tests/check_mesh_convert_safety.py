#!/usr/bin/env python3
"""DMConvertFromMeshes input admission and frozen initialization contract.

Runs extracted production code with injectable Maya boundaries. Native mesh
generation, DAG instancing and DG evaluation remain host integration tests.
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_point_utility_safety import declarations

SOURCE = 'src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp'


def initializer(root):
    source = (root / SOURCE).read_text()
    return '\n'.join([extract(source, 'enum DMConvertMeshType') + ';'] +
                     declarations('DMConvertFromMeshes', source, True) +
                     [extract(source, 'MStatus DMConvertFromMeshes::initialize(')])


def input_body(root):
    source = (root / SOURCE).read_text()
    name = 'DMConvertFromMeshes'
    parts = [extract(source, 'enum DMConvertMeshType') + ';',
             extract(source, 'struct MeshInput') + ';',
             extract(source, 'MStatus getLegacyWorldMatrix('),
             extract(source, 'MStatus collectMeshInputs(')]
    node = declarations(name, source, False)
    node[0] = node[0].replace(' {MStatus compute',
        ' {MObject thisMObject() const {return MObject{"node"};} MStatus compute')
    parts += node
    parts += ['std::vector<MeshInput> admittedInputs;']
    start = source.index('MStatus DMConvertFromMeshes::compute(')
    end = source.index('    std::size_t outputMeshCount = 0U;', start)
    prefix = source[start:end]
    assert 'DOMMY_RETURN_IF_FAILURE(collectMeshInputs(' in prefix
    assert end < source.index('createPluginDataObject<DMeshData>', start)
    parts += [prefix + 'admittedInputs.swap(inputs); ++publications; return MS::kSuccess; }\n'
              'DOMMY_COMPUTE_CATCH("DMConvertFromMeshes")']
    return '\n'.join(parts)


def geometry_body(root):
    source = (root / SOURCE).read_text()
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    preflight = (root / 'src/core/common/utility/DMayaMeshImport.h').read_text()
    # Integration guards supplement the executed helpers; they do not emulate
    # native polygon generation, triangulation or DG publication.
    compute = extract(source, 'MStatus DMConvertFromMeshes::compute(')
    assert 'DOMMY_RETURN_IF_FAILURE(polygonNormal(points, normal));' in compute
    assert 'dommy::maya::visitMeshPolygons(' in compute
    assert 'input->importPlan.faces' in compute
    assert 'if (points.length() > remainingCorners) return MS::kFailure;' in compute
    assert 'if (remainingCorners != 0U) return MS::kFailure;' in compute
    assert 'if (hasUVs && input->importPlan.uvs == 0U) return MS::kFailure;' in compute
    return '\n'.join(['namespace dommy { namespace maya {'] +
        [extract(values, signature) for signature in (
            'inline bool isFinitePoint(', 'inline bool isFiniteVector(',
            'inline bool isFloatRepresentablePoint(')] + ['}}',
        extract(preflight, 'namespace dommy {'),
        extract(source, 'enum DMConvertMeshType') + ';',
        extract(source, 'struct MeshInput') + ';',
        extract(source, 'MStatus validateInputBudget('),
        extract(source, 'MStatus polygonNormal(')])


def sdk_check(root, sdk):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'),
                '-idirafter', str(root / 'tests/stubs')]
    source = str(root / SOURCE)
    subprocess.run(command + ['-fsyntax-only', source], check=True)
    subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                              '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, source], check=True)
    print('DMConvertFromMeshes: complete Maya SDK unit syntax/analysis passed (limited Imath test double)')


def mutations(root):
    initial = initializer(root)
    inputs = input_body(root)
    geometry = geometry_body(root)
    cases = [
        ('initializer setter failure ignored', 'mesh_convert_init', initial,
         'DOMMY_RETURN_IF_FAILURE(genericAttribute.setArray(true));',
         '(void)genericAttribute.setArray(true);'),
        ('default string failure overwritten', 'mesh_convert_init', initial,
         'DOMMY_RETURN_IF_FAILURE(status);\n    aNormalAttributeName', 'aNormalAttributeName'),
        ('initializer exception escapes', 'mesh_convert_init', initial,
         'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
        ('nonfinite legacy matrix becomes absent', 'mesh_convert_input', inputs,
         'if (!dommy::maya::isFiniteMatrix(candidate)) return MS::kInvalidParameter;',
         'if (!dommy::maya::isFiniteMatrix(candidate)) return MS::kSuccess;'),
        ('explicit matrix jump failure ignored', 'mesh_convert_input', inputs,
         'DOMMY_RETURN_IF_FAILURE(matrixArray.jumpToArrayElement(matrix->second));',
         '(void)matrixArray.jumpToArrayElement(matrix->second);'),
        ('mesh index status ignored', 'mesh_convert_input', inputs,
         'meshArray.elementIndex(&status);\n        DOMMY_RETURN_IF_FAILURE(status);',
         'meshArray.elementIndex(&status);'),
        ('matrix count not bounded', 'mesh_convert_input', inputs,
         'matrixCount > dommy::generation::kMaxConnectedGeometryCount', 'false'),
        ('mesh preflight status ignored', 'mesh_convert_geometry', geometry,
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(mesh, input->importPlan));',
         '(void)dommy::maya::preflightMeshImport(mesh, input->importPlan);'),
        ('mesh aggregate storage unbounded', 'mesh_convert_geometry', geometry,
         '!dommy::mesh::validMeshOperationStorage(total)', 'false'),
        ('per-polygon positions not budgeted', 'mesh_convert_geometry', geometry,
         'separate ? plan.connections : plan.points', 'plan.points'),
        ('per-polygon UV duplication not budgeted', 'mesh_convert_geometry', geometry,
         'separate && plan.uvs != 0U ? plan.connections : plan.uvs', 'plan.uvs'),
        ('polygon normal absolute cutoff restored', 'mesh_convert_geometry', geometry,
         'if (maximum == 0.0L)', 'if (maximum <= std::numeric_limits<double>::epsilon())'),
        ('polygon normal direction reversed', 'mesh_convert_geometry', geometry,
         '(a[j] - b[j]) * (a[k] + b[k])', '(b[j] - a[j]) * (a[k] + b[k])'),
        ('polygon input validation omitted', 'mesh_convert_geometry', geometry,
         'if (!dommy::maya::isFloatRepresentablePoint(point)) return MS::kInvalidParameter;', ''),
    ]
    for label, suite, original, old, new in cases:
        assert old in original, label
        try:
            run(root, original.replace(old, new, 1), capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1, 'Compile failure is not detection: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializer(root), suite='mesh_convert_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializer(root), suite='mesh_convert_init')
        run(root, input_body(root), suite='mesh_convert_input', arguments=[])
        run(root, geometry_body(root), suite='mesh_convert_geometry', arguments=[])

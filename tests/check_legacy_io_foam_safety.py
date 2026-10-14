#!/usr/bin/env python3
"""Actual legacy reader/Foam bodies, API failure injection and frozen schemas."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check
from check_mesh_color_safety import color_output_body

NODES = {
    'applyVtxColorFromFileNode': 'src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp',
    'ReadDPointDataNode': 'src/core/DDataUtility/IO/ReadDPointDataNode.cpp',
    'foamGenerateNode': 'src/modules/mesh/mesh/foamGenerateNode.cpp',
    'DSkinInfo': 'src/modules/mesh/mesh/DSkinInfo.cpp',
}


def bodies(root, initialize):
    result = []
    if not initialize:
        validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
        result += ['namespace dommy { namespace maya {']
        for name in ('isFinitePoint', 'isFiniteVector', 'isFloatRepresentablePoint'):
            result.append(extract(validation, 'inline bool ' + name + '('))
        result += ['}}']
        result += [color_output_body(root)]
    for name, path in NODES.items():
        if not initialize and name == 'DSkinInfo':
            continue  # The existing legacy-mesh suite exercises its compute.
        source = (root / path).read_text()
        result += declarations(name, source, initialize)
        result += [extract(source, 'MStatus ' + name + '::' +
                           ('initialize(' if initialize else 'compute('))]
        if not initialize:
            result += ['DOMMY_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(result)


def run_compute(root, source, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-legacy-io-') as directory:
        return run(root, source, suite='legacy_io_foam_compute',
                   capture=capture, arguments=[directory])


def mutations(root):
    initial, compute = bodies(root, True), bodies(root, False)
    cases = [
        ('initializer failure', initial, 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;', True),
        ('initializer exception', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', True),
        ('unchecked input handle', compute, 'CHECK_MSTATUS_AND_RETURN_IT(stat);', '(void)stat;', False),
        ('wrong named color set', compute, 'colDataArray, MString("colorSet1")',
         'colDataArray, MString("other")', False),
        ('point output setter', compute, 'DOMMY_RETURN_IF_FAILURE(outputData.set(pointData));',
         '(void)outputData.set(pointData);', False),
        ('point read failure publication', compute,
         'MGlobal::displayError("ReadDPointDataNode: failed to read valid point data.");\n\t\t\treturn MS::kFailure;',
         'MGlobal::displayError("ReadDPointDataNode: failed to read valid point data.");', False),
        ('Foam face append', compute, 'DOMMY_RETURN_IF_FAILURE(outputFaceCounts.append(3));',
         '(void)outputFaceCounts.append(3);', False),
        ('Foam deformation setter', compute,
         'DOMMY_RETURN_IF_FAILURE(deformation.set(\n\t\t\t\t\t\tupdated, static_cast<unsigned int>(vertexIndex)));',
         '(void)deformation.set(updated, static_cast<unsigned int>(vertexIndex));', False),
        ('Foam null mesh', compute, 'if (generatedMesh.isNull()) return MS::kFailure;', '', False),
        ('point array cardinality', compute, 'if (points.length() != decodedPoints.size()) return MS::kFailure;', '', False),
        ('color array cardinality', compute, 'if (colDataArray.length() != colors.size()) return MS::kFailure;', '', False),
    ]
    for label, source, old, new, initializer in cases:
        assert old in source, label
        try:
            if initializer:
                run(root, source.replace(old, new), capture=True, suite='legacy_io_foam_init')
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
        result = run(root, bodies(root, True), capture=True,
                     suite='legacy_io_foam_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--sdk' in sys.argv:
        for name, path in NODES.items():
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], path, name)
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, bodies(root, True), suite='legacy_io_foam_init')
        run_compute(root, bodies(root, False))

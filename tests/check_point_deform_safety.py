#!/usr/bin/env python3
"""Complete point-deform bodies with injectable Maya/search/data boundaries."""
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

SOURCE = 'src/modules/point/DPoint/Deform/DPPointDeform.cpp'


def initializer(root):
    source = (root / SOURCE).read_text()
    return '\n'.join(declarations('DPPointDeform', source, True) +
                     [extract(source, 'MStatus DPPointDeform::initialize(')])


def computes(root):
    source = (root / SOURCE).read_text()
    folder = root / 'src/core/common/utility'
    budget = (folder / 'DPointPayloadBudget.h').read_text()
    result = ['namespace dommy {namespace attribute {',
              extract((folder / 'DAttributeArrayConversion.h').read_text(),
                      'inline bool validArrayConversionBudget('),
              '} namespace point {',
              extract(budget, 'class PointPayloadBudget') + ';',
              'template<class Owner> void preflightMetadataPayload(const Owner& input, '
              'std::size_t, PointPayloadBudget& budget) {budget.add(input.metadataBytes, 1U);}',
              extract(budget, 'template <typename Owner>\nvoid preflightPointPayload('), '}}']
    values = (folder / 'DMayaValueValidation.h').read_text()
    result += ['namespace dommy {namespace maya {']
    for name in ('isFinitePoint', 'isFiniteVector', 'isFiniteMatrix'):
        result += [extract(values, 'inline bool ' + name + '(')]
    mesh = (folder / 'DMayaMeshInput.h').read_text()
    for name in ('getMeshObject', 'getConnectedMeshObject'):
        result += [extract(mesh, 'inline MStatus ' + name + '(')]
    result += [extract((folder / 'DMayaMeshTraversal.h').read_text(),
                       'template <typename Iterator, typename Visitor>')]
    publication = (folder / 'DMayaPluginDataOutput.h').read_text()
    result += [extract(publication,
        'template <typename DataType, typename Populate>\nMStatus createPluginDataObject(')]
    for name in ('publishPluginDataObject', 'publishTypedPluginDataObject'):
        result += [extract(publication, 'template <typename DataType>\nMStatus ' + name + '(')]
    result += [extract(publication,
        'template <typename DataType, typename Populate>\nMStatus setPluginDataOutput(')]
    result += ['}}']
    transform = (root / 'src/core/common/object/DTransformOp.h').read_text()
    for name in ('DMatrixDivide', 'DMakeTransform'):
        result += [extract(transform, 'inline void ' + name + '(')]
    result += [extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                       'MStatus getValidInputDP('), extract(source, 'namespace {')]
    declared = declarations('DPPointDeform', source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    result += declared + [extract(source, 'MStatus DPPointDeform::compute('),
                           'DOMMY_COMPUTE_CATCH("DPPointDeform")']
    return '\n'.join(result)


def mutations(root):
    initial, body = initializer(root), computes(root)
    cases = [
        ('initializer setter', initial, 'DOMMY_RETURN_IF_FAILURE(typed.setReadable(false));',
         '(void)typed.setReadable(false);', True),
        ('initializer create', initial, 'DOMMY_RETURN_IF_FAILURE(status);', '', True),
        ('initializer exception', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', True),
        ('input status', body, 'if (!status) return status;\n    if (handle.type()',
         'if (handle.type()', False),
        ('radius status', body, 'DOMMY_RETURN_IF_FAILURE(status);\n    const float radius',
         'const float radius', False),
        ('cardinality', body, 'restPoints.length() != vertices || animatedPoints.length() != vertices',
         'false', False),
        ('finite mesh', body, '!dommy::maya::isFinitePoint(restPoints[i])', 'false', False),
        ('early traversal', body, 'visited == expectedCount ? MS::kSuccess : MS::kFailure',
         'MS::kSuccess', False),
        ('iterator status', body, 'if (!status) return status;\n        if (done)', 'if (done)', False),
        ('adjacency status', body, 'DOMMY_RETURN_IF_FAILURE(current.getConnectedVertices(ids));',
         '(void)current.getConnectedVertices(ids);', False),
        ('matrix allocation', body, 'DOMMY_RETURN_IF_FAILURE(transforms.setLength(static_cast<unsigned int>(vertices)));',
         '(void)transforms.setLength(static_cast<unsigned int>(vertices));', False),
        ('matrix cardinality', body, 'if (transforms.length() != vertices) return MS::kFailure;', '', False),
        ('preflight omitted', body, 'preflightPointDeform(*input, vertices, connections);', '', False),
        ('neighbor payload omitted', body, 'workspace.add(neighbors, sizeof(uint32_t) + 2U * sizeof(double));',
         '(void)neighbors;', False),
        ('metadata omitted', body, 'preflightMetadataPayload(input, count, workspace);', '', False),
        ('premature float conversion', body, 'stagedPoints[point] = moved;',
         'stagedPoints[point] += DPoint(delta[0], delta[1], delta[2]);', False),
    ]
    for label, source, old, new, init in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), capture=True,
                suite='point_deform_init' if init else 'point_deform_compute',
                arguments=None if init else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    source = (root / SOURCE).read_text()
    body = computes(root)
    start = body.index('struct DPPointDeform')
    end = body.index('MStatus DPPointDeform::compute(')
    declared = declarations('DPPointDeform', source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode').replace(
        'static MStatus initialize();',
        'static MStatus initialize(); MStatus compute(const MPlug&, MDataBlock&);')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', line) for line in declared[1:]]
    body = '#include "point_deform_sdk_boundaries.h"\n' + body[:start] + '\n'.join(declared) + body[end:]
    body += '\n' + extract(source, 'MStatus DPPointDeform::initialize(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'tests/cpp'),
        '-I' + str(root / 'src/core'), '-I' + str(root / 'src'),
        '-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-point-deform-sdk-') as directory:
        cpp = Path(directory) / 'point_deform.cpp'
        cpp.write_text(body)
        subprocess.run(command + ['-fsyntax-only', str(cpp)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(cpp)], check=True)
    print('Point deform: complete compute/initializer and mesh helpers passed SDK syntax/analysis')
    print('Non-Maya dependencies are compile-only declarations; full legacy linking/runtime remains unverified')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    else:
        run(root, initializer(root), suite='point_deform_init')
        run(root, computes(root), suite='point_deform_compute', arguments=[])

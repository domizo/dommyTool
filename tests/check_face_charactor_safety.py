#!/usr/bin/env python3
"""Complete face/character utility bodies with injectable Maya boundaries."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check

NODES = {
    'DPolyFaceInfo': 'src/core/UtilityNode/DPolyFaceInfo/DPolyFaceInfo.cpp',
    'DMCharactor': 'src/modules/mesh/DMesh/Utility/DMCharactor.cpp',
}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)


def computes(root):
    folder = root / 'src/core/common/utility'
    validation = (folder / 'DMayaValueValidation.h').read_text()
    result = ['namespace dommy { namespace maya {']
    for name in ('isFinitePoint', 'isFiniteVector', 'isFiniteColor',
                 'isFloatRepresentableVector', 'normalizedVector'):
        result += [extract(validation, 'inline bool ' + name + '(')]
    result += [extract((folder / 'DMayaMeshTraversal.h').read_text(),
                       'template <typename Iterator, typename Visitor>')]
    result += [extract((folder / 'DMayaPluginDataOutput.h').read_text(),
                       'template <typename DataType>\nMStatus publishPluginDataObject('), '}}']
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    result += ['namespace dommy { namespace math {',
               re.search(r'const double kRadiansToDegrees\s*=\s*[^;]+;', math).group(0),
               extract(math, 'double radiansToDegrees('), '}}']
    result.append(extract((root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text(),
                          'MStatus getValidInputDM('))
    for name, path in NODES.items():
        source = (root / path).read_text()
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};} MStatus compute(')
        if name == 'DMCharactor':
            result += [extract(source, 'namespace {')]
        result += attrs + [extract(source, 'MStatus ' + name + '::compute('),
                           'DOMMY_COMPUTE_CATCH("' + name + '")']
    return '\n'.join(result)


def mutations(root):
    init, body = initializers(root), computes(root)
    cases = [
        ('initializer setter', init, 'DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));',
         '(void)typedAttribute.setReadable(false);', True),
        ('default creation overwritten', init,
         'const MObject centerDefault = stringData.create("center", &status);\n    DOMMY_RETURN_IF_FAILURE(status);',
         'const MObject centerDefault = stringData.create("center", &status);', True),
        ('initializer exception', init, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', True),
        ('premature position output', body, 'MDataHandle rotateOutput = data.outputValue(outRotate, &status);',
         'translateOutput.set3Double(center.x, center.y, center.z);\n'
         'MDataHandle rotateOutput = data.outputValue(outRotate, &status);', False),
        ('cancelled point mean', body,
         'if (!dommy::numeric::checkedPointMean(facePoints, mean)) return MS::kFailure;',
         'mean = {{0,0,0}}; for (std::size_t i=0;i<facePoints.size();++i) '
         'for (unsigned int a=0;a<3;++a) mean[a] += (facePoints[i][a]-mean[a])/(i+1);', False),
        ('point cardinality', body,
         'if (points.length() != static_cast<unsigned int>(vertexCount)) return MS::kFailure;', '', False),
        ('point storage admission', body, '!dommy::mesh::validMeshOperationStorage(plan)', 'false', False),
        ('degree conversion overflow', body, 'if (!std::isfinite(value)) return MS::kInvalidParameter;',
         '(void)value;', False),
        ('premature iterator termination', body, 'visited == expectedCount ? MS::kSuccess : MS::kFailure',
         'MS::kSuccess', False),
        ('iterator error', body, 'if (!status) return status;\n        if (done)', 'if (done)', False),
        ('normal allocation status', body,
         'DOMMY_RETURN_IF_FAILURE(meanNormals.setLength(static_cast<unsigned int>(meshVertexCount)));',
         '(void)meanNormals.setLength(static_cast<unsigned int>(meshVertexCount));', False),
        ('normal array cardinality', body,
         'if (meanNormals.length() != meshVertexCount) return MS::kFailure;', '', False),
        ('in-place smoothing', body, 'const DDoubleArray previousCurvature(finalCurvature);',
         'const DDoubleArray& previousCurvature = finalCurvature;', False),
    ]
    for label, source, old, new, initializer in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new), capture=True,
                suite='face_charactor_init' if initializer else 'face_charactor_compute',
                arguments=None if initializer else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializers(root), suite='face_charactor_init',
                     capture=True, arguments=['--emit'])
        print(result.stdout, end='')
    elif '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        for name, path in NODES.items():
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], path, name)
    else:
        run(root, initializers(root), suite='face_charactor_init')
        run(root, computes(root), suite='face_charactor_compute', arguments=[])

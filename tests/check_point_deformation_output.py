#!/usr/bin/env python3
"""Complete VectorDeform/Transform computes with explicit native boundaries."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

PATHS = {
    'DPVectorDeform': 'src/modules/point/DPoint/Deform/DPVectorDeform.cpp',
    'DPTransform': 'src/core/DDataUtility/Utility/DPTransform.cpp',
}


def bodies(root, overrides=None):
    overrides = overrides or {}
    result = []
    for path, names in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp', ('DP', 'DPData')),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', ('DC', 'DCurveData')),
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', ('DM', 'DMeshData'))):
        source = (root / path).read_text()
        result.extend(extract(source, signature) for signature in (
            'MStatus getValidInput' + names[0] + '(',
            'const ' + names[1] + '* getInput' + names[0] + '('))
    for name, path in PATHS.items():
        source = overrides.get(path, (root / path).read_text())
        declared = declarations(name, source, False)
        if name == 'DPTransform':
            declared[0] = declared[0].replace('MStatus compute(',
                'static void* creator(); MObject thisMObject()const{return {"node"};}'
                'MString name()const{return "DPTransform";}'
                'MStatus process(const std::vector<DPointList*>&,MDataBlock&); MStatus compute(')
        result.extend(declared)
        if name == 'DPTransform':
            result.append(extract(source, 'void* DPTransform::creator('))
            result.append(extract(source, 'MStatus DPTransform::process('))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    flags = ['-I' + str(root / 'src/core/common/utility')]
    run(root, source, suite='point_deformation_output', arguments=[], capture=capture,
        extra_flags=flags)
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    run(root, source, suite='point_deformation_output', arguments=['budget'],
        header_overrides={path: header}, capture=capture, extra_flags=flags)


def mutations(root):
    source = bodies(root)
    for label, old, new in (
        ('vector admission omitted', 'dommy::point::preflightPointPayload(*input,',
         'if (false) dommy::point::preflightPointPayload(*input,'),
        ('vector weights omitted from admission', 'sizeof(DPoint) + sizeof(Imath::V3d) + sizeof(double)',
         'sizeof(DPoint) + sizeof(Imath::V3d)'),
        ('transform admission omitted', 'dommy::point::preflightPointPayload(*inputData,',
         'if (false) dommy::point::preflightPointPayload(*inputData,'),
        ('vector placement omitted', 'point = points[index];', '(void)point; (void)index; (void)points;'),
        ('transform placement omitted', 'point = points[index];\n                        return',
         '(void)point; (void)index; (void)points;\n                        return'),
        ('matrix connection status ignored', 'DOMMY_RETURN_IF_FAILURE(status);\n    if (!connected)',
         'if (!connected)'),
        ('duplicate bounds', 'return status ? data.setClean(plug) : status;',
         'if (status) outputPoints.updateBBox(); return status ? data.setClean(plug) : status;'),
        ('parallel failure ignored', 'if (!valid.load(std::memory_order_relaxed)) return MS::kInvalidParameter;', ''),
    ):
        assert old in source, label
        try:
            execute(root, source.replace(old, new, 1), True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_deformation_output_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    status_mutations(root)


def status_mutations(root):
    source = bodies(root)
    cases = []
    for suffix, kind in (('DP', 'DPData'), ('DC', 'DCurveData'), ('DM', 'DMeshData')):
        old = ('const ' + kind + '* inputData = nullptr;\n'
               '        DOMMY_RETURN_IF_FAILURE(getValidInput' + suffix + '(inDD, data, inputData));')
        new = ('const ' + kind + '* inputData = getInput' + suffix + '(inDD, data);\n'
               '        if (!inputData) return MS::kFailure;')
        assert source.count(old) == 1
        cases.append((suffix + ' transform admission status', source.replace(old, new)))
    old = 'DOMMY_RETURN_IF_FAILURE(stat);\n        if (outputObject.isNull()) return MS::kFailure;'
    assert source.count(old) == 2
    for index, kind in enumerate(('Curve', 'Mesh')):
        offset = source.index(old) if index == 0 else source.index(old, source.index(old) + 1)
        changed = source[:offset] + source[offset:].replace(old,
            'if (!stat || outputObject.isNull()) return MS::kFailure;', 1)
        cases.append((kind + ' transform factory status', changed))
    for label, changed in cases:
        try:
            execute(root, changed, True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_deformation_output_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], tuple(PATHS.values()) + (
            'src/modules/point/DPoint/Utility/DPBlend.cpp',
            'src/modules/point/DPoint/Utility/DPSmooth.cpp',
        ), 'Point deformation')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
        status_mutations(root)

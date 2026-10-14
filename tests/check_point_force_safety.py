#!/usr/bin/env python3
"""Point-force node contracts and failure-injectable Maya boundaries."""
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPAvoid', 'DPMagnet', 'DPGoal')}


def initializers(root):
    result = ['const int kMaximumAvoidIterations = 10000;']
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_body(root, name):
    source = (root / NODES[name]).read_text()
    declared = declarations(name, source, False)
    declared[0] = declared[0].replace(' {', ' {MString name()const{return MString("' + name + '");}', 1)
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(), 'MStatus getValidInputDC('),
              extract((root / 'src/core/common/utility/DPointForceBudget.h').read_text(), 'namespace dommy {'),
              extract((root / 'src/modules/point/DPoint/Common/DPointForceOutput.h').read_text(), 'namespace dommy {')]
    if name != 'DPGoal':
        param = 'AvoidParams' if name == 'DPAvoid' else 'MagneteParams'
        header = (root / NODES[name]).with_suffix('.h').read_text()
        result.append(extract(header, 'struct ' + param) + ';')
        declared[0] = declared[0].replace('MStatus compute(', 'MStatus evalMagnetParms(MDataBlock&, ' + param + '&);MStatus compute(')
        helpers = extract(source, 'namespace {')
        prefix = 'Avoid' if name == 'DPAvoid' else 'Magnet'
        for suffix in ('CurveOp', 'SurfaceOp', 'MeshOp', 'DCOp'):
            helpers = helpers.replace(extract(helpers, 'class ' + prefix + suffix) + ';', '')
        if name == 'DPAvoid':
            helpers = helpers.replace(extract(helpers, 'MStatus displaceMayaPoint('), '')
        result.extend(declared)
        result.append(helpers)
        result.append(extract(source, 'MStatus ' + name + '::evalMagnetParms('))
    else:
        result.extend(declared)
    result.append(extract(source, 'MStatus ' + name + '::compute(') + '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    result.append('using ForceNode=' + name + ';')
    return '\n\n'.join(result)


def check_compute(root):
    for name in NODES:
        run(root, compute_body(root, name), suite='point_force_compute', arguments=[],
            extra_flags=['-D' + name.upper()])
        if name != 'DPGoal':
            path = 'common/utility/DAttributeArrayConversion.h'
            header = (root / 'src/core' / path).read_text().replace(
                'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
            run(root, compute_body(root, name), suite='point_force_compute', arguments=['budget'],
                extra_flags=['-D' + name.upper(), '-I' + str(root / 'src/core/common/utility')],
                header_overrides={path: header})


def mutations(root):
    init = initializers(root)
    avoid, magnet, goal = (compute_body(root, name) for name in NODES)
    cases = [
        ('failed source handle consumed', 'DPAvoid', avoid.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    dommy::point::PointForceInput input;',
            'dommy::point::PointForceInput input;')),
        ('radius read before status', 'DPAvoid', avoid.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);\n    parms.m_radius = handle.asFloat();',
            'parms.m_radius = handle.asFloat();\n    DOMMY_RETURN_IF_FAILURE(stat);')),
        ('quadratic work unbounded', 'DPMagnet', magnet.replace(
            'checkedProduct(points, targets, kMaxWorkItemCount, work)', 'true')),
        ('avoid iteration budget omitted', 'DPAvoid', avoid.replace(
            'checkedProduct(work, iterations, kMaxWorkItemCount, work)', 'true')),
        ('curve partitions not accumulated', 'DPAvoid', avoid.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),', 1)),
        ('mesh partitions not accumulated', 'DPMagnet', magnet.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),')),
        ('point workspace unbounded', 'DPAvoid', avoid.replace(
            'attribute::validArrayConversionBudget(points, 4U * sizeof(float))', 'true')),
        ('partition workspace unbounded', 'DPMagnet', magnet.replace(
            'partitions <= kMaxConnectedGeometryCount', 'true').replace(
            'attribute::validArrayConversionBudget(partitions,\n            2U * sizeof(std::vector<float>) + sizeof(void*))', 'true')),
        ('nearest tie selects last', 'DPMagnet', magnet.replace('delta.length < nearestDistance', 'delta.length <= nearestDistance')),
        ('scalar weights discarded', 'DPMagnet', magnet.replace(
            'if (field && isSingleRealAttrType(field->getAttrType()))', 'if (false && field && isSingleRealAttrType(field->getAttrType()))')),
        ('goal velocity prematurely narrowed', 'DPGoal', goal.replace(
            'std::vector<Imath::V3d> velocities;', 'std::vector<Imath::V3f> velocities;').replace(
            'velocities.assign(pointCount, Imath::V3d(0.0, 0.0, 0.0));', 'velocities.assign(pointCount, Imath::V3f(0.0f, 0.0f, 0.0f));')),
        ('publication failure ignored', 'DPGoal', goal.replace(
            'return status ? data.setClean(plug) : status;', 'return data.setClean(plug);')),
        ('coordinate kernel shape check omitted', 'DPAvoid', avoid.replace(
            'positions.size() != input.partitionCount ||\n            (!positions.empty() && positions[0].size() != input.pointCount)',
            'false')),
        ('initializer failure ignored', None, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', None, init.replace(
            'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    originals = dict(zip(NODES, (avoid, magnet, goal)))
    for label, name, source in cases:
        assert source != (originals[name] if name else init), label
        try:
            run(root, source, capture=True, suite='point_force_compute' if name else 'point_force_init',
                arguments=[] if name else None, extra_flags=['-D' + name.upper()] if name else [])
        except subprocess.CalledProcessError as error:
            # Compile errors are not evidence of an effective regression test.
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    'ERROR: AddressSanitizer' in error.stderr or
                    (label == 'initializer exception escapes' and
                     'uncaught exception' in error.stderr)), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

    header_path = 'common/utility/DPointDeformMath.h'
    header = (root / 'src/core' / header_path).read_text()
    for label, old, new in (
        ('magnet target endpoint lost to cancellation',
         'weight == 1.0 ? static_cast<double>(target[axis]) :', ''),
        ('magnet zero-weight source replaced by target',
         'weight == 0.0 ? static_cast<double>(source[axis]) :',
         'weight == 0.0 ? static_cast<double>(target[axis]) :'),
        ('magnet extrapolation clamped', 'delta.values[axis] * weight;',
         'delta.values[axis] * std::max(0.0, std::min(1.0, weight));'),
    ):
        assert header.count(old) == 1, label
        try:
            run(root, magnet, capture=True, suite='point_force_compute', arguments=[],
                extra_flags=['-DDPMAGNET'], header_overrides={header_path: header.replace(old, new, 1)})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_force_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

    payload_path = 'common/utility/DAttributeArrayConversion.h'
    payload = (root / 'src/core' / payload_path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    for label, old, new in (
        ('point retained metadata admission omitted',
         'preflightMetadataPayload(*input.points, input.pointCount, workspace);', ''),
        ('curve collection metadata admission omitted',
         'preflightMetadataPayload(*input.curves, input.partitionCount, workspace);', ''),
        ('partition metadata admission omitted',
         'preflightMetadataPayload(**it, (*it)->numberOfPoints(), workspace);', '(void)it;'),
    ):
        assert old in avoid, label
        try:
            run(root, avoid.replace(old, new), capture=True, suite='point_force_compute',
                arguments=['budget'], extra_flags=['-DDPAVOID', '-I' + str(root / 'src/core/common/utility')],
                header_overrides={payload_path: payload})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_force_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], NODES, 'Point force', boundary_source='''
#include <maya/MFloatPoint.h>
#include "common/utility/DPointDeformMath.h"
#include "DPoint/Common/DPointForceOutput.h"
bool compileMoveToward(const MFloatPoint& source, const MFloatPoint& target,
                      double weight, MFloatPoint& output) {
    return dommy::point::moveToward(source, target, weight, output);
}
MStatus compileForceOutput(const dommy::point::PointForceInput& input,
                          MDataBlock& block, const MObject& output) {
    return dommy::point::evaluatePointForceOutput(input, block, output, "weight", 1.0f,
        [](std::vector<DPointList*>&, std::vector<DScalarDataList>&) -> MStatus {
            return MS::kSuccess;
        });
}
''')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='point_force_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            check_compute(root)

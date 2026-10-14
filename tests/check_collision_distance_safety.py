#!/usr/bin/env python3
"""Collision/distance arithmetic, aggregate budgets and Maya boundary contracts."""
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPSelfCollision', 'DPDistance')}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    result = [extract((root / 'src/core/common/utility/DPointForceBudget.h').read_text(), 'namespace dommy {'),
              extract((root / 'src/modules/point/DPoint/Common/DPointForceOutput.h').read_text(), 'namespace dommy {'),
              extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(), 'void checkDPAttribute(')]
    source = (root / 'src/core/Data/DDataUtility.cpp').read_text()
    for signature in ('void checkDDataPointAttribute(', 'void checkDDataAttribute('):
        start = 0
        for _ in range(2):
            start = source.index(signature, start)
            result.append(extract(source[start:], signature))
            start += len(signature)
    for name, path in NODES.items():
        source = (root / path).read_text()
        helpers = extract(source, 'namespace {')
        if name == 'DPDistance':
            helpers = helpers.replace(extract(helpers, 'MStatus pointDistance(const MPoint&'), '')
            for cls in ('DistanceCurveOp', 'DistanceSurfaceOp', 'DistanceMeshOp'):
                helpers = helpers.replace(extract(helpers, 'class ' + cls) + ';', '')
        result.append(helpers)
        declared = declarations(name, source, False)
        declared[0] = declared[0].replace(' {', ' {MString name()const{return MString("' + name + '");}', 1)
        result.extend(declared)
        result.append(extract(source, 'MStatus ' + name + '::compute(') + '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('small collisions omitted', False, compute.replace('delta.length == 0.0',
            'delta.length * delta.length <= std::numeric_limits<float>::epsilon()')),
        ('mass sum narrowed', False, compute.replace('const double massSum =', 'const float massSum =')),
        ('small mass skipped', False, compute.replace('massSum == 0.0',
            'massSum <= std::numeric_limits<float>::epsilon()')),
        ('bad neighbor silently skipped', False, compute.replace(
            'if (neighbor >= count) {\n                        m_succeeded.store(false, std::memory_order_relaxed);\n                        return;\n                    }',
            'if (neighbor >= count) continue;')),
        ('failed collision source handle consumed', False, compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const DPData* input = nullptr;',
            'const DPData* input = nullptr;')),
        ('quadratic budget omitted', False, compute.replace(
            'checkedProduct(points, targets, kMaxWorkItemCount, work)', 'true')),
        ('distance workspace omitted', False, compute.replace(
            'sizeof(DPoint) + 2U * (sizeof(float) + sizeof(int))', 'sizeof(float)')),
        ('distance compute preflight omitted', False, compute.replace(
            'if (!validDistanceBudget(distanceTarget, queryCount, input.partitionCount))', 'if (false)')),
        ('distance dispatcher preflight omitted', False, compute.replace(
            'if (!validDistanceBudget(*m_target, queryCount, m_pts->size()))', 'if (false)')),
        ('curve partitions not accumulated', False, compute.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),', 1)),
        ('mesh partitions not accumulated', False, compute.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),')),
        ('nearest tie selects last', False, compute.replace('delta.length < nearestValue', 'delta.length <= nearestValue')),
        ('publication failure ignored', False, compute.replace(
            'return status ? data.setClean(plug) : status;', 'return data.setClean(plug);')),
        ('initializer failure ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initializer, source in cases:
        assert source != (init if initializer else compute), label
        try:
            run(root, source, capture=True, suite='collision_distance_init' if initializer else 'collision_distance_compute',
                arguments=None if initializer else [])
        except subprocess.CalledProcessError as error:
            # A compile failure is not evidence that a regression was detected.
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], NODES, 'Collision/distance')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='collision_distance_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            run(root, compute_bodies(root), suite='collision_distance_compute', arguments=[])

#!/usr/bin/env python3
"""Direction/search node contracts and failure-injectable Maya boundaries."""
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPDirection', 'DPSearchPoint')}


def initializers(root):
    result = []
    source = (root / 'src/modules/point/DPoint/DType.h').read_text()
    result.append(re.search(r'enum\s*\{\s*DP_ATTR_CLASS_POINT[^}]+\};', source).group(0))
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(' {', ' {static const int typeId=1;', 1)
        result.extend(declared)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    result = []
    for path, signature in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp', 'void checkDPAttribute('),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'void checkDCurvePointAttribute('),
            ('src/modules/mesh/DMesh/Common/DMeshUtil.cpp', 'void checkDMeshPointAttribute('),
            ('src/modules/mesh/DMesh/Common/DMeshUtil.cpp', 'void checkDMeshAttribute(')):
        result.append(extract((root / path).read_text(), signature))
    for name, path in NODES.items():
        source = (root / path).read_text()
        helpers = extract(source, 'namespace {')
        if name == 'DPDirection':
            # Native Maya geometry queries are explicit doubles; all value-only
            # kernels, dispatch, preflight and complete compute bodies are real.
            for cls in ('CurveDirOp', 'SurfaceDirOp', 'MeshDirOp'):
                helpers = helpers.replace(extract(helpers, 'class ' + cls) + ';', '')
        result.append(helpers)
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('Primitive output deletes Point field', 'direction_search_compute', compute.replace(
            'checkDMeshAttribute(attributeName, output, true);',
            'checkDMeshPointAttribute(attributeName, output, true);')),
        ('wrong-type Primitive field retained', 'direction_search_compute', compute.replace(
            'checkDMeshAttribute(attributeName, output, true);', '')),
        ('quadratic work unbounded', 'direction_search_compute', compute.replace(
            'checkedProduct(queryCount, targetCount, kMaxWorkItemCount, work)', 'true')),
        ('curve partitions not summed', 'direction_search_compute', compute.replace(
            'checkedSum(queryCount, (*curve)->numberOfPoints(),', 'checkedSum(0U, (*curve)->numberOfPoints(),')),
        ('mesh partitions not summed', 'direction_search_compute', compute.replace(
            'checkedSum(queryCount, (*mesh)->numberOfPoints(),', 'checkedSum(0U, (*mesh)->numberOfPoints(),')),
        ('adjustment work omitted', 'direction_search_compute', compute.replace(
            'checkedSum(work, queryCount, kMaxWorkItemCount, work)', 'true')),
        ('workspace limit omitted', 'direction_search_compute', compute.replace(
            '!dommy::attribute::validArrayConversionBudget(queryCount, sizeof(DVec3Data))', 'false')),
        ('Match ID charged quadratic work', 'direction_search_compute', compute.replace(
            'directionType != 3 || targetCalculationType == 2 ||',
            'directionType != 3 || targetCalculationType < 0 ||')),
        ('failed direction input read', 'direction_search_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const MTypeId inputType = inputHandle.typeId();',
            'const MTypeId inputType = inputHandle.typeId();\n    DOMMY_RETURN_IF_FAILURE(status);', 1)),
        ('failed search scalar read', 'direction_search_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const double radius = handle.asFloat();',
            'const double radius = handle.asFloat();\n    DOMMY_RETURN_IF_FAILURE(status);')),
        ('nearest tie selects last target', 'direction_search_compute', compute.replace(
            'delta.length < nearestDistance', 'delta.length <= nearestDistance')),
        ('strict distance boundary changed', 'direction_search_compute', compute.replace(
            'delta.length < maximumDistance', 'delta.length <= maximumDistance')),
        ('publication failure ignored', 'direction_search_compute', compute.replace(
            'return status ? data.setClean(plug) : status;', 'return data.setClean(plug);', 1)),
        ('initializer failure ignored', 'direction_search_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('manipulator registration failure ignored', 'direction_search_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(DPDirectionManip::addToManipConnectTable(typeId));',
            '(void)DPDirectionManip::addToManipConnectTable(typeId);')),
        ('initializer exception escapes', 'direction_search_init', init.replace(
            'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, suite, source in cases:
        assert source != (init if suite.endswith('_init') else compute), label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], NODES, 'Direction/search')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='direction_search_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            run(root, compute_bodies(root), suite='direction_search_compute', arguments=[])

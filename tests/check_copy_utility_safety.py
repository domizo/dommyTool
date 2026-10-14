#!/usr/bin/env python3
"""Complete duplication/transform bodies with failure-injectable host boundaries."""
from pathlib import Path
import re
import sys
import subprocess
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_point_utility_safety import declarations

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPDuplicator', 'DPCopyTransform', 'DPBlend')}

def initializers(root):
    result = re.findall(r'^#define ROTATE_ORDER_.*$',
                        (root / 'src/modules/point/DPoint/DType.h').read_text(), re.M)
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)

def compute_bodies(root):
    result = []
    result.extend(re.findall(r'^#define ROTATE_ORDER_.*$',
                            (root / 'src/modules/point/DPoint/DType.h').read_text(), re.M))
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, False))
        if 'namespace {' in source:
            result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def check_compute(root, source, capture=False):
    flags = ['-I' + str(root / 'src/core/common/utility')]
    run(root, source, suite='copy_utility_compute', arguments=[], capture=capture,
        extra_flags=flags)
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    run(root, source, suite='copy_utility_compute', arguments=['budget'], capture=capture,
        extra_flags=flags, header_overrides={path: header})


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('failed input handle accessed', 'copy_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const DPData* input = nullptr;',
            'const DPData* input = nullptr;', 1)),
        ('failed target treated as success', 'copy_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle',
            'if (!status) return MS::kSuccess;\n        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle', 1)),
        ('pivot API failure ignored', 'copy_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(transform.setRotatePivot(pivot, MSpace::kObject, false));',
            '(void)transform.setRotatePivot(pivot, MSpace::kObject, false);', 1)),
        ('nonfinite matrix accepted', 'copy_utility_compute', compute.replace(
            'if (!dommy::maya::isFiniteMatrix(candidate)) return MS::kInvalidParameter;', '', 1)),
        ('duplicator preflight omitted', 'copy_utility_compute', compute.replace(
            '!dommy::point::buildPointCopyBudget(*input, target->numberOfPoints(), workItems)', 'false', 1)),
        ('transform preflight omitted', 'copy_utility_compute', compute.replace(
            'if (!dommy::point::buildPointCopyBudget(*input, request.effectiveCount(), workItems))',
            'if (false)', 1)),
        ('mesh normal baking omitted', 'copy_utility_compute', compute.replace(
            'return copy.bakeTransform(matrix);',
            '(void)matrix; copy.setTransformMatrix(MMatrix::identity); return MS::kSuccess;', 1)),
        ('scale advanced per primitive', 'copy_utility_compute', compute.replace(
            'if (index == 0U)', 'if ((void)index, step == 1U)', 1)),
        ('primitive matrix multiplication reversed', 'copy_utility_compute', compute.replace(
            'original.asMatrix() * delta', 'delta * original.asMatrix()', 1)),
        ('original point batch transformed', 'copy_utility_compute', compute.replace(
            'if (step == 0U) return MS::kSuccess;', '')),
        ('initializer status ignored', 'copy_utility_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', 'copy_utility_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('blend array acquisition ignored', 'copy_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n        const unsigned int count = array.elementCount(&status);',
            'const unsigned int count = array.elementCount(&status);', 1)),
        ('blend element count status ignored', 'copy_utility_compute', compute.replace(
            'array.elementCount(&status);\n        DOMMY_RETURN_IF_FAILURE(status);',
            'array.elementCount(&status);', 1)),
        ('blend element status ignored', 'copy_utility_compute', compute.replace(
            'array.inputValue(&status);\n            DOMMY_RETURN_IF_FAILURE(status);',
            'array.inputValue(&status);', 1)),
        ('active blend target silently skipped', 'copy_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, target));\n            if (!target)',
            'if (!dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, target)) continue;\n            if (!target)', 1)),
        ('unstable float blend restored', 'copy_utility_compute', compute.replace(
            'dommy::attribute::checkedLinearBlend(\n                            points[p][axis], targetPoints[p % targetCount][axis],\n                            weight, candidate[axis])',
            '((candidate[axis] = points[p][axis] + (targetPoints[p % targetCount][axis] - points[p][axis]) * weight), std::isfinite(candidate[axis]))', 1)),
        ('blend work ceiling omitted', 'copy_utility_compute', compute.replace(
            '!dommy::generation::checkedSum(budget.workItems, targetWork,\n                    dommy::generation::kMaxWorkItemCount, budget.workItems)',
            'false', 1)),
        ('curve copy admission omitted', 'copy_utility_compute', compute.replace(
            'if (!dommy::curve::buildCurveCopyPlan(*input, request.effectiveCount(), plan))',
            'if (false)', 1)),
        ('mesh copy admission omitted', 'copy_utility_compute', compute.replace(
            '!dommy::mesh::validateRepeatedMeshMetadata(*input, request.effectiveCount(), outputPlan)',
            'false', 1)),
        ('blend aggregate admission omitted', 'copy_utility_compute', compute.replace(
            'dommy::point::preflightPointPayload(*input,',
            'if (false) dommy::point::preflightPointPayload(*input,', 1)),
        ('blend target storage omitted', 'copy_utility_compute', compute.replace(
            'workspace.add(1U, sizeof(PointBlendTarget));', '', 1)),
        ('blend coordinate placement omitted', 'copy_utility_compute', compute.replace(
            'if (!points.empty()) point = points[index];', '(void)point; (void)index; (void)points;', 1)),
        ('blend initialization status ignored', 'copy_utility_init', init.replace(
            'const MObject defaultWeight = fnStringData.create("weight", &stat);\n    DOMMY_RETURN_IF_FAILURE(stat);',
            'const MObject defaultWeight = fnStringData.create("weight", &stat);', 1)),
    ]
    for label, suite, source in cases:
        assert source != (init if suite.endswith('_init') else compute), label
        try:
            if suite.endswith('_init'):
                run(root, source, capture=True, suite=suite)
            else:
                check_compute(root, source, True)
        except subprocess.CalledProcessError as error:
            # Compilation failures are not evidence that a regression was caught.
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    for label, path, old, new in (
        ('point group work omitted', 'DPointMergeBudget.h',
         '!checkedSum(plan.workItems, plan.points, kMaxWorkItemCount, plan.workItems)', 'false'),
        ('repeated point strings unbounded', 'DPointMergeBudget.h',
         '!checkedProduct(rows.stringBytes, copies, kMaxGeneratedStringByteCount, strings)', 'false'),
        ('repeated component strings omitted', 'DMeshCopyBudget.h',
         '!checkedProduct(componentCost.stringBytes, copies, kMaxGeneratedStringByteCount, budget.stringBytes)', 'false'),
    ):
        header = 'common/utility/' + path
        text = (root / 'src/core' / header).read_text()
        assert old in text, label
        try:
            run(root, compute, capture=True, suite='copy_utility_compute', arguments=[],
                header_overrides={header: text.replace(old, new, 1)},
                extra_flags=['-I' + str(root / 'src/core/common/utility')])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'copy_utility_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        import check_group_transfer_safety as shared
        shared.sdk_check(root, sys.argv[2], NODES, 'Copy utilities')
    elif len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run(root, initializers(root), suite='copy_utility_init', arguments=['--emit'])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run(root, initializers(root), suite='copy_utility_init')
        check_compute(root, compute_bodies(root))

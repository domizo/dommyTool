#!/usr/bin/env python3
"""Exact Motion Trail admission/selection and storage plan, without native DG.

The existing feedback/trail suite owns the frozen initializer. This suite stops
before native historical evaluation, curve construction and array publication.
"""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
import check_motion_duplicator_safety as sdk

SOURCE = 'src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp'


def admission(root):
    source = (root / SOURCE).read_text()
    header = (root / SOURCE.replace('.cpp', '.h')).read_text()
    budget = (root / 'src/core/common/utility/DMotionGenerationPlan.h').read_text()
    numeric = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result = [extract(budget, 'namespace dommy {'), extract(header, 'enum {') + ';',
              'namespace dommy {namespace maya {',
              extract(numeric, 'template <typename DataBlock, typename Attribute>'),
              extract(values, 'inline bool isFinitePoint('), '}}']
    result += declarations('DMotionTrailCurveNode', source, False)
    start = source.index('MStatus DMotionTrailCurveNode::compute(')
    end = source.index('    MFnDependencyNode dependencyNode', start)
    prefix = source[start:end]
    for bulk in ('curveFn.getCVs(', 'surfaceFn.getCVs(', 'meshFn.getPoints(',
                 'trailPositions.copy(', 'std::vector<unsigned int> indices('):
        assert prefix.index('buildMotionTrailBudget(') < prefix.index(bulk)
    result += [prefix + '\nselectedPositions=trailPositions; ++admissions;'
               'admittedBytes=budget.estimatedBytes; return MS::kSuccess;}\n'
               'DOMMY_COMPUTE_CATCH("DMotionTrailCurveNode")']
    return '\n'.join(result)


def mutations(root):
    body = admission(root)
    cases = [
        ('input getter before status', 'DOMMY_RETURN_IF_FAILURE(status);\n    const MTime evaluationTime',
         'const MTime evaluationTime'),
        ('early controls omitted', '!dommy::generation::validMotionTrailControls(\n'
         '            timeSamplingValue, evaluationTime.value(), trailLengthValue,\n'
         '            trailLengthRandomValue)', 'false'),
        ('mesh count status ignored', 'meshFn.numVertices(&status);\n        DOMMY_RETURN_IF_FAILURE(status);',
         'meshFn.numVertices(&status);'),
        ('surface CV dimensions added', 'checkedProduct(\n                u, v,',
         'checkedSum(\n                u, v,'),
        ('position cardinality ignored', 'if (objectPoints.length() != sourcePointCount) return MS::kFailure;', ''),
        ('selection cardinality ignored', 'if (trailPositions.length() != budget.curveCount) return MS::kFailure;', ''),
        ('source workspace omitted', 'budget, sourcePointCount,', 'budget, 0U,'),
        ('source width omitted', '{64U, 1088U, 96U, 512U, sizeof(unsigned int)}',
         '{0U, 1088U, 96U, 512U, sizeof(unsigned int)}'),
        ('curve overhead omitted', '{64U, 1088U, 96U, 512U, sizeof(unsigned int)}',
         '{64U, 0U, 96U, 512U, sizeof(unsigned int)}'),
        ('retained output omitted', '{64U, 1088U, 96U, 512U, sizeof(unsigned int)}',
         '{64U, 1088U, 0U, 512U, sizeof(unsigned int)}'),
        ('matrix construction workspace omitted', '{64U, 1088U, 96U, 512U, sizeof(unsigned int)}',
         '{64U, 1088U, 96U, 0U, sizeof(unsigned int)}'),
        ('shuffle indices omitted', 'randomSelection && candidate.curveCount != 0U ? sourcePointCount : 0U',
         'randomSelection && candidate.curveCount != 0U ? 0U : 0U'),
        ('random selection seed changed', 'static_cast<unsigned int>(seedValue)', 'static_cast<unsigned int>(seedValue + 1)'),
        ('empty selection bypasses finite points', 'if (!dommy::maya::isFinitePoint(objectPoints[index]))',
         'if (budget.curveCount != 0U && !dommy::maya::isFinitePoint(objectPoints[index]))'),
    ]
    for label, old, new in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new, 1), capture=True, suite='motion_trail_admission', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'motion_trail_admission_tests', \
                'Compile failure is not detection: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk.sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], SOURCE, 'DMotionTrailCurveNode')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, admission(root), suite='motion_trail_admission', arguments=[])

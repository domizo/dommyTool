#!/usr/bin/env python3
"""Curve conversion/legacy controls, admission and frozen node contracts."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check
from check_active_cpp_safety import INITIALIZE_METHOD_PATTERN

NODES = {
    'DCConvertFromCurves': 'src/modules/curve/DCurve/DCConvertFromCurves.cpp',
    'DSpiralCurve': 'src/modules/curve/legacy/DSpiralCurve.cpp',
    'woolCurve': 'src/modules/curve/legacy/woolCurve.cpp',
}


def initializers(root):
    source = (root / NODES['DCConvertFromCurves']).read_text()
    result = [extract(source, 'enum {') + ';']
    for name, path in NODES.items():
        source = (root / path).read_text()
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)


def controls(root):
    result = []
    paths = dict(NODES, DCNoise='src/modules/curve/DCurve/Utility/DCNoise.cpp')
    for name, end in (('DCNoise', '    if (deformType <'),
                      ('DSpiralCurve', '\t\tdommy::curve::SpiralCurvePlan plan;'),
                      ('woolCurve', '\tdommy::curve::WoolCurvePlan plan;')):
        source = (root / paths[name]).read_text()
        start = source.index('MDataHandle control =', source.index('::compute('))
        body = source[start:source.index(end, start)]
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(const MPlug&, MDataBlock&);', 'static MStatus controls(MDataBlock&);')
        result += attrs + ['MStatus ' + name + '::controls(MDataBlock& data) { MStatus stat, status;' +
                           body + 'return MS::kSuccess;}']
    return '\n'.join(result)


def plans(root):
    return '\n'.join(extract((root / path).read_text(), 'namespace dommy {') for path in (
        'src/core/common/utility/DCurveGenerationStorage.h',
        'src/modules/curve/DCurve/DCurveConversionPlan.h',
        'src/modules/curve/legacy/DSpiralCurvePlan.h'))


def conversion(root):
    assert not INITIALIZE_METHOD_PATTERN.search('dommy::curve::initializeCurveConversionPlan(1, 0, 1, plan);')
    assert INITIALIZE_METHOD_PATTERN.search('MStatus DCConvertFromCurves::initialize() try {')
    assert INITIALIZE_METHOD_PATTERN.search('MStatus DSpiralCurve::initialize () {')
    source = (root / NODES['DCConvertFromCurves']).read_text()
    dag = (root / 'src/core/common/object/DNodeOperation.cpp').read_text()
    result = [extract(dag, signature) for signature in (
        'MStatus getChildNodeFromNode(', 'MStatus extractDagNodeFromTypeName(',
        'MStatus getNurbsCurveNodeFromHierarchy(')]
    result += [extract(source, 'enum {') + ';', 'typedef short DCConvertCurveType;', extract(source, 'namespace {')]
    attrs = declarations('DCConvertFromCurves', source, False)
    attrs[0] = attrs[0].replace('MStatus compute(', 'MObject thisMObject()const {return {"node"};} MStatus compute(')
    result += attrs + [extract(source, 'MStatus DCConvertFromCurves::compute('),
                       'DOMMY_COMPUTE_CATCH("DCConvertFromCurves")']
    assert source.count('output.insert(') == 1 and 'generated.push_back(curve);' in source
    assert source.index('admitCurveConversionCounts(') < source.index('createFromMayaObject(')
    spiral = (root / NODES['DSpiralCurve']).read_text()
    assert spiral.index('buildSpiralCurvePlan(') < spiral.index('xQuatArray.reserve(')
    assert 'const std::size_t knotCount = plan.knots;' in spiral
    return '\n'.join(result)


def mutations(root):
    init, reads, plan, compute = initializers(root), controls(root), plans(root), conversion(root)
    cases = [
        ('initializer status', 'curve_conversion_init', init, 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;', None),
        ('initializer setter', 'curve_conversion_init', init, 'DOMMY_RETURN_IF_FAILURE(cnvEnumFn.setKeyable(true));',
         '(void)cnvEnumFn.setKeyable(true);', None),
        ('initializer exception', 'curve_conversion_init', init, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', None),
        ('noise failed handle', 'curve_conversion_controls', reads, 'if (!status)\n        return status;', '', []),
        ('legacy failed handle', 'curve_conversion_controls', reads, 'CHECK_MSTATUS_AND_RETURN_IT(status);', '', []),
    ]
    for label, old, new in (
        ('curve object storage', 'curves, 2048U', 'curves, 0U'),
        ('curve point storage', 'totalPoints, 96U', 'totalPoints, 0U'),
        ('curve scratch storage', 'pointsPerCurve, 512U', 'pointsPerCurve, 0U'),
        ('collection slots', 'plan.inputSlots, 128U', 'plan.inputSlots, 0U'),
        ('knots storage', 'plan.knots, 32U', 'plan.knots, 0U'),
        ('mask storage', 'masks, 4U', 'masks, 0U'),
        ('group storage', 'plan.groups, 576U', 'plan.groups, 0U'),
        ('Spiral point scratch', 'candidate.samples, 256U', 'candidate.samples, 0U'),
        ('Spiral knot scratch', 'candidate.knots, 32U', 'candidate.knots, 0U'),
        ('Spiral knot padding', 'candidate.samples, 2U, generation::kMaxGeneratedPointCount, candidate.knots',
         'candidate.samples, 0U, generation::kMaxGeneratedPointCount, candidate.knots'),
    ):
        cases.append((label, 'curve_conversion_plans', plan, old, new, []))
    for label, old, new in (
        ('input status', 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;'),
        ('count query status', 'DOMMY_RETURN_IF_FAILURE(status);', '(void)status;'),
        ('physical element progression', 'DOMMY_RETURN_IF_FAILURE(inputs.jumpToArrayElement(i));', '(void)i;'),
        ('query budget bypass', '!dommy::curve::admitCurveConversionCounts(cvs, knots, plan)', 'false'),
        ('short output batch', 'output.numberOfCurves() != plan.curves', 'false'),
        ('native CV mismatch', 'curve->numberOfPoints() != static_cast<std::size_t>(counts[i].first)', 'false'),
        ('native knot mismatch', 'curve->getKnots().length() != static_cast<unsigned int>(counts[i].second)', 'false'),
        ('group offset', 'members.push_back(objects.length() - 1U);', 'members.push_back(0U);'),
        ('group write verification', '!written || written->getGroupIds() != mask', 'false'),
        ('DAG failed query', 'if (!status) return status;', '(void)status;'),
        ('DAG visit charge', 'remaining -= objectArray.length();', ''),
        ('intermediate shapes', 'if (!intermediate) {', 'if (true) {'),
    ):
        cases.append((label, 'curve_conversion_compute', compute, old, new, []))
    for label, suite, body, old, new, arguments in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), capture=True, suite=suite, arguments=arguments)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializers(root), capture=True, suite='curve_conversion_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        for name, path in dict(NODES, DCNoise='src/modules/curve/DCurve/Utility/DCNoise.cpp',
                               DNodeOperation='src/core/common/object/DNodeOperation.cpp').items():
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], path, name)
    else:
        run(root, initializers(root), suite='curve_conversion_init')
        run(root, controls(root), suite='curve_conversion_controls', arguments=[])
        run(root, plans(root), suite='curve_conversion_plans', arguments=[])
        run(root, conversion(root), suite='curve_conversion_compute', arguments=[])

#!/usr/bin/env python3
"""Surface/point curve generation admission and frozen initializer checks."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check
from arnold_name_list_source import name_helper

FOLDER = 'src/modules/curve/DCurve/Generate/'
NAMES = ('DCurveFromDP', 'DCurveCircleGen', 'DCurveFromSurface', 'DCurveClothFromSurface')


def enums(root):
    source = (root / (FOLDER + 'DCurveFromDP.h')).read_text()
    result = re.findall(r'enum\s*\{[^}]+\};typedef int \w+;', source)
    source = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    result += [re.search(r'enum\s*\{\s*DC_kLinear[^}]+\};typedef short DC_DEGREE_TYPE;', source)[0]]
    return '\n'.join(result)


def initializers(root):
    result = [enums(root)]
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result += declarations(name, source, True)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def plans(root):
    result = [extract((root / 'src/core/common/utility/DCurveGenerationStorage.h').read_text(), 'namespace dommy {')]
    surface = (root / 'src/core/common/utility/DCurveSampling.h').read_text()
    result += ['namespace dommy {namespace curve {', extract(surface, 'struct SurfaceSamplingPlan {') + ';',
               extract(surface, 'inline bool checkedSurfaceSamplingPlan('), '}}',
               extract((root / (FOLDER + 'DCurveClothSamplingPlan.h')).read_text(), 'namespace dommy {')]
    for node, planner, allocation in (
        ('DCurveFromSurface', 'checkedSurfaceSamplingPlan(', 'generatedCurves.reserve('),
        ('DCurveClothFromSurface', 'checkedClothNormalStorage(', 'curves.reserve(')):
        source = (root / (FOLDER + node + '.cpp')).read_text()
        assert source.index(planner) < source.index(allocation)
    cloth = (root / (FOLDER + 'DCurveClothFromSurface.cpp')).read_text()
    start = cloth.index('std::size_t estimatedBytes = plan.estimatedBytes;')
    end = cloth.index('DCurveData::DCurveList curves;', start)
    result += ['MStatus clothAdmission(const dommy::curve::ClothSamplingPlan& plan, '
               'bool addNormalAttribute, const std::string& normalAttributeName) {' + cloth[start:end] +
               '++admissions;return MS::kSuccess;}']
    return '\n'.join(result)


def point_compute(root):
    source = (root / (FOLDER + 'DCurveFromDP.cpp')).read_text()
    names = (root / 'src/core/common/utility/DMayaNameList.h').read_text()
    result = [extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'MStatus getValidInputDP('),
              enums(root), extract((root / 'src/core/common/utility/DCurveGenerationStorage.h').read_text(), 'namespace dommy {'),
              name_helper(names), extract(source, 'namespace {')]
    attrs = re.findall(r'^MObject\s+DCurveFromDP::(\w+);', source, re.M)
    result += ['struct DCurveFromDP { MStatus compute(const MPlug&, MDataBlock&);'
               'MString name()const{return "DCurveFromDP";}' + ''.join('static MObject '+a+';' for a in attrs) + '};']
    result += ['MObject DCurveFromDP::'+a+'{"'+a+'"};' for a in attrs]
    compute = extract(source, 'MStatus DCurveFromDP::compute(')
    # Observation only, just before the host publication boundary.
    compute = compute.replace('MDataHandle output = data.outputValue',
                              'admittedBytes = storage.estimatedBytes(); MDataHandle output = data.outputValue')
    result += [compute + '\nDOMMY_COMPUTE_CATCH("DCurveFromDP")']
    assert 'setDPData(' not in source and 'deletePointFromIds(' not in source
    return '\n'.join(result)


def mutations(root):
    initial, plan, point = initializers(root), plans(root), point_compute(root)
    cases = [
        ('initializer status ignored', 'curve_generation_init', initial,
         'DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));', '(void)tAttr.setReadable(false);', None),
        ('circle setter ignored', 'curve_generation_init', initial,
         'DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));', '(void)numericAttribute.setKeyable(true);', None),
        ('initializer exception escapes', 'curve_generation_init', initial,
         'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', None),
        ('curve payload omitted', 'surface_generation_plan', plan, 'totalPoints, 96U', 'totalPoints, 0U', []),
        ('curve object cost omitted', 'surface_generation_plan', plan, 'curves, 2048U', 'curves, 0U', []),
        ('construction scratch omitted', 'surface_generation_plan', plan, 'pointsPerCurve, 512U', 'pointsPerCurve, 0U', []),
        ('normal payload omitted', 'surface_generation_plan', plan, 'plan.totalPointCount, 48U', 'plan.totalPointCount, 0U', []),
        ('normal names omitted', 'surface_generation_plan', plan, 'names, 4U', 'names, 0U', []),
        ('normal objects omitted', 'surface_generation_plan', plan, 'plan.totalCurveCount, 512U', 'plan.totalCurveCount, 0U', []),
        ('normal preflight failure ignored', 'surface_generation_plan', plan,
         'return MS::kInvalidParameter;', '(void)0;', []),
    ]
    for old, new, label in (
        ('storage.add(count, 8U);', '', 'selection masks'),
        ('storage.add(selectedCount, 256U);', '', 'selected point scratch'),
        ('storage.add(characters, 64U);', '', 'group name storage'),
        ('storage.add(1U, metadata.estimatedBytes());', '', 'metadata aggregate'),
        ('storage.add(1U, 512U);', '(void)0;', 'metadata objects'),
        ('storage.add(1U, 128U);', '', 'distance pair storage'),
        ('if (!foundGroup) std::fill(selected.begin(), selected.end(), true);', '', 'missing group fallback'),
        ('inverted ? !ids[i] : ids[i]', 'ids[i]', 'inverted group selection'),
        ('selected[i] || (inverted ? !ids[i] : ids[i])', '(inverted ? !ids[i] : ids[i])', 'group union'),
        ('!selected->cropArrayFromIds(selection)', '(static_cast<void>(selection), false)', 'selected attribute remapping'),
        ('if (createTypeVal == DC_DP_GEN_ALL && pointCount > 1U)',
         'if (createTypeVal == DC_DP_GEN_ALL && pointCount > 100U)', 'All attribute admission'),
        ('if (!group || group->getGroupIds().size() != count)', 'if (!group)', 'group cardinality'),
        ('storage.add(1U, bytes);', 'storage.add(0U, bytes);', 'output aggregate admission'),
    ):
        cases.append((label, 'point_curve_generation', point, old, new, []))
    for label, suite, body, old, new, arguments in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), suite=suite, arguments=arguments, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='curve_generation_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        for name in NAMES:
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], FOLDER + name + '.cpp', name)
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='curve_generation_init')
        run(root, plans(root), suite='surface_generation_plan', arguments=[])
        run(root, point_compute(root), suite='point_curve_generation', arguments=[])

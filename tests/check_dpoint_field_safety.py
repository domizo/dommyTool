#!/usr/bin/env python3
"""Execute all three DPoint field compute bodies with fallible host boundaries.

Attribute storage/conversion and field math are real. Maya arrays, datablocks,
falloff sampling and publication are doubles, not a native dynamics simulation.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from point_input_source import point_input_bodies, optional_point_mutations

NAMES = ('DPFluidVectorField', 'DPFluidNoiseField', 'DPGoalField')
FOLDER = 'src/core/DDataUtility/Particle/field/'


def bodies(root, overrides=None):
    access = (root / 'src/modules/point/DPoint/Common/DPAttributeAccess.h').read_text()
    result = [point_input_bodies(root, overrides),
              'namespace dommy { namespace point { namespace detail {',
              extract(access, 'template <typename ValueType>\ninline bool copyCheckedScalarAttribute('),
              '}}}', extract(access, 'inline bool getCheckedDScalarArrayFromAttribute(')]
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = (overrides or {}).get(path, (root / path).read_text())
        attributes = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':FieldBoundary {' +
                      ''.join('static MObject ' + attr + ';' for attr in attributes) +
                      'MStatus compute(const MPlug&, MDataBlock&);};')
        result.extend('MObject ' + name + '::' + attr + '{"' + attr + '"};'
                      for attr in attributes)
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def mutations(root, original):
    cases = []
    for name in NAMES:
        body = extract(original, 'MStatus ' + name + '::compute(')
        old = 'if (receptorCount > dommy::generation::kMaxGeneratedPointCount)'
        cases.append((name + ' early count limit', body, body.replace(old, 'if (false)', 1)))
        checked = re.search(r'status = dommy::maya::buildCheckedArray\([\s\S]+?if \(!status\) return status;', body).group(0)
        cases.append((name + ' array status', body,
                      body.replace(checked, checked.replace('if (!status) return status;', ''), 1)))
        cases.append((name + ' unchecked constructor', body,
                      body.replace('MVectorArray forceArray;\n    ' + checked,
                                   'MVectorArray forceArray(receptorCount, MVector::zero);', 1)))
    goal = extract(original, 'MStatus DPGoalField::compute(')
    old = 'normalizedDistance, &status);\n            if (!status) return status;'
    assert old in goal
    cases.append(('Goal falloff failure', goal, goal.replace(old, 'normalizedDistance, &status);', 1)))
    for label, old, new in cases:
        assert old != new, label
        try:
            run(root, original.replace(old, new, 1), suite='dpoint_field', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'dpoint_field_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('DPoint field mutations: all {} detected at runtime'.format(len(cases)))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    run(root, source, suite='dpoint_field', arguments=[])
    optional_point_mutations(root, bodies, 'dpoint_field', [
        (FOLDER + name + '.cpp', 'MStatus ' + name + '::compute(') for name in NAMES])
    if '--mutations' in sys.argv:
        mutations(root, source)

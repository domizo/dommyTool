#!/usr/bin/env python3
"""Exercise complete particle emitter/Kill compute bodies and shared copies.

Real converters, budgets, kill math and helpers run with failure-injectable
host doubles. Mesh queries, DG storage and particle side effects are not Maya.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from point_input_source import point_input_bodies, optional_point_mutations

SOURCES = (
    ('DPEmitter', 'src/core/DDataUtility/Particle/DPEmitter.cpp'),
    ('DMeshVolumeEmitter', 'src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp'),
    ('DKillField', 'src/modules/particles/particle/field/DKillField.cpp'),
)


def bodies(root, overrides=None):
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result = ['namespace dommy { namespace maya {']
    result.extend(extract(validation, 'inline bool ' + name + '(') for name in (
        'isFiniteMatrix', 'isFinitePoint', 'isFiniteBoundingBox'))
    result.append('}}')
    result.append(point_input_bodies(root, overrides))
    helper = (root / 'src/core/common/object/DParticleAttributeUtils.h').read_text()
    result.extend(['namespace dommy {', extract(helper, 'namespace particle {'), '}'])
    access = (root / 'src/modules/point/DPoint/Common/DPAttributeAccess.h').read_text()
    result.append('namespace dommy { namespace point { namespace detail {')
    for kind in ('Vector', 'Double', 'Int'):
        result.append(extract(access, 'template <typename ValueType>\ninline bool copyChecked' + kind + 'Attribute('))
    result.extend([extract(access, 'inline bool copyCheckedStringAttribute('), '}}}'])
    result.extend(extract(access, 'inline bool getCheckedM' + kind + 'ArrayFromAttribute(')
                  for kind in ('Vector', 'Double', 'Int', 'String'))
    kill_header = (root / SOURCES[2][1]).with_suffix('.h').read_text()
    result.append(kill_header[kill_header.index('enum DKillType'):kill_header.index('class DKillField')])
    for name, path in SOURCES:
        source = (overrides or {}).get(path, (root / path).read_text())
        attributes = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result.append('namespace test_' + name + ' {')
        result.append('struct ' + name + '{' +
                      ''.join('static MObject ' + attr + ';' for attr in attributes) +
                      'MObject thisMObject() const { return {"node"}; }'
                      'MStatus compute(const MPlug&, MDataBlock&);};')
        result.extend('MObject ' + name + '::' + attr + '{"' + attr + '"};'
                      for attr in attributes)
        result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
        result.extend(['}', 'using ' + name + '=test_' + name + '::' + name + ';'])
    return '\n\n'.join(result)


def mutations(root, source):
    cases = []
    for name, _ in SOURCES:
        body = extract(source, 'MStatus ' + name + '::compute(')
        arrays = re.findall(r'(M(?:Vector|Double)Array (\w+);\n    status = dommy::maya::buildCheckedArray\([\s\S]+?if \(!status\) return status;)', body)
        assert arrays, name
        for statement, variable in arrays:
            cases.append((name + ' ' + variable + ' status', statement,
                          statement.replace('if (!status) return status;', '')))
            if variable != 'positions':
                count = 'pointCount' if name == 'DPEmitter' else (
                    'emittedCount' if name == 'DMeshVolumeEmitter' else 'receptorCount')
                value = 'kSurvivalLifespan' if variable == 'lifespans' else 'MVector::zero'
                array_type = 'MDoubleArray' if variable == 'lifespans' else 'MVectorArray'
                cases.append((name + ' ' + variable + ' unchecked constructor', statement,
                              '{} {}({}, {});'.format(array_type, variable, count, value)))
    cases.extend([
        ('mesh initial array length', 'if (emittedPositions.length() != gridSampleCount) return MS::kFailure;', ''),
        ('mesh final array length', 'if (emittedPositions.length() != emittedCount) return MS::kFailure;', ''),
    ])
    kill = extract(source, 'MStatus DKillField::compute(')
    prepare = ('const MObject outputObject = outputData.create(forceArray, &status);\n'
               '    if (!status) return status;\n    ')
    assert prepare in kill
    late = kill.replace(prepare, '', 1).replace(
        'const auto finishOutput = [&]() -> MStatus {',
        'const auto finishOutput = [&]() -> MStatus {\n    ' + prepare, 1)
    cases.append(('Kill output prepared after lifespan mutation', kill, late))
    helper = re.search(r'inline MStatus copyArrayAttribute\(MFnArrayAttrsData& outputData,\s*const MString& name,\s*const MStringArray& source\)[\s\S]+?\n}', source).group(0)
    cases.append(('string copy status', helper, helper.replace(
        'return dommy::maya::copyCheckedArray(source, output);',
        'dommy::maya::copyCheckedArray(source, output); return MS::kSuccess;')))
    for label, old, new in cases:
        assert old in source and old != new, label
        try:
            run(root, source.replace(old, new, 1), suite='particle_output', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'particle_output_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Particle output mutations: all {} detected at runtime'.format(len(cases)))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    run(root, source, suite='particle_output', arguments=[])
    optional_point_mutations(root, bodies, 'particle_output',
        [(SOURCES[0][1], 'MStatus DPEmitter::compute(')], shared=True)
    if '--mutations' in sys.argv:
        mutations(root, source)

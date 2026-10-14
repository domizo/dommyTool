#!/usr/bin/env python3
"""Execute complete point-emitter control flow against failure-injectable hosts."""
from pathlib import Path
import re
import sys
import subprocess

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

PATH = 'src/core/DDataUtility/Generate/DPointCurveEmit.cpp'


def bodies(root, overrides=None):
    source = (overrides or {}).get(PATH, (root / PATH).read_text())
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result = [extract(utility, signature) for signature in (
        'const DCurveData* getInputDC(', 'MStatus getValidInputDC(')]
    result.append(extract(source, 'namespace {'))
    result.insert(0, 'namespace dommy { namespace math {\n' + extract(
        (root / 'src/core/math/dommyMath.h').read_text(),
        'inline bool isInsideUnitCircle(') + '\n} }')
    begin = source.index('enum {')
    result.append(source[begin:source.index('typedef int D_CURVE_SAMPLING_TYPE;', begin) +
                         len('typedef int D_CURVE_SAMPLING_TYPE;')])
    attrs = re.findall(r'^MObject\s+DPointCurveEmit::(\w+);', source, re.M)
    process_type = 'MStatus' if 'MStatus DPointCurveEmit::processGeneratePoint(' in source else 'void'
    result.append('struct DPointCurveEmit { MObject thisMObject() const {return {"node"};}'
                  'MStatus compute(const MPlug&, MDataBlock&);' + process_type +
                  ' processGeneratePoint(MDataBlock&, const MObjectArray&, DPointList&);' +
                  ''.join('static MObject ' + a + ';' for a in attrs) + '};')
    result.extend('MObject DPointCurveEmit::' + a + '{"' + a + '"};' for a in attrs)
    result.append(extract(source, 'MStatus DPointCurveEmit::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DPointCurveEmit")')
    result.append(extract(source, process_type + ' DPointCurveEmit::processGeneratePoint('))
    text = '\n\n'.join(result)
    # Only host APIs are replaced; complete node bodies and numerical helpers remain.
    for real, fake in (('MDataBlock', 'EmitterBlock'), ('MFnNurbsCurve', 'EmitterCurve'),
                       ('MRampAttribute', 'EmitterRamp'), ('MFnDependencyNode', 'EmitterDependency')):
        text = re.sub(r'\b' + real + r'\b', fake, text)
    return text


def main():
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root), suite='curve_emitter', arguments=[str(
        root/'tests/fixtures/curve_emitter_output_contract.txt')])
    source = (root / PATH).read_text()
    for before, after in (
        ('const float sectionRadius = radius * ramp;', 'const float sectionRadius = radius;'),
        ('if (!(fieldValue * densityAmplitude > 0.0)) return;', '(void)densityAmplitude; if (false) return;'),
        ('const double displacement = fieldValue * displacementAmplitude;',
         'const double displacement = fieldValue * displacementAmplitude * radius;'),
        ('parameter / noiseScale + noiseOffset.z', 'parameter + noiseOffset.z'),
    ):
        assert source.count(before) == 1, before
        try:
            run(root, bodies(root, {PATH: source.replace(before, after)}),
                suite='curve_emitter', capture=True, arguments=[str(
                    root/'tests/fixtures/curve_emitter_output_contract.txt')])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_emitter_tests', 'Compile failure: ' + before
            assert 'Assertion' in error.stderr, before
        else:
            raise AssertionError('Undetected Pyro placement mutation: ' + before)
    print('Independent Pyro placement: coordinates, rejection, radius/ramp, density and negative controls passed')


if __name__ == '__main__':
    main()

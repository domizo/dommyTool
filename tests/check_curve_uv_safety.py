#!/usr/bin/env python3
"""Execute the complete DCUVMap body, retaining the real UV planning math."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

PATH = 'src/modules/curve/DCurve/Utility/DCUVMap.cpp'


def bodies(root, overrides=None):
    source = (overrides or {}).get(PATH, (root / PATH).read_text())
    utility = (root/'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result = [extract(utility, signature) for signature in (
        'const DCurveData* getInputDC(', 'MStatus getValidInputDC(')]
    result.append('namespace dommy { namespace maya {\n' + extract(
        (root/'src/core/common/utility/DMayaRampSampling.h').read_text(),
        'inline bool sampleFiniteRamp(').replace('MRampAttribute', 'UVRamp')+'\n} }')
    attrs = re.findall(r'^MObject\s+DCUVMap::(\w+);', source, re.M)
    result.append('struct DCUVMap {MObject thisMObject() const {return {"node"};}'
                  'MStatus compute(const MPlug&, MDataBlock&);' +
                  ''.join('static MObject '+a+';' for a in attrs)+'};')
    result.extend('MObject DCUVMap::'+a+'{"'+a+'"};' for a in attrs)
    result.append(extract(source, 'MStatus DCUVMap::compute(').replace('MRampAttribute', 'UVRamp') +
                  '\nDOMMY_COMPUTE_CATCH("DCUVMap")')
    return '\n\n'.join(result)


def mutations(root):
    header_path = 'common/utility/DCurveUVMapping.h'
    header = (root / 'src/core' / header_path).read_text()
    source = (root / PATH).read_text()
    unchecked = source.replace('''                if (!dommy::numeric::checkedFloatCast(
                        mappedValue, values[index])) {''', '''                values[index] = static_cast<DScalarData>(mappedValue);
                if (!std::isfinite(values[index])) {''')
    wrong_sample = source.replace('plan, normalizedValue, mappedValue)',
                                  'plan, position + 0.0 * normalizedValue, mappedValue)')
    cases = [
        ('float narrowing checked only after conversion', unchecked, header),
        ('maximum reconstructed by cancelling arithmetic', source, header.replace(
            'normalizedValue == 1.0 ? plan.maximum :', '')),
        ('minimum endpoint replaced by maximum', source, header.replace(
            'normalizedValue == 0.0 ? plan.minimum :', 'normalizedValue == 0.0 ? plan.maximum :')),
        ('UV extrapolation clamped', source, header.replace(
            'plan.minimum + normalizedValue * plan.span',
            'plan.minimum + std::max(0.0, std::min(1.0, normalizedValue)) * plan.span')),
        ('Ramp mapping pinned to curve position', wrong_sample, header),
    ]
    for label, altered, altered_header in cases:
        assert altered != source or altered_header != header, label
        try:
            run(root, bodies(root, {PATH: altered}), capture=True, suite='curve_uv',
                header_overrides={header_path: altered_header})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_uv_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if sys.argv[1:] == ['--mutations']:
        mutations(root)
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], {'DCUVMap': PATH}, 'Curve UV', boundary_source='''
#include <maya/MDataHandle.h>
#include "common/utility/DCurveUVMapping.h"
#include "common/utility/DNumericValidation.h"
bool compileUVMapping(const MDataHandle& rangeHandle, double position, float& output) {
    const double2& range = rangeHandle.asDouble2();
    dommy::curve::UVMappingPlan plan;
    double value = 0.0;
    return dommy::curve::buildUVMappingPlan(0, 0, range[0], range[1], 1.0, 0.0, plan) &&
        dommy::curve::mapUVValue(plan, position, value) &&
        dommy::numeric::checkedFloatCast(value, output);
}
''')
    else:
        assert not sys.argv[1:]
        run(root, bodies(root), suite='curve_uv')

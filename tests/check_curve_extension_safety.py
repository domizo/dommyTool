#!/usr/bin/env python3
"""Execute DCExtend and five curve initializers with host failure injection."""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/modules/curve/DCurve/Utility/'
NAMES = ('DCStitch', 'DCExtend', 'DCFillBetween', 'DCConnect', 'DCReplace')


def bodies(root, overrides=None):
    overrides = overrides or {}
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC(')]
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DCExtend':
            result.append(extract(source, 'MStatus DCExtend::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("DCExtend")')
    return '\n\n'.join(result)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root), suite='curve_extension')

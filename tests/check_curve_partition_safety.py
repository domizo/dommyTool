#!/usr/bin/env python3
"""Execute complete convex/partition bodies with real hull and polygon algorithms."""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/modules/curve/DCurve/Utility/'
NAMES = ('DCConvex2D', 'DCPartition', 'DCDelete', 'DCExplodeView',
         'DCWeightMap', 'DCUVMap', 'DCClumping')


def bodies(root, overrides=None):
    overrides = overrides or {}
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                      'class DCurveOutputBatch final') + ';']
    result.extend(extract(utility, signature) for signature in (
        'MStatus getValidInputDC(',
        'void getDCurveMatchedGroups('))
    result.append('namespace dommy { namespace attribute {\n' + extract(
        (root / 'src/core/common/utility/DAttributeArrayConversion.h').read_text(),
        'inline bool validArrayConversionBudget(') + '\n} }')
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        if name == 'DCPartition':
            result.append(extract(source, 'namespace {'))
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name in ('DCConvex2D', 'DCPartition'):
            result.append(extract(source, 'MStatus ' + name + '::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root), suite='curve_partition')

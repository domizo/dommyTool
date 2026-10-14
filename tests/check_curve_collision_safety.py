#!/usr/bin/env python3
"""Run full curve collision/offset/normal nodes with strict data-only API spaces."""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/modules/curve/DCurve/Utility/'
NAMES = ('DCCutObject', 'DCReflect', 'DCOffset', 'DCNormal')


def bodies(root, overrides=None):
    overrides = overrides or {}
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                      'class DCurveOutputBatch final') + ';',
              extract(utility, 'MStatus getValidInputDC(')]
    result.append(extract(utility, 'MStatus generateWorldMCurveFromDcurve('))
    mesh = (root / 'src/core/common/utility/DMayaMeshInput.h').read_text()
    result.append('namespace dommy { namespace maya {\n' +
                  extract(mesh, 'inline MStatus getTransformedMeshObject(') + '\n} }')
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        if name == 'DCNormal':
            result.append(extract(source, 'enum {') + '; typedef int DC_NORMAL_GEN_TYPE;')
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        compute = extract(source, 'MStatus ' + name + '::compute(')
        compute = compute.replace('MRampAttribute', 'TestCurveRamp')
        result.append(compute + '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root), suite='curve_collision')

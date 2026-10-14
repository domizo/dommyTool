#!/usr/bin/env python3
"""Run complete mesh-emitter bodies and both emitter attribute initializers."""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/core/DDataUtility/Generate/'


def bodies(root, overrides=None):
    overrides = overrides or {}
    utility = (root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text()
    result = [extract(utility, signature) for signature in (
        'const DMeshData* getInputDM(', 'MStatus getValidInputDM(')]
    resets = []
    for name in ('DPointCurveEmit', 'DPointMeshEmit'):
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        extra = ''
        if name == 'DPointCurveEmit':
            start = source.index('enum {')
            result.append(source[start:source.index('typedef int D_CURVE_SAMPLING_TYPE;', start) +
                                 len('typedef int D_CURVE_SAMPLING_TYPE;')])
        else:
            extra = ('MStatus compute(const MPlug&, MDataBlock&);'
                     'void emitDPointFromUVBase(MDataBlock&, const MObject&, DPointList&);')
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();' + extra +
                      'MObject thisMObject() const {return {"node"};}'
                      'MString name() const {return "mesh emitter";}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        resets.extend(name + '::' + a + ' = {"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DPointMeshEmit':
            result.append(extract(source, 'namespace {'))
            result.append(extract(source, 'void DPointMeshEmit::emitDPointFromUVBase('))
            result.append(extract(source, 'MStatus DPointMeshEmit::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("DPointMeshEmit")')
    result.append('void resetAttributes() {' + ''.join(resets) + '}')
    text = '\n\n'.join(result)
    # Host boundaries only; all production sampling/control-flow bodies run.
    text = re.sub(r'\bMDataBlock\b', 'EmitterBlock', text)
    text = re.sub(r'\bMFnPluginData\b', 'MeshPluginData', text)
    return text


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    args = sys.argv[1:] or [str(root/'tests/fixtures/mesh_emitter_initialization_contract.txt'),
                           str(root/'tests/fixtures/mesh_emitter_output_contract.txt')]
    run(root, bodies(root), suite='mesh_emitter', arguments=args)

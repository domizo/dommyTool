#!/usr/bin/env python3
"""Run complete circle/surface emitters against failure-injectable Maya boundaries."""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/core/DDataUtility/Generate/'
NAMES = ('DPointCircleEmit', 'DPointSurfaceEmit')


def bodies(root, overrides=None):
    overrides = overrides or {}
    result, resets = [], []
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result.append('namespace dommy { namespace maya {\n' + extract(
        validation, 'inline bool isFloatRepresentableVector(') + '\n} }')
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        header = (root / (FOLDER + name + '.h')).read_text()
        enum_start = header.index('enum {')
        enum_end = header.index(';', header.index('typedef int', enum_start)) + 1
        result.append(header[enum_start:enum_end])
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MString name() const {return "point emitter";}'
                      'bool withinCircle(float, float);' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        resets.extend(name + '::' + a + ' = {"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
        if name == 'DPointCircleEmit':
            result.append(extract(header, 'inline bool DPointCircleEmit::withinCircle('))
    result.append('void resetAttributes() {' + ''.join(resets) + '}')
    text = '\n\n'.join(result)
    # Only host boundaries are replaced; production sampling and RNG remain.
    for real, fake in (('MDataBlock', 'EmitterBlock'), ('MFnPluginData', 'MeshPluginData'),
                       ('MFnNurbsSurface', 'EmitterSurface'), ('MVector', 'EmitterVector')):
        text = re.sub(r'\b' + real + r'\b', fake, text)
    return text


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    args = sys.argv[1:] or [str(root/'tests/fixtures/circle_surface_emitter_initialization_contract.txt'),
                           str(root/'tests/fixtures/circle_surface_emitter_output_contract.txt')]
    run(root, bodies(root), suite='circle_surface_emitter', arguments=args)

#!/usr/bin/env python3
"""Run DPFluidEmitter's real callback and both emission paths without Maya.

Typed attributes and grid planning are real; host storage, time, transforms and
fluid API calls are doubles. This does not emulate native fluid simulation.
"""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from point_input_source import point_input_bodies, optional_point_mutations


def bodies(root, overrides=None):
    path = 'src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp'
    source = (overrides or {}).get(path, (root / path).read_text())
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    return '\n\n'.join([
        point_input_bodies(root, overrides),
        'namespace dommy { namespace maya {',
        extract(validation, 'inline bool isFiniteMatrix('),
        extract(validation, 'inline bool isFinitePoint('), '}}',
        extract(source, 'struct DPFluidEmitter::EmissionValues') + ';',
        extract(source, 'namespace {'),
        extract(source, 'MStatus DPFluidEmitter::fluidEmitter('),
        'DOMMY_COMPUTE_CATCH("DPFluidEmitter")',
        extract(source, 'MStatus DPFluidEmitter::emitFromPoints('),
        extract(source, 'MStatus DPFluidEmitter::emitFromGridAttributes('),
    ])


def mutations(root, source):
    cases = [
        ('zero-default grid admission', 'if (emitType == 0 && !densityToEmit',
         'if (!densityToEmit'),
        ('point no-emission fast path', 'if (emitType == 0 && !densityToEmit',
         'if (false && !densityToEmit'),
        ('point/voxel count', 'if (pointCount != gridPlan.scalar.voxelCount)',
         'if (false)'),
        ('scalar length boundary', 'attribute->attrSize() != expectedCount', 'false'),
        ('disabled density channel', 'if (!emission.densityEnabled) emission.density = 0.0f;', ''),
        ('density attribute priority', '? densityValues[voxelIndex] : emission.density',
         '? emission.density : emission.density'),
        ('heat attribute priority', '? heatValues[voxelIndex] : emission.heat',
         '? emission.heat : emission.heat'),
        ('fuel attribute priority', '? fuelValues[voxelIndex] : emission.fuel',
         '? emission.fuel : emission.fuel'),
    ]
    for label, old, new in cases:
        assert old in source, label
        changed = source.replace(old, new, 1)
        if label == 'scalar length boundary':
            # Both length checks enforce the same boundary for real typed data.
            changed = changed.replace('if (source.size() != expectedCount) return false;', '', 1)
        try:
            run(root, changed, suite='fluid_emitter', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'fluid_emitter_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Fluid emitter mutations: all {} detected at runtime'.format(len(cases)))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    run(root, source, suite='fluid_emitter', arguments=[])
    optional_point_mutations(root, bodies, 'fluid_emitter', [
        ('src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp', 'MStatus DPFluidEmitter::' + name + '(')
        for name in ('emitFromPoints', 'emitFromGridAttributes')])
    if '--mutations' in sys.argv:
        mutations(root, source)

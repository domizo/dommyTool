#!/usr/bin/env python3
"""Run the complete legacy swept-emitter computes with injectable host APIs.

Shared math, particle helpers, random generation and vendored Perlin are real.
Swept queries, arrays and DG publication are doubles, not a native Maya test.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

SOURCES = (
    ('DEmitter', 'src/modules/particles/DEmitter/DEmitter.cpp'),
    ('curveEmitter', 'src/modules/curve/curveEmitter/curveEmitter.cpp'),
)


def bodies(root):
    helper = (root / 'src/core/common/object/DParticleAttributeUtils.h').read_text()
    result = ['namespace dommy {', extract(helper, 'namespace particle {'), '}']
    for name, path in SOURCES:
        source = (root / path).read_text()
        attributes = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result.append('namespace test_' + name + ' { struct ' + name + ' {' +
                      ''.join('static MObject ' + a + ';' for a in attributes) +
                      'MObject thisMObject() const {return {"node"};}'
                      'MStatus compute(const MPlug&,MDataBlock&);};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};'
                      for a in attributes)
        result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
        result.append('} using ' + name + '=test_' + name + '::' + name + ';')
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    vendor = root / 'third_party/libnoise/source'
    # The retained libnoise base class has an intentionally unused assignment
    # parameter. Suppress that warning without modifying the vendored sources.
    flags = ['-Wno-unused-parameter', '-Wno-delete-abstract-non-virtual-dtor',
             '-I' + str(vendor)]
    flags.extend(str(vendor / name) for name in ('modulebase.cpp', 'perlin.cpp', 'noisegen.cpp'))
    return run(root, source, capture=capture, suite='swept_emitter',
               arguments=[], extra_flags=flags)


def mutations(root, source):
    cases = []
    for name, _ in SOURCES:
        body = extract(source, 'MStatus ' + name + '::compute(')
        old = ('const bool isConnected = sweptGeometryPlug.isConnected(&status);\n'
               '    if (!status) return status;')
        assert old in body
        cases.append((name + ' ignored connection error', body,
                      body.replace(old, 'const bool isConnected = sweptGeometryPlug.isConnected();', 1)))
        blend = re.search(r'MVector sample;\n\s*if \(!dommy::attribute::checkedLinearBlend\('
                          r'[\s\S]+?return MS::kFailure;\n\s*}', body).group(0)
        formula = ('currentCenter + (previousCenter - currentCenter)' if name == 'DEmitter'
                   else 'previousPoint + (currentPoint - previousPoint)')
        cases.append((name + ' unchecked interpolation', body,
                      body.replace(blend, 'MVector sample = ' + formula + ' * fraction;', 1)))
    center = extract(source, 'bool checkedTriangleCenter(')
    cases.append(('unchecked triangle mean', center,
                  '''bool checkedTriangleCenter(MDynSweptTriangle& triangle, double time, MVector& center) {
    center = (triangle.vertex(0,time) + triangle.vertex(1,time) + triangle.vertex(2,time)) / 3.0;
    return isFiniteVector(center);
}'''))
    cases.append(('ignored mean validation', center,
                  center.replace('std::array<double, 3> mean;', 'std::array<double, 3> mean = {{0,0,0}};')
                  .replace('if (!dommy::numeric::checkedPointMean(vertices, mean)) return false;',
                                 'dommy::numeric::checkedPointMean(vertices, mean);')))
    for label, old, new in cases:
        assert old in source and old != new, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'swept_emitter_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Swept emitter mutations: all {} detected at runtime'.format(len(cases)))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    execute(root, source)
    if '--mutations' in sys.argv:
        mutations(root, source)

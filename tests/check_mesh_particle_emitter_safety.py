#!/usr/bin/env python3
"""Execute three complete mesh-particle emitter bodies with injectable host APIs."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/modules/particles/particle/emitter/'
NAMES = ('DPolygonEmitter', 'DRadialGeometryEmitter', 'DMeshVolumeEmitter')


def bodies(root):
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result = ['#include "mesh_particle_emitter_doubles.h"', 'namespace dommy { namespace maya {']
    result.extend(extract(validation, 'inline bool ' + name + '(')
                  for name in ('isFiniteMatrix', 'isFinitePoint'))
    result.append('}}')
    helper = (root / 'src/core/common/object/DParticleAttributeUtils.h').read_text()
    result.extend(['namespace dommy {', extract(helper, 'namespace particle {'), '}'])
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attributes = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result.append('namespace test_' + name + ' { struct ' + name + ' {' +
                      ''.join('static MObject ' + a + ';' for a in attributes) +
                      'MObject thisMObject() const {return {"node"};}'
                      'MStatus compute(const MPlug&,MDataBlock&);};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attributes)
        result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
        result.append('} using ' + name + '=test_' + name + '::' + name + ';')
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    return run(root, source, suite='mesh_particle_emitter', arguments=[], capture=capture,
               extra_flags=['-I' + str(root / 'tests/cpp'),
                            str(root / 'src/core/noise/PerlinNoise.cpp')])


def mutations(root, source):
    cases = []
    for name in NAMES:
        signature = ('MStatus collectMeshSamples(' if name == 'DRadialGeometryEmitter'
                     else 'MStatus ' + name + '::compute(')
        body = extract(source, signature)
        array = 'vertices' if name == 'DRadialGeometryEmitter' else 'meshPoints'
        count = 'vertexCount' if name == 'DMeshVolumeEmitter' else 'checkedVertexCount'
        check = 'if ({}.length() != {}) return MS::kFailure;'.format(array, count)
        cases.append((name + ' missing cardinality check', body, body.replace(check, '', 1)))
        read = 'MPointArray {0};\n    status = mesh.getPoints({0}, MSpace::kObject);\n    if (!status) return status;'.format(array)
        assert read in body
        late = body.replace(read, '', 1).replace('const int rawVertexCount =', read + '\n    const int rawVertexCount =', 1)
        cases.append((name + ' allocation before count checks', body, late))
        status_read = 'const int rawVertexCount = mesh.numVertices(&status);\n    if (!status) return status;'
        cases.append((name + ' ignored vertex count status', body,
                      body.replace(status_read, 'const int rawVertexCount = mesh.numVertices(&status);', 1)))
    # Compile alternate helper implementations in the generated source rather
    # than editing the production header. Removing the included original via
    # its explicit test macro keeps these mutations local to the test process.
    helper = (root / (FOLDER + 'DMeshEmitterSampling.h')).read_text()
    helper_body = extract(helper, 'namespace dommy {')
    blend = extract(helper_body, 'template <typename Point>\nbool blendMeshSample(')
    quad = extract(helper_body, 'inline bool quadSampleCoordinate(')
    alternatives = [
        ('unchecked edge/diagonal blend', blend, '''template <typename Point>
bool blendMeshSample(const Point& a,const Point& b,double weight,Point& output) {
    output=a+(b-a)*weight; return true;
}'''),
        ('missing quad overflow fallback', quad, '''inline bool quadSampleCoordinate(
    double origin,double first,double third,double u,double v,double& output) {
    output=origin+(first-origin)*u+(third-origin)*v; return std::isfinite(output);
}'''),
    ]
    for label, old, new in alternatives:
        cases.append((label, source, '#define DOMMY_TEST_REPLACE_MESH_SAMPLING\n' +
                      '#include "mesh_particle_emitter_doubles.h"\n' +
                      helper_body.replace(old, new, 1) + '\n' + source))
    for label, old, new in cases:
        assert old in source and old != new, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_particle_emitter_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Mesh particle emitter mutations: all {} detected at runtime'.format(len(cases)))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    execute(root, source)
    if '--mutations' in sys.argv:
        mutations(root, source)

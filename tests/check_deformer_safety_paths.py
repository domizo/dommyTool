#!/usr/bin/env python3
"""Exercise production deformer math and API boundaries without running Maya.

The iterator and data handles are failure-injectable doubles, not a Maya SDK
replacement. Geometry queries and Maya's internal commit behavior need Maya.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    root = Path(__file__).resolve().parent.parent
    names = ('noiseDeformer', 'DCollisionDeformer', 'pushDeformer',
             'surfaceDeformer', 'DMotionDeformer', 'DRayDeformer')
    sources = {name: (root / ('src/modules/deformers/Deformer/' + name + '.cpp')).read_text()
               for name in names}
    header = (root / 'src/core/common/utility/DMayaDeformerGeometry.h').read_text()
    bodies = [header[header.index('namespace dommy {'):]]
    reads = []
    for name, source in sources.items():
        evaluation = source[:source.index('MStatus ' + name + '::initialize(')]
        deform = extract(evaluation, 'MStatus ' + name + '::deform(')
        for forbidden in (r'inputValue\([^;]*\)\.(?:as\w+|child)\(',
                          r'\biter\.(?:setPosition|setAllPositions|position|index|next|reset)\(',
                          r'\bCHECK_MSTATUS\('):
            if re.search(forbidden, deform):
                raise SystemExit(name + ': unsafe input/iterator boundary: ' + forbidden)
        if deform.count('editDeformerPoints(') != 1:
            raise SystemExit(name + ': expected one staged geometry publication')
        # Execute the actual acquisition/check/access statements. The surrounding
        # node-specific geometry queries are not replaced by these reader tests.
        pattern = (r'MDataHandle (\w+) =\s*(block|data)\.inputValue\((\w+), &(stat|status)\);'
                   r'\s*CHECK_MSTATUS_AND_RETURN_IT\(\4\);\s*'
                   r'const (float|double|int|long|short|bool|MMatrix|DT_DROPOFF_TYPE) (\w+) = '
                   r'\1\.(as\w+)\(\);')
        for match in re.finditer(pattern, evaluation):
            statement = match.group().replace('inputValue(' + match[3] + ',', 'inputValue(1,')
            reads.append('MStatus read_' + str(len(reads)) + '(MDataBlock& block) {\n'
                         'MDataBlock& data = block; (void)data; MStatus stat, status; '
                         '(void)stat; (void)status;\n' + statement + '\n(void)' + match[6] +
                         '; return MS::kSuccess; }')
    noise = sources['noiseDeformer']
    for required in ('kMaxNoiseOctaves = noise::kMaxPerlinOctaves',
                     'setMax(kMaxNoiseOctaves)', 'pNoise.tryNoise(', 'offsetPoint('):
        if required not in noise:
            raise SystemExit('noiseDeformer: missing ' + required)
    if re.search(r'\bfloat\s+(noiseVal|dropoffWeight)', noise):
        raise SystemExit('noiseDeformer: narrowed evaluation intermediate')
    collision = sources['DCollisionDeformer']
    for required in ('smoothCollisionPoints(', 'visitDeformerPoints(', 'weights[index] * double(env)',
                     'adjacencyWork'):
        if required not in collision:
            raise SystemExit('collision: missing ' + required)
    if re.search(r'float\s+(xSum|ySum|zSum|avgX|avgY|avgZ)', collision):
        raise SystemExit('collision: narrowed smoothing arithmetic')
    # Weighting belongs only to the final blend, not collision/bulge candidates.
    candidates = collision[collision.index('double maxDisplacement ='):collision.index(
        'return dommy::maya::editDeformerPoints')]
    if re.search(r'\b(env|weights|weightValue)\b', candidates):
        raise SystemExit('collision: weights applied before final publication')
    collision_deform = extract(collision, 'MStatus DCollisionDeformer::deform(')
    publication = collision_deform[collision_deform.rindex('return dommy::maya::editDeformerPoints'):]
    bodies.append('MStatus publishCollision(MItGeometry& iter, const std::vector<MPoint>& points, '
                  'const std::vector<double>& weights, float env) {\n' + publication)
    start = collision.index('const bool hit = colMeshFn.allIntersections(')
    end = collision.index('if (!hit', start)
    bodies.append('MStatus queryCollision(FakeMesh& colMeshFn, bool& returnedHit) {\n'
                  'MStatus stat; MPoint colliderPoint; MVector rayDirection; '
                  'int mmAccelParams = 0; MFloatPointArray hits; MFloatArray parameters;\n' +
                  collision[start:end] + '\nreturnedHit = hit; return MS::kSuccess; }')
    if len(reads) != 45:
        raise SystemExit('Unexpectedly incomplete deformer input-reader inventory')
    bodies.extend(reads)
    bodies.append('using Reader = MStatus (*)(MDataBlock&);\nReader readers[] = {' +
                  ','.join('read_' + str(i) for i in range(len(reads))) + '};')
    # The actual DNoise controls (including composite reads and runtime limits).
    start = noise.index('    MStatus stat;', noise.index('::deform('))
    end = noise.index('    MArrayDataHandle inputDataH', start)
    controls = noise[start:end]
    attributes = sorted(set(re.findall(r'inputValue\((\w+),', controls)))
    bodies.append('enum NoiseAttribute { ' + ','.join(attributes) + ' };')
    bodies.append('MStatus readNoiseControls(MDataBlock& block) { const MMatrix localToWorld;\n' + controls +
                  '\n(void)seedVal; (void)absOnVal; (void)enableTransformVal; '
                  '(void)isDropoffVal; return MS::kSuccess; }')
    with tempfile.TemporaryDirectory(prefix='dommy-deformer-safety-') as directory:
        temporary = Path(directory)
        (temporary / 'deformer_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'deformer_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/deformer_safety_tests.cpp'),
            str(root / 'src/core/noise/PerlinNoise.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)
    print('Deformer safety: numeric contracts, staged geometry, and ' + str(len(reads)) +
          ' input-reader failure boundaries passed')


if __name__ == '__main__':
    main()

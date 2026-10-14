#!/usr/bin/env python3
"""Exercise production connection, normal and mesh-query paths without Maya."""

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
    folder = root / 'src/modules/curve/DCurve/Utility'
    normal = (folder / 'DCNormal.cpp').read_text()
    reflect = (folder / 'DCReflect.cpp').read_text()
    cut = (folder / 'DCCutObject.cpp').read_text()
    connect = (folder / 'DCConnect.cpp').read_text()
    for name, source in (('DCReflect', reflect), ('DCCutObject', cut), ('DCConnect', connect)):
        compute = extract(source, 'MStatus ' + name + '::compute(')
        for required in ('getValidInputDC(', 'createPluginData(', 'CHECK_MSTATUS_AND_RETURN_IT(status)'):
            if required not in compute:
                raise SystemExit(name + ': missing checked boundary ' + required)
        if name != 'DCConnect':
            for required in ('sampleAtLength(', 'checkedAllIntersections(', 'MeshAcceleratorGuard<MFnMesh>'):
                if required not in compute:
                    raise SystemExit(name + ': missing checked query ' + required)
            if re.search(r'\.(allIntersections|closestIntersection|tangent|findParamFromLength)\(', compute):
                raise SystemExit(name + ': unchecked geometry query returned')
    if 'if (finished) break;' not in reflect or 'buildCurveConnectionChains(' not in connect or 'emitCurveChain<MPoint>(' not in connect:
        raise SystemExit('Connection orientation / reflection termination boundary missing')
    bodies = [extract(normal, 'enum {') + ';']
    normal_loop = extract(normal, 'for (size_t c=0; c<cvCount; c++)')
    bodies.append('MStatus normal_samples(int genType, Curve& crvFn, Ramp& twistRampAttr, '
                  'std::vector<MVector>& output) {\nMStatus stat; const size_t cvCount = 2; '
                  'const double totalLength = 1; const double kMFnNurbsEpsilon = 1e-6; '
                  'std::vector<MPoint> cvs{MPoint(0,0,0),MPoint(1,0,0)}; '
                  'std::vector<MVector> calcnorms(cvCount), tangents(cvCount); '
                  'MVector oldBiNormal; FloatArray twistValueArray;\n' + normal_loop +
                  '\noutput = calcnorms; return MS::kSuccess; }')
    # Extract complete production sample loops, including checked appends and
    # state transitions, so fixture success cannot mask a missing node break.
    for name, source in (('reflect', reflect), ('cut', cut)):
        loop = extract(source, 'for (std::size_t i = 0U; i < sampleCount; ++i)')
        append = extract(source, 'const auto appendPoint =') + ';'
        prefix = ('MStatus ' + name + '_samples(Curve& function, Mesh& mesh, bool isFit, '
                  'std::vector<MPoint>& published) {\nMStatus status; '
                  'const std::size_t sampleCount = 3; const double length = 10; '
                  'int accelerator = 0; MPointArray points;\n' + append + '\n')
        if name == 'reflect':
            prefix += ('(void)isFit; const double step = 5, bias = 0; bool continuing = false; '
                       'MPoint nextPoint, previous; MVector continuationNormal;\n')
            suffix = '\npublished = points.values; return MS::kSuccess; }'
        else:
            prefix += ('bool wasInside = false; MPoint previousHit; std::vector<MPoint> staged; '
                       'const auto finishSegment = [&]() -> MStatus { '
                       'staged.insert(staged.end(), points.values.begin(), points.values.end()); '
                       'return points.clear(); };\n')
            suffix = ('\nstatus = finishSegment(); CHECK_MSTATUS_AND_RETURN_IT(status); '
                      'published = staged; return MS::kSuccess; }')
        bodies.append(prefix + loop + suffix)
    with tempfile.TemporaryDirectory(prefix='dommy-curve-queries-') as directory:
        temporary = Path(directory)
        (temporary / 'curve_query_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/curve_connection_query_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)
    print('Curve connection, tangent and intersection failure tests: OK')


if __name__ == '__main__':
    main()

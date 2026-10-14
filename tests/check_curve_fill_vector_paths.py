#!/usr/bin/env python3
"""Exercise extracted fill/stitch generation and vector-deformation boundaries.

Host mocks inject status/array failures; they do not simulate Maya geometry,
allocator behavior, or the real TBB scheduler.
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
    paths = {
        'fill': 'src/modules/curve/DCurve/Utility/DCFillBetween.cpp',
        'stitch': 'src/modules/curve/DCurve/Utility/DCStitch.cpp',
        'mesh': 'src/modules/mesh/DMesh/Deform/DMVectorDeform.cpp',
        'curve': 'src/modules/curve/DCurve/deform/DCVectorDeform.cpp',
        'point': 'src/modules/point/DPoint/Deform/DPVectorDeform.cpp',
    }
    sources = {name: (root / path).read_text() for name, path in paths.items()}
    for name, source in sources.items():
        compute = source[source.index('MStatus '):source.index('DOMMY_COMPUTE_CATCH')]
        if re.search(r'(?:inputValue|outputValue)\([^,)]*\)', compute):
            raise SystemExit(name + ': unchecked data handle acquisition')
        if 'CHECK_MSTATUS(' in compute or not any(
                boundary in compute for boundary in ('createPluginData(', 'setPluginDataOutput<')):
            raise SystemExit(name + ': unchecked Maya failure boundary')
        if name in ('fill', 'stitch'):
            for boundary in ('isFiniteAffineTransform(', 'checkedTransformPoint(',
                             'buildCheckedArray(', 'copyCheckedArray(', 'generatedCurves.publishTo('):
                if boundary not in compute:
                    raise SystemExit(name + ': missing ' + boundary)
        else:
            for boundary in ('offsetAlongVector(', 'convertScalarAttribute(',
                             'convertVector3Attribute(',
                             'copyRepeatedFrom(' if name == 'point' else 'replacePositions('):
                if boundary not in compute:
                    raise SystemExit(name + ': missing ' + boundary)
        if name in ('point', 'curve'):
            worker = extract(compute, '[&](const tbb::blocked_range<std::size_t>& range)')
            if any(token in worker for token in ('inputValue', 'getAttr', 'getCurve', 'MFn', 'MGlobal')):
                raise SystemExit(name + ': Maya access in parallel worker')
            if 'valid.load(std::memory_order_relaxed)' not in compute:
                raise SystemExit(name + ': worker failure must propagate')
    if 'cvs.append(' in sources['fill'] or 'meshFn.create(' in sources['mesh']:
        raise SystemExit('Legacy unchecked array/mesh construction returned')
    if sources['fill'].index('checkedProduct(') > sources['fill'].index('createPluginData('):
        raise SystemExit('Fill generation must be planned before output allocation')

    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    bodies = [extract(utility, 'class DCurveOutputBatch final') + ';']
    for name in ('fill', 'stitch'):
        source = sources[name]
        start = source.index('    DCurveOutputBatch generatedCurves;')
        body = source[start:source.index('    outDCrvs->updateBBox();', start)]
        parameters = ('std::size_t fillCount, std::size_t sampling, std::size_t outputCurveCount'
                      if name == 'fill' else
                      'dommy::curve::StitchSamplingPlan plan, double range, double minimum, double offset')
        bodies.append('MStatus generate_' + name + '(const DCurveData* input, DCurveData* outDCrvs, '
                      + parameters + ') try {\nMStatus status;\n' + body +
                      '\nreturn MS::kSuccess; } catch (...) { return MS::kFailure; }')
    for name, reads in (('fill', 2), ('mesh', 6), ('curve', 6), ('point', 8)):
        source = sources[name]
        start = source.index('    MDataHandle handle =')
        end = source.index('    const std::size_t count =', start) if name == 'point' else source.index('    MFnPluginData pluginData;', start)
        body = source[start:end]
        if body.count('data.inputValue(') != reads:
            raise SystemExit(name + ': input-read inventory changed')
        unused = ('vectorAttribute',) if name == 'mesh' else (
            ('vectorAttribute', 'weightAttribute') if name in ('curve', 'point') else ())
        bodies.append('MStatus read_' + name + '(MDataBlock& data) { MStatus status; '
                      'const DCurveData value; const DCurveData* input = &value; (void)input;\n' + body +
                      '\n' + ''.join('(void)' + value + ';' for value in unused) +
                      '\nreturn MS::kSuccess; }')
        if name == 'fill':
            bodies.append('MStatus plan_fill(MDataBlock& data, const DCurveData* input) { '
                          'MStatus status;\n' + body + '\nreturn MS::kSuccess; }')
    for name in ('curve', 'point'):
        source = sources[name]
        start = source.index('std::atomic<bool> valid(true);')
        end_marker = 'if (!valid.load(std::memory_order_relaxed)) return MS::kInvalidParameter;'
        end = source.index(end_marker, start) + len(end_marker)
        bodies.append('MStatus deform_' + name + '(const DPointList& source, '
                      'const std::vector<Imath::V3d>& vectors, const std::vector<double>& weights, '
                      'double magnitude, double weight, bool clamp, double maximum, DPointList& output) {\n'
                      'DPointList points(source); const std::size_t count = points.size(); '
                      '(void)clamp; (void)maximum;\n' + source[start:end] +
                      '\noutput.swap(points); return MS::kSuccess; }')
    normals = extract(sources['mesh'], 'if (vectorType == 0 && count != 0U)')
    bodies.append('MStatus query_normals(const std::shared_ptr<Mesh>& value, std::size_t count, '
                  'std::vector<Imath::V3d>& vectors) { const auto* mesh = &value; '
                  'const int vectorType = 0; MStatus status;\n' + normals + '\nreturn MS::kSuccess; }')
    with tempfile.TemporaryDirectory(prefix='dommy-fill-vector-') as directory:
        temporary = Path(directory)
        (temporary / 'fill_vector_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/curve_fill_vector_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)
    print('Curve fill/stitch and vector-deformation regression checks: OK')


if __name__ == '__main__':
    main()

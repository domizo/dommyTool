#!/usr/bin/env python3
"""Guard curve-node publication and run extracted production input/batch paths.

The fixture models failed API calls, not Maya geometry or its allocator.
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
from check_active_cpp_safety import strip_comments_and_literals


def main():
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/curve/DCurve/Utility'
    curl = (folder / 'DCCurl.cpp').read_text()
    offset = (folder / 'DCOffset.cpp').read_text()
    noise = (root / 'src/modules/point/DPoint/Utility/DPFastNoise.cpp').read_text()
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    weight = (folder / 'DCWeightMap.cpp').read_text()
    stitch = (folder / 'DCStitch.cpp').read_text()
    # DCCopy's complete compute/shared emitter is executed in check_curve_copy_safety.py.
    # MeshCopy/Replicator complete computes (including batch failures) are exercised
    # in check_mesh_curve_copy_safety.py, rather than guarded by legacy line layout.
    # DCReform's full compute is covered in check_curve_utility_safety.py.
    nodes = ('DCFitCopy', 'DCFillBetween',
             'DCConnect', 'DCReflect', 'DCCutObject', 'DCStitch', 'DCReplace')
    for name in nodes:
        source = strip_comments_and_literals((folder / (name + '.cpp')).read_text())
        compute = source.split('DOMMY_COMPUTE_CATCH', 1)[0]
        if re.search(r'outDCrvs->insert\(', compute):
            raise SystemExit(name + ': incremental unchecked insertion returned')
        if not re.search(r'if \(!generatedCurves\.append\([^;]+\)\) return MS::kFailure;', compute):
            raise SystemExit(name + ': batch append failure must propagate')
        if compute.count('if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;') != 1:
            raise SystemExit(name + ': batch publication failure must propagate exactly once')
    extend = (folder / 'DCExtend.cpp').read_text()
    if 'if (!generatedCurves.empty() && !outDCrvs->insert(generatedCurves)) return MS::kFailure;' not in extend:
        raise SystemExit('DCExtend must propagate insertion failure')
    for name, source, reads, required in (
            ('DCWeightMap', weight, 9, ('weightMapPosition(', 'weightMapDistance(', 'checkedFloatCast(', 'DCPAddAttrScalar(')),
            ('DCStitch', stitch, 3, ('checkedStitchSamplingPlan(', 'buildCheckedArray(', 'copyCheckedArray(')),
            ('DCExtend', extend, 9, ('checkedCurveExtensionPlan(', 'getKnotDomain(', 'directionAtParameter(', 'isFloatRepresentablePoint('))):
        compute = extract(source, 'MStatus ' + name + '::compute(')
        if len(re.findall(r'inputValue\([^;]+;\s*CHECK_MSTATUS_AND_RETURN_IT\(status\)', compute)) != reads:
            raise SystemExit(name + ': every parameter read must be checked')
        for boundary in required + ('getValidInputDC(', 'createPluginData('):
            if boundary not in compute:
                raise SystemExit(name + ': missing checked boundary ' + boundary)
        if 'CHECK_MSTATUS(' in compute or 'new MPointArray' in compute:
            raise SystemExit(name + ': unchecked failure/allocation returned')
    if 'static_cast<int>(distValue)' in weight:
        raise SystemExit('DCWeightMap: unbounded float-to-int conversion returned')
    for source, plan in ((stitch, 'checkedStitchSamplingPlan('), (extend, 'checkedCurveExtensionPlan(')):
        if source.index(plan) > source.index('createPluginData('):
            raise SystemExit('Curve generation must be planned before output allocation')
    numeric_templates = ('getDCSingleValueToFloat', 'getDCPSingleValueToInt', 'getDCPSingleValueToFloat',
                         'getDCTwoArray', 'getDCPTwoArray', 'getDCVec2ToVectorArray',
                         'getDCPVec2ToVectorArray', 'getDCPVec3ToVectorArray')
    checked = 0
    for match in re.finditer(r'inline void (' + '|'.join(numeric_templates) + r')\(', utility):
        body = extract(utility[match.start():], 'inline void ' + match.group(1) + '(')
        if 'setLength(' in body or not any(call in body for call in ('requireArrayStatus(', 'requireTypedArray<')):
            raise SystemExit(match.group(1) + ': unchecked numeric conversion returned')
        checked += 1
    if checked != 13:
        raise SystemExit('Curve numeric adapter inventory changed; review new paths')
    for name, source, reads in (('DCCurl', curl, 8), ('DCOffset', offset, 10)):
        compute = extract(source, 'MStatus ' + name + '::compute(')
        if len(re.findall(r'inputValue\([^;]+;\s*CHECK_MSTATUS_AND_RETURN_IT\(status\)', compute)) != reads:
            raise SystemExit(name + ': every parameter read must be checked')
        if 'getParamAtPoint(' in compute or 'frameAtClosestPoint(' not in compute:
            raise SystemExit(name + ': checked closest-point frame queries are required')
        for required in ('getValidInputDC(', 'isFloatRepresentablePoint(', 'createPluginData('):
            if required not in compute:
                raise SystemExit(name + ': missing checked boundary ' + required)
    for required in ('convertWeightAttribute(', 'weights.size() != pointCount',
                     'convertScalarAttribute(', 'std::isfinite(localRadius)', 'std::isfinite(radians)'):
        if required not in curl:
            raise SystemExit('DCCurl: missing weight/numeric boundary ' + required)
    if offset.index('appendOffsetRebuildPlan(') > offset.index('sourceCurve.rebuild('):
        raise SystemExit('DCOffset must plan before rebuilding')
    for required in ('const bool isOpen = (*curve)->isCurveOpen();',
                     'convertVector3Attribute(',
                     'new DCurveInternalData(resultPoints, degree, isOpen)',
                     'if (!outputCurves->insert(generatedCurves)) return MS::kFailure;'):
        if required not in offset:
            raise SystemExit('DCOffset: missing topology/publication boundary ' + required)
    if 'getDCPAttributeMVectorArrayFromVec3(' in offset:
        raise SystemExit('DCOffset must not use unchecked legacy Maya vector allocation')
    if 'getParamAtPoint(' in noise or 'directionAtClosestPoint(' not in noise:
        raise SystemExit('DPFastNoise must use checked curve-direction queries')
    weight_reader = extract(utility, 'inline void getDCPAttributeMDoubleArray(')
    if 'convertWeightAttribute(' not in weight_reader or 'buildCheckedArray(' not in weight_reader:
        raise SystemExit('Curve weight export must validate conversion and Maya allocation')

    bodies = []
    for name, source, end, unused in (
            ('curl', curl, '    MFnPluginData pluginData;',
             'weightAttribute radiusAttribute arcAttribute spiralAttribute'),
            ('offset', offset, '    MVector xAxis =',
             'offsetAttribute useAimVector checkOverlap'),
            ('weight', weight, '    if (weightAttribute.empty())',
             'paramAttribute weightAttribute repeatWeight'),
            ('stitch', stitch, '    const double minimum =', 'sampling'),
            ('extend', extend, '    std::size_t inputPoints =',
             'startInvert endInvert separate deleteOriginal')):
        begin = source.index('    MDataHandle handle =')
        body = source[begin:source.index(end, begin)]
        bodies.append('MStatus read_' + name + '(MDataBlock& data) {\nMStatus status;\n' + body +
                      '\n' + ''.join('(void)' + variable + ';\n' for variable in unused.split()) +
                      'return MS::kSuccess;\n}')
    bodies.append(extract(utility, 'class DCurveOutputBatch final') + ';')
    begin = offset.index('        if (retained.size() < 2U)')
    closing = offset[begin:offset.index('        MPointArray resultPoints;', begin)]
    bodies.append('MStatus close_result(std::vector<MVector>& retained, PointCount points, '
                  'bool isOpen, bool& changed) {\n' + closing +
                  '\nchanged = topologyChanged; return MS::kSuccess;\n}')
    begin = stitch.index('                    const double fraction =')
    sample = stitch[begin:stitch.index('                }, samples);', begin)]
    bodies.append('template <typename Curve> MStatus sample_stitch(Curve& function, '
                  'double length, double minimum, double range, double offset, '
                  'dommy::curve::StitchSamplingPlan plan, unsigned int i, MPoint& point) {\n'
                  'const MMatrix matrix;\n' + sample + '\n}')
    begin = extend.index('                const MPoint original =')
    endpoint = extend[begin:extend.index('                if (separate) {', begin)]
    bodies.append('MStatus extend_endpoint(const std::shared_ptr<const std::vector<MPoint>>& sourcePoints, '
                  'bool start, bool separate, const MVector& direction, double length, '
                  'double startOffset, double endOffset, MPoint& first, MPoint& second) {\n' + endpoint +
                  '\nfirst = anchor; second = extended; return MS::kSuccess;\n}')
    with tempfile.TemporaryDirectory(prefix='dommy-curve-deformation-') as directory:
        temporary = Path(directory)
        (temporary / 'curve_deformation_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'curve_deformation_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/curve_deformation_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)
    print('Curve deformation and batch publication guards: OK')


if __name__ == '__main__':
    main()

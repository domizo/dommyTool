#!/usr/bin/env python3
"""Run extracted curve replacement/emission/blend paths with host-free failures."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps


def bodies(root):
    folder = root / 'src/modules/curve/DCurve/Utility'
    replace = (folder / 'DCReplace.cpp').read_text()
    connect = (folder / 'DCConnect.cpp').read_text()
    delete = (folder / 'DCDelete.cpp').read_text()
    clump = (folder / 'DCClumping.cpp').read_text()
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    for name, source, required in (
            ('DCReplace', replace, ('buildCurveReplacementPlan(', 'checkedNumericCast(', 'isFiniteAffineTransform(')),
            ('DCConnect', connect, ('checkedTransformPoint(', 'checkedTransformDirection(', 'isFiniteAffineTransform(', 'emitCurveChain<MPoint>(')),
            ('DCDelete', delete, ('buildWorldLineDeletionPlan(', 'getTransformMatrix()')),
            ('DCClumping', clump, ('checkedMaskedLinearBlend(', 'getValidInputDC('))):
        compute = extract(source, 'MStatus ' + name + '::compute(')
        for text in required:
            if text not in compute:
                raise SystemExit(name + ': missing checked boundary ' + text)
        if re.search(r'inputValue\([^,)]*\)', compute):
            raise SystemExit(name + ': unchecked input read returned')
    if replace.index('buildCurveReplacementPlan(') > replace.index('createPluginData('):
        raise SystemExit('DCReplace must plan before output allocation')
    if 'outDCrvs->insert(' in replace:
        raise SystemExit('DCReplace must publish a single checked batch')
    if 'getDCAttributeListPtr<' in replace:
        raise SystemExit('DCReplace must check typed attributes before dereference')
    bodies = [extract(utility, 'class DCurveOutputBatch final') + ';']
    curve_data = (root / 'src/core/Data/DCurveData.cpp').read_text()
    bodies.append(extract(curve_data, 'bool DCurveInternalData::appendOperationCost('))
    bodies.append(extract(replace, 'MStatus preflightCurveReplacements('))
    metadata = replace[replace.index('const auto pathAttribute ='):
                       replace.index('std::map<std::string, DPIndexList> resolved;')]
    bodies.append('MStatus validate_metadata(const DCurveData* input) {\n'
                  'const std::string sourceName = "src_path", restName = "restmat"; '
                  'const DMatrixArray* restMatrices = nullptr;\n' + metadata +
                  '\nreturn MS::kSuccess; }')
    resolve = extract(replace, 'const auto resolve =') + ';'
    bodies.append('bool resolve_replacements(const DCurveData& targetData, const DStringList& sourcePaths, '
                  'dommy::curve::CurveReplacementPlan& plan) {\n'
                  'const DCurveData* target = &targetData; const DStringList* paths = &sourcePaths; '
                  'std::map<std::string, DPIndexList> resolved;\n' + resolve + '\n'
                  'return dommy::curve::buildCurveReplacementPlan(paths->size(), target->numberOfCurves(), '
                  'resolve, [&](std::size_t index) { return target->getCurve(index).numberOfPoints(); }, plan); }')
    loop = extract(replace, 'for (const dommy::curve::CurveReplacement& replacement : plan.curves)')
    # Execute the actual admission call, not a manually added equivalent. This
    # lets a missing/ignored compute-level check fail the zero-copy assertions.
    admission = replace[replace.index('// Selected target CVs/knots,'):
                        replace.index('    MFnPluginData pluginData;')]
    bodies.append('MStatus apply_replacements(const DCurveData* input, const DCurveData* target, '
                  'const DMatrixArray* restMatrices, const dommy::curve::CurveReplacementPlan& plan, '
                  'DCurveData* outDCrvs) try {\n' +
                  admission +
                  'DCurveOutputBatch generatedCurves;\n' + loop + '\n'
                  'if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure; return MS::kSuccess; '
                  '} catch (...) { return MS::kFailure; }')
    loop = extract(connect, 'for (const auto& chain : chains)')
    bodies.append('MStatus emit_connections(const DCurveData* input, const std::vector<MMatrix>& matrices, '
                  'const std::vector<dommy::curve::CurveChain>& chains, DCurveData* outDCrvs) try {\n'
                  'const short degreeType = DC_kLinear, openType = DC_kOpen; '
                  'DCurveOutputBatch generatedCurves;\n' + loop + '\n'
                  'if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure; return MS::kSuccess; '
                  '} catch (...) { return MS::kFailure; }')
    loop = extract(clump, 'for (auto &pIt : points)')
    bodies.append('MStatus clump_points(const DPointList& source, const DPointList& target, '
                  'float weight, Ramp& clmpRampAttr, DPointList& output) {\n'
                  'DPointList points = source; const DPointList* tPts = &target; '
                  'const size_t tpSize = target.size(); size_t pIndex = 0;\n' + loop +
                  '\noutput.swap(points); return MS::kSuccess; }')
    return '\n\n'.join(bodies)


def execute(root, source, capture=False, overrides=None):
    with tempfile.TemporaryDirectory(prefix='dommy-world-replacement-') as directory:
        temporary = Path(directory)
        limits = (root / 'src/core/common/utility/DGenerationLimits.h').read_text()
        limits = limits.replace('kMaxGeneratedStringByteCount = 100000000U',
                                'kMaxGeneratedStringByteCount = 128U')
        limits = limits.replace('kMaxWorkItemCount = 100000000U', 'kMaxWorkItemCount = 512U')
        headers = temporary / 'common/utility'
        headers.mkdir(parents=True)
        (headers / 'DGenerationLimits.h').write_text(limits)
        for name in ('DCurveConnection.h', 'DIndexPattern.h', 'DMayaCheckedArray.h', 'DPointTransform.h'):
            text = (root / 'src/core/common/utility' / name).read_text()
            (headers / name).write_text(text.replace('#include "DGenerationLimits.h"',
                                                   '#include "common/utility/DGenerationLimits.h"'))
        for name, text in (overrides or {}).items():
            path = temporary / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        (temporary / 'curve_world_methods.inc').write_text(source)
        binary = temporary / 'tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(headers), '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            '-I' + str(root / 'src/core/common/utility'),
            str(root / 'tests/cpp/curve_world_replacement_tests.cpp'),
            str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment, cwd=temporary,
                       capture_output=capture, text=True,
                       preexec_fn=disable_core_dumps if os.name == 'posix' else None)


def admission_mutations(root, source):
    for label, old, new in (
            ('compute admission ignored',
             'DOMMY_RETURN_IF_FAILURE(preflightCurveReplacements(*input, *target, restMatrices, plan));',
             '(void)preflightCurveReplacements(*input, *target, restMatrices, plan);'),
            ('target aggregate cost omitted',
             '!target.getCurve(replacement.target).appendOperationCost(budget)', 'false'),
            ('rest frame admission omitted',
             '!dommy::numeric::isFiniteAffineTransform((*restMatrices)[replacement.source])', 'false'),
            ('source frame admission omitted',
             '!dommy::numeric::isFiniteAffineTransform(\n                input.getCurve(replacement.source).getTransformMatrix())', 'false')):
        assert source.count(old) == 1, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected replacement admission mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'common/utility/DPrimitiveCopyBudget.h'
    header = (root / 'src/core' / path).read_text()
    for label, old, new in (
            ('component group cost omitted',
             'addCopyGroupCost(owner.beginGrp(), owner.endGrp(), cost)', 'true'),
            ('numeric component cost omitted',
             'checkedProduct(count, point::mergeAttributeComponents(field->second->getAttrType())',
             'checkedProduct(count, 0U'),
            ('copied dynamic defaults omitted',
             'attribute::validateAttribute(field->second, count, cost)',
             'attribute::validateAttribute(field->second, count, cost, false)')):
        assert header.count(old) == 1, label
        try:
            execute(root, source, capture=True, overrides={path: header.replace(old, new, 1)})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected replacement admission mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = bodies(root)
    execute(root, source)
    admission_mutations(root, source)
    print('Curve world-space, replacement and blend regression checks: OK')

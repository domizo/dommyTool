#!/usr/bin/env python3
"""Run the real regularizer, payload budget and complete DPAdjustSpace compute."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/point/DPoint/Utility/DPAdjustSpace.cpp'
REGULARIZER = 'src/core/geometry/DPointSetRegularizer.h'
WORK_BUDGET = 'common/utility/DWorkBudget.h'


def computes(root):
    source = (root / SOURCE).read_text()
    declared = declarations('DPAdjustSpace', source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MString name()const{return "DPAdjustSpace";} MStatus compute(')
    return '\n'.join([extract((root / REGULARIZER).read_text(), 'namespace dommy {'),
        extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                'MStatus getValidInputDP(')] +
        declared + [extract(source, 'namespace {'),
        extract(source, 'MStatus DPAdjustSpace::compute('),
        'DOMMY_COMPUTE_CATCH("DPAdjustSpace")'])


def node_budget(root, source=None, capture=False):
    header = (root / 'src/core' / WORK_BUDGET).read_text()
    reduced = header.replace('std::size_t limit = kMaxWorkItemCount', 'std::size_t limit = 1024U')
    assert reduced != header
    run(root, source or computes(root), suite='adjust_space', capture=capture,
        arguments=['--work-budget'], extra_flags=['-DDOMMY_POISON_IMATH_DEFAULT'],
        header_overrides={WORK_BUDGET: reduced})


def sdk_check(root, sdk):
    # Maya and DPData headers are real; compile complete methods and actual
    # numeric, metadata and output helpers. The input reader is declaration-only.
    source = (root / SOURCE).read_text()
    body = '#include "adjust_space_sdk_boundaries.h"\n'
    body += extract(source, 'namespace {') + '\n'
    body += extract(source, 'MStatus DPAdjustSpace::compute(')
    body += '\nDOMMY_COMPUTE_CATCH("DPAdjustSpace")\n'
    body += extract(source, 'MStatus DPAdjustSpace::initialize(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-unused-parameter', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src/core'), '-I' + str(root / 'src/modules/point/DPoint'),
        '-I' + str(root / 'src/core/Debug'),
        '-I' + str(root / 'tests/cpp'), '-I' + str(root),
        '-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-adjust-sdk-') as directory:
        path = Path(directory) / 'adjust.cpp'
        path.write_text(body)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('DPAdjustSpace: header, complete methods, metadata budget and regularizer passed SDK checks')
    print('Real DPData/output-helper headers; input reader declared, Imath uses the test stub')
    print('No full legacy link or Maya execution')


def mutations(root):
    source = computes(root)
    float_check = '''if (!dommy::numeric::checkedFloatCast(
                        regularized.points[index][axis], stagedPoints[index][axis])) {
                    return MS::kInvalidParameter;
                }'''
    cases = [
        ('compute preflight omitted',
         'preflightAdjustSpace(*input, regularize, uniform);', ''),
        ('metadata preflight omitted',
         'dommy::point::preflightPointPayload(input, sizeof(DPoint), workspace);',
         'workspace.add(input.numberOfPoints(), sizeof(DPoint));'),
        ('point staging omitted from budget',
         '3U * sizeof(dommy::geometry::Point3) + sizeof(DPoint)', '0U'),
        ('spatial maps omitted from budget',
         'workspace.add(count, 2U * gridEntryBytes);', ''),
        ('uniform scratch omitted from budget', 'if (uniform) {', 'if (false && uniform) {'),
        ('neighbors omitted from budget',
         'workspace.add(2U * std::min(count, kRegularizerNeighborLimit), sizeof(WeightedNeighbor));', ''),
        ('unchecked float narrowing', float_check,
         'stagedPoints[index][axis] = static_cast<float>(regularized.points[index][axis]);'),
        ('incomplete float staging', 'for (unsigned int axis = 0U; axis < 3U; ++axis)',
         'for (unsigned int axis = 1U; axis < 3U; ++axis)'),
        ('input handle status ignored',
         'data.inputValue(aNeighborRadius, &status);\n    if (!status) return status;',
         'data.inputValue(aNeighborRadius, &status);'),
        ('remap status ignored', 'if (!output->remapPointFromIds(selection) ||',
         'if ((output->remapPointFromIds(selection), false) ||'),
        ('publication status ignored',
         'status = output.set(object);\n    if (!status) return status;',
         'status = output.set(object);'),
        ('clean status ignored', 'return data.setClean(plug);',
         '(void)data.setClean(plug); return MS::kSuccess;'),
    ]
    variants = []
    for label, old, new in cases:
        assert old in source, label
        variants.append((label, source.replace(old, new, 1)))
    copy_block = '''    MObject outputObject;
    DPData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DPData>(
        DPData::id, outputObject, output);
    if (!status) return status;
    if (!output) return MS::kFailure;
    output->setDPData(*input);
'''
    assert copy_block in source
    variants.append(('output copied before checked staging', source.replace(copy_block, '', 1)
        .replace('    dommy::geometry::RegularizedPointSet regularized;',
                 copy_block + '    dommy::geometry::RegularizedPointSet regularized;', 1)))
    numeric_helper = extract((root / 'src/core/common/utility/DNumericValidation.h').read_text(),
                             'inline bool checkedSpatialCellIndex(')
    for label, old, new in (
        ('unchecked integer conversion', '!checkedNumericCast(std::floor(scaled), candidate)',
         '(candidate = static_cast<long long>(std::floor(scaled)), false)'),
        ('integer neighbor headroom omitted',
         'candidate < std::numeric_limits<long long>::min() + 2LL ||\n'
         '        candidate > std::numeric_limits<long long>::max() - 2LL', 'false'),
    ):
        assert old in numeric_helper, label
        modified_helper = numeric_helper.replace(old, new, 1).replace(
            'checkedSpatialCellIndex(', 'checkedSpatialCellIndexMutated(')
        modified_source = 'namespace dommy { namespace numeric {\n' + modified_helper + '\n}}\n'
        modified_source += source.replace('numeric::checkedSpatialCellIndex(',
                                          'numeric::checkedSpatialCellIndexMutated(')
        try:
            run(root, modified_source, capture=True, suite='adjust_space', arguments=[],
                extra_flags=['-DDOMMY_POISON_IMATH_DEFAULT'])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    for label, variant in variants:
        try:
            run(root, variant, capture=True, suite='adjust_space', arguments=[],
                extra_flags=['-DDOMMY_POISON_IMATH_DEFAULT'])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    for label, old, new in (
        ('exhausted cells keep being scanned', 'groups.resize(activeCount);',
         '(void)activeCount;'),
        ('selection omitted from shared budget',
         'candidate.sourceIndices, budget)', 'candidate.sourceIndices, selectionBudget)'),
        ('grid lookup cost omitted',
         '!budget.consume(2U * generation::orderedContainerWork(m_buckets.size()))', 'false'),
    ):
        assert old in source, label
        changed = source.replace(old, new, 1)
        if label == 'selection omitted from shared budget':
            changed = changed.replace('if (!regularizer_detail::selectPoints(',
                'generation::WorkBudget selectionBudget;\n    if (!regularizer_detail::selectPoints(', 1)
        try:
            run(root, changed, capture=True, suite='adjust_space', arguments=[],
                extra_flags=['-DDOMMY_POISON_IMATH_DEFAULT'])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    bypass = source.replace('regularizationBudget,', 'dommy::generation::kMaxWorkItemCount,', 1)
    assert bypass != source
    try:
        node_budget(root, bypass, capture=True)
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: node bypasses shared budget'
        print('Detected mutation: node bypasses shared budget', flush=True)
    else:
        raise AssertionError('Undetected mutation: node bypasses shared budget')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    else:
        run(root, computes(root), suite='adjust_space', arguments=[],
            extra_flags=['-DDOMMY_POISON_IMATH_DEFAULT'])
        node_budget(root)
